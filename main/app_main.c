#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "s31_board_idf.h"
#include "s31_serial_upload.h"
#include "s31/config.h"
#include "s31/gamepad.h"
#include "s31/game_catalog.h"
#include "s31/nes.h"
#include "s31/gb.h"
#include "s31/sms.h"
#include "s31/pce.h"

extern esp_err_t s31_bt_gamepad_init(void);
extern esp_err_t s31_usb_gamepad_init(void);

typedef enum { APP_CATALOG, APP_RUNNING, APP_PAUSED } app_mode_t;
typedef enum { CORE_NONE, CORE_NES, CORE_GB, CORE_SMS, CORE_PCE } core_type_t;
static const char *TAG = "s31_game";
static s31_game_catalog_t catalog;
static s31_nes_t *active_nes;
static s31_gb_t *active_gb;
static s31_sms_t *active_sms;
static s31_pce_t *active_pce;
static core_type_t active_core;
static volatile s31_buttons_t input_buttons;
static volatile bool nes_task_running;
static volatile bool serial_upload_finished;
static volatile bool serial_command_pending;
static bool catalog_input_armed;
static size_t selected;
static s31_game_nav_repeat_t nav_repeat;
static char active_save_path[320];
static char serial_upload_name[96];
static char serial_command_name[96];
static char serial_command_action[8];
static volatile app_mode_t mode = APP_CATALOG;

static bool serial_upload_allowed(void *context)
{
    (void)context;
    return mode == APP_CATALOG;
}

static void serial_upload_complete(const char *filename, void *context)
{
    (void)context;
    snprintf(serial_upload_name, sizeof(serial_upload_name), "%s", filename);
    serial_upload_finished = true;
}

static void serial_command_received(const char *command, const char *argument, void *context)
{
    (void)context;
    if (serial_command_pending) {
        s31_serial_reply("CMD ERR BUSY");
        return;
    }
    snprintf(serial_command_action, sizeof(serial_command_action), "%s", command);
    snprintf(serial_command_name, sizeof(serial_command_name), "%s", argument ? argument : "");
    serial_command_pending = true;
}

static void on_key(int key, bool down, void *context)
{
    (void)context;
    s31_buttons_t button = key == 0 ? S31_BUTTON_A : key == 1 ? S31_BUTTON_B :
                           key == 2 ? S31_BUTTON_UP : S31_BUTTON_DOWN;
    if (down) input_buttons |= button; else input_buttons &= ~button;
}

static void on_gamepad(const s31_gamepad_state_t *state, void *context)
{
    (void)context;
    input_buttons = state->connected ? state->buttons : 0;
}

static void nes_task(void *arg)
{
    (void)arg;
    nes_task_running = true;
    vTaskDelay(pdMS_TO_TICKS(50));
    s31_nes_run(active_nes);
    nes_task_running = false;
    vTaskDelete(NULL);
}

static void gb_task(void *arg)
{
    (void)arg;
    nes_task_running = true;
    vTaskDelay(pdMS_TO_TICKS(50));
    while (active_gb && mode != APP_CATALOG) {
        if (mode == APP_RUNNING) s31_gb_run_frame(active_gb);
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    nes_task_running = false;
    vTaskDelete(NULL);
}

static void sms_task(void *arg)
{
    (void)arg;
    nes_task_running = true;
    vTaskDelay(pdMS_TO_TICKS(50));
    while (active_sms && mode != APP_CATALOG) {
        if (mode == APP_RUNNING) s31_sms_run_frame(active_sms);
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    nes_task_running = false;
    vTaskDelete(NULL);
}

static void pce_task(void *arg)
{
    (void)arg;
    nes_task_running = true;
    vTaskDelay(pdMS_TO_TICKS(50));
    while (active_pce && mode != APP_CATALOG) {
        if (mode == APP_RUNNING) s31_pce_run_frame(active_pce);
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    nes_task_running = false;
    vTaskDelete(NULL);
}

static void refresh_catalog(const char *status)
{
    int count = s31_game_catalog_scan(s31_board_rom_root(), &catalog);
    size_t supported = 0;
    for (size_t i = 0; i < catalog.count; ++i) supported += catalog.entries[i].supported ? 1u : 0u;
    if (selected >= catalog.count) selected = catalog.count ? catalog.count - 1 : 0;
    ESP_LOGI(TAG, "game catalog: %d entries, %u supported, %u unsupported", count,
             (unsigned)supported, (unsigned)(catalog.count - supported));
    s31_board_show_catalog(&catalog, selected, status);
}

static int start_selected_game(void)
{
    if (selected >= catalog.count) return -1;
    const s31_game_entry_t *entry = &catalog.entries[selected];
    if (!entry->supported) return -2;
    FILE *file = fopen(entry->path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0) { if (file) fclose(file); return -3; }
    long length = ftell(file);
    rewind(file);
    uint8_t *data = length > 0 ? malloc((size_t)length) : NULL;
    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data); fclose(file); return -4;
    }
    fclose(file);
    if (entry->platform == S31_GAME_NES) {
        active_nes = s31_nes_create();
        if (!active_nes || s31_nes_load_rom(active_nes, data, (size_t)length) != 0) { free(data); if(active_nes)s31_nes_destroy(active_nes);active_nes=NULL;return -5; }
        active_core = CORE_NES;
    } else if (entry->platform == S31_GAME_GB || entry->platform == S31_GAME_GBC) {
        active_gb = s31_gb_create();
        if (!active_gb || s31_gb_load_rom(active_gb, data, (size_t)length) != 0) { free(data); if(active_gb)s31_gb_destroy(active_gb);active_gb=NULL;return -5; }
        active_core = CORE_GB;
    } else if (entry->platform == S31_GAME_SMS || entry->platform == S31_GAME_GG || entry->platform == S31_GAME_SG1000) {
        active_sms = s31_sms_create();
        if (!active_sms || s31_sms_load_rom(active_sms, data, (size_t)length, entry->platform) != 0) { free(data); if(active_sms)s31_sms_destroy(active_sms);active_sms=NULL;return -5; }
        active_core = CORE_SMS;
    } else if (entry->platform == S31_GAME_PCE) {
        active_pce = s31_pce_create();
        if (!active_pce || s31_pce_load_rom(active_pce, data, (size_t)length) != 0) { free(data); if(active_pce)s31_pce_destroy(active_pce);active_pce=NULL;return -5; }
        active_core = CORE_PCE;
    } else {
        free(data); return -2;
    }
    free(data);
    mkdir("/sdcard/saves", 0775);
    snprintf(active_save_path, sizeof(active_save_path), "/sdcard/saves/%08lx.sav",
             (unsigned long)entry->size ^ (unsigned long)(active_nes ? s31_nes_mapper(active_nes) : entry->platform));
    if(active_nes){s31_nes_load_battery(active_nes,active_save_path);s31_nes_set_input(active_nes,input_buttons);}
    if(active_gb){s31_gb_load_sram(active_gb,active_save_path);s31_gb_set_input(active_gb,input_buttons);}
    if(active_sms){s31_sms_load_sram(active_sms,active_save_path);s31_sms_set_input(active_sms,input_buttons);}
    if(active_pce){s31_pce_load_sram(active_pce,active_save_path);s31_pce_set_input(active_pce,input_buttons);}
    mode = APP_RUNNING;
    s31_board_show_game();
    nes_task_running = true;
    TaskFunction_t task = active_core == CORE_NES ? nes_task : active_core == CORE_GB ? gb_task : active_core == CORE_SMS ? sms_task : pce_task;
    uint32_t stack_size = active_core == CORE_PCE ? 12288 : 8192;
    if (xTaskCreatePinnedToCoreWithCaps(task, "emu_core", stack_size, NULL, 5, NULL, 1,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        nes_task_running = false;
        s31_nes_destroy(active_nes); active_nes = NULL;
        s31_gb_destroy(active_gb); active_gb = NULL;
        s31_sms_destroy(active_sms); active_sms = NULL;
        s31_pce_destroy(active_pce); active_pce = NULL;
        active_core = CORE_NONE; mode = APP_CATALOG; return -6;
    }
    ESP_LOGI(TAG, "%s started: %s", s31_game_platform_name(entry->platform), entry->path);
    return 0;
}

static void stop_game(void)
{
    if (!active_nes && !active_gb && !active_sms && !active_pce) return;
    mode = APP_CATALOG;
    catalog_input_armed = false;
    if (active_nes) s31_nes_stop(active_nes);
    while (nes_task_running) vTaskDelay(pdMS_TO_TICKS(10));
    if (active_save_path[0]) {
        if (active_nes) s31_nes_save_battery(active_nes, active_save_path);
        if (active_gb) s31_gb_save_sram(active_gb, active_save_path);
        if (active_sms) s31_sms_save_sram(active_sms, active_save_path);
        if (active_pce) s31_pce_save_sram(active_pce, active_save_path);
    }
    s31_nes_destroy(active_nes);
    if(active_gb)s31_gb_destroy(active_gb);
    if(active_sms)s31_sms_destroy(active_sms);
    if(active_pce)s31_pce_destroy(active_pce);
    active_nes = NULL;
    active_gb = NULL;
    active_sms = NULL;
    active_pce = NULL;
    active_core = CORE_NONE;
    active_save_path[0] = 0;
    mode = APP_CATALOG;
    refresh_catalog("saved");
}

void app_main(void)
{
    s31_video_t video = {0};
    ESP_ERROR_CHECK(s31_board_init(&video));
    ESP_ERROR_CHECK(s31_board_start_input(on_key, NULL));
    ESP_ERROR_CHECK(s31_serial_upload_start(serial_upload_allowed, serial_upload_complete,
                                             serial_command_received, NULL));
    s31_gamepad_set_callback(on_gamepad, NULL);
    if (s31_bt_gamepad_init() != ESP_OK) ESP_LOGW(TAG, "Bluetooth gamepad unavailable");
    if (s31_usb_gamepad_init() != ESP_OK) ESP_LOGW(TAG, "USB gamepad unavailable");
    refresh_catalog(NULL);
    s31_buttons_t previous = 0;
    for (;;) {
        if (serial_command_pending) {
            serial_command_pending = false;
            if (!strcmp(serial_command_action, "STOP")) {
                if (mode == APP_CATALOG) s31_serial_reply("STOP OK");
                else { stop_game(); s31_serial_reply("STOP OK"); }
            } else if (!strcmp(serial_command_action, "RUN")) {
                if (mode != APP_CATALOG || s31_serial_upload_active()) {
                    s31_serial_reply("RUN ERR BUSY");
                } else {
                    refresh_catalog(NULL);
                    size_t match = catalog.count;
                    for (size_t i = 0; i < catalog.count; ++i) {
                        const char *name = strrchr(catalog.entries[i].path, '/');
                        if (!strcmp(name ? name + 1 : catalog.entries[i].path, serial_command_name)) {
                            match = i;
                            break;
                        }
                    }
                    if (match == catalog.count) {
                        s31_serial_reply("RUN ERR NOTFOUND");
                    } else {
                        selected = match;
                        int result = start_selected_game();
                        char response[48];
                        if (result) snprintf(response, sizeof(response), "RUN ERR %d", result);
                        else snprintf(response, sizeof(response), "RUN OK");
                        vTaskDelay(pdMS_TO_TICKS(20));
                        s31_serial_reply(response);
                    }
                }
            }
        }
        if (serial_upload_finished && mode == APP_CATALOG) {
            serial_upload_finished = false;
            refresh_catalog(serial_upload_name);
        }
        s31_buttons_t buttons = input_buttons;
        s31_buttons_t pressed = buttons & ~previous;
        if (mode == APP_CATALOG) {
            if (s31_serial_upload_active()) {
                previous = buttons;
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
            if (!catalog_input_armed) {
                if (buttons == 0) {
                    catalog_input_armed = true;
                    previous = 0;
                } else {
                    previous = buttons;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
            uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
            s31_buttons_t navigation = s31_game_catalog_repeat(&nav_repeat, buttons, pressed, now_ms);
            if ((navigation & S31_BUTTON_UP) && selected) selected--;
            if ((navigation & S31_BUTTON_DOWN) && selected + 1 < catalog.count) selected++;
            if (navigation & S31_BUTTON_LEFT) selected = s31_game_catalog_page_move(selected, catalog.count, -1);
            if (navigation & S31_BUTTON_RIGHT) selected = s31_game_catalog_page_move(selected, catalog.count, 1);
            if (navigation)
                s31_board_show_catalog(&catalog, selected, NULL);
            if (pressed & S31_BUTTON_A) {
                int result = start_selected_game();
                if (result) s31_board_show_catalog(&catalog, selected, "unsupported or load failed");
            }
            if (pressed & S31_BUTTON_B) refresh_catalog("refreshed");
        } else if (active_nes || active_gb || active_sms || active_pce) {
            if(active_nes)s31_nes_set_input(active_nes,buttons);
            if(active_gb)s31_gb_set_input(active_gb,buttons);
            if(active_sms)s31_sms_set_input(active_sms,buttons);
            if(active_pce)s31_pce_set_input(active_pce,buttons);
            s31_buttons_t pause = S31_BUTTON_START | S31_BUTTON_SELECT;
            s31_buttons_t exit = S31_BUTTON_START | S31_BUTTON_L | S31_BUTTON_R;
            if ((buttons & exit) == exit && (previous & exit) != exit) stop_game();
            else if ((buttons & pause) == pause && (previous & pause) != pause) {
                bool paused = mode != APP_PAUSED;
                if(active_nes)s31_nes_set_paused(active_nes,paused);
                if(paused&&active_save_path[0]){if(active_nes)s31_nes_save_battery(active_nes,active_save_path);if(active_gb)s31_gb_save_sram(active_gb,active_save_path);if(active_sms)s31_sms_save_sram(active_sms,active_save_path);if(active_pce)s31_pce_save_sram(active_pce,active_save_path);}
                mode = paused ? APP_PAUSED : APP_RUNNING;
                s31_board_show_pause(paused);
            }
        }
        previous = buttons;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
