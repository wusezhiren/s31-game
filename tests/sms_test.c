#include "s31/sms.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

static int frames;

int s31_board_present_sms(const uint16_t *pixels, uint16_t width, uint16_t height)
{
    assert(pixels);
    assert(width == 256);
    assert(height == 192);
    frames++;
    return 0;
}

int main(void)
{
    uint8_t *rom = calloc(0x4000, 1);
    assert(rom);
    s31_sms_t *sms = s31_sms_create();
    assert(sms);
    assert(s31_sms_load_rom(sms, rom, 0x4000, S31_GAME_SG1000) == 0);
    s31_sms_set_input(sms, S31_BUTTON_A | S31_BUTTON_RIGHT);
    s31_sms_run_frame(sms);
    assert(frames == 1);
    s31_sms_destroy(sms);
    free(rom);
    return 0;
}
