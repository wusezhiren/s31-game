#ifndef S31_NES_H
#define S31_NES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "s31/input.h"

typedef struct s31_nes s31_nes_t;

s31_nes_t *s31_nes_create(void);
void s31_nes_destroy(s31_nes_t *nes);
int s31_nes_load_rom(s31_nes_t *nes, const uint8_t *data, size_t size);
void s31_nes_set_input(s31_nes_t *nes, s31_buttons_t buttons);
void s31_nes_frame(s31_nes_t *nes);
void s31_nes_run(s31_nes_t *nes);
void s31_nes_set_paused(s31_nes_t *nes, bool paused);
bool s31_nes_paused(const s31_nes_t *nes);
void s31_nes_stop(s31_nes_t *nes);
int s31_nes_save_battery(s31_nes_t *nes, const char *path);
int s31_nes_load_battery(s31_nes_t *nes, const char *path);
bool s31_nes_loaded(const s31_nes_t *nes);
uint16_t s31_nes_mapper(const s31_nes_t *nes);

#endif
