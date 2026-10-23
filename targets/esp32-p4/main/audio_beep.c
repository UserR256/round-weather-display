#include "audio_beep.h"
#include <math.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_beep";

#define PIN_POWER_AMP  53
#define PIN_I2S_MCLK   13
#define PIN_I2S_BCLK   12
#define PIN_I2S_LRCK   10
#define PIN_I2S_DOUT   9

#define SAMPLE_RATE    16000
#define AMPLITUDE      8000

static i2s_chan_handle_t tx_handle = NULL;

static void generate_tone(int16_t *buffer, size_t samples, float freq) {
  static float phase = 0;
  const float step = freq * 2.0f * (float)M_PI / SAMPLE_RATE;
  for (size_t i = 0; i < samples; i++) {
    int16_t sample = (int16_t)(sinf(phase) * AMPLITUDE);
    buffer[i * 2]     = sample;
    buffer[i * 2 + 1] = sample;
    phase += step;
    if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
  }
}

esp_err_t audio_beep_init(void) {
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << PIN_POWER_AMP),
    .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&io_conf);
  gpio_set_level(PIN_POWER_AMP, 1);

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = PIN_I2S_MCLK,
      .bclk = PIN_I2S_BCLK,
      .ws   = PIN_I2S_LRCK,
      .dout = PIN_I2S_DOUT,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv   = false,
      },
    },
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

  ESP_LOGI(TAG, "Audio beep initialized (raw I2S, no codec init)");
  return ESP_OK;
}

void audio_beep_play(int frequency_hz, int duration_ms) {
  if (!tx_handle) return;

  const size_t samples_total = (SAMPLE_RATE * duration_ms) / 1000;
  const size_t chunk = 128;
  int16_t buffer[chunk * 2];

  size_t remaining = samples_total;
  while (remaining > 0) {
    size_t to_gen = (remaining < chunk) ? remaining : chunk;
    generate_tone(buffer, to_gen, (float)frequency_hz);
    size_t written = 0;
    i2s_channel_write(tx_handle, buffer, to_gen * 2 * sizeof(int16_t), &written, portMAX_DELAY);
    remaining -= to_gen;
  }
}

void audio_beep_deinit(void) {
  if (tx_handle) {
    i2s_channel_disable(tx_handle);
    i2s_del_channel(tx_handle);
    tx_handle = NULL;
  }
  gpio_set_level(PIN_POWER_AMP, 0);
}
