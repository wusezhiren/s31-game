#include "s31/audio.h"
#include "s31/config.h"
#include "s31/input.h"
#include "s31/rom_loader.h"
#include <assert.h>

int main(void) {
    const s31_platform_info_t *fc = s31_platform_info(S31_PLATFORM_FC);
    const s31_platform_info_t *cps2 = s31_platform_info(S31_PLATFORM_CPS2);
    assert(fc && fc->tier == S31_TIER_EASY && fc->max_rom_bytes == 1024u * 1024u);
    assert(cps2 && cps2->tier == S31_TIER_UNSUPPORTED);

    uint8_t data[16] = {0};
    s31_rom_t rom = {0};
    assert(s31_rom_load(&rom, data, sizeof(data), 16) == S31_ROM_OK);
    assert(s31_rom_load(&rom, data, sizeof(data), 15) == S31_ROM_TOO_LARGE);
    s31_rom_unload(&rom);

    s31_input_t input = {0};
    s31_input_update(&input, S31_BUTTON_A | S31_BUTTON_START);
    assert(input.pressed == (S31_BUTTON_A | S31_BUTTON_START));
    s31_input_update(&input, S31_BUTTON_A);
    assert(input.pressed == 0);

    s31_audio_t audio;
    int16_t in[4] = {1, 2, 3, 4}, out[4] = {0};
    s31_audio_init(&audio);
    assert(s31_audio_push(&audio, in, 2) == 2);
    assert(s31_audio_pop(&audio, out, 2) == 2 && out[0] == 1 && out[3] == 4);
    return 0;
}
