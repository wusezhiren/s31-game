#include "s31/gamepad.h"

static s31_gamepad_callback_t callback;
static void *callback_context;

void s31_gamepad_set_callback(s31_gamepad_callback_t cb, void *context)
{
    callback = cb;
    callback_context = context;
}

void s31_gamepad_publish(const s31_gamepad_state_t *state)
{
    if (callback && state) callback(state, callback_context);
}

bool s31_gamepad_decode_report(const uint8_t *report, size_t length,
                               s31_gamepad_state_t *state)
{
    /* Common 8-byte HID gamepad layout used by many Xbox-compatible pads:
     * buttons[0:2], axes[2:6], triggers[6:8]. Device-specific reports are
     * intentionally not guessed here; the HID descriptor should select a
     * mapper before enabling a production whitelist. */
    if (!report || !state || length < 4) return false;
    if (report[0] == 3 && length >= 11) {
        uint8_t hat = report[2] & 0x0f;
        uint16_t buttons = (uint16_t)report[3] | ((uint16_t)report[4] << 8);
        state->buttons = 0;
        if (hat == 0 || hat == 1 || hat == 7) state->buttons |= S31_BUTTON_UP;
        if (hat == 1 || hat == 2 || hat == 3) state->buttons |= S31_BUTTON_RIGHT;
        if (hat == 3 || hat == 4 || hat == 5) state->buttons |= S31_BUTTON_DOWN;
        if (hat == 5 || hat == 6 || hat == 7) state->buttons |= S31_BUTTON_LEFT;
        if (buttons & (1u << 0)) state->buttons |= S31_BUTTON_A;
        if (buttons & (1u << 1)) state->buttons |= S31_BUTTON_B;
        if (buttons & (1u << 6)) state->buttons |= S31_BUTTON_L;
        if (buttons & (1u << 7)) state->buttons |= S31_BUTTON_R;
        if (buttons & (1u << 10)) state->buttons |= S31_BUTTON_SELECT;
        if (buttons & (1u << 11)) state->buttons |= S31_BUTTON_START;
        state->left_x = ((int16_t)report[5] - 128) * 256;
        state->left_y = ((int16_t)report[6] - 128) * 256;
        state->right_x = ((int16_t)report[7] - 128) * 256;
        state->right_y = ((int16_t)report[8] - 128) * 256;
        state->left_trigger = report[9];
        state->right_trigger = report[10];
        return true;
    }
    size_t offset = length >= 9 && report[0] > 0 && report[0] < 16 ? 1 : 0;
    if (length - offset < 4) return false;
    report += offset;
    length -= offset;
    state->buttons = 0;
    if (report[0] & 0x01) state->buttons |= S31_BUTTON_A;
    if (report[0] & 0x02) state->buttons |= S31_BUTTON_B;
    if (report[0] & 0x04) state->buttons |= S31_BUTTON_SELECT;
    if (report[0] & 0x08) state->buttons |= S31_BUTTON_START;
    if (report[0] & 0x10) state->buttons |= S31_BUTTON_UP;
    if (report[0] & 0x20) state->buttons |= S31_BUTTON_DOWN;
    if (report[0] & 0x40) state->buttons |= S31_BUTTON_LEFT;
    if (report[0] & 0x80) state->buttons |= S31_BUTTON_RIGHT;
    if (length >= 8) {
        state->left_x = (int16_t)((uint16_t)report[2] | ((uint16_t)report[3] << 8));
        state->left_y = (int16_t)((uint16_t)report[4] | ((uint16_t)report[5] << 8));
        state->left_trigger = report[6];
        state->right_trigger = report[7];
    }
    return true;
}
