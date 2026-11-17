#ifndef S31_GAMEPAD_H
#define S31_GAMEPAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "s31/input.h"

typedef struct {
    s31_buttons_t buttons;
    int16_t left_x;
    int16_t left_y;
    int16_t right_x;
    int16_t right_y;
    uint8_t left_trigger;
    uint8_t right_trigger;
    bool connected;
} s31_gamepad_state_t;

typedef void (*s31_gamepad_callback_t)(const s31_gamepad_state_t *state, void *context);

bool s31_gamepad_decode_report(const uint8_t *report, size_t length,
                               s31_gamepad_state_t *state);
void s31_gamepad_set_callback(s31_gamepad_callback_t callback, void *context);
void s31_gamepad_publish(const s31_gamepad_state_t *state);

#endif
