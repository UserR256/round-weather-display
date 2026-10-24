#include "audio_alarm.h"
#include "audio_beep.h"
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static const char *TAG = "audio_alarm";
static const char *NVS_NAMESPACE = "alarm";
static const char *RADIO_URL = "https://25483.live.streamtheworld.com/RADIO538.mp3";

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

static void radio_task(void *arg) {
  (void)arg;
  ESP_LOGI(TAG, "Starting Radio 538");
  esp_http_client_config_t config = {
    .url = RADIO_URL,
    .timeout_ms = 10000,
    .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (esp_http_client_open(client, 0) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open radio stream");
    s_radio_task = NULL;
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "Radio stream opened, reading...");
  char buffer[1024];
  while (s_ringing) {
    int len = esp_http_client_read(client, buffer, sizeof(buffer));
    if (len <= 0) break;
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  s_radio_task = NULL;
  vTaskDelete(NULL);
}

static void start_alarm_sound(void) {
  s_ringing = true;
  if (s_alarm.type == ALARM_TYPE_RADIO) {
    if (!s_radio_task) {
      xTaskCreate(radio_task, "radio", 8192, NULL, 5, &s_radio_task);
    }
  } else {
    if (!s_beep_task) {
      xTaskCreate(beep_task, "beep", 4096, NULL, 5, &s_beep_task);
    }
  }
}

static void stop_alarm_sound(void) {
  // Only set the flag. The tasks will exit themselves cleanly.
  // Do NOT call vTaskDelete from here — it runs from the LVGL event
  // callback and can deadlock the LVGL lock.
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

  struct tm tm;
  localtime_r(&now, &tm);

  if (tm.tm_hour == s_alarm.hour && tm.tm_min == s_alarm.minute) {
    s_alarm_triggered = true;
    ESP_LOGI(TAG, "Alarm triggered!");
    start_alarm_sound();
  }
}

void audio_alarm_stop(void) {
  ESP_LOGI(TAG, "Alarm stopped");
  s_alarm_triggered = false;
  s_snooze_until = 0;
  stop_alarm_sound();
}

void audio_alarm_snooze(void) {
  ESP_LOGI(TAG, "Snoozed for 10 minutes");
// s_snooze_until = time(NULL) + 10 * 60;
  s_snooze_until = time(NULL) + 10 ; //for testing, set to 10 seconds
  s_alarm_triggered = false;
  stop_alarm_sound();
}

bool audio_alarm_is_ringing(void) {
  return s_ringing;
}
