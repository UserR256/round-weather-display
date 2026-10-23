#include "audio_beep.h"
#include <math.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "es8311.h"

static const char *TAG = "audio_beep";

#define PIN_POWER_AMP  53
#define PIN_I2S_MCLK   13
#define PIN_I2S_BCLK   12
#define PIN_I2S_LRCK   10
#define PIN_I2S_DOUT   9
#define PIN_I2C_SDA    7
#define PIN_I2C_SCL    8

#define SAMPLE_RATE    16000
#define AMPLITUDE      8000

static i2s_chan_handle_t tx_handle = NULL;
static es8311_handle_t es_handle = NULL;

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

static esp_err_t init_es8311(void) {
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = PIN_I2C_SDA,
    .scl_io_num = PIN_I2C_SCL,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000,
  };
  ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));

  es_handle = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  if (!es_handle) {
    ESP_LOGE(TAG, "Failed to create ES8311 handle");
    return ESP_FAIL;
  }

  const es8311_clock_config_t es_clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = SAMPLE_RATE * 256,
    .sample_frequency = SAMPLE_RATE,
  };

  ESP_ERROR_CHECK(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
  ESP_ERROR_CHECK(es8311_sample_frequency_config(es_handle, es_clk.mclk_frequency, es_clk.sample_frequency));
  ESP_ERROR_CHECK(es8311_voice_volume_set(es_handle, 80, NULL));
  ESP_ERROR_CHECK(es8311_microphone_config(es_handle, false));

  ESP_LOGI(TAG, "ES8311 initialized");
  return ESP_OK;
}

esp_err_t audio_beep_init(void) {
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << PIN_POWER_AMP),
    .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&io_conf);
  gpio_set_level(PIN_POWER_AMP, 1);

  ESP_ERROR_CHECK(init_es8311());

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

  ESP_LOGI(TAG, "Audio beep initialized");
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
  if (es_handle) {
    es8311_delete(es_handle);
    es_handle = NULL;
  }
  i2c_driver_delete(I2C_NUM_0);
  gpio_set_level(PIN_POWER_AMP, 0);
}
