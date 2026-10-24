#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
  ALARM_TYPE_BEEP = 0,
  ALARM_TYPE_RADIO,
} alarm_type_t;

typedef struct {
  bool enabled;
  uint8_t hour;
  uint8_t minute;
  alarm_type_t type;
} alarm_config_t;

esp_err_t audio_alarm_init(void);
esp_err_t audio_alarm_set(const alarm_config_t *config);
esp_err_t audio_alarm_get(alarm_config_t *config);
void audio_alarm_check(void);
void audio_alarm_stop(void);
void audio_alarm_snooze(void);
bool audio_alarm_is_ringing(void);

// Volume 0-100 for alarm sounds. Applies immediately; persist=true also writes
// it to NVS (do that on slider release, not while dragging).
void audio_alarm_set_volume(uint8_t volume, bool persist);
uint8_t audio_alarm_get_volume(void);
void audio_alarm_preview_volume(void);   // short confirmation beep, only when idle
