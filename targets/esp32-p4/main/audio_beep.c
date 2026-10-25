#include "audio_beep.h"
#include <math.h>
#include <stdbool.h>
#include "esp_heap_caps.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"

#include "heap_diag.h"

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
static uint8_t s_volume = 70;   // 0-100
static bool s_open = false;     // codec currently open (amp on)

void audio_beep_set_volume(uint8_t volume) {
  s_volume = volume > 100 ? 100 : volume;
  if (codec_dev) esp_codec_dev_set_out_vol(codec_dev, s_volume);
}

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
  if (!codec_dev || !s_open) return ESP_FAIL;
  esp_codec_dev_close(codec_dev);
  if (tx_handle) i2s_channel_enable(tx_handle);   // so the codec's disable-before-reconfigure succeeds
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
  esp_codec_dev_set_out_vol(codec_dev, s_volume);
  return ESP_OK;
}

static const audio_codec_ctrl_if_t *s_ctrl_if = NULL;
static const audio_codec_data_if_t *s_data_if = NULL;
static const audio_codec_gpio_if_t *s_gpio_if = NULL;
static const audio_codec_if_t *s_codec_if = NULL;

// Releases everything audio_beep_init() may have created, in reverse order.
// Safe to call at any point, including after a partial init.
static void audio_release(void) {
  bool codec_owned_channel = (codec_dev != NULL);
  if (codec_dev) {
    esp_codec_dev_close(codec_dev);
    esp_codec_dev_delete(codec_dev);
    codec_dev = NULL;
  }
  if (s_codec_if) { audio_codec_delete_codec_if(s_codec_if); s_codec_if = NULL; }
  if (s_data_if)  { audio_codec_delete_data_if(s_data_if);   s_data_if = NULL; }
  if (s_ctrl_if)  { audio_codec_delete_ctrl_if(s_ctrl_if);   s_ctrl_if = NULL; }
  if (s_gpio_if)  { audio_codec_delete_gpio_if(s_gpio_if);   s_gpio_if = NULL; }
  if (tx_handle) {
    // esp_codec_dev_close() already disabled the channel; only disable it
    // ourselves after a partial init, or the driver logs "not been enabled".
    if (!codec_owned_channel) i2s_channel_disable(tx_handle);
    i2s_del_channel(tx_handle);
    tx_handle = NULL;
  }
  s_open = false;
  gpio_set_level(PIN_POWER_AMP, 0);
}

esp_err_t audio_beep_init(void) {
  if (codec_dev && s_open) return ESP_OK;

  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << PIN_POWER_AMP),
    .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&io_conf);
  gpio_set_level(PIN_POWER_AMP, 1);

  heap_diag("before i2s alloc");
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

  // The DMA buffers live in internal RAM, which is scarce once Wi-Fi and the UI
  // are up (measured: ~30 KB DMA-capable free). Each buffer is 1008 frames x 4
  // bytes = ~4 KB (23 ms). Prefer 4 buffers (~91 ms, 16 KB) and step down
  // rather than fail outright; 2 buffers (~46 ms) still plays.
  static const uint32_t desc_tries[] = {4, 3, 2};
  bool i2s_ready = false;
  for (size_t t = 0; t < sizeof(desc_tries) / sizeof(desc_tries[0]) && !i2s_ready; t++) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = desc_tries[t];
    chan_cfg.dma_frame_num = 1000;
    chan_cfg.auto_clear    = true;
    if (i2s_new_channel(&chan_cfg, &tx_handle, NULL) != ESP_OK) {
      tx_handle = NULL;
      continue;
    }
    if (i2s_channel_init_std_mode(tx_handle, &std_cfg) == ESP_OK) {
      ESP_LOGI(TAG, "I2S ready with %u DMA buffers", (unsigned) desc_tries[t]);
      i2s_ready = true;
    } else {
      ESP_LOGW(TAG, "I2S init with %u DMA buffers failed, trying fewer", (unsigned) desc_tries[t]);
      i2s_del_channel(tx_handle);   // never enabled, so no disable needed
      tx_handle = NULL;
    }
  }
  if (!i2s_ready) {
    ESP_LOGE(TAG, "I2S init failed even with the smallest buffer");
    heap_diag("i2s alloc failed");
    audio_release();
    return ESP_FAIL;
  }
  i2s_channel_enable(tx_handle);

  i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();
  if (!i2c_bus) {
    ESP_LOGE(TAG, "BSP I2C bus not available");
    audio_release();
    return ESP_FAIL;
  }

  audio_codec_i2c_cfg_t i2c_cfg = {
    .port = I2C_NUM_0,
    .addr = ES8311_CODEC_DEFAULT_ADDR,
    .bus_handle = i2c_bus,
  };
  s_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

  audio_codec_i2s_cfg_t i2s_cfg = {
    .port = I2S_NUM_0,
    .tx_handle = tx_handle,
  };
  s_data_if = audio_codec_new_i2s_data(&i2s_cfg);

  s_gpio_if = audio_codec_new_gpio();

  es8311_codec_cfg_t es8311_cfg = {
    .ctrl_if = s_ctrl_if,
    .gpio_if = s_gpio_if,
    .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
    .pa_pin = PIN_POWER_AMP,
    .use_mclk = true,
    .hw_gain.pa_voltage = 5.0,
    .hw_gain.codec_dac_voltage = 3.3,
  };
  s_codec_if = es8311_codec_new(&es8311_cfg);

  esp_codec_dev_cfg_t dev_cfg = {
    .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    .codec_if = s_codec_if,
    .data_if = s_data_if,
  };
  codec_dev = esp_codec_dev_new(&dev_cfg);
  if (!codec_dev) {
    ESP_LOGE(TAG, "Failed to create codec device");
    audio_release();
    return ESP_FAIL;
  }

  esp_codec_dev_sample_info_t fs = {
    .sample_rate = SAMPLE_RATE,
    .channel = 2,
    .bits_per_sample = 16,
  };
  if (esp_codec_dev_open(codec_dev, &fs) != ESP_CODEC_DEV_OK) {
    ESP_LOGE(TAG, "codec open failed");
    audio_release();
    return ESP_FAIL;
  }
  esp_codec_dev_set_out_vol(codec_dev, s_volume);

  s_open = true;
  ESP_LOGI(TAG, "ES8311 codec ready");
  return ESP_OK;
}

void audio_beep_play(int frequency_hz, int duration_ms) {
  if (!codec_dev || !s_open) return;

  const size_t samples_total = (SAMPLE_RATE * duration_ms) / 1000;
  const size_t chunk = 128;
  int16_t buffer[chunk * 2];

  size_t remaining = samples_total;
  while (remaining > 0) {
    size_t to_gen = (remaining < chunk) ? remaining : chunk;
    generate_tone(buffer, to_gen, (float)frequency_hz);
    int ret = esp_codec_dev_write(codec_dev, buffer, to_gen * 2 * sizeof(int16_t));
    if (ret != ESP_CODEC_DEV_OK) ESP_LOGE(TAG, "write failed: %d", ret);
    remaining -= to_gen;
  }
}

void audio_beep_write_pcm(const int16_t *pcm, size_t bytes) {
  if (!codec_dev || !s_open || !pcm || bytes == 0) return;
  int ret = esp_codec_dev_write(codec_dev, (void *)pcm, bytes);
  if (ret != ESP_CODEC_DEV_OK) {
    static int errs = 0;
    if (errs++ < 10) ESP_LOGE(TAG, "pcm write failed: %d", ret);
  }
}

// Releases the codec, the I2S channel and its DMA buffers (amp off).
void audio_beep_deinit(void) {
  audio_release();
}
