#include "s31/scale.h"
void s31_scale_rgb565_integer(const uint16_t *s,unsigned w,unsigned h,uint16_t *d,unsigned z){if(!s||!d||!z)return;unsigned dw=w*z;for(unsigned y=0;y<h;y++)for(unsigned yy=0;yy<z;yy++)for(unsigned x=0;x<w;x++)for(unsigned xx=0;xx<z;xx++)d[(y*z+yy)*dw+x*z+xx]=s[y*w+x];}
