#ifndef S31_SMS_H
#define S31_SMS_H

#include <stddef.h>
#include <stdint.h>
#include "s31/game_catalog.h"
#include "s31/input.h"

typedef struct s31_sms s31_sms_t;

s31_sms_t *s31_sms_create(void);
void s31_sms_destroy(s31_sms_t *sms);
int s31_sms_load_rom(s31_sms_t *sms, const uint8_t *data, size_t size,
                     s31_game_platform_t platform);
void s31_sms_set_input(s31_sms_t *sms, s31_buttons_t buttons);
void s31_sms_run_frame(s31_sms_t *sms);
int s31_sms_load_sram(s31_sms_t *sms, const char *path);
int s31_sms_save_sram(s31_sms_t *sms, const char *path);

#endif
