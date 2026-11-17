#ifndef S31_NES_VIDEO_H
#define S31_NES_VIDEO_H

#include <stdint.h>

#define S31_NES_WIDTH 256u
#define S31_NES_HEIGHT 240u
#define S31_NES_SCALE 2u
#define S31_NES_SCALED_WIDTH (S31_NES_WIDTH * S31_NES_SCALE)
#define S31_NES_SCALED_HEIGHT (S31_NES_HEIGHT * S31_NES_SCALE)

void s31_nes_scale_2x_rgb565(const uint16_t *source, uint16_t *target);

#endif
