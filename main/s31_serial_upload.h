#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef bool (*s31_serial_upload_allowed_cb_t)(void *context);
typedef void (*s31_serial_upload_complete_cb_t)(const char *filename, void *context);
typedef void (*s31_serial_command_cb_t)(const char *command, const char *argument,
                                        void *context);

esp_err_t s31_serial_upload_start(s31_serial_upload_allowed_cb_t allowed,
                                  s31_serial_upload_complete_cb_t complete,
                                  s31_serial_command_cb_t command,
                                  void *context);
bool s31_serial_upload_active(void);
void s31_serial_reply(const char *message);
