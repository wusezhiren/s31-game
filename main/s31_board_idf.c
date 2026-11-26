#include "s31_board_idf.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include "s31/config.h"
#include "s31/nes_video.h"
#include "s31/scale.h"

static const char *TAG = "s31_game_board";
static esp_codec_dev_handle_t speaker;
static s31_board_key_cb_t key_callback;
static void *key_context;
static void (*touch_callback)(int32_t x, int32_t y, bool down, void *context);
static void *touch_context;
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

static lv_indev_read_cb_t touch_read;
static lv_obj_t *level_label;
static lv_timer_t *level_timer;
static esp_lcd_panel_handle_t lcd_panel;
static void *lcd_fbs[2];
static ppa_client_handle_t ppa_srm;
static SemaphoreHandle_t direct_fb_mux;
static volatile bool direct_game_active;
static uint8_t direct_fb_idx;
static uint16_t direct_last_w;
static uint16_t direct_last_h;
static unsigned direct_last_scale;
static char direct_osd_text[24];
static volatile int64_t direct_osd_expire_us;
static bool direct_osd_drawn[2];

/* 5x7 极简点阵字模，用于 PPA 直通模式下的音量/亮度轻提示，不触发 LVGL 刷屏 */
static const uint8_t *osd_glyph_5x7(char c)
{
    static const uint8_t digits[10][7] = {
        {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}, /* 0 */
        {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e}, /* 1 */
        {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}, /* 2 */
        {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e}, /* 3 */
        {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}, /* 4 */
        {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e}, /* 5 */
        {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e}, /* 6 */
        {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
        {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}, /* 8 */
        {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c}, /* 9 */
    };
    static const uint8_t g_v[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04};
    static const uint8_t g_o[7] = {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    static const uint8_t g_l[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
    static const uint8_t g_b[7] = {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e};
    static const uint8_t g_r[7] = {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
    static const uint8_t g_i[7] = {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e};
    static const uint8_t g_pct[7] = {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03};
    static const uint8_t g_sp[7] = {0, 0, 0, 0, 0, 0, 0};
    if (c >= '0' && c <= '9') return digits[c - '0'];
    switch (c) {
    case 'V': return g_v;
    case 'O': return g_o;
    case 'L': return g_l;
    case 'B': return g_b;
    case 'R': return g_r;
    case 'I': return g_i;
    case '%': return g_pct;
    default: return g_sp;
    }
}

/* 在 RGB888 帧缓冲顶部居中叠加轻量提示条，并仅刷新该区域缓存 */
static void draw_direct_osd(uint8_t *fb, const char *text)
{
    const int box_w = 180;
    const int box_h = 32;
    const int box_x = (BSP_LCD_H_RES - box_w) / 2;
    const int box_y = 14;
    const size_t stride = (size_t)BSP_LCD_H_RES * 3u;
    for (int y = 0; y < box_h; ++y) {
        uint8_t *row = fb + (size_t)(box_y + y) * stride + (size_t)box_x * 3u;
        bool border = (y == 0 || y == box_h - 1);
        for (int x = 0; x < box_w; ++x) {
            uint8_t c = (border || x == 0 || x == box_w - 1) ? 0x90 : 0x10;
            row[x * 3 + 0] = c;
            row[x * 3 + 1] = c;
            row[x * 3 + 2] = c;
        }
    }
    size_t len = strlen(text);
    int text_w = (int)len * 12 - 2;
    int start_x = box_x + (box_w - text_w) / 2;
    int start_y = box_y + (box_h - 14) / 2;
    for (size_t idx = 0; idx < len; ++idx) {
        const uint8_t *glyph = osd_glyph_5x7(text[idx]);
        int gx = start_x + (int)idx * 12;
        for (int r = 0; r < 7; ++r) {
            uint8_t bits = glyph[r];
            for (int c = 0; c < 5; ++c) {
                if (!(bits & (0x10 >> c))) continue;
                for (int sy = 0; sy < 2; ++sy) {
                    uint8_t *dst = fb + (size_t)(start_y + r * 2 + sy) * stride + (size_t)(gx + c * 2) * 3u;
                    dst[0] = dst[1] = dst[2] = 0xff;
                    dst[3] = dst[4] = dst[5] = 0xff;
                }
            }
        }
    }
    esp_cache_msync(fb + (size_t)box_y * stride, (size_t)box_h * stride,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

static void clear_direct_fbs(void)
{
    const size_t chunk = 16u * 1024u;
    for (int i = 0; i < 2; ++i) {
        if (!lcd_fbs[i]) continue;
        memset(lcd_fbs[i], 0, BSP_LCD_FB_SIZE);
        /* 分块回写缓存，避免单次长临界区阻塞 RGB LCD bounce buffer 中断 */
        for (size_t off = 0; off < BSP_LCD_FB_SIZE; off += chunk) {
            size_t len = (off + chunk <= BSP_LCD_FB_SIZE) ? chunk : (BSP_LCD_FB_SIZE - off);
            esp_cache_msync((uint8_t *)lcd_fbs[i] + off, len, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        }
        direct_osd_drawn[i] = false;
    }
}

static bool direct_ppa_ready(void)
{
    return ppa_srm && lcd_panel && lcd_fbs[0] && lcd_fbs[1] && direct_fb_mux;
}

static void hide_level(lv_timer_t *timer)
{
    lv_obj_set_hidden(level_label, true);
    lv_timer_pause(timer);
}

static void show_level(const char *name, const char *osd_tag, int level)
{
    if (direct_game_active && direct_ppa_ready()) {
        snprintf(direct_osd_text, sizeof(direct_osd_text), "%s %d%%", osd_tag, level);
        direct_osd_expire_us = esp_timer_get_time() + 1000000;
        return;
    }
    if (!level_label || !level_timer || !bsp_display_lock(100)) return;
    lv_label_set_text_fmt(level_label, "%s %d%%", name, level);
    lv_obj_set_hidden(level_label, false);
    lv_timer_reset(level_timer);
    lv_timer_resume(level_timer);
    bsp_display_unlock();
}

static void read_touch(lv_indev_t *indev, lv_indev_data_t *data)
{
    static bool pressed;
    static lv_point_t last;
    touch_read(indev, data);
    if (!touch_callback) return;
    if (data->state == LV_INDEV_STATE_PRESSED) {
        /* The controller Y axis already follows the display. */
        last.x = BSP_LCD_H_RES - 1 - data->point.x;
        last.y = data->point.y;
        data->point = last;
        if (!pressed) {
            ESP_LOGI(TAG, "touch down x=%d y=%d", (int)last.x, (int)last.y);
        }
        touch_callback(last.x, last.y, true, touch_context);
        pressed = true;
    } else if (pressed) {
        pressed = false;
        ESP_LOGI(TAG, "touch up x=%d y=%d", (int)last.x, (int)last.y);
        touch_callback(last.x, last.y, false, touch_context);
    }
}

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

/* 打印指定目录下的条目，便于串口确认 SD 卡实际路径与文件名 */
static void log_sd_dir(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGW(TAG, "SD dir open failed: %s", path);
        return;
    }
    ESP_LOGI(TAG, "SD dir listing: %s", path);
    struct dirent *entry;
    int shown = 0;
    while ((entry = readdir(dir)) != NULL && shown < 32) {
        if (entry->d_name[0] == '.') continue;
        ESP_LOGI(TAG, "  [%s] %s", entry->d_type == DT_DIR ? "DIR" : "FILE", entry->d_name);
        ++shown;
    }
    closedir(dir);
}

static esp_err_t mount_sd(void)
{
    /* 同时兼容初版与 V1.1 硬件：GPIO60 与 BSP_SD_POWER(GPIO57) 均执行下电复位后再拉低上电 */
    gpio_config_t power = {
        .pin_bit_mask = (1ULL << 60) | (1ULL << BSP_SD_POWER),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&power), TAG, "SD power GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_60, 1), TAG, "SD power disable failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(BSP_SD_POWER, 1), TAG, "SD V1.1 power disable failed");
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_config_t bus_reset = {
        .pin_bit_mask = (1ULL << 20) | (1ULL << 21) | (1ULL << 22) |
                        (1ULL << 23) | (1ULL << 24) | (1ULL << 25),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bus_reset), TAG, "SD bus reset failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_60, 0), TAG, "SD power enable failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(BSP_SD_POWER, 0), TAG, "SD V1.1 power enable failed");
    vTaskDelay(pdMS_TO_TICKS(500));

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = GPIO_NUM_24;
    slot.cmd = GPIO_NUM_25;
    slot.d0 = GPIO_NUM_20;
    slot.d1 = GPIO_NUM_21;
    slot.d2 = GPIO_NUM_22;
    slot.d3 = GPIO_NUM_23;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_card_t *card = NULL;
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < 3; ++attempt) {
        err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount, &card);
        if (err == ESP_OK) {
            sdmmc_card_print_info(stdout, card);
            log_sd_dir("/sdcard");
            log_sd_dir("/sdcard/roms");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "SD mount try %d failed: %s", attempt + 1, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(250));
    }
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
    lv_indev_t *touch_indev = bsp_display_get_input_dev();
    if (touch_indev && (touch_read = lv_indev_get_read_cb(touch_indev))) {
        lv_indev_set_read_cb(touch_indev, read_touch);
        ESP_LOGI(TAG, "touch sample reader installed");
    } else {
        ESP_LOGW(TAG, "touch input device unavailable");
    }

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
    level_label = lv_label_create(screen);
    use_catalog_font(level_label);
    lv_obj_set_style_bg_color(level_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(level_label, LV_OPA_80, 0);
    lv_obj_set_style_text_color(level_label, lv_color_white(), 0);
    lv_obj_set_style_pad_all(level_label, 8, 0);
    lv_obj_align(level_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_hidden(level_label, true);
    level_timer = lv_timer_create(hide_level, 1000, NULL);
    if (level_timer) lv_timer_pause(level_timer);
    ESP_LOGI(TAG, "LVGL catalog ready: black background, game images hidden");
    bsp_display_unlock();

    /* 初始化 PPA 硬件缩放通道与 RGB 双帧缓冲直通路径，游戏运行期绕过 LVGL 软转 */
    lcd_panel = panel;
    if (bsp_display_get_framebuffers(&lcd_fbs[0], &lcd_fbs[1]) == ESP_OK && lcd_fbs[0] && lcd_fbs[1]) {
        direct_fb_mux = xSemaphoreCreateMutex();
        const ppa_client_config_t ppa_cfg = {
            .oper_type = PPA_OPERATION_SRM,
            .max_pending_trans_num = 1,
            /* 使用中等突发长度，在保持高吞吐的同时为 RGB LCD 留出 PSRAM 带宽 */
            .data_burst_length = PPA_DATA_BURST_LENGTH_64,
        };
        esp_err_t ppa_err = ppa_register_client(&ppa_cfg, &ppa_srm);
        if (ppa_err == ESP_OK) {
            ESP_LOGI(TAG, "PPA SRM ready for direct RGB888 game rendering");
        } else {
            ESP_LOGW(TAG, "PPA SRM unavailable (%s), fallback to LVGL rendering", esp_err_to_name(ppa_err));
        }
    }

    speaker = bsp_audio_codec_speaker_init();
    if (!speaker) return ESP_ERR_NOT_FOUND;
    esp_codec_dev_sample_info_t sample = {
        .bits_per_sample = 16, .channel = 2, .sample_rate = S31_AUDIO_RATE,
    };
    ESP_RETURN_ON_ERROR(esp_codec_dev_open(speaker, &sample), TAG, "ES8389 open failed");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(speaker, 20), TAG, "speaker volume failed");
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

void s31_board_set_touch_callback(void (*callback)(int32_t x, int32_t y, bool down, void *context),
                                  void *context)
{
    touch_callback = callback;
    touch_context = context;
}

esp_err_t s31_board_audio_write(const int16_t *samples, size_t bytes)
{
    if (!speaker || !samples || !bytes) return ESP_ERR_INVALID_ARG;
    return esp_codec_dev_write(speaker, (void *)samples, bytes);
}

esp_err_t s31_board_set_volume(int volume)
{
    if (!speaker) return ESP_ERR_INVALID_STATE;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    esp_err_t result = esp_codec_dev_set_out_vol(speaker, volume);
    ESP_LOGI(TAG, "touch volume=%d result=%s", volume, esp_err_to_name(result));
    if (result == ESP_OK) show_level("音量", "VOL", volume);
    return result;
}

esp_err_t s31_board_set_brightness(int brightness)
{
    if (brightness < 10) brightness = 10;
    if (brightness > 100) brightness = 100;
    esp_err_t result = bsp_display_brightness_set(brightness);
    ESP_LOGI(TAG, "touch brightness=%d result=%s", brightness, esp_err_to_name(result));
    if (result == ESP_OK) show_level("亮度", "BRI", brightness);
    return result;
}

const char *s31_board_rom_root(void) { return "/sdcard/roms"; }

/* 使用硬件 PPA SRM 将原生 RGB565 帧一步整数倍缩放并转为 RGB888 直写隐藏帧缓冲 */
static int present_frame_direct(const uint16_t *pixels, uint16_t width, uint16_t height, unsigned scale)
{
    if (!direct_ppa_ready() || !pixels || !width || !height || !scale) return -1;
    uint32_t out_w = (uint32_t)width * scale;
    uint32_t out_h = (uint32_t)height * scale;
    if (out_w > BSP_LCD_H_RES || out_h > BSP_LCD_V_RES) return -1;
    uint32_t off_x = (BSP_LCD_H_RES - out_w) / 2u;
    uint32_t off_y = (BSP_LCD_V_RES - out_h) / 2u;

    if (xSemaphoreTake(direct_fb_mux, pdMS_TO_TICKS(20)) != pdTRUE) return 0;
    if (!direct_game_active) {
        xSemaphoreGive(direct_fb_mux);
        return 0;
    }
    if (width != direct_last_w || height != direct_last_h || scale != direct_last_scale) {
        clear_direct_fbs();
        direct_last_w = width;
        direct_last_h = height;
        direct_last_scale = scale;
    }

    uint8_t fb_idx = direct_fb_idx;
    uint8_t *dst_fb = (uint8_t *)lcd_fbs[fb_idx];
    /* 按 30 行输入条带分段调用 PPA，缩短单次 M2C 缓存失效关中断时间并兼顾 60FPS 吞吐 */
    const uint32_t strip_h = 30u;
    esp_err_t err = ESP_OK;
    for (uint32_t sy = 0; sy < height; sy += strip_h) {
        uint32_t bh = (sy + strip_h <= height) ? strip_h : (height - sy);
        const ppa_srm_oper_config_t srm_cfg = {
            .in = {
                .buffer = pixels,
                .pic_w = width,
                .pic_h = height,
                .block_w = width,
                .block_h = bh,
                .block_offset_x = 0,
                .block_offset_y = sy,
                .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
            },
            .out = {
                .buffer = dst_fb,
                .buffer_size = BSP_LCD_FB_SIZE,
                .pic_w = BSP_LCD_H_RES,
                .pic_h = BSP_LCD_V_RES,
                .block_offset_x = off_x,
                .block_offset_y = off_y + sy * scale,
                .srm_cm = PPA_SRM_COLOR_MODE_RGB888,
            },
            .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
            .scale_x = (float)scale,
            .scale_y = (float)scale,
            .mirror_x = false,
            .mirror_y = false,
            .rgb_swap = false,
            .byte_swap = false,
            .alpha_update_mode = PPA_ALPHA_NO_CHANGE,
            .mode = PPA_TRANS_MODE_BLOCKING,
            .user_data = NULL,
        };
        err = ppa_do_scale_rotate_mirror(ppa_srm, &srm_cfg);
        if (err != ESP_OK) break;
    }
    if (err != ESP_OK) {
        static bool ppa_err_logged;
        if (!ppa_err_logged) {
            ppa_err_logged = true;
            ESP_LOGE(TAG, "PPA SRM failed (%s), falling back to LVGL", esp_err_to_name(err));
        }
        xSemaphoreGive(direct_fb_mux);
        return -1;
    }

    int64_t now_us = esp_timer_get_time();
    if (now_us < direct_osd_expire_us) {
        draw_direct_osd(dst_fb, direct_osd_text);
        direct_osd_drawn[fb_idx] = true;
    } else if (direct_osd_drawn[fb_idx]) {
        /* 提示过期后清理游戏画面上方可能残留的顶部黑边区域 */
        if (off_y > 0) {
            size_t clear_lines = off_y < 48u ? off_y : 48u;
            size_t clear_bytes = clear_lines * (size_t)BSP_LCD_H_RES * 3u;
            memset(dst_fb, 0, clear_bytes);
            esp_cache_msync(dst_fb, clear_bytes,
                            ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        }
        direct_osd_drawn[fb_idx] = false;
    }

    /* 传 1x1 窗口仅触发底层切换 cur_fb_index，避免对整帧做多余的全屏 cache 回写 */
    esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, 1, 1, dst_fb);
    direct_fb_idx = (uint8_t)(fb_idx ^ 1u);
    xSemaphoreGive(direct_fb_mux);
    return 0;
}

int s31_board_present_nes(const uint16_t *pixels, uint16_t width, uint16_t height)
{
    if (!pixels || width != S31_NES_WIDTH || height != S31_NES_HEIGHT) return -1;
    if (present_frame_direct(pixels, width, height, 2u) == 0) return 0;
    if (!nes_scaled_frame) return -1;
    s31_nes_scale_2x_rgb565(pixels, nes_scaled_frame);
    if (!bsp_display_lock(20)) return -1;
    hide_catalog();
    lv_obj_invalidate(nes_image);
    bsp_display_unlock();
    return 0;
}

void s31_board_show_catalog(const s31_game_catalog_t *catalog, size_t first, size_t selected, const char *status)
{
    if (!catalog || !catalog_label) return;
    if (direct_fb_mux && xSemaphoreTake(direct_fb_mux, pdMS_TO_TICKS(1000)) == pdTRUE) {
        direct_game_active = false;
        xSemaphoreGive(direct_fb_mux);
    } else {
        direct_game_active = false;
    }
    if (!bsp_display_lock(1000)) {
        ESP_LOGE(TAG, "catalog display lock timed out");
        return;
    }
    char status_text[96];
    snprintf(status_text, sizeof(status_text), "%u / %u%s%s",
             (unsigned)(catalog->count ? selected + 1 : 0), (unsigned)catalog->count,
             status ? "   " : "", status ? status : "");
    lv_label_set_text(catalog_status, status_text);
    for (size_t row = 0; row < 10; ++row) {
        size_t index = first + row;
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
        lv_label_set_text(catalog_title, "/sdcard/roms 中没有游戏");
    } else {
        lv_label_set_text(catalog_title, "S31 游戏库");
    }
    lv_obj_set_hidden(catalog_label, true);
    lv_obj_set_hidden(catalog_root, false);
    lv_obj_set_hidden(nes_image, true);
    lv_obj_set_hidden(gb_image, true);
    lv_obj_set_hidden(sms_image, true);
    lv_obj_set_hidden(pce_image, true);
    lv_obj_invalidate(lv_screen_active());
    bsp_display_unlock();
}

void s31_board_show_pause(bool paused)
{
    if (!catalog_label) return;
    if (paused) {
        if (direct_fb_mux && xSemaphoreTake(direct_fb_mux, pdMS_TO_TICKS(1000)) == pdTRUE) {
            direct_game_active = false;
            xSemaphoreGive(direct_fb_mux);
        } else {
            direct_game_active = false;
        }
        if (!bsp_display_lock(1000)) return;
        if (catalog_root) lv_obj_set_hidden(catalog_root, true);
        lv_label_set_text(catalog_label, "PAUSED\nStart+Select: resume\nL+R+Start: save and exit");
        lv_obj_center(catalog_label);
        lv_obj_set_hidden(catalog_label, false);
        lv_obj_invalidate(lv_screen_active());
        bsp_display_unlock();
    } else {
        if (direct_fb_mux && xSemaphoreTake(direct_fb_mux, pdMS_TO_TICKS(1000)) != pdTRUE) return;
        if (bsp_display_lock(1000)) {
            lv_obj_align(catalog_label, LV_ALIGN_TOP_LEFT, 18, 14);
            lv_obj_set_hidden(catalog_label, true);
            if (level_label) lv_obj_set_hidden(level_label, true);
            if (direct_ppa_ready()) {
                /* 立刻刷新清空 LVGL 脏区，之后交回 PPA 直通渲染避免竞态 */
                lv_refr_now(NULL);
            }
            bsp_display_unlock();
        }
        if (direct_ppa_ready()) {
            clear_direct_fbs();
            direct_fb_idx = 0;
            direct_osd_expire_us = 0;
            direct_game_active = true;
            esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, 1, 1, lcd_fbs[0]);
        }
        if (direct_fb_mux) xSemaphoreGive(direct_fb_mux);
    }
}

void s31_board_show_game(void)
{
    if (!nes_image) return;
    if (direct_fb_mux && xSemaphoreTake(direct_fb_mux, pdMS_TO_TICKS(1000)) != pdTRUE) return;
    if (bsp_display_lock(1000)) {
        hide_catalog();
        if (level_label) lv_obj_set_hidden(level_label, true);
        if (direct_ppa_ready()) {
            lv_obj_set_hidden(nes_image, true);
            lv_obj_set_hidden(gb_image, true);
            lv_obj_set_hidden(sms_image, true);
            lv_obj_set_hidden(pce_image, true);
            /* 同步刷完纯黑屏并清空 LVGL 脏区，运行期不再唤醒 LVGL 绘制 */
            lv_refr_now(NULL);
        } else {
            lv_obj_set_hidden(nes_image, false);
            lv_obj_set_hidden(gb_image, true);
            lv_obj_set_hidden(sms_image, true);
            lv_obj_set_hidden(pce_image, true);
        }
        bsp_display_unlock();
    }
    if (direct_ppa_ready()) {
        clear_direct_fbs();
        direct_fb_idx = 0;
        direct_last_w = 0;
        direct_last_h = 0;
        direct_last_scale = 0;
        direct_osd_expire_us = 0;
        direct_game_active = true;
        esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, 1, 1, lcd_fbs[0]);
    }
    if (direct_fb_mux) xSemaphoreGive(direct_fb_mux);
}

int s31_board_present_gb(const uint16_t *pixels, uint16_t width, uint16_t height)
{
    if (!pixels || width != 160 || height != 144) return -1;
    if (present_frame_direct(pixels, width, height, 3u) == 0) return 0;
    if (!gb_scaled_frame) return -1;
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
    if (!pixels || !width || !height || width > 352 || height > 242) return -1;
    unsigned scale = width * 2u <= 800u && height * 2u <= 480u ? 2u : 1u;
    if (present_frame_direct(pixels, width, height, scale) == 0) return 0;
    if (!pce_scaled_frame) return -1;
    uint16_t output_width = (uint16_t)(width * scale);
    uint16_t output_height = (uint16_t)(height * scale);
    s31_scale_rgb565_integer(pixels, width, height, pce_scaled_frame, scale);
    if (!bsp_display_lock(20)) return -1;
    if (pce_image_dsc.header.w != output_width || pce_image_dsc.header.h != output_height) {
        pce_image_dsc.header.w = output_width;
        pce_image_dsc.header.h = output_height;
        pce_image_dsc.header.stride = output_width * sizeof(uint16_t);
        pce_image_dsc.data_size = output_width * output_height * sizeof(uint16_t);
        lv_image_set_src(pce_image, &pce_image_dsc);
        lv_obj_center(pce_image);
    }
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
    if (!pixels || !width || !height || width > 256 || height > 240) return -1;
    unsigned scale = width <= 160 && height <= 144 ? 3u : 2u;
    if (present_frame_direct(pixels, width, height, scale) == 0) return 0;
    if (!sms_scaled_frame) return -1;
    uint16_t output_width = (uint16_t)(width * scale);
    uint16_t output_height = (uint16_t)(height * scale);
    s31_scale_rgb565_integer(pixels, width, height, sms_scaled_frame, scale);
    if (!bsp_display_lock(20)) return -1;
    if (sms_image_dsc.header.w != output_width || sms_image_dsc.header.h != output_height) {
        sms_image_dsc.header.w = output_width;
        sms_image_dsc.header.h = output_height;
        sms_image_dsc.header.stride = output_width * sizeof(uint16_t);
        sms_image_dsc.data_size = output_width * output_height * sizeof(uint16_t);
        lv_image_set_src(sms_image, &sms_image_dsc);
        lv_obj_center(sms_image);
    }
    hide_catalog();
    lv_obj_set_hidden(nes_image, true);
    lv_obj_set_hidden(gb_image, true);
    lv_obj_set_hidden(sms_image, false);
    lv_obj_invalidate(sms_image);
    bsp_display_unlock();
    return 0;
}
