#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

// Radio API on the clock's local web server (the page itself is the shared
// messages.html, which got a "Radio stations" panel):
//
//   GET  /api/radio/stations        current list, selected index, play state
//   POST /api/radio/stations        { "stations": [{ "name", "url" }, ...] } replaces the list
//   POST /api/radio/stations/reset  back to the built-in default list
//   POST /api/radio/play            { "index": n } selects and plays, { "stop": true } stops
esp_err_t radio_web_register(httpd_handle_t server);
