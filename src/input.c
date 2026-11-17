#include "s31/input.h"

void s31_input_update(s31_input_t *input, s31_buttons_t buttons) {
    if (!input) return;
    input->pressed = buttons & ~input->current;
    input->current = buttons;
}
