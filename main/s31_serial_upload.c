#include "s31_serial_upload.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "driver/uart.h"
#include "esp_crc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define S31_UPLOAD_UART UART_NUM_0
#define S31_UPLOAD_MAX_SIZE (8u * 1024u * 1024u)
#define S31_UPLOAD_LINE_SIZE 160
#define S31_UPLOAD_NAME_SIZE 96
#define S31_UPLOAD_CHUNK_SIZE 1024

static const char *TAG = "serial_upload";
static s31_serial_upload_allowed_cb_t upload_allowed;
static s31_serial_upload_complete_cb_t upload_complete;
static s31_serial_command_cb_t serial_command;
static void *upload_context;
static volatile bool upload_active;

void s31_serial_reply(const char *message)
{
    char line[128];
    int length = snprintf(line, sizeof(line), "\r\n@S31 %s\r\n", message);
    if (length > 0) uart_write_bytes(S31_UPLOAD_UART, line, (size_t)length);
}

static bool valid_filename(const char *name)
{
    size_t length = strlen(name);
    if (!length || length >= S31_UPLOAD_NAME_SIZE || name[0] == '.') return false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!(isalnum(c) || c == '.' || c == '_' || c == '-')) return false;
    }
    return true;
}

static int read_line(char *line, size_t capacity)
{
    size_t length = 0;
    for (;;) {
        uint8_t byte;
        int count = uart_read_bytes(S31_UPLOAD_UART, &byte, 1, pdMS_TO_TICKS(200));
        if (count != 1) return 0;
        if (byte == '\n') {
            line[length] = '\0';
            return (int)length;
        }
        if (byte != '\r' && length + 1 < capacity) line[length++] = (char)byte;
    }
}

static bool receive_file(size_t size, uint32_t expected_crc, const char *filename)
{
    char final_path[160];
    const char *part_path = "/sdcard/roms/.s31-upload.part";
    snprintf(final_path, sizeof(final_path), "/sdcard/roms/%s", filename);
    if (access(final_path, F_OK) == 0) {
        s31_serial_reply("ERR EXISTS");
        return false;
    }
    mkdir("/sdcard/roms", 0775);
    FILE *file = fopen(part_path, "wb");
    if (!file) {
        s31_serial_reply("ERR OPEN");
        return false;
    }

    s31_serial_reply("READY");
    uint8_t buffer[S31_UPLOAD_CHUNK_SIZE];
    uint32_t crc = 0;
    size_t received = 0;
    bool ok = true;
    while (received < size) {
        size_t wanted = size - received;
        if (wanted > sizeof(buffer)) wanted = sizeof(buffer);
        int count = uart_read_bytes(S31_UPLOAD_UART, buffer, wanted, pdMS_TO_TICKS(5000));
        if (count <= 0 || fwrite(buffer, 1, (size_t)count, file) != (size_t)count) {
            ok = false;
            break;
        }
        crc = esp_crc32_le(crc, buffer, (uint32_t)count);
        received += (size_t)count;
    }
    if (fclose(file) != 0) ok = false;

    if (!ok || received != size) {
        unlink(part_path);
        s31_serial_reply("ERR TRANSFER");
        return false;
    }
    if (crc != expected_crc) {
        unlink(part_path);
        s31_serial_reply("ERR CRC");
        return false;
    }
    if (rename(part_path, final_path) != 0) {
        ESP_LOGE(TAG, "rename failed: errno=%d", errno);
        unlink(part_path);
        s31_serial_reply("ERR RENAME");
        return false;
    }
    s31_serial_reply("OK");
    return true;
}

static void upload_task(void *argument)
{
    (void)argument;
    char line[S31_UPLOAD_LINE_SIZE];
    for (;;) {
        int length = read_line(line, sizeof(line));
        if (length <= 0) continue;
        if (!strncmp(line, "S31RUN ", 7)) {
            const char *filename = line + 7;
            if (!valid_filename(filename)) s31_serial_reply("RUN ERR HEADER");
            else if (serial_command) serial_command("RUN", filename, upload_context);
            continue;
        }
        if (!strcmp(line, "S31STOP")) {
            if (serial_command) serial_command("STOP", NULL, upload_context);
            continue;
        }
        if (!strncmp(line, "S31TOUCH ", 9)) {
            if (serial_command) serial_command("TOUCH", line + 9, upload_context);
            continue;
        }
        if (strncmp(line, "S31PUT ", 7) != 0) continue;

        unsigned long size = 0;
        unsigned long crc = 0;
        char filename[S31_UPLOAD_NAME_SIZE] = {0};
        char extra = '\0';
        if (sscanf(line + 7, "%lu %lx %95s %c", &size, &crc, filename, &extra) != 3 ||
            size == 0 || size > S31_UPLOAD_MAX_SIZE || crc > UINT32_MAX ||
            !valid_filename(filename)) {
            s31_serial_reply("ERR HEADER");
            continue;
        }
        upload_active = true;
        if (upload_allowed && !upload_allowed(upload_context)) {
            upload_active = false;
            s31_serial_reply("ERR BUSY");
            continue;
        }
        bool success = receive_file((size_t)size, (uint32_t)crc, filename);
        upload_active = false;
        if (success && upload_complete) upload_complete(filename, upload_context);
    }
}

bool s31_serial_upload_active(void)
{
    return upload_active;
}

esp_err_t s31_serial_upload_start(s31_serial_upload_allowed_cb_t allowed,
                                  s31_serial_upload_complete_cb_t complete,
                                  s31_serial_command_cb_t command,
                                  void *context)
{
    upload_allowed = allowed;
    upload_complete = complete;
    serial_command = command;
    upload_context = context;
    if (!uart_is_driver_installed(S31_UPLOAD_UART)) {
        esp_err_t err = uart_driver_install(S31_UPLOAD_UART, 4096, 0, 0, NULL, 0);
        if (err != ESP_OK) return err;
    }
    return xTaskCreate(upload_task, "serial_upload", 4096, NULL, 3, NULL) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}
