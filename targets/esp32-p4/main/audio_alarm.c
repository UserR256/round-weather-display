#include "audio_alarm.h"
#include "audio_beep.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#define RADIO_STACK_SIZE 24576
#define RADIO_MAX_RETRIES     5
#define RADIO_RETRY_DELAY_MS  2000
#define MP3_BUF_SIZE     8192
#define MP3_REFILL_AT    4096
#define STREAM_RATE      44100   // must match SAMPLE_RATE in audio_beep.c


static StaticTask_t s_radio_tcb;            // task control block, internal RAM
static StackType_t *s_radio_stack = NULL;   // the stack itself, lives in PSRAM

static const char *TAG = "audio_alarm";
static const char *NVS_NAMESPACE = "alarm";
static const char *RADIO_URL = "http://25483.live.streamtheworld.com/RADIO538.mp3";

static alarm_config_t s_alarm = {
  .enabled = false,
  .hour = 7,
  .minute = 0,
  .type = ALARM_TYPE_BEEP,
};
static bool s_alarm_triggered = false;
static bool s_ringing = false;
static time_t s_snooze_until = 0;
static TaskHandle_t s_radio_task = NULL;
static TaskHandle_t s_beep_task = NULL;
static int s_last_triggered_minute = -1;

static esp_err_t load_from_nvs(void) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
  if (err != ESP_OK) return err;

  uint8_t enabled = 0, hour = 7, minute = 0, type = 0;
  nvs_get_u8(nvs, "enabled", &enabled);
  nvs_get_u8(nvs, "hour", &hour);
  nvs_get_u8(nvs, "minute", &minute);
  nvs_get_u8(nvs, "type", &type);
  nvs_close(nvs);

  s_alarm.enabled = enabled;
  s_alarm.hour = hour;
  s_alarm.minute = minute;
  s_alarm.type = type;
  return ESP_OK;
}

static esp_err_t save_to_nvs(void) {
  nvs_handle_t nvs;
  ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));
  ESP_ERROR_CHECK(nvs_set_u8(nvs, "enabled", s_alarm.enabled));
  ESP_ERROR_CHECK(nvs_set_u8(nvs, "hour", s_alarm.hour));
  ESP_ERROR_CHECK(nvs_set_u8(nvs, "minute", s_alarm.minute));
  ESP_ERROR_CHECK(nvs_set_u8(nvs, "type", s_alarm.type));
  ESP_ERROR_CHECK(nvs_commit(nvs));
  nvs_close(nvs);
  return ESP_OK;
}

static void beep_task(void *arg) {
  (void)arg;
  audio_beep_init();
  while (s_ringing) {
    audio_beep_play(1000, 250);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (!s_ringing) break;
    audio_beep_play(1200, 250);
    vTaskDelay(pdMS_TO_TICKS(400));
  }
  audio_beep_deinit();
  s_beep_task = NULL;
  vTaskDelete(NULL);
}

static bool radio_open(esp_http_client_handle_t client) {
  esp_http_client_set_url(client, RADIO_URL);   // reset: een eerdere redirect heeft de URL veranderd
  for (int attempt = 0; attempt < 5; attempt++) {
    if (esp_http_client_open(client, 0) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to open radio stream");
      break;
    }
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status == 200) return true;
    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
      esp_http_client_set_redirection(client);
      esp_http_client_close(client);
      continue;
    }
    ESP_LOGE(TAG, "Unexpected HTTP status %d", status);
    break;
  }
  esp_http_client_close(client);
  return false;
}

static void radio_task(void *arg) {
  (void)arg;

  esp_http_client_config_t config = {
    .url = RADIO_URL,
    .timeout_ms = 5000,
    .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  mp3dec_t *dec = malloc(sizeof(mp3dec_t));
  uint8_t *mp3_buf = malloc(MP3_BUF_SIZE);
  int16_t *pcm = malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t));
  if (!client || !dec || !mp3_buf || !pcm) {
    ESP_LOGE(TAG, "Failed to allocate radio resources");
    free(dec); free(mp3_buf); free(pcm);
    if (client) esp_http_client_cleanup(client);
    s_radio_task = NULL;
    vTaskDelete(NULL);
    return;
  }

  int cur_rate = STREAM_RATE;
  int failures = 0;

  while (s_ringing) {
    if (!radio_open(client)) {
      if (++failures >= RADIO_MAX_RETRIES) break;
      ESP_LOGW(TAG, "Retry %d/%d", failures, RADIO_MAX_RETRIES);
      vTaskDelay(pdMS_TO_TICKS(RADIO_RETRY_DELAY_MS));
      continue;
    }

    audio_beep_init();        // doet niets als de codec al open is
    mp3dec_init(dec);
    int mp3_buf_len = 0;
    bool got_audio = false;

    while (s_ringing) {
      if (mp3_buf_len < MP3_REFILL_AT) {
        int n = esp_http_client_read(client, (char *)(mp3_buf + mp3_buf_len),
                                     MP3_BUF_SIZE - mp3_buf_len);
        if (n <= 0) { ESP_LOGW(TAG, "stream read returned %d", n); break; }
        mp3_buf_len += n;
      }

      mp3dec_frame_info_t info;
      int samples = mp3dec_decode_frame(dec, mp3_buf, mp3_buf_len, pcm, &info);
      int consumed = info.frame_bytes;

      if (samples > 0) {
        got_audio = true;
        if (info.hz != cur_rate) {
          if (audio_beep_set_format(info.hz, 2) == ESP_OK) cur_rate = info.hz;
        }
        int ch = info.channels;
        if (ch == 1) {                     // codec is stereo: expand in place
          for (int i = samples - 1; i >= 0; i--) {
            pcm[2 * i] = pcm[2 * i + 1] = pcm[i];
          }
          ch = 2;
        }
        audio_beep_write_pcm(pcm, (size_t)samples * ch * sizeof(int16_t));
      }

      if (consumed > 0 && consumed <= mp3_buf_len) {
        memmove(mp3_buf, mp3_buf + consumed, mp3_buf_len - consumed);
        mp3_buf_len -= consumed;
      } else if (consumed == 0 && mp3_buf_len >= MP3_REFILL_AT) {
        ESP_LOGW(TAG, "no MP3 frame in %d bytes, resyncing", mp3_buf_len);
        mp3_buf_len = 0;
      }

      vTaskDelay(1);   // always give IDLE1 a chance to run
    }

    esp_http_client_close(client);
    if (!s_ringing) break;                 // gestopt of gesnoozed: geen reconnect

    // Stream viel weg. Als hij eerst goed liep, begint de teller opnieuw.
    failures = got_audio ? 1 : failures + 1;
    if (failures >= RADIO_MAX_RETRIES) break;
    ESP_LOGW(TAG, "Stream lost, reconnecting (%d/%d)", failures, RADIO_MAX_RETRIES);
    vTaskDelay(pdMS_TO_TICKS(RADIO_RETRY_DELAY_MS));
  }

  // Radio lukt niet meer: val terug op de piep tot het alarm stopt
  if (s_ringing) {
    ESP_LOGE(TAG, "Radio failed, falling back to beep");
    if (audio_beep_init() == ESP_OK) {
      if (cur_rate != STREAM_RATE && audio_beep_set_format(STREAM_RATE, 2) == ESP_OK) {
        cur_rate = STREAM_RATE;            // de piep-generator werkt op STREAM_RATE
      }
      while (s_ringing) {
        audio_beep_play(1000, 250);
        vTaskDelay(pdMS_TO_TICKS(150));
        if (!s_ringing) break;
        audio_beep_play(1200, 250);
        vTaskDelay(pdMS_TO_TICKS(400));
      }
    }
  }

  free(dec);
  free(mp3_buf);
  free(pcm);
  esp_http_client_cleanup(client);
  audio_beep_deinit();
  s_radio_task = NULL;
  vTaskDelete(NULL);
}

static void start_alarm_sound(void) {
  s_ringing = true;
  if (s_alarm.type == ALARM_TYPE_RADIO) {
    if (!s_radio_task) {
      if (!s_radio_stack) {
        s_radio_stack = heap_caps_malloc(RADIO_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      }
      if (s_radio_stack) {
        s_radio_task = xTaskCreateStatic(radio_task, "radio", RADIO_STACK_SIZE,
                                         NULL, 5, s_radio_stack, &s_radio_tcb);
      } else {
        ESP_LOGE(TAG, "No PSRAM for radio stack");
      }
    }
  } else {
    if (!s_beep_task) {
      xTaskCreate(beep_task, "beep", 4096, NULL, 5, &s_beep_task);
    }
  }
}

static void stop_alarm_sound(void) {
  s_ringing = false;
}

esp_err_t audio_alarm_init(void) {
  load_from_nvs();
  ESP_LOGI(TAG, "Alarm loaded: %02d:%02d %s %s",
           s_alarm.hour, s_alarm.minute,
           s_alarm.enabled ? "ON" : "OFF",
           s_alarm.type == ALARM_TYPE_RADIO ? "RADIO" : "BEEP");
  return ESP_OK;
}

esp_err_t audio_alarm_set(const alarm_config_t *config) {
  if (!config) return ESP_ERR_INVALID_ARG;
  s_alarm = *config;
  s_alarm_triggered = false;
  s_snooze_until = 0;
  s_last_triggered_minute = -1;
  return save_to_nvs();
}

esp_err_t audio_alarm_get(alarm_config_t *config) {
  if (!config) return ESP_ERR_INVALID_ARG;
  *config = s_alarm;
  return ESP_OK;
}

void audio_alarm_check(void) {
  if (!s_alarm.enabled) return;

  time_t now = time(NULL);
  if (now < 1704067200) return;

  struct tm tm;
  localtime_r(&now, &tm);
  int current_minute_key = tm.tm_hour * 60 + tm.tm_min;

  // Reset trigger flag when the minute has changed
  if (s_last_triggered_minute != -1 && s_last_triggered_minute != current_minute_key) {
    s_alarm_triggered = false;
    s_last_triggered_minute = -1;
  }

  if (s_snooze_until > 0) {
    if (now >= s_snooze_until) {
      s_snooze_until = 0;
      s_alarm_triggered = false;
      ESP_LOGI(TAG, "Snooze ended, ringing again");
      start_alarm_sound();
    }
    return;
  }

  if (s_alarm_triggered || s_ringing) return;

  if (tm.tm_hour == s_alarm.hour && tm.tm_min == s_alarm.minute) {
    s_alarm_triggered = true;
    s_last_triggered_minute = current_minute_key;
    ESP_LOGI(TAG, "Alarm triggered!");
    start_alarm_sound();
  }
}

void audio_alarm_stop(void) {
  ESP_LOGI(TAG, "Alarm stopped");
  // Keep s_alarm_triggered = true so it does not re-trigger while still on this minute
  s_snooze_until = 0;
  stop_alarm_sound();
}

void audio_alarm_snooze(void) {
  ESP_LOGI(TAG, "Snoozed for 10 minutes");
  s_snooze_until = time(NULL) + 10 * 60;
  s_alarm_triggered = false;
  stop_alarm_sound();
}

bool audio_alarm_is_ringing(void) {
  return s_ringing;
}