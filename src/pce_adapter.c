#include "s31/pce.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pce-go.h"
#include "pce.h"
#include "psg.h"

#define PCE_MAX_WIDTH 352
#define PCE_MAX_HEIGHT 242
#define PCE_AUDIO_FRAMES 800
#define PCE_SRAM_SIZE 0x8000

struct s31_pce {
    uint8_t *indexed;
    uint16_t *frame;
    int16_t *audio;
    uint16_t palette[256];
    uint16_t width;
    uint16_t height;
    s31_buttons_t buttons;
    bool initialized;
    bool loaded;
    bool presented;
};

static s31_pce_t *active;

__attribute__((weak)) int s31_board_present_pce(const uint16_t *pixels, uint16_t width, uint16_t height)
{ (void)pixels; (void)width; (void)height; return 0; }
__attribute__((weak)) int s31_board_audio_write_pce(const int16_t *samples, size_t bytes)
{ (void)samples; (void)bytes; return 0; }

uint8_t *osd_gfx_framebuffer(int width, int height)
{
    if (!active || width < 1 || width > PCE_MAX_WIDTH || height < 1 || height > PCE_MAX_HEIGHT) return NULL;
    active->width = (uint16_t)width;
    active->height = (uint16_t)height;
    return active->indexed + 16;
}
void osd_input_read(uint8_t joypads[8]) { if (joypads) memcpy(joypads, PCE.Joypad.regs, 8); }
void osd_vsync(void) {}

s31_pce_t *s31_pce_create(void)
{
    if (active) return NULL;
    s31_pce_t *pce = calloc(1, sizeof(*pce));
    if (!pce) return NULL;
    pce->indexed = calloc(XBUF_WIDTH * XBUF_HEIGHT, 1);
    pce->frame = calloc(PCE_MAX_WIDTH * PCE_MAX_HEIGHT, sizeof(uint16_t));
    pce->audio = calloc(PCE_AUDIO_FRAMES * 2, sizeof(int16_t));
    uint16_t *palette = PalettePCE(16);
    if (!pce->indexed || !pce->frame || !pce->audio || !palette) { free(palette); s31_pce_destroy(pce); return NULL; }
    memcpy(pce->palette, palette, sizeof(pce->palette));
    free(palette);
    active = pce;
    if (InitPCE(48000, true)) { s31_pce_destroy(pce); return NULL; }
    pce->initialized = true;
    return pce;
}

void s31_pce_destroy(s31_pce_t *pce)
{
    if (!pce) return;
    if (pce->initialized) ShutdownPCE();
    if (active == pce) active = NULL;
    free(pce->indexed); free(pce->frame); free(pce->audio); free(pce);
}

int s31_pce_load_rom(s31_pce_t *pce, const uint8_t *data, size_t size)
{
    size_t offset = size & 0x1fff;
    if (!pce || !data || pce->loaded || !size || size > 0x280000) return -1;
    size_t header = offset == 512 && size > 0x2200 ? 512 : 0;
    size_t payload = size - header;
    if (payload >= 0x2000 && (payload & 0x1fff)) return -1;
    size_t normalized = payload < 0x2000 ? 0x2000 : size;
    uint8_t *copy = malloc(normalized);
    if (!copy) return -1;
    if (payload < 0x2000) {
        for (size_t i = 0; i < normalized; ++i) copy[i] = data[i % payload];
    } else {
        memcpy(copy, data, size);
    }
    if (LoadCard(copy, normalized)) { if (PCE.ROM == copy) PCE.ROM = NULL; free(copy); return -1; }
    pce->loaded = true;
    return 0;
}

void s31_pce_set_input(s31_pce_t *pce, s31_buttons_t buttons) { if (pce) pce->buttons = buttons; }

void s31_pce_run_frame(s31_pce_t *pce)
{
    if (!pce || !pce->loaded) return;
    s31_buttons_t b = pce->buttons;
    PCE.Joypad.regs[0] = ((b&S31_BUTTON_A)?JOY_A:0)|((b&S31_BUTTON_B)?JOY_B:0)|
        ((b&S31_BUTTON_SELECT)?JOY_SELECT:0)|((b&S31_BUTTON_START)?JOY_RUN:0)|
        ((b&S31_BUTTON_UP)?JOY_UP:0)|((b&S31_BUTTON_RIGHT)?JOY_RIGHT:0)|
        ((b&S31_BUTTON_DOWN)?JOY_DOWN:0)|((b&S31_BUTTON_LEFT)?JOY_LEFT:0);
    pce_run();
    if (!pce->width || !pce->height) return;
    const uint8_t *base = pce->indexed + 16;
    for (unsigned y = 0; y < pce->height; ++y)
        for (unsigned x = 0; x < pce->width; ++x)
            pce->frame[y * pce->width + x] = pce->palette[base[y * XBUF_WIDTH + x]];
    if (!pce->presented) { pce->presented = true; printf("PCE first frame: %ux%u\n", pce->width, pce->height); }
    s31_board_present_pce(pce->frame, pce->width, pce->height);
    psg_update(pce->audio, PCE_AUDIO_FRAMES, 0x3f);
    s31_board_audio_write_pce(pce->audio, sizeof(int16_t) * PCE_AUDIO_FRAMES * 2);
}

int s31_pce_load_sram(s31_pce_t *pce, const char *path)
{
    if (!pce || !pce->loaded || !PCE.ExRAM || !path) return -1;
    FILE *file = fopen(path, "rb"); if (!file) return -1;
    size_t count = fread(PCE.ExRAM, 1, PCE_SRAM_SIZE, file); fclose(file);
    return count == PCE_SRAM_SIZE ? 0 : -1;
}

int s31_pce_save_sram(s31_pce_t *pce, const char *path)
{
    if (!pce || !pce->loaded || !PCE.ExRAM || !path) return -1;
    FILE *file = fopen(path, "wb"); if (!file) return -1;
    size_t count = fwrite(PCE.ExRAM, 1, PCE_SRAM_SIZE, file); int error = fclose(file);
    return count == PCE_SRAM_SIZE && !error ? 0 : -1;
}
