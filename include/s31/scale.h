#ifndef S31_SCALE_H
#define S31_SCALE_H
#include <stdint.h>
void s31_scale_rgb565_integer(const uint16_t *src,unsigned sw,unsigned sh,uint16_t *dst,unsigned scale);
#endif
