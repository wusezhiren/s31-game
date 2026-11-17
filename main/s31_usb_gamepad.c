#include "s31/gamepad.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_log_buffer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"

static const char *TAG = "s31_usb_pad";
extern esp_err_t s31_betop_dual_init(void);
static unsigned report_log_count[8];
typedef struct { bool active; uint8_t addr; uint8_t iface; s31_buttons_t buttons; } interface_state_t;
static interface_state_t interfaces[8];

static interface_state_t *interface_slot(uint8_t addr, uint8_t iface, bool create)
{
    interface_state_t *free_slot = NULL;
    for (size_t i = 0; i < 8; ++i) {
        if (interfaces[i].active && interfaces[i].addr == addr && interfaces[i].iface == iface) return &interfaces[i];
        if (!interfaces[i].active && !free_slot) free_slot = &interfaces[i];
    }
    if (create && free_slot) { *free_slot = (interface_state_t){.active=true,.addr=addr,.iface=iface}; }
    return create ? free_slot : NULL;
}

static void publish_device(uint8_t addr)
{
    s31_gamepad_state_t state = {0};
    for (size_t i = 0; i < 8; ++i) if (interfaces[i].active && interfaces[i].addr == addr) {
        state.connected = true;
        state.buttons |= interfaces[i].buttons;
    }
    s31_gamepad_publish(&state);
}



static void usb_lib_task(void *arg)
{
    TaskHandle_t caller = (TaskHandle_t)arg;
    const usb_host_config_t config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
    };
    ESP_ERROR_CHECK(usb_host_install(&config));
    ESP_LOGI(TAG, "USB host ready; hubs and multiple HID devices enabled");
    xTaskNotifyGive(caller);
    for (;;) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
            ESP_ERROR_CHECK(usb_host_device_free_all());
    }
}

static void hid_interface_callback(hid_host_device_handle_t handle,
                                   hid_host_interface_event_t event, void *arg)
{
    (void)arg;
    hid_host_dev_params_t params;
    if (hid_host_device_get_params(handle, &params) != ESP_OK) return;
    if (event == HID_HOST_INTERFACE_EVENT_INPUT_REPORT) {
        uint8_t report[64];
        size_t length = 0;
        if (hid_host_device_get_raw_input_report_data(handle, report, sizeof(report), &length) == ESP_OK) {
            unsigned slot = params.iface_num < 8 ? params.iface_num : 7;
            if (report_log_count[slot]++ < 16) {
                ESP_LOGI(TAG, "HID report addr=%u iface=%u len=%u",
                         params.addr, params.iface_num, (unsigned)length);
                ESP_LOG_BUFFER_HEXDUMP(TAG, report, length, ESP_LOG_INFO);
            }
            s31_gamepad_state_t state = {.connected = true};
            if (s31_gamepad_decode_report(report, length, &state)) {
                interface_state_t *slot_state = interface_slot(params.addr, params.iface_num, true);
                if (slot_state) slot_state->buttons = state.buttons;
                publish_device(params.addr);
            }
        }
    } else if (event == HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR) {
        ESP_LOGW(TAG, "HID transfer error addr=%u iface=%u", params.addr, params.iface_num);
    } else if (event == HID_HOST_INTERFACE_EVENT_DISCONNECTED) {
        hid_host_device_close(handle);
        interface_state_t *slot_state = interface_slot(params.addr, params.iface_num, false);
        if (slot_state) *slot_state = (interface_state_t){0};
        publish_device(params.addr);
    }
}

static void hid_device_callback(hid_host_device_handle_t handle,
                                hid_host_driver_event_t event, void *arg)
{
    (void)arg;
    if (event != HID_HOST_DRIVER_EVENT_CONNECTED) return;
    hid_host_dev_params_t params;
    ESP_ERROR_CHECK(hid_host_device_get_params(handle, &params));
    if (params.proto == HID_PROTOCOL_KEYBOARD || params.proto == HID_PROTOCOL_MOUSE) return;
    hid_host_dev_info_t info = {0};
    esp_err_t info_error = hid_host_get_device_info(handle, &info);
    if (info_error == ESP_OK && info.VID == 0x20bc && info.PID == 0x507f) {
        ESP_LOGI(TAG, "BETOP interface %u delegated to dedicated driver", params.iface_num);
        return;
    }
    const hid_host_device_config_t config = {
        .callback = hid_interface_callback,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(hid_host_device_open(handle, &config));
    if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE)
        ESP_ERROR_CHECK(hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_REPORT));
    ESP_ERROR_CHECK(hid_host_device_start(handle));
    ESP_LOGI(TAG, "USB HID connected addr=%u iface=%u subclass=%u proto=%u VID=%04x PID=%04x info=%s",
             params.addr, params.iface_num, params.sub_class, params.proto,
             info.VID, info.PID, esp_err_to_name(info_error));
}

esp_err_t s31_usb_gamepad_init(void)
{
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();
    if (xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096, caller, 2, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0) return ESP_ERR_TIMEOUT;
    esp_err_t err = s31_betop_dual_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BETOP dedicated driver unavailable: %s", esp_err_to_name(err));
    }
    const hid_host_driver_config_t config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_device_callback,
        .callback_arg = NULL,
    };
    return hid_host_install(&config);
}
