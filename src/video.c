#include "s31/video.h"
#include "s31/config.h"
#include <stdlib.h>
#include <string.h>

int s31_video_init(s31_video_t *video) {
    if (!video) return -1;
    video->width = S31_SCREEN_WIDTH;
    video->height = S31_SCREEN_HEIGHT;
    video->pixel_count = (size_t)video->width * video->height;
    video->pixels = calloc(video->pixel_count, sizeof(*video->pixels));
    return video->pixels ? 0 : -1;
}

void s31_video_deinit(s31_video_t *video) {
    if (!video) return;
    free(video->pixels);
    video->pixels = NULL;
    video->pixel_count = 0;
}

void s31_video_clear(s31_video_t *video, uint16_t rgb565) {
    if (video && video->pixels)
        for (size_t i = 0; i < video->pixel_count; ++i) video->pixels[i] = rgb565;
}
