#include "audio_beep.h"
#include <math.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"

static const char *TAG = "audio_beep";

#define PIN_POWER_AMP  53
#define PIN_I2S_MCLK   13
#define PIN_I2S_BCLK   12
#define PIN_I2S_LRCK   10
#define PIN_I2S_DOUT   9

#define SAMPLE_RATE    44100
#define AMPLITUDE      8000

static i2s_chan_handle_t tx_handle = NULL;
static esp_codec_dev_handle_t codec_dev = NULL;

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

esp_err_t audio_beep_set_format(int sample_rate, int channels) {
  if (!codec_dev) return ESP_FAIL;
  esp_codec_dev_close(codec_dev);
  esp_codec_dev_sample_info_t fs = {
    .sample_rate = sample_rate,
    .channel = channels,
    .bits_per_sample = 16,
  };
  int ret = esp_codec_dev_open(codec_dev, &fs);
  if (ret != ESP_CODEC_DEV_OK) {
    ESP_LOGE(TAG, "reopen failed: %d", ret);
    return ESP_FAIL;
  }
  esp_codec_dev_set_out_vol(codec_dev, 50);
  ESP_LOGI(TAG, "format set: %d Hz, %d ch", sample_rate, channels);
  return ESP_OK;
}

esp_err_t audio_beep_init(void) {
  if (codec_dev) return ESP_OK; 

  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << PIN_POWER_AMP),
    .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&io_conf);
  gpio_set_level(PIN_POWER_AMP, 1);

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num  = 6;
  chan_cfg.dma_frame_num = 1000;   // 6 x 1000 frames is about 136 ms (max is 1023 for 16-bit stereo)
  chan_cfg.auto_clear    = true;   // silence instead of repeating old data on underrun
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
  i2s_channel_enable(tx_handle);   // lets the codec's disable-before-reconfigure succeed
  

  // Codec via BSP I2C
  i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();
  if (!i2c_bus) {
    ESP_LOGE(TAG, "BSP I2C bus not available");
    return ESP_FAIL;
  }

  audio_codec_i2c_cfg_t i2c_cfg = {
    .port = I2C_NUM_0,
    .addr = ES8311_CODEC_DEFAULT_ADDR,
    .bus_handle = i2c_bus,
  };
  const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

  audio_codec_i2s_cfg_t i2s_cfg = {
    .port = I2S_NUM_0,
    .tx_handle = tx_handle,
  };
  const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

  const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

  es8311_codec_cfg_t es8311_cfg = {
    .ctrl_if = ctrl_if,
    .gpio_if = gpio_if,
    .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
    .pa_pin = PIN_POWER_AMP,
    .use_mclk = true,
    .hw_gain.pa_voltage = 5.0,
    .hw_gain.codec_dac_voltage = 3.3,
  };
  const audio_codec_if_t *codec_if = es8311_codec_new(&es8311_cfg);

  esp_codec_dev_cfg_t dev_cfg = {
    .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    .codec_if = codec_if,
    .data_if = data_if,
  };
  codec_dev = esp_codec_dev_new(&dev_cfg);
  if (!codec_dev) {
    ESP_LOGE(TAG, "Failed to create codec device");
    return ESP_FAIL;
  }

  esp_codec_dev_sample_info_t fs = {
    .sample_rate = SAMPLE_RATE,
    .channel = 2,
    .bits_per_sample = 16,
  };
  ESP_ERROR_CHECK(esp_codec_dev_open(codec_dev, &fs));
  ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(codec_dev, 70));

  ESP_LOGI(TAG, "ES8311 codec ready");
  
  return ESP_OK;
}

void audio_beep_play(int frequency_hz, int duration_ms) {
  if (!codec_dev) return;

  const size_t samples_total = (SAMPLE_RATE * duration_ms) / 1000;
  const size_t chunk = 128;
  int16_t buffer[chunk * 2];

  size_t remaining = samples_total;
  while (remaining > 0) {
    size_t to_gen = (remaining < chunk) ? remaining : chunk;
    generate_tone(buffer, to_gen, (float)frequency_hz);
    
    esp_codec_dev_write(codec_dev, buffer, to_gen * 2 * sizeof(int16_t));
    int ret = esp_codec_dev_write(codec_dev, buffer, to_gen * 2 * sizeof(int16_t));
    if (ret != ESP_CODEC_DEV_OK) ESP_LOGE(TAG, "write failed: %d", ret);

    remaining -= to_gen;
  }
}

    void audio_beep_write_pcm(const int16_t *pcm, size_t bytes) {
      if (!codec_dev || !pcm || bytes == 0) return;
      int ret = esp_codec_dev_write(codec_dev, (void *)pcm, bytes);
      if (ret != ESP_CODEC_DEV_OK) {
        static int errs = 0;
        if (errs++ < 10) ESP_LOGE(TAG, "pcm write failed: %d", ret);
      }
    }

void audio_beep_deinit(void) {
  if (codec_dev) {
    esp_codec_dev_close(codec_dev);
    esp_codec_dev_delete(codec_dev);
    codec_dev = NULL;
  }
  if (tx_handle) {
    i2s_del_channel(tx_handle);
    tx_handle = NULL;
  }
  gpio_set_level(PIN_POWER_AMP, 0);
}
