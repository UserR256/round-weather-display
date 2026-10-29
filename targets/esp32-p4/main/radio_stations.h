#pragma once

// User-editable list of radio stations, stored in NVS (namespace "radio").
// Replaces the old hard-coded table in audio_alarm.c. On first boot (or after a
// reset) the built-in default list is used. The list is edited from the local
// web page (radio_web.c); the round screen only picks from it.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"
#include "esp_err.h"

#define RADIO_STATIONS_MAX      16
#define RADIO_STATION_NAME_MAX  40    // including the terminating NUL
#define RADIO_STATION_URL_MAX   200   // including the terminating NUL

typedef struct {
  char name[RADIO_STATION_NAME_MAX];
  char url[RADIO_STATION_URL_MAX];   // must be an MP3 stream (the decoder is MP3 only)
} radio_station_t;

// Loads the list from NVS, or the defaults if nothing is saved. Call before
// audio_alarm_init() (that clamps the saved station index against the count).
esp_err_t radio_stations_init(void);

size_t radio_stations_count(void);

// Copies station `index` into `out` (thread safe: the list can be replaced from
// the web server while the radio task reads it). False if out of range.
bool radio_stations_get(size_t index, radio_station_t *out);

// Index of the station with this URL, or -1.
int radio_stations_find_url(const char *url);

// Bumped on every change, so the UI can notice a new list.
uint32_t radio_stations_generation(void);

// Replaces the list from a JSON array [{ "name": "...", "url": "..." }, ...],
// validates every entry and saves it. On a validation error nothing changes and
// *out_error (if given) points to a static message for the web page.
esp_err_t radio_stations_replace_from_json(const cJSON *array, const char **out_error);

// Back to the built-in default list (and saved).
esp_err_t radio_stations_reset_to_defaults(void);

// { "stations": [...], "max": 16, "nameMax": 39, "urlMax": 199 }
cJSON *radio_stations_to_json(void);
