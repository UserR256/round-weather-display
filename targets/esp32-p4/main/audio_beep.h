#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

esp_err_t audio_beep_set_format(int sample_rate, int channels);
esp_err_t audio_beep_init(void);
void audio_beep_play(int frequency_hz, int duration_ms);
void audio_beep_write_pcm(const int16_t *pcm, size_t bytes);
void audio_beep_deinit(void);
void audio_beep_prepare(void);   // allocate I2S/codec early (call once at boot)
void audio_beep_set_volume(uint8_t volume);   // 0-100, applies live if the codec is open
