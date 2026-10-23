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
  uint8_t hour;      // 0-23
  uint8_t minute;    // 0-59
  alarm_type_t type;
} alarm_config_t;

esp_err_t audio_alarm_init(void);
esp_err_t audio_alarm_set(const alarm_config_t *config);
esp_err_t audio_alarm_get(alarm_config_t *config);
void audio_alarm_check(void);  // Call periodically (e.g. every second)
void audio_alarm_stop(void);
