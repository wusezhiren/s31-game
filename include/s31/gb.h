#ifndef S31_GB_H
#define S31_GB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "s31/input.h"
typedef struct s31_gb s31_gb_t;
s31_gb_t *s31_gb_create(void);
void s31_gb_destroy(s31_gb_t *gb);
int s31_gb_load_rom(s31_gb_t *gb, const uint8_t *data, size_t size);
void s31_gb_set_input(s31_gb_t *gb, s31_buttons_t buttons);
void s31_gb_run_frame(s31_gb_t *gb);
int s31_gb_load_sram(s31_gb_t *gb, const char *path);
int s31_gb_save_sram(s31_gb_t *gb, const char *path);
#endif
