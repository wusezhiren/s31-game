#include "s31/gamepad.h"
#include "esp_hidh.h"
#include "esp_hidh_bluedroid.h"
#include "esp_hidh_bluedroid.h"
#include "esp_check.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "nvs_flash.h"

static const char *TAG = "s31_bt_pad";

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    if (event == ESP_BT_GAP_DISC_RES_EVT) {
        for (int i = 0; i < param->disc_res.num_prop; ++i) {
            esp_bt_gap_dev_prop_t *property = &param->disc_res.prop[i];
            if (property->type != ESP_BT_GAP_DEV_PROP_COD) continue;
            uint32_t cod = *(uint32_t *)property->val;
            if (((cod & ESP_BT_COD_MAJOR_DEV_BIT_MASK) >> ESP_BT_COD_MAJOR_DEV_BIT_OFFSET) == ESP_BT_COD_MAJOR_DEV_PERIPHERAL) {
                ESP_LOGI(TAG, "Bluetooth HID candidate found; opening");
                esp_hidh_dev_open(param->disc_res.bda, ESP_HID_TRANSPORT_BT, 0);
                esp_bt_gap_cancel_discovery();
                return;
            }
        }
    }
}

static void hid_callback(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    esp_hidh_event_data_t *event = data;
    if (id == ESP_HIDH_INPUT_EVENT && event->input.usage == ESP_HID_USAGE_JOYSTICK) {
        s31_gamepad_state_t state = {.connected = true};
        s31_gamepad_decode_report(event->input.data, event->input.length, &state);
        s31_gamepad_publish(&state);
    } else if (id == ESP_HIDH_CLOSE_EVENT) {
        s31_gamepad_state_t state = {.connected = false};
        s31_gamepad_publish(&state);
    }
}

esp_err_t s31_bt_gamepad_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "NVS init failed");
    esp_bt_controller_config_t controller_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&controller_config), TAG, "BT controller init failed");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT), TAG, "BT controller enable failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_init(), TAG, "Bluedroid init failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_enable(), TAG, "Bluedroid enable failed");
    ESP_RETURN_ON_ERROR(esp_bt_gap_register_callback(gap_callback), TAG, "GAP callback failed");
    esp_hidh_config_t config = {.callback = hid_callback, .event_stack_size = 4096};
    ESP_RETURN_ON_ERROR(esp_hidh_init(&config), TAG, "HID host init failed");
    ESP_RETURN_ON_ERROR(esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 8, 0), TAG, "BT scan failed");
    ESP_LOGI(TAG, "Bluetooth Classic HID scan started");
    return ESP_OK;
}
