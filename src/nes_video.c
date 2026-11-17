#include "s31/nes_video.h"

void s31_nes_scale_2x_rgb565(const uint16_t *source, uint16_t *target)
{
    if (!source || !target) return;
    for (unsigned y = 0; y < S31_NES_HEIGHT; ++y) {
        const uint16_t *src = source + y * S31_NES_WIDTH;
        uint16_t *dst0 = target + (y * 2u) * S31_NES_SCALED_WIDTH;
        uint16_t *dst1 = dst0 + S31_NES_SCALED_WIDTH;
        for (unsigned x = 0; x < S31_NES_WIDTH; ++x) {
            uint16_t pixel = src[x];
            dst0[x * 2u] = pixel;
            dst0[x * 2u + 1u] = pixel;
            dst1[x * 2u] = pixel;
            dst1[x * 2u + 1u] = pixel;
        }
    }
}
