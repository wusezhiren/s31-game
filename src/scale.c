#include "s31/scale.h"
#include <string.h>
void s31_scale_rgb565_integer(const uint16_t *s,unsigned w,unsigned h,uint16_t *d,unsigned z){
    if(!s||!d||!z)return;
    unsigned dw=w*z;
    size_t row_bytes=dw*sizeof(uint16_t);
    for(unsigned y=0;y<h;y++){
        const uint16_t *src=s+y*w;
        uint16_t *dst0=d+(y*z)*dw;
        if(z==1){
            memcpy(dst0,src,row_bytes);
            continue;
        }
        if(z==2){
            for(unsigned x=0;x<w;x++){
                uint16_t px=src[x];
                dst0[x*2]=px;
                dst0[x*2+1]=px;
            }
        }else if(z==3){
            for(unsigned x=0;x<w;x++){
                uint16_t px=src[x];
                uint16_t *p=dst0+x*3;
                p[0]=px;p[1]=px;p[2]=px;
            }
        }else{
            for(unsigned x=0;x<w;x++){
                uint16_t px=src[x];
                for(unsigned xx=0;xx<z;xx++)dst0[x*z+xx]=px;
            }
        }
        for(unsigned yy=1;yy<z;yy++)memcpy(dst0+yy*dw,dst0,row_bytes);
    }
}
