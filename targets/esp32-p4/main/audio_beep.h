#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

// Simple beep tone player using the onboard ES8311 codec + speaker
// Pins (Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C):
//   MCLK=13, BCLK=12, LRCK=10, DOUT=9, PA_EN=53

esp_err_t audio_beep_set_format(int sample_rate, int channels);
esp_err_t audio_beep_init(void);
void audio_beep_play(int frequency_hz, int duration_ms);
void audio_beep_write_pcm(const int16_t *pcm, size_t bytes);
void audio_beep_deinit(void);
