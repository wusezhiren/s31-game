#ifndef S31_AUDIO_H
#define S31_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#define S31_AUDIO_BUFFER_SAMPLES 4096u

typedef struct {
    int16_t samples[S31_AUDIO_BUFFER_SAMPLES * 2u];
    size_t read_index;
    size_t write_index;
} s31_audio_t;

void s31_audio_init(s31_audio_t *audio);
size_t s31_audio_push(s31_audio_t *audio, const int16_t *samples,
                      size_t stereo_frames);
size_t s31_audio_pop(s31_audio_t *audio, int16_t *samples,
                     size_t stereo_frames);

#endif
