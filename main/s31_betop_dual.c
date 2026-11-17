#include "s31/gamepad.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"

static const char *TAG = "s31_betop";
static usb_host_client_handle_t client;
static usb_device_handle_t device;
static usb_transfer_t *input_transfers[2];
static uint8_t input_endpoints[2];
static uint16_t input_sizes[2];
static unsigned input_count;
static usb_transfer_t *descriptor_transfer;
static usb_transfer_t *control_transfer;
static void close_device(void);
static void control_done(usb_transfer_t *transfer);

static bool decode_report(const uint8_t *report, size_t length, s31_gamepad_state_t *state)
{
    if (!report || length < 11 || report[0] != 3) return false;
    uint8_t hat = report[2] & 0x0f;
    uint16_t buttons = (uint16_t)report[3] | ((uint16_t)report[4] << 8);
    state->connected = true;
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
    state->left_trigger = report[9]; state->right_trigger = report[10];
    return true;
}

static void input_done(usb_transfer_t *transfer)
{
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED && transfer->actual_num_bytes) {
        s31_gamepad_state_t state = {0};
        if (decode_report(transfer->data_buffer, transfer->actual_num_bytes, &state))
            s31_gamepad_publish(&state);
    } else if (transfer->status != USB_TRANSFER_STATUS_CANCELED) {
        ESP_LOGW(TAG, "endpoint 0x%02x status=%d", transfer->bEndpointAddress, transfer->status);
    }
    if (device && transfer->status != USB_TRANSFER_STATUS_ERROR)
        usb_host_transfer_submit(transfer);
}

static void start_inputs(void)
{
    for (unsigned i = 0; i < input_count; ++i) {
        if (usb_host_transfer_alloc(input_sizes[i], 0, &input_transfers[i]) != ESP_OK) {
            close_device();
            return;
        }
        input_transfers[i]->device_handle = device;
        input_transfers[i]->bEndpointAddress = input_endpoints[i];
        input_transfers[i]->callback = input_done;
        input_transfers[i]->num_bytes = input_sizes[i];
        input_transfers[i]->timeout_ms = 0;
        esp_err_t err = usb_host_transfer_submit(input_transfers[i]);
        ESP_LOGI(TAG, "submit ep=0x%02x mps=%u: %s", input_endpoints[i], input_sizes[i],
                 esp_err_to_name(err));
    }
}

static void descriptor_done(usb_transfer_t *transfer)
{
    ESP_LOGI(TAG, "report descriptor status=%d bytes=%u", transfer->status,
             (unsigned)transfer->actual_num_bytes);
    usb_host_transfer_free(descriptor_transfer);
    descriptor_transfer = NULL;
    if (!device || usb_host_transfer_alloc(18, 0, &control_transfer) != ESP_OK) {
        if (device) start_inputs();
        return;
    }
    usb_setup_packet_t *setup = (usb_setup_packet_t *)control_transfer->data_buffer;
    setup->bmRequestType = 0x21;
    setup->bRequest = 0x0a;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 0;
    control_transfer->device_handle = device;
    control_transfer->bEndpointAddress = 0;
    control_transfer->num_bytes = 8;
    control_transfer->timeout_ms = 1000;
    control_transfer->callback = control_done;
    esp_err_t err = usb_host_transfer_submit_control(client, control_transfer);
    ESP_LOGI(TAG, "SET_IDLE submit: %s", esp_err_to_name(err));
    if (err != ESP_OK) {
        usb_host_transfer_free(control_transfer);
        control_transfer = NULL;
        start_inputs();
    }
}

static void control_done(usb_transfer_t *transfer)
{
    ESP_LOGI(TAG, "SET_IDLE status=%d bytes=%u", transfer->status,
             (unsigned)transfer->actual_num_bytes);
    usb_host_transfer_free(control_transfer);
    control_transfer = NULL;
    if (device) start_inputs();
}

static void close_device(void)
{
    if (!device) return;
    for (unsigned i = 0; i < input_count; ++i) {
        if (!input_transfers[i]) continue;
        usb_host_endpoint_halt(device, input_transfers[i]->bEndpointAddress);
        usb_host_endpoint_flush(device, input_transfers[i]->bEndpointAddress);
        usb_host_transfer_free(input_transfers[i]);
        input_transfers[i] = NULL;
    }
    usb_host_interface_release(client, device, 0);
    usb_host_device_close(client, device); device = NULL;
}

static void open_device(uint8_t address)
{
    if (device || usb_host_device_open(client, address, &device) != ESP_OK) return;
    const usb_device_desc_t *desc = NULL;
    if (usb_host_get_device_descriptor(device, &desc) != ESP_OK ||
        desc->idVendor != 0x20bc || desc->idProduct != 0x507f) {
        usb_host_device_close(client, device); device=NULL; return;
    }
    if (usb_host_interface_claim(client, device, 0, 0) != ESP_OK) {
        ESP_LOGW(TAG, "failed to claim interface 0"); usb_host_device_close(client, device); device=NULL; return;
    }
    const usb_config_desc_t *config = NULL;
    if (usb_host_get_active_config_descriptor(device, &config) == ESP_OK) {
        const uint8_t *p = (const uint8_t *)config;
        const uint8_t *end = p + config->wTotalLength;
        bool interface_zero = false;
        input_count = 0;
        while (p + 2 <= end && p[0] >= 2 && p + p[0] <= end) {
            if (p[1] == 4 && p[0] >= 9) interface_zero = (p[2] == 0 && p[3] == 0);
            if (interface_zero && p[1] == 5 && p[0] >= 7 && p[2] == 0x81 && input_count < 2) {
                input_endpoints[input_count] = p[2];
                input_sizes[input_count] = ((uint16_t)p[4] | ((uint16_t)p[5] << 8)) & 0x7ff;
                ESP_LOGI(TAG, "interface 0 IN ep=0x%02x mps=%u interval=%u", p[2],
                         input_sizes[input_count], p[6]);
                ++input_count;
            }
            p += p[0];
        }
    }
    if (!input_count) {
        input_endpoints[0] = 0x81;
        input_sizes[0] = 64;
        input_count = 1;
    }
    if (usb_host_transfer_alloc(8 + 202, 0, &descriptor_transfer) != ESP_OK) { close_device(); return; }
    usb_setup_packet_t *setup = (usb_setup_packet_t *)descriptor_transfer->data_buffer;
    setup->bmRequestType = 0x81;
    setup->bRequest = USB_B_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (0x22u << 8);
    setup->wIndex = 0;
    setup->wLength = 202;
    descriptor_transfer->device_handle = device;
    descriptor_transfer->bEndpointAddress = 0;
    descriptor_transfer->callback = descriptor_done;
    descriptor_transfer->num_bytes = 8 + 202;
    descriptor_transfer->timeout_ms = 1000;
    esp_err_t err = usb_host_transfer_submit_control(client, descriptor_transfer);
    ESP_LOGI(TAG, "report descriptor submit: %s", esp_err_to_name(err));
    if (err != ESP_OK) { usb_host_transfer_free(descriptor_transfer); descriptor_transfer=NULL; start_inputs(); }
}

static void event_cb(const usb_host_client_event_msg_t *event, void *arg)
{
    (void)arg;
    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV) open_device(event->new_dev.address);
    else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE && event->dev_gone.dev_hdl == device) close_device();
}

static void driver_task(void *arg)
{
    TaskHandle_t caller = (TaskHandle_t)arg;
    usb_host_client_config_t config = {.is_synchronous=false,.max_num_event_msg=4,
        .async={.client_event_callback=event_cb,.callback_arg=NULL}};
    if (usb_host_client_register(&config, &client) != ESP_OK) {
        xTaskNotifyGive(caller);
        vTaskDelete(NULL);
        return;
    }
    xTaskNotifyGive(caller);
    for (;;) usb_host_client_handle_events(client, portMAX_DELAY);
}

esp_err_t s31_betop_dual_init(void)
{
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();
    if (xTaskCreatePinnedToCore(driver_task, "betop_dual", 4096, caller, 4, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0) return ESP_ERR_TIMEOUT;
    return client ? ESP_OK : ESP_FAIL;
}
