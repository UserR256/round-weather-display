#pragma once

#include "device_config.h"
#include "esp_err.h"
#include "esp_http_server.h"

// Settings API for the "Settings" tab of the clock's web page (web/index.html).
//
//   GET  /api/settings   device settings (the same ones as the settings screen)
//   POST /api/settings   changed fields only; validates, looks up a new location,
//                        saves to NVS and restarts the clock (like the screen does)
//   GET  /api/alarm      alarm time/type/enabled and volume
//   POST /api/alarm      applies immediately, no restart
//
// `config` is the running configuration (main.c); it is only read here. A save
// works on a copy and restarts, so the running copy never changes underneath
// the UI.
esp_err_t settings_web_register(httpd_handle_t server, const device_config_t *config);
