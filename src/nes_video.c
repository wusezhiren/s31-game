#include "s31/nes_video.h"
#include <stdbool.h>
#include <string.h>

void s31_nes_scale_2x_rgb565(const uint16_t *source, uint16_t *target)
{
    if (!source || !target) return;
    bool aligned = ((uintptr_t)target & 3u) == 0;
    for (unsigned y = 0; y < S31_NES_HEIGHT; ++y) {
        const uint16_t *src = source + y * S31_NES_WIDTH;
        uint16_t *dst0 = target + (y * 2u) * S31_NES_SCALED_WIDTH;
        uint16_t *dst1 = dst0 + S31_NES_SCALED_WIDTH;
        uint16_t *out = aligned ? __builtin_assume_aligned(dst0, 4) : dst0;
        for (unsigned x = 0; x < S31_NES_WIDTH; ++x) {
            uint32_t pixel = src[x];
            uint32_t pair = pixel | (pixel << 16);
            memcpy(out + x * 2u, &pair, sizeof(pair));
        }
        memcpy(dst1, dst0, S31_NES_SCALED_WIDTH * sizeof(*dst0));
    }
}
