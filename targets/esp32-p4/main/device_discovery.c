#include "device_discovery.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "audio_alarm.h"
#include "connectivity.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs_flash.h"

static const char *TAG = "discovery";

#define HOSTNAME_BASE   "weerklok"
#define BOOT_BUTTON_GPIO GPIO_NUM_35   // BOOT button on the Waveshare ESP32-P4 boards (strapping pin)
#define RESET_HOLD_S    10

static char s_hostname[DISCOVERY_HOSTNAME_MAX] = HOSTNAME_BASE;
static bool s_mdns_started = false;

// ---- host name --------------------------------------------------------------

void discovery_hostname(const device_config_t *config, char *out, size_t out_size) {
  if (out == NULL || out_size == 0) return;
  char slug[DISCOVERY_HOSTNAME_MAX] = "";
  size_t n = 0;
  bool dash = false;
  const char *room = config != NULL ? config->room_name : "";
  // The default room name says nothing; keep the plain base name then.
  if (strcasecmp(room, "Clock") == 0) room = "";
  for (const unsigned char *p = (const unsigned char *) room; *p && n + 1 < sizeof(slug); p++) {
    if (isalnum(*p) && *p < 0x80) {
      if (dash && n > 0 && n + 2 < sizeof(slug)) slug[n++] = '-';
      slug[n++] = (char) tolower(*p);
      dash = false;
    } else if (*p < 0x80) {
      dash = true;     // spaces, punctuation -> one '-'
    }                  // other UTF-8 bytes (accents) are dropped
  }
  slug[n] = '\0';
  // "weerklok-" + slug, within the DNS label limit we allow
  size_t room_max = out_size > sizeof(HOSTNAME_BASE) + 1 ? out_size - sizeof(HOSTNAME_BASE) - 1 : 0;
  if (n > 0 && room_max > 0) {
    snprintf(out, out_size, "%s-%.*s", HOSTNAME_BASE, (int) (n < room_max ? n : room_max), slug);
    // never end on a dash after cutting
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == '-') out[--len] = '\0';
  } else {
    snprintf(out, out_size, "%s", HOSTNAME_BASE);
  }
}

void discovery_set_dhcp_hostname(void *esp_netif_sta, const device_config_t *config) {
  discovery_hostname(config, s_hostname, sizeof(s_hostname));
  if (esp_netif_sta == NULL) return;
  esp_err_t err = esp_netif_set_hostname((esp_netif_t *) esp_netif_sta, s_hostname);
  if (err != ESP_OK) ESP_LOGW(TAG, "Setting DHCP host name failed: %s", esp_err_to_name(err));
}

esp_err_t discovery_start_mdns(const device_config_t *config) {
  discovery_hostname(config, s_hostname, sizeof(s_hostname));
  if (s_mdns_started) return ESP_OK;
  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
    return err;
  }
  mdns_hostname_set(s_hostname);
  char instance[16 + DEVICE_CONFIG_STR_LEN];
  snprintf(instance, sizeof(instance), "Weerklok %s", config != NULL ? config->room_name : "");
  mdns_instance_name_set(instance);
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  s_mdns_started = true;
  ESP_LOGI(TAG, "Web page at http://%s.local/", s_hostname);
  return ESP_OK;
}

void discovery_local_url(char *out, size_t out_size) {
  snprintf(out, out_size, "http://%s.local/", s_hostname);
}

bool discovery_ip_url(char *out, size_t out_size) {
  char ip[20];
  if (!connectivity_get_local_ipv4(ip, sizeof(ip))) return false;
  snprintf(out, out_size, "http://%s/", ip);
  return true;
}

// ---- factory reset ----------------------------------------------------------

void device_factory_reset(void) {
  ESP_LOGW(TAG, "Factory reset: erasing all settings");
  audio_radio_stop();
  // Forget the network on the Wi-Fi co-processor too (best effort).
  (void) esp_wifi_disconnect();
  (void) esp_wifi_restore();
  esp_err_t err = nvs_flash_erase();   // deinitialises NVS first
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Erasing settings failed: %s", esp_err_to_name(err));
    return;
  }
  ESP_LOGW(TAG, "Settings erased, restarting");
  vTaskDelay(pdMS_TO_TICKS(200));
  esp_restart();
}

static void reset_task(void *arg) {
  vTaskDelay(pdMS_TO_TICKS((uint32_t) (uintptr_t) arg));
  device_factory_reset();
  vTaskDelete(NULL);
}

void device_factory_reset_async(uint32_t delay_ms) {
  static bool started = false;
  if (started) return;
  started = true;
  if (xTaskCreate(reset_task, "factory_reset", 4096, (void *) (uintptr_t) delay_ms, 5, NULL) != pdPASS) {
    device_factory_reset();
  }
}

uint32_t device_reset_button_poll(void) {
  static bool configured = false;
  static bool armed = false;     // seen released at least once (not stuck low, not held from boot)
  static uint32_t held_s = 0;
  if (!configured) {
    const gpio_config_t io = {
      .pin_bit_mask = 1ULL << BOOT_BUTTON_GPIO,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    configured = gpio_config(&io) == ESP_OK;
    if (!configured) return 0;
  }
  bool pressed = gpio_get_level(BOOT_BUTTON_GPIO) == 0;
  if (!pressed) {
    armed = true;
    if (held_s > 0) ESP_LOGI(TAG, "BOOT button released after %lus, no reset", (unsigned long) held_s);
    held_s = 0;
    return 0;
  }
  if (!armed) return 0;
  held_s++;
  if (held_s == 3) ESP_LOGW(TAG, "BOOT button held: keep holding for a factory reset");
  if (held_s >= RESET_HOLD_S) device_factory_reset_async(0);
  return held_s;
}
