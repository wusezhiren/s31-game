#ifndef S31_INPUT_H
#define S31_INPUT_H

#include <stdint.h>

typedef uint32_t s31_buttons_t;

enum {
    S31_BUTTON_UP = 1u << 0,
    S31_BUTTON_DOWN = 1u << 1,
    S31_BUTTON_LEFT = 1u << 2,
    S31_BUTTON_RIGHT = 1u << 3,
    S31_BUTTON_A = 1u << 4,
    S31_BUTTON_B = 1u << 5,
    S31_BUTTON_START = 1u << 6,
    S31_BUTTON_SELECT = 1u << 7,
    S31_BUTTON_L = 1u << 8,
    S31_BUTTON_R = 1u << 9
};

typedef struct {
    s31_buttons_t current;
    s31_buttons_t pressed;
} s31_input_t;

void s31_input_update(s31_input_t *input, s31_buttons_t buttons);

#endif
