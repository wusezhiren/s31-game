#include "s31/sms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smsplus.h"

#define SMS_FRAME_WIDTH 256
#define SMS_FRAME_HEIGHT 240
#define SMS_SRAM_SIZE 0x8000

struct s31_sms {
    uint8_t *rom;
    uint8_t *indexed;
    uint16_t *frame;
    uint16_t palette[256];
    s31_buttons_t buttons;
    s31_game_platform_t platform;
    bool powered;
    bool presented;
};

__attribute__((weak)) int s31_board_present_sms(const uint16_t *pixels,
                                                 uint16_t width, uint16_t height)
{
    (void)pixels;
    (void)width;
    (void)height;
    return 0;
}

s31_sms_t *s31_sms_create(void)
{
    s31_sms_t *instance = calloc(1, sizeof(*instance));
    if (!instance) return NULL;
    instance->indexed = calloc(SMS_FRAME_WIDTH * SMS_FRAME_HEIGHT, 1);
    instance->frame = calloc(SMS_FRAME_WIDTH * SMS_FRAME_HEIGHT, sizeof(uint16_t));
    if (!instance->indexed || !instance->frame) {
        s31_sms_destroy(instance);
        return NULL;
    }
    return instance;
}

void s31_sms_destroy(s31_sms_t *instance)
{
    if (!instance) return;
    if (instance->powered) {
        system_shutdown();
        z80_exit();
    }
    free(cart.sram);
    cart.sram = NULL;
    cart.rom = NULL;
    cart.loaded = 0;
    free(instance->rom);
    free(instance->indexed);
    free(instance->frame);
    free(instance);
}

int s31_sms_load_rom(s31_sms_t *instance, const uint8_t *data, size_t size,
                     s31_game_platform_t platform)
{
    if (!instance || !data || !size || instance->powered ||
        (platform != S31_GAME_SMS && platform != S31_GAME_GG &&
         platform != S31_GAME_SG1000)) return -1;

    size_t offset = size > 0x4000 && ((size / 512) & 1) ? 512 : 0;
    size_t source_size = size - offset;
    if (!source_size || source_size > 0x3fc000) return -1;
    size_t rom_size = (source_size + 0x3fff) & ~(size_t)0x3fff;
    if (rom_size < 0x8000) rom_size = 0x8000;
    instance->rom = malloc(rom_size);
    if (!instance->rom) return -1;
    for (size_t i = 0; i < rom_size; ++i) instance->rom[i] = data[offset + i % source_size];

    system_reset_config();
    option.console = platform == S31_GAME_SMS ? 2 : platform == S31_GAME_GG ? 3 : 5;
    option.overscan = 0;
    option.use_bios = 0;
    bitmap.data = instance->indexed;
    bitmap.width = SMS_FRAME_WIDTH;
    bitmap.height = SMS_FRAME_HEIGHT;
    bitmap.pitch = SMS_FRAME_WIDTH;
    if (!load_rom(instance->rom, (int)rom_size, (int)source_size)) return -1;
    system_poweron();
    instance->powered = true;
    instance->platform = platform;
    render_copy_palette(instance->palette);
    return 0;
}

void s31_sms_set_input(s31_sms_t *instance, s31_buttons_t buttons)
{
    if (instance) instance->buttons = buttons;
}

void s31_sms_run_frame(s31_sms_t *instance)
{
    if (!instance || !instance->powered) return;
    s31_buttons_t b = instance->buttons;
    input.pad[0] = ((b & S31_BUTTON_UP) ? INPUT_UP : 0) |
                   ((b & S31_BUTTON_DOWN) ? INPUT_DOWN : 0) |
                   ((b & S31_BUTTON_LEFT) ? INPUT_LEFT : 0) |
                   ((b & S31_BUTTON_RIGHT) ? INPUT_RIGHT : 0) |
                   ((b & S31_BUTTON_A) ? INPUT_BUTTON1 : 0) |
                   ((b & S31_BUTTON_B) ? INPUT_BUTTON2 : 0);
    input.pad[1] = 0;
    input.system = (b & S31_BUTTON_START) ?
                   (instance->platform == S31_GAME_GG ? INPUT_START : INPUT_PAUSE) : 0;
    system_frame(0);
    render_copy_palette(instance->palette);

    int x = bitmap.viewport.x;
    int y = bitmap.viewport.y;
    int width = bitmap.viewport.w;
    int height = bitmap.viewport.h;
    if (width <= 0 || height <= 0 || x < 0 || y < 0 ||
        x + width > bitmap.width || y + height > bitmap.height) return;
    for (int row = 0; row < height; ++row) {
        const uint8_t *src = bitmap.data + (y + row) * bitmap.pitch + x;
        uint16_t *dst = instance->frame + row * width;
        for (int col = 0; col < width; ++col) {
            uint16_t color = instance->palette[src[col]];
            dst[col] = (uint16_t)((color >> 8) | (color << 8));
        }
    }
    bitmap.viewport.changed = 0;
    if (!instance->presented) {
        instance->presented = true;
        printf("SMS Plus first frame: %dx%d\n", width, height);
    }
    s31_board_present_sms(instance->frame, (uint16_t)width, (uint16_t)height);
}

int s31_sms_load_sram(s31_sms_t *instance, const char *path)
{
    if (!instance || !instance->powered || !path || !cart.sram) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    size_t count = fread(cart.sram, 1, SMS_SRAM_SIZE, file);
    fclose(file);
    return count ? 0 : -1;
}

int s31_sms_save_sram(s31_sms_t *instance, const char *path)
{
    if (!instance || !instance->powered || !path || !cart.sram) return -1;
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    size_t count = fwrite(cart.sram, 1, SMS_SRAM_SIZE, file);
    int error = fclose(file);
    return count == SMS_SRAM_SIZE && !error ? 0 : -1;
}
