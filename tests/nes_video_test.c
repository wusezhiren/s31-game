#include "s31/nes_video.h"
#include <assert.h>
#include <stdint.h>

int main(void)
{
    uint16_t source[S31_NES_WIDTH * S31_NES_HEIGHT] = {0};
    uint16_t target[S31_NES_SCALED_WIDTH * S31_NES_SCALED_HEIGHT] = {0};
    source[0] = 0x1234;
    source[1] = 0xabcd;
    source[S31_NES_WIDTH] = 0x5678;
    s31_nes_scale_2x_rgb565(source, target);
    assert(target[0] == 0x1234 && target[1] == 0x1234);
    assert(target[S31_NES_SCALED_WIDTH] == 0x1234);
    assert(target[2] == 0xabcd && target[S31_NES_SCALED_WIDTH + 2] == 0xabcd);
    assert(target[S31_NES_SCALED_WIDTH * 2] == 0x5678);
    return 0;
}
