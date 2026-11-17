#include "s31_board_idf.h"

#include <stdlib.h>
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include "s31/config.h"
#include "s31/nes_video.h"
#include "s31/scale.h"

static const char *TAG = "s31_game_board";
static esp_codec_dev_handle_t speaker;
static s31_board_key_cb_t key_callback;
static void *key_context;
static lv_obj_t *nes_image;
static uint16_t *nes_scaled_frame;
static lv_image_dsc_t nes_image_dsc;
static lv_obj_t *gb_image;
static uint16_t *gb_scaled_frame;
static lv_image_dsc_t gb_image_dsc;
static lv_obj_t *sms_image;
static uint16_t *sms_scaled_frame;
static lv_image_dsc_t sms_image_dsc;
static lv_obj_t *pce_image;
static uint16_t *pce_scaled_frame;
static lv_image_dsc_t pce_image_dsc;
static lv_obj_t *catalog_label;
static lv_obj_t *catalog_root;
static lv_obj_t *catalog_title;
static lv_obj_t *catalog_status;
static lv_obj_t *catalog_rows[10];
static lv_obj_t *catalog_badges[10];
static lv_obj_t *catalog_names[10];
static lv_obj_t *catalog_states[10];
LV_FONT_DECLARE(s31_font_cjk_16);

static void use_catalog_font(lv_obj_t *label)
{
    lv_obj_set_style_text_font(label, &s31_font_cjk_16, LV_PART_MAIN);
}

static lv_color_t platform_color(s31_game_platform_t platform)
{
    static const uint32_t colors[] = {
        0x64748b, 0xef4444, 0x84cc16, 0x22c55e, 0x3b82f6,
        0x06b6d4, 0x8b5cf6, 0xf97316, 0xec4899, 0xeab308,
    };
    return lv_color_hex(colors[platform <= S31_GAME_ARCADE ? platform : 0]);
}

static void hide_catalog(void)
{
    if (catalog_root) lv_obj_set_hidden(catalog_root, true);
    if (catalog_label) lv_obj_set_hidden(catalog_label, true);
}

static void keys_task(void *arg)
{
    (void)arg;
    int last = -1;
    for (;;) {
        int mv = bsp_adc_read_mv();
        int key = mv < 100 ? -1 : mv < 600 ? 0 : mv < 1080 ? 1 :
                  mv < 1605 ? 2 : mv < 2200 ? 3 : -1;
        if (key != last && key_callback) key_callback(key, key >= 0, key_context);
        last = key;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static esp_err_t mount_sd(void)
{
    gpio_config_t power = {.pin_bit_mask = 1ULL << 60, .mode = GPIO_MODE_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&power), TAG, "SD power GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_60, 1), TAG, "SD power disable failed");
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_config_t bus_reset = {
        .pin_bit_mask = (1ULL << 20) | (1ULL << 21) | (1ULL << 22) |
                        (1ULL << 23) | (1ULL << 24) | (1ULL << 25),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bus_reset), TAG, "SD bus reset failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_60, 0), TAG, "SD power enable failed");
    vTaskDelay(pdMS_TO_TICKS(1000));

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = GPIO_NUM_24;
    slot.cmd = GPIO_NUM_25;
    slot.d0 = GPIO_NUM_20;
    slot.d1 = GPIO_NUM_21;
    slot.d2 = GPIO_NUM_22;
    slot.d3 = GPIO_NUM_23;
    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_card_t *card = NULL;
    esp_err_t err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount, &card);
    if (err == ESP_OK) sdmmc_card_print_info(stdout, card);
    return err;
}

esp_err_t s31_board_init(s31_video_t *video)
{
    if (!video) return ESP_ERR_INVALID_ARG;
    esp_lcd_panel_handle_t panel = NULL;
    const bsp_display_config_t panel_cfg = {
        .num_fbs = 2,
        .bounce_lines = 10,
    };
    ESP_RETURN_ON_ERROR(bsp_display_new(&panel_cfg, &panel, NULL), TAG,
                        "RGB panel allocation failed");
    bsp_display_cfg_t display_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = true,
        .flags = {.buff_spiram = true},
    };
    display_cfg.lvgl_port_cfg.task_affinity = 0;
    lv_display_t *display = bsp_display_start_with_config(&display_cfg);
    if (!display) return ESP_ERR_NO_MEM;
    lv_font_glyph_dsc_t glyph;
    bool cjk_ok = lv_font_get_glyph_dsc(&s31_font_cjk_16, &glyph, 0x4e2d, 0) &&
                  lv_font_get_glyph_dsc(&s31_font_cjk_16, &glyph, 0x6e38, 0);
    bool ascii_ok = lv_font_get_glyph_dsc(&s31_font_cjk_16, &glyph, 'A', 0);
    bool bitmap_ok = false;
    if (lv_font_get_glyph_dsc(&s31_font_cjk_16, &glyph, 0x4e2d, 0)) {
        lv_draw_buf_t *glyph_buf = lv_draw_buf_create(glyph.box_w, glyph.box_h, LV_COLOR_FORMAT_A8, 0);
        if (glyph_buf && s31_font_cjk_16.get_glyph_bitmap(&glyph, glyph_buf)) {
            size_t bytes = glyph_buf->data_size;
            for (size_t i = 0; i < bytes; ++i) {
                if (glyph_buf->data[i]) { bitmap_ok = true; break; }
            }
        }
        if (glyph_buf) lv_draw_buf_destroy(glyph_buf);
    }
    ESP_LOGI(TAG, "CJK font self-test: Chinese=%s ASCII=%s Bitmap=%s",
             cjk_ok ? "OK" : "FAIL", ascii_ok ? "OK" : "FAIL",
             bitmap_ok ? "OK" : "FAIL");
    if (s31_video_init(video) != 0) return ESP_ERR_NO_MEM;
    nes_scaled_frame = heap_caps_calloc(S31_NES_SCALED_WIDTH * S31_NES_SCALED_HEIGHT,
                                        sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!nes_scaled_frame) return ESP_ERR_NO_MEM;
    nes_image_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    nes_image_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    nes_image_dsc.header.w = S31_NES_SCALED_WIDTH;
    nes_image_dsc.header.h = S31_NES_SCALED_HEIGHT;
    nes_image_dsc.header.stride = S31_NES_SCALED_WIDTH * sizeof(uint16_t);
    nes_image_dsc.data_size = S31_NES_SCALED_WIDTH * S31_NES_SCALED_HEIGHT * sizeof(uint16_t);
    nes_image_dsc.data = (const uint8_t *)nes_scaled_frame;
    gb_scaled_frame = heap_caps_calloc(480u * 432u, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!gb_scaled_frame) return ESP_ERR_NO_MEM;
    gb_image_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    gb_image_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    gb_image_dsc.header.w = 480;
    gb_image_dsc.header.h = 432;
    gb_image_dsc.header.stride = 480 * sizeof(uint16_t);
    gb_image_dsc.data_size = 480 * 432 * sizeof(uint16_t);
    gb_image_dsc.data = (const uint8_t *)gb_scaled_frame;
    sms_scaled_frame = heap_caps_calloc(512u * 480u, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!sms_scaled_frame) return ESP_ERR_NO_MEM;
    sms_image_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    sms_image_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    sms_image_dsc.header.w = 512;
    sms_image_dsc.header.h = 480;
    sms_image_dsc.header.stride = 512 * sizeof(uint16_t);
    sms_image_dsc.data_size = 512 * 480 * sizeof(uint16_t);
    sms_image_dsc.data = (const uint8_t *)sms_scaled_frame;
    pce_scaled_frame = heap_caps_calloc(704u * 480u, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pce_scaled_frame) return ESP_ERR_NO_MEM;
    pce_image_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    pce_image_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    pce_image_dsc.data = (const uint8_t *)pce_scaled_frame;
    if (!bsp_display_lock(1000)) return ESP_ERR_TIMEOUT;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    nes_image = lv_image_create(screen);
    if (!nes_image) { bsp_display_unlock(); return ESP_ERR_NO_MEM; }
    lv_image_set_src(nes_image, &nes_image_dsc);
    lv_obj_center(nes_image);
    lv_obj_set_hidden(nes_image, true);
    gb_image = lv_image_create(screen);
    lv_image_set_src(gb_image, &gb_image_dsc);
    lv_obj_center(gb_image);
    lv_obj_set_hidden(gb_image, true);
    sms_image = lv_image_create(screen);
    lv_image_set_src(sms_image, &sms_image_dsc);
    lv_obj_center(sms_image);
    lv_obj_set_hidden(sms_image, true);
    pce_image = lv_image_create(screen);
    lv_obj_center(pce_image);
    lv_obj_set_hidden(pce_image, true);
    catalog_label = lv_label_create(screen);
    use_catalog_font(catalog_label);
    lv_label_set_text(catalog_label, "");
    lv_obj_align(catalog_label, LV_ALIGN_TOP_LEFT, 18, 14);
    lv_obj_set_style_text_color(catalog_label, lv_color_white(), 0);
    lv_obj_set_hidden(catalog_label, true);

    catalog_root = lv_obj_create(screen);
    lv_obj_remove_style_all(catalog_root);
    lv_obj_set_size(catalog_root, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(catalog_root, lv_color_hex(0x07111f), 0);
    lv_obj_set_style_bg_opa(catalog_root, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(catalog_root, &s31_font_cjk_16, 0);
    lv_obj_set_scrollbar_mode(catalog_root, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *header = lv_obj_create(catalog_root);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 24, 18);
    lv_obj_set_size(header, 752, 54);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x10243d), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(header, 12, 0);
    catalog_title = lv_label_create(header);
    use_catalog_font(catalog_title);
    lv_label_set_text(catalog_title, "S31 游戏库");
    lv_obj_set_style_text_color(catalog_title, lv_color_hex(0xf8fafc), 0);
    lv_obj_align(catalog_title, LV_ALIGN_LEFT_MID, 18, 0);
    catalog_status = lv_label_create(header);
    use_catalog_font(catalog_status);
    lv_obj_set_style_text_color(catalog_status, lv_color_hex(0x93c5fd), 0);
    lv_obj_align(catalog_status, LV_ALIGN_RIGHT_MID, -18, 0);

    for (size_t i = 0; i < 10; ++i) {
        lv_obj_t *row = lv_obj_create(catalog_root);
        catalog_rows[i] = row;
        lv_obj_remove_style_all(row);
        lv_obj_set_pos(row, 24, 82 + (int32_t)i * 34);
        lv_obj_set_size(row, 752, 30);
        lv_obj_set_style_radius(row, 7, 0);

        lv_obj_t *badge = lv_label_create(row);
        catalog_badges[i] = badge;
        use_catalog_font(badge);
        lv_obj_set_pos(badge, 8, 4);
        lv_obj_set_size(badge, 72, 22);
        lv_obj_set_style_radius(badge, 5, 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(badge, lv_color_white(), 0);
        lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t *name = lv_label_create(row);
        catalog_names[i] = name;
        use_catalog_font(name);
        lv_obj_set_pos(name, 94, 5);
        lv_obj_set_width(name, 540);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_color(name, lv_color_hex(0xe2e8f0), 0);

        lv_obj_t *state = lv_label_create(row);
        catalog_states[i] = state;
        use_catalog_font(state);
        lv_obj_set_width(state, 88);
        lv_obj_align(state, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_set_style_text_align(state, LV_TEXT_ALIGN_RIGHT, 0);
    }

    lv_obj_t *footer = lv_label_create(catalog_root);
    use_catalog_font(footer);
    lv_label_set_text(footer, "方向键  选择     A  启动     B  刷新");
    lv_obj_set_style_text_color(footer, lv_color_hex(0x64748b), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -16);
    ESP_LOGI(TAG, "LVGL catalog ready: black background, game images hidden");
    bsp_display_unlock();

    speaker = bsp_audio_codec_speaker_init();
    if (!speaker) return ESP_ERR_NOT_FOUND;
    esp_codec_dev_sample_info_t sample = {
        .bits_per_sample = 16, .channel = 2, .sample_rate = S31_AUDIO_RATE,
    };
    ESP_RETURN_ON_ERROR(esp_codec_dev_open(speaker, &sample), TAG, "ES8389 open failed");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(speaker, 80), TAG, "speaker volume failed");
    ESP_RETURN_ON_ERROR(bsp_audio_poweramp_enable(true), TAG, "power amplifier failed");

    esp_err_t sd_err = mount_sd();
    if (sd_err != ESP_OK) ESP_LOGW(TAG, "SD unavailable: %s", esp_err_to_name(sd_err));
    ESP_LOGI(TAG, "S31 board ready: RGB 800x480, PSRAM frame buffers, ES8389, SDMMC 4-bit");
    return ESP_OK;
}

esp_err_t s31_board_start_input(s31_board_key_cb_t callback, void *context)
{
    key_callback = callback;
    key_context = context;
    return xTaskCreatePinnedToCoreWithCaps(keys_task, "s31_keys", 3072, NULL, 4, NULL, 0,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t s31_board_audio_write(const int16_t *samples, size_t bytes)
{
    if (!speaker || !samples || !bytes) return ESP_ERR_INVALID_ARG;
    return esp_codec_dev_write(speaker, (void *)samples, bytes);
}

const char *s31_board_rom_root(void) { return "/sdcard/roms"; }

int s31_board_present_nes(const uint16_t *pixels, uint16_t width, uint16_t height)
{
    if (!pixels || width != S31_NES_WIDTH || height != S31_NES_HEIGHT || !nes_scaled_frame) return -1;
    s31_nes_scale_2x_rgb565(pixels, nes_scaled_frame);
    if (!bsp_display_lock(20)) return -1;
    hide_catalog();
    lv_obj_invalidate(nes_image);
    bsp_display_unlock();
    return 0;
}

void s31_board_show_catalog(const s31_game_catalog_t *catalog, size_t selected, const char *status)
{
    if (!catalog || !catalog_label) return;
    if (!bsp_display_lock(1000)) {
        ESP_LOGE(TAG, "catalog display lock timed out");
        return;
    }
    size_t page_start = (selected / 10u) * 10u;
    char status_text[96];
    snprintf(status_text, sizeof(status_text), "%u / %u%s%s",
             (unsigned)(catalog->count ? selected + 1 : 0), (unsigned)catalog->count,
             status ? "   " : "", status ? status : "");
    lv_label_set_text(catalog_status, status_text);
    for (size_t row = 0; row < 10; ++row) {
        size_t index = page_start + row;
        if (index >= catalog->count) {
            lv_obj_set_hidden(catalog_rows[row], true);
            continue;
        }
        const s31_game_entry_t *entry = &catalog->entries[index];
        bool active = index == selected;
        lv_obj_set_hidden(catalog_rows[row], false);
        lv_obj_set_style_bg_color(catalog_rows[row], active ? lv_color_hex(0x1d4ed8) : lv_color_hex(0x0d1b2d), 0);
        lv_obj_set_style_bg_opa(catalog_rows[row], active ? LV_OPA_COVER : LV_OPA_70, 0);
        lv_label_set_text(catalog_badges[row], s31_game_platform_icon(entry->platform));
        lv_obj_set_style_bg_color(catalog_badges[row], platform_color(entry->platform), 0);
        lv_label_set_text(catalog_names[row], entry->title);
        lv_obj_set_style_text_color(catalog_names[row], active ? lv_color_white() : lv_color_hex(0xe2e8f0), 0);
        const char *state_text = entry->supported ? "可运行" :
                                 (entry->platform == S31_GAME_NES && !entry->valid_header) ? "无效ROM" :
                                 (entry->platform == S31_GAME_GBA || entry->platform == S31_GAME_ARCADE) ? "缺少核心" :
                                 "不支持";
        lv_label_set_text(catalog_states[row], state_text);
        lv_obj_set_style_text_color(catalog_states[row], entry->supported ? lv_color_hex(0x86efac) : lv_color_hex(0xfca5a5), 0);
    }
    if (!catalog->count) {
        lv_label_set_text(catalog_title, "/SDCARD/ROMS 中没有游戏");
    } else {
        lv_label_set_text(catalog_title, "S31 游戏库");
    }
    lv_obj_set_hidden(catalog_label, true);
    lv_obj_set_hidden(catalog_root, false);
    lv_obj_set_hidden(nes_image, true);
    lv_obj_set_hidden(gb_image, true);
    lv_obj_set_hidden(sms_image, true);
    lv_obj_set_hidden(pce_image, true);
    bsp_display_unlock();
}

void s31_board_show_pause(bool paused)
{
    if (!catalog_label || !bsp_display_lock(100)) return;
    if (paused) {
        if (catalog_root) lv_obj_set_hidden(catalog_root, true);
        lv_label_set_text(catalog_label, "PAUSED\nStart+Select: resume\nL+R+Start: save and exit");
        lv_obj_center(catalog_label);
        lv_obj_set_hidden(catalog_label, false);
    } else {
        lv_obj_align(catalog_label, LV_ALIGN_TOP_LEFT, 18, 14);
        lv_obj_set_hidden(catalog_label, true);
    }
    bsp_display_unlock();
}

void s31_board_show_game(void)
{
    if (!nes_image || !bsp_display_lock(100)) return;
    hide_catalog();
    lv_obj_set_hidden(nes_image, false);
    lv_obj_set_hidden(gb_image, true);
    lv_obj_set_hidden(sms_image, true);
    lv_obj_set_hidden(pce_image, true);
    bsp_display_unlock();
}

int s31_board_present_gb(const uint16_t *pixels, uint16_t width, uint16_t height)
{
    if (!pixels || width != 160 || height != 144 || !gb_scaled_frame) return -1;
    s31_scale_rgb565_integer(pixels, width, height, gb_scaled_frame, 3);
    if (!bsp_display_lock(20)) return -1;
    hide_catalog();
    lv_obj_set_hidden(nes_image, true);
    lv_obj_set_hidden(gb_image, false);
    lv_obj_set_hidden(sms_image, true);
    lv_obj_set_hidden(pce_image, true);
    lv_obj_invalidate(gb_image);
    bsp_display_unlock();
    return 0;
}

int s31_board_present_pce(const uint16_t *pixels, uint16_t width, uint16_t height)
{
    if (!pixels || !width || !height || width > 352 || height > 242 || !pce_scaled_frame) return -1;
    unsigned scale = width * 2u <= 800u && height * 2u <= 480u ? 2u : 1u;
    uint16_t output_width = (uint16_t)(width * scale);
    uint16_t output_height = (uint16_t)(height * scale);
    s31_scale_rgb565_integer(pixels, width, height, pce_scaled_frame, scale);
    if (!bsp_display_lock(20)) return -1;
    pce_image_dsc.header.w = output_width;
    pce_image_dsc.header.h = output_height;
    pce_image_dsc.header.stride = output_width * sizeof(uint16_t);
    pce_image_dsc.data_size = output_width * output_height * sizeof(uint16_t);
    lv_image_set_src(pce_image, &pce_image_dsc);
    lv_obj_center(pce_image);
    hide_catalog();
    lv_obj_set_hidden(nes_image, true);
    lv_obj_set_hidden(gb_image, true);
    lv_obj_set_hidden(sms_image, true);
    lv_obj_set_hidden(pce_image, false);
    lv_obj_invalidate(pce_image);
    bsp_display_unlock();
    return 0;
}

int s31_board_audio_write_pce(const int16_t *samples, size_t bytes)
{
    return s31_board_audio_write(samples, bytes) == ESP_OK ? 0 : -1;
}

int s31_board_present_sms(const uint16_t *pixels, uint16_t width, uint16_t height)
{
    if (!pixels || !width || !height || width > 256 || height > 240 || !sms_scaled_frame) return -1;
    unsigned scale = width <= 160 && height <= 144 ? 3u : 2u;
    uint16_t output_width = (uint16_t)(width * scale);
    uint16_t output_height = (uint16_t)(height * scale);
    s31_scale_rgb565_integer(pixels, width, height, sms_scaled_frame, scale);
    if (!bsp_display_lock(20)) return -1;
    sms_image_dsc.header.w = output_width;
    sms_image_dsc.header.h = output_height;
    sms_image_dsc.header.stride = output_width * sizeof(uint16_t);
    sms_image_dsc.data_size = output_width * output_height * sizeof(uint16_t);
    lv_image_set_src(sms_image, &sms_image_dsc);
    lv_obj_center(sms_image);
    hide_catalog();
    lv_obj_set_hidden(nes_image, true);
    lv_obj_set_hidden(gb_image, true);
    lv_obj_set_hidden(sms_image, false);
    lv_obj_invalidate(sms_image);
    bsp_display_unlock();
    return 0;
}
