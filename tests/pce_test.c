#include "s31/pce.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

static int frames;
int s31_board_present_pce(const uint16_t *pixels, uint16_t width, uint16_t height)
{
    assert(pixels && width > 0 && width <= 352 && height > 0 && height <= 242);
    frames++;
    return 0;
}
int s31_board_audio_write_pce(const int16_t *samples, size_t bytes)
{ assert(samples && bytes == 3200); return 0; }

int main(void)
{
    s31_pce_t *pce = s31_pce_create();
    assert(pce);
    uint8_t byte = 0;
    assert(s31_pce_load_rom(pce, NULL, 1) == -1);
    assert(s31_pce_load_rom(pce, &byte, 0) == -1);
    assert(s31_pce_load_rom(pce, &byte, 0x280001) == -1);
    s31_pce_destroy(pce);
    assert(frames == 0);
    return 0;
}
