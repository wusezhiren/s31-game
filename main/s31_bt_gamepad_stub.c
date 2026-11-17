#include "esp_err.h"

/* Bluetooth HID needs the ESP-IDF example GAP adapter on this preview target.
 * Keep the board image linkable until that adapter is intentionally vendored. */
esp_err_t s31_bt_gamepad_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}
