#include "s31/gamepad.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    s31_gamepad_state_t state;
    uint8_t report[11] = {3, 0, 8, 0, 0, 128, 128, 128, 128, 0, 0};

    memset(&state, 0, sizeof(state));
    assert(s31_gamepad_decode_report(report, sizeof(report), &state));
    assert(state.buttons == 0);

    report[2] = 4;
    memset(&state, 0, sizeof(state));
    assert(s31_gamepad_decode_report(report, sizeof(report), &state));
    assert(state.buttons == S31_BUTTON_DOWN);

    report[2] = 8;
    report[3] = 1;
    memset(&state, 0, sizeof(state));
    assert(s31_gamepad_decode_report(report, sizeof(report), &state));
    assert(state.buttons == S31_BUTTON_A);
    return 0;
}
