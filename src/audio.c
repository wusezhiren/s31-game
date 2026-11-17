#include "s31/audio.h"

void s31_audio_init(s31_audio_t *audio) {
    if (audio) { audio->read_index = 0; audio->write_index = 0; }
}

static size_t available(const s31_audio_t *audio) {
    return audio->write_index - audio->read_index;
}

size_t s31_audio_push(s31_audio_t *audio, const int16_t *samples, size_t frames) {
    if (!audio || (!samples && frames)) return 0;
    size_t capacity = S31_AUDIO_BUFFER_SAMPLES;
    size_t count = frames < capacity - available(audio) ? frames : capacity - available(audio);
    for (size_t i = 0; i < count; ++i) {
        size_t slot = (audio->write_index + i) % capacity;
        audio->samples[slot * 2] = samples[i * 2];
        audio->samples[slot * 2 + 1] = samples[i * 2 + 1];
    }
    audio->write_index += count;
    return count;
}

size_t s31_audio_pop(s31_audio_t *audio, int16_t *samples, size_t frames) {
    if (!audio || (!samples && frames)) return 0;
    size_t count = frames < available(audio) ? frames : available(audio);
    for (size_t i = 0; i < count; ++i) {
        size_t slot = (audio->read_index + i) % S31_AUDIO_BUFFER_SAMPLES;
        samples[i * 2] = audio->samples[slot * 2];
        samples[i * 2 + 1] = audio->samples[slot * 2 + 1];
    }
    audio->read_index += count;
    return count;
}
