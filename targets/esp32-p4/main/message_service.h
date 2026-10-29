#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_http_server.h"

#include "app_runtime.h"
#include "device_config.h"

esp_err_t message_service_init(const device_config_t *config);
esp_err_t message_service_get_snapshot(
  const char *device_id,
  app_message_snapshot_t *out_snapshot,
  uint32_t *out_unread_count,
  bool *out_has_important
);
esp_err_t message_service_acknowledge_active(const char *device_id);

// The local web server (port 80) started by message_service_init(), so other
// modules can register their own pages and API handlers on it. NULL before init.
httpd_handle_t message_service_http_server(void);
