#include "s31/nes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nes.h"
#include "s31/video.h"
#include "s31/nes_video.h"
#ifdef ESP_PLATFORM
#include "s31_board_idf.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static int64_t next_frame_us;
static int64_t last_yield_us;
static TickType_t fps_tick;
static unsigned frames;
static int64_t profile_start, profile_video, profile_audio, profile_wait;
extern int64_t s31_nes_prof_cpu_us, s31_nes_prof_bg_us, s31_nes_prof_spr_us, s31_nes_prof_apu_us;
static bool pcm_logged;
static int16_t *pcm_stereo;
static int16_t *pcm_mono;

int nes_audio_reference_path_enabled(void) { return 0; }

int nes_sound_output_s16(const int16_t *buffer, size_t len)
{
    static int32_t dc;
    if (!pcm_stereo || len > NES_APU_SAMPLE_PER_SYNC_MAX) return -1;
    bool nonzero = false;
    for (size_t i = 0; i < len; ++i) {
        int32_t sample = buffer[i];
        dc += (sample * 256 - dc) / 256;
        pcm_stereo[2 * i] = pcm_stereo[2 * i + 1] = (int16_t)(sample - dc / 256);
        nonzero |= pcm_stereo[2 * i] != 0;
    }
    int64_t start = esp_timer_get_time();
    int result = s31_board_audio_write(pcm_stereo, len * 2 * sizeof(*pcm_stereo));
    profile_audio += esp_timer_get_time() - start;
    if (!pcm_logged && nonzero && result == 0) {
        pcm_logged = true;
        printf("NES audio: nonzero PCM accepted (%u frames)\n", (unsigned)len);
    }
    return result;
}

int nes_sound_output(uint8_t *buffer, size_t len)
{
    if (!pcm_mono || len > NES_APU_SAMPLE_PER_SYNC_MAX) return -1;
    for (size_t i = 0; i < len; ++i) pcm_mono[i] = ((int)buffer[i] - 128) * 128;
    return nes_sound_output_s16(pcm_mono, len);
}
#endif

struct s31_nes {
    nes_t *core;
    uint8_t *rom;
    size_t rom_size;
    s31_buttons_t buttons;
    bool loaded;
    volatile bool paused;
};
static s31_nes_t *running_nes;

__attribute__((weak)) int s31_board_present_nes(const uint16_t *pixels, uint16_t width, uint16_t height) {
    (void)pixels; (void)width; (void)height; return 0;
}

void *nes_sram_malloc(int size) {
#ifdef ESP_PLATFORM
    /* 显式分配到带 64KB L1 DCache 的 PSRAM，避免落入低速 0x2E00xxxx RTCRAM */
    void *ptr = heap_caps_malloc((size_t)size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr) return ptr;
#endif
    return malloc((size_t)size);
}
void nes_sram_free(void *ptr) { free(ptr); }
uint32_t nes_get_ms(void) { return 0; }
int nes_log_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stderr, format, args);
    va_end(args);
    return result;
}

int nes_initex(nes_t *core) { (void)core; return NES_OK; }
int nes_deinitex(nes_t *core) { (void)core; return NES_OK; }

int nes_draw(int x1, int y1, int x2, int y2, nes_color_t *pixels) {
    (void)x1; (void)y1; (void)x2; (void)y2;
#ifdef ESP_PLATFORM
    int64_t start = esp_timer_get_time();
#endif
    int result = s31_board_present_nes((const uint16_t *)pixels, NES_WIDTH, NES_HEIGHT);
#ifdef ESP_PLATFORM
    profile_video += esp_timer_get_time() - start;
#endif
    return result;
}

static uint16_t to_nes_buttons(s31_buttons_t buttons) {
    uint16_t result = 0;
    if (buttons & S31_BUTTON_A) result |= 1u << 15;
    if (buttons & S31_BUTTON_B) result |= 1u << 14;
    if (buttons & S31_BUTTON_SELECT) result |= 1u << 13;
    if (buttons & S31_BUTTON_START) result |= 1u << 12;
    if (buttons & S31_BUTTON_UP) result |= 1u << 11;
    if (buttons & S31_BUTTON_DOWN) result |= 1u << 10;
    if (buttons & S31_BUTTON_LEFT) result |= 1u << 9;
    if (buttons & S31_BUTTON_RIGHT) result |= 1u << 8;
    return result;
}

s31_nes_t *s31_nes_create(void) {
    s31_nes_t *nes = calloc(1, sizeof(*nes));
    if (nes) nes->core = nes_init();
    if (!nes || !nes->core) { free(nes); return NULL; }
    return nes;
}

void s31_nes_destroy(s31_nes_t *nes) {
    if (!nes) return;
    if (nes->core) nes_deinit(nes->core);
    free(nes->rom);
    free(nes);
}

int s31_nes_load_rom(s31_nes_t *nes, const uint8_t *data, size_t size) {
    if (!nes || !nes->core || !data || size < 16) return -1;
    if (memcmp(data, "NES\x1a", 4) != 0) return -1;
    if (nes->rom) { nes_unload_rom(nes->core); free(nes->rom); nes->rom = NULL; }
    nes->rom = malloc(size);
    if (!nes->rom) return -1;
    memcpy(nes->rom, data, size);
    nes->rom_size = size;
    if (nes_load_rom(nes->core, nes->rom) != NES_OK) {
        free(nes->rom); nes->rom = NULL; nes->rom_size = 0; return -1;
    }
    nes->loaded = true;
    return 0;
}

void s31_nes_set_input(s31_nes_t *nes, s31_buttons_t buttons) {
    if (nes) nes->buttons = buttons;
}

void s31_nes_frame(s31_nes_t *nes) {
    if (!nes || !nes->loaded) return;
    nes->core->nes_cpu.joypad.joypad = to_nes_buttons(nes->buttons);
    nes_frame(nes->core);
}

void nes_frame(nes_t *core) {
    if (running_nes) core->nes_cpu.joypad.joypad = to_nes_buttons(running_nes->buttons);
#ifdef ESP_PLATFORM
    int64_t wait_start = esp_timer_get_time();
    while (running_nes && running_nes->paused) {
        vTaskDelay(pdMS_TO_TICKS(20));
        next_frame_us = esp_timer_get_time();
        last_yield_us = next_frame_us;
    }
    if (running_nes) {
        /* 60Hz 微秒级帧节拍，仅在有富余时间或较久未让出 CPU 时休眠 */
        next_frame_us += 16667;
        int64_t now_us = esp_timer_get_time();
        int64_t remain_us = next_frame_us - now_us;
        if (remain_us >= 1000) {
            vTaskDelay(pdMS_TO_TICKS((uint32_t)(remain_us / 1000)));
            last_yield_us = esp_timer_get_time();
        } else {
            if (remain_us < -33334) {
                next_frame_us = now_us;
            }
            if (now_us - last_yield_us >= 100000) {
                vTaskDelay(1);
                last_yield_us = esp_timer_get_time();
            }
        }
    }
    if (running_nes) {
        TickType_t now = xTaskGetTickCount();
        ++frames;
        if (now - fps_tick >= pdMS_TO_TICKS(3000)) {
            int64_t end = esp_timer_get_time();
            profile_wait += end - wait_start;
            printf("NES profile us/frame: total=%lld video=%lld cpu=%lld bg=%lld spr=%lld apu=%lld audio_write=%lld wait=%lld\n",
                   (long long)((end - profile_start) / frames), (long long)(profile_video / frames),
                   (long long)(s31_nes_prof_cpu_us / frames), (long long)(s31_nes_prof_bg_us / frames),
                   (long long)(s31_nes_prof_spr_us / frames), (long long)(s31_nes_prof_apu_us / frames),
                   (long long)(profile_audio / frames), (long long)(profile_wait / frames));
            profile_start = esp_timer_get_time();
            profile_video = profile_audio = profile_wait = 0;
            s31_nes_prof_cpu_us = s31_nes_prof_bg_us = s31_nes_prof_spr_us = s31_nes_prof_apu_us = 0;
            printf("NES emulation: %.1f FPS\n", (double)frames * configTICK_RATE_HZ / (now - fps_tick));
            frames = 0;
            fps_tick = now;
        } else profile_wait += esp_timer_get_time() - wait_start;
    }
#endif
}

void s31_nes_run(s31_nes_t *nes) {
    if (!nes || !nes->loaded) return;
    running_nes = nes;
#ifdef ESP_PLATFORM
    next_frame_us = esp_timer_get_time();
    last_yield_us = next_frame_us;
    fps_tick = xTaskGetTickCount();
    frames = 0;
    profile_start = next_frame_us;
    profile_video = profile_audio = profile_wait = 0;
    s31_nes_prof_cpu_us = s31_nes_prof_bg_us = s31_nes_prof_spr_us = s31_nes_prof_apu_us = 0;
    printf("NES mem: core=%p draw=%p stack=%p\n", (void *)nes->core, (void *)nes->core->nes_draw_data, (void *)&nes);
    pcm_logged = false;
    pcm_stereo = heap_caps_malloc(NES_APU_SAMPLE_PER_SYNC_MAX * 3 * sizeof(*pcm_stereo),
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    pcm_mono = pcm_stereo ? pcm_stereo + NES_APU_SAMPLE_PER_SYNC_MAX * 2 : NULL;
#endif
    nes_run(nes->core);
#ifdef ESP_PLATFORM
    free(pcm_stereo);
    pcm_stereo = pcm_mono = NULL;
#endif
    running_nes = NULL;
}

void s31_nes_set_paused(s31_nes_t *nes, bool paused) { if (nes) nes->paused = paused; }
bool s31_nes_paused(const s31_nes_t *nes) { return nes && nes->paused; }
void s31_nes_stop(s31_nes_t *nes) {
    if (nes && nes->core) { nes->paused = false; nes->core->nes_quit = 1; }
}

int s31_nes_save_battery(s31_nes_t *nes, const char *path) {
    if (!nes || !nes->loaded || !nes->core->nes_rom.sram || !path) return -1;
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    size_t written = fwrite(nes->core->nes_rom.sram, 1, SRAM_SIZE, file);
    fclose(file);
    return written == SRAM_SIZE ? 0 : -1;
}

int s31_nes_load_battery(s31_nes_t *nes, const char *path) {
    if (!nes || !nes->loaded || !nes->core->nes_rom.sram || !path) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    size_t read = fread(nes->core->nes_rom.sram, 1, SRAM_SIZE, file);
    fclose(file);
    return read == SRAM_SIZE ? 0 : -1;
}

bool s31_nes_loaded(const s31_nes_t *nes) { return nes && nes->loaded; }
uint16_t s31_nes_mapper(const s31_nes_t *nes) {
    return nes && nes->core ? nes->core->nes_rom.mapper_number : 0xffff;
}
