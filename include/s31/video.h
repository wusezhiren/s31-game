#ifndef S31_VIDEO_H
#define S31_VIDEO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t *pixels;
    size_t pixel_count;
    uint16_t width;
    uint16_t height;
} s31_video_t;

int s31_video_init(s31_video_t *video);
void s31_video_deinit(s31_video_t *video);
void s31_video_clear(s31_video_t *video, uint16_t rgb565);

#endif
