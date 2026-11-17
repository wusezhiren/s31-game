#include "s31/nes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nes.h"
#include "s31/video.h"
#include "s31/nes_video.h"
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

struct s31_nes {
    nes_t *core;
    uint8_t *rom;
    size_t rom_size;
    s31_buttons_t buttons;
    bool loaded;
    volatile bool paused;
};
static s31_nes_t *running_nes;

__attribute__((weak)) int s31_board_present_nes(const uint16_t *pixels, uint16_t width, uint16_t height) {
    (void)pixels; (void)width; (void)height; return 0;
}

void *nes_sram_malloc(int size) { return malloc((size_t)size); }
void nes_sram_free(void *ptr) { free(ptr); }
uint32_t nes_get_ms(void) { return 0; }
int nes_log_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stderr, format, args);
    va_end(args);
    return result;
}

int nes_initex(nes_t *core) { (void)core; return NES_OK; }
int nes_deinitex(nes_t *core) { (void)core; return NES_OK; }

int nes_draw(int x1, int y1, int x2, int y2, nes_color_t *pixels) {
    (void)x1; (void)y1; (void)x2; (void)y2;
    return s31_board_present_nes((const uint16_t *)pixels, NES_WIDTH, NES_HEIGHT);
}

static uint16_t to_nes_buttons(s31_buttons_t buttons) {
    uint16_t result = 0;
    if (buttons & S31_BUTTON_A) result |= 1u << 15;
    if (buttons & S31_BUTTON_B) result |= 1u << 14;
    if (buttons & S31_BUTTON_SELECT) result |= 1u << 13;
    if (buttons & S31_BUTTON_START) result |= 1u << 12;
    if (buttons & S31_BUTTON_UP) result |= 1u << 11;
    if (buttons & S31_BUTTON_DOWN) result |= 1u << 10;
    if (buttons & S31_BUTTON_LEFT) result |= 1u << 9;
    if (buttons & S31_BUTTON_RIGHT) result |= 1u << 8;
    return result;
}

s31_nes_t *s31_nes_create(void) {
    s31_nes_t *nes = calloc(1, sizeof(*nes));
    if (nes) nes->core = nes_init();
    if (!nes || !nes->core) { free(nes); return NULL; }
    return nes;
}

void s31_nes_destroy(s31_nes_t *nes) {
    if (!nes) return;
    if (nes->core) nes_deinit(nes->core);
    free(nes->rom);
    free(nes);
}

int s31_nes_load_rom(s31_nes_t *nes, const uint8_t *data, size_t size) {
    if (!nes || !nes->core || !data || size < 16) return -1;
    if (memcmp(data, "NES\x1a", 4) != 0) return -1;
    if (nes->rom) { nes_unload_rom(nes->core); free(nes->rom); nes->rom = NULL; }
    nes->rom = malloc(size);
    if (!nes->rom) return -1;
    memcpy(nes->rom, data, size);
    nes->rom_size = size;
    if (nes_load_rom(nes->core, nes->rom) != NES_OK) {
        free(nes->rom); nes->rom = NULL; nes->rom_size = 0; return -1;
    }
    nes->loaded = true;
    return 0;
}

void s31_nes_set_input(s31_nes_t *nes, s31_buttons_t buttons) {
    if (nes) nes->buttons = buttons;
}

void s31_nes_frame(s31_nes_t *nes) {
    if (!nes || !nes->loaded) return;
    nes->core->nes_cpu.joypad.joypad = to_nes_buttons(nes->buttons);
    nes_frame(nes->core);
}

void nes_frame(nes_t *core) {
    if (running_nes) core->nes_cpu.joypad.joypad = to_nes_buttons(running_nes->buttons);
#ifdef ESP_PLATFORM
    while (running_nes && running_nes->paused) vTaskDelay(pdMS_TO_TICKS(20));
    vTaskDelay(pdMS_TO_TICKS(16));
#endif
}

void s31_nes_run(s31_nes_t *nes) {
    if (!nes || !nes->loaded) return;
    running_nes = nes;
    nes_run(nes->core);
    running_nes = NULL;
}

void s31_nes_set_paused(s31_nes_t *nes, bool paused) { if (nes) nes->paused = paused; }
bool s31_nes_paused(const s31_nes_t *nes) { return nes && nes->paused; }
void s31_nes_stop(s31_nes_t *nes) {
    if (nes && nes->core) { nes->paused = false; nes->core->nes_quit = 1; }
}

int s31_nes_save_battery(s31_nes_t *nes, const char *path) {
    if (!nes || !nes->loaded || !nes->core->nes_rom.sram || !path) return -1;
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    size_t written = fwrite(nes->core->nes_rom.sram, 1, SRAM_SIZE, file);
    fclose(file);
    return written == SRAM_SIZE ? 0 : -1;
}

int s31_nes_load_battery(s31_nes_t *nes, const char *path) {
    if (!nes || !nes->loaded || !nes->core->nes_rom.sram || !path) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    size_t read = fread(nes->core->nes_rom.sram, 1, SRAM_SIZE, file);
    fclose(file);
    return read == SRAM_SIZE ? 0 : -1;
}

bool s31_nes_loaded(const s31_nes_t *nes) { return nes && nes->loaded; }
uint16_t s31_nes_mapper(const s31_nes_t *nes) {
    return nes && nes->core ? nes->core->nes_rom.mapper_number : 0xffff;
}
