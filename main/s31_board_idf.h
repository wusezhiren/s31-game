#pragma once

#include "esp_err.h"
#include "s31/input.h"
#include "s31/video.h"
#include "s31/game_catalog.h"

typedef void (*s31_board_key_cb_t)(int key, bool down, void *context);

esp_err_t s31_board_init(s31_video_t *video);
esp_err_t s31_board_start_input(s31_board_key_cb_t callback, void *context);
esp_err_t s31_board_audio_write(const int16_t *samples, size_t bytes);
const char *s31_board_rom_root(void);
int s31_board_present_nes(const uint16_t *pixels, uint16_t width, uint16_t height);
int s31_board_present_gb(const uint16_t *pixels, uint16_t width, uint16_t height);
int s31_board_present_sms(const uint16_t *pixels, uint16_t width, uint16_t height);
int s31_board_present_pce(const uint16_t *pixels, uint16_t width, uint16_t height);
int s31_board_audio_write_pce(const int16_t *samples, size_t bytes);
void s31_board_show_catalog(const s31_game_catalog_t *catalog, size_t selected, const char *status);
void s31_board_show_game(void);
void s31_board_show_pause(bool paused);
