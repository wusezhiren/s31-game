#ifndef S31_PCE_H
#define S31_PCE_H

#include <stddef.h>
#include <stdint.h>
#include "s31/input.h"

typedef struct s31_pce s31_pce_t;
s31_pce_t *s31_pce_create(void);
void s31_pce_destroy(s31_pce_t *pce);
int s31_pce_load_rom(s31_pce_t *pce, const uint8_t *data, size_t size);
void s31_pce_set_input(s31_pce_t *pce, s31_buttons_t buttons);
void s31_pce_run_frame(s31_pce_t *pce);
int s31_pce_load_sram(s31_pce_t *pce, const char *path);
int s31_pce_save_sram(s31_pce_t *pce, const char *path);

#endif
