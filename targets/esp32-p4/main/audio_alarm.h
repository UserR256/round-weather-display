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

// Radio on demand, independent of the alarm (used by the radio page in the UI).
typedef enum {
  RADIO_STATE_OFF = 0,
  RADIO_STATE_CONNECTING,
  RADIO_STATE_PLAYING,
  RADIO_STATE_FAILED,   // gave up after the retries; stays until the next start
  RADIO_STATE_AUDIO_FAILED,   // stream is fine but the audio output could not be set up (memory)
} radio_state_t;

esp_err_t audio_radio_start(void);   // ESP_ERR_INVALID_STATE if still shutting down: try again
void audio_radio_stop(void);
radio_state_t audio_radio_get_state(void);
