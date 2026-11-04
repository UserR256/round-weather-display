#include "settings_web.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "audio_alarm.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"   /* connectivity.h uses TickType_t */
#include "connectivity.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "location_lookup.h"
#include "device_discovery.h"

static const char *TAG = "settings_web";

#define SETTINGS_MAX_HTTP_BODY 2048
#define RESTART_DELAY_US       (1500 * 1000)   // lets the response reach the browser first

static const device_config_t *s_config = NULL;
static esp_timer_handle_t s_restart_timer = NULL;
static volatile bool s_restart_pending = false;

// --- small HTTP/JSON helpers -------------------------------------------------

static esp_err_t send_json(httpd_req_t *req, const char *status, cJSON *json) {
  if (json == NULL) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON allocate failed");
  char *body = cJSON_PrintUnformatted(json);
  cJSON_Delete(json);
  if (body == NULL) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encode failed");
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  esp_err_t err = httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
  free(body);
  return err;
}

static esp_err_t send_error(httpd_req_t *req, const char *status, const char *message) {
  cJSON *json = cJSON_CreateObject();
  if (json != NULL) cJSON_AddStringToObject(json, "error", message);
  return send_json(req, status, json);
}

// Parses the request body as JSON. On failure it has already answered.
static cJSON *read_json_body(httpd_req_t *req) {
  if (req->content_len <= 0) { send_error(req, "400 Bad Request", "Empty request"); return NULL; }
  if (req->content_len > SETTINGS_MAX_HTTP_BODY) { send_error(req, "400 Bad Request", "Request too large"); return NULL; }
  char *buf = calloc(req->content_len + 1, 1);
  if (buf == NULL) { send_error(req, "500 Internal Server Error", "Out of memory"); return NULL; }
  size_t received = 0;
  while (received < req->content_len) {
    int ret = httpd_req_recv(req, buf + received, req->content_len - received);
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (ret <= 0) { free(buf); send_error(req, "400 Bad Request", "Read failed"); return NULL; }
    received += (size_t) ret;
  }
  cJSON *json = cJSON_Parse(buf);
  free(buf);
  if (json == NULL) send_error(req, "400 Bad Request", "Invalid JSON");
  return json;
}

// Copies a trimmed string field into dst. Returns NULL if absent (dst untouched),
// "" on success, or an error message.
static const char *take_string(const cJSON *body, const char *key, char *dst, size_t dst_size,
                               bool allow_empty, bool *present) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(body, key);
  if (present) *present = false;
  if (item == NULL || cJSON_IsNull(item)) return NULL;
  if (!cJSON_IsString(item)) return "Invalid value";
  const char *s = item->valuestring;
  while (*s && isspace((unsigned char) *s)) s++;
  size_t len = strlen(s);
  while (len > 0 && isspace((unsigned char) s[len - 1])) len--;
  if (len == 0 && !allow_empty) return "Value is empty";
  if (len >= dst_size) return "Value is too long";
  memcpy(dst, s, len);
  dst[len] = '\0';
  if (present) *present = true;
  return "";
}

// Takes one of the allowed values (case-insensitive), stored as the allowed spelling.
static const char *take_choice(const cJSON *body, const char *key, char *dst, size_t dst_size,
                               const char *const *allowed) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(body, key);
  if (item == NULL || cJSON_IsNull(item)) return NULL;
  if (!cJSON_IsString(item)) return "Invalid value";
  for (; *allowed; allowed++) {
    if (strcasecmp(item->valuestring, *allowed) == 0) {
      strlcpy(dst, *allowed, dst_size);
      return "";
    }
  }
  return "Unknown option";
}

static bool valid_hhmm(const char *s) {
  return strlen(s) == 5 && isdigit((unsigned char) s[0]) && isdigit((unsigned char) s[1]) && s[2] == ':'
      && isdigit((unsigned char) s[3]) && isdigit((unsigned char) s[4])
      && ((s[0] - '0') * 10 + (s[1] - '0')) < 24 && ((s[3] - '0') * 10 + (s[4] - '0')) < 60;
}

// Device ids end up in HTTP headers and message targets between clocks.
static bool valid_device_id(const char *s) {
  for (; *s; s++) {
    if (!isalnum((unsigned char) *s) && *s != '-' && *s != '_' && *s != '.') return false;
  }
  return true;
}

static void restart_timer_cb(void *arg) {
  (void) arg;
  ESP_LOGI(TAG, "Restarting to apply settings from the web page");
  esp_restart();
}

static void schedule_restart(void) {
  s_restart_pending = true;
  if (s_restart_timer == NULL) {
    const esp_timer_create_args_t args = { .callback = restart_timer_cb, .name = "web_restart" };
    if (esp_timer_create(&args, &s_restart_timer) != ESP_OK) {
      esp_restart();
      return;
    }
  }
  esp_timer_start_once(s_restart_timer, RESTART_DELAY_US);
}

// --- /api/settings -----------------------------------------------------------

static cJSON *settings_to_json(const device_config_t *c) {
  cJSON *json = cJSON_CreateObject();
  if (json == NULL) return NULL;
  cJSON_AddStringToObject(json, "roomName", c->room_name);
  cJSON_AddStringToObject(json, "deviceId", c->device_id);
  cJSON_AddStringToObject(json, "messageSharing", c->message_sharing);
  cJSON_AddStringToObject(json, "language", c->language);
  cJSON_AddStringToObject(json, "clockFace", c->default_clock_face);
  cJSON_AddStringToObject(json, "timeFormat", c->time_format);
  cJSON_AddBoolToObject(json, "leadingZero", c->leading_zero_12h);
  cJSON_AddStringToObject(json, "units", c->units);
  cJSON_AddBoolToObject(json, "nightShift", c->night_shift_enabled);
  cJSON_AddStringToObject(json, "nightShiftStart", c->night_shift_start);
  cJSON_AddStringToObject(json, "nightShiftEnd", c->night_shift_end);
  cJSON_AddStringToObject(json, "location", c->location);
  cJSON_AddBoolToObject(json, "locationReady", c->location_ready);
  cJSON_AddStringToObject(json, "timezone", c->timezone);
  cJSON_AddStringToObject(json, "latitude", c->latitude);
  cJSON_AddStringToObject(json, "longitude", c->longitude);
  cJSON_AddStringToObject(json, "wifiSsid", c->wifi_ssid);   // never the password
  char url[64];
  discovery_local_url(url, sizeof(url));
  cJSON_AddStringToObject(json, "localUrl", url);
  cJSON_AddBoolToObject(json, "restartPending", s_restart_pending);
  return json;
}

static esp_err_t settings_get_handler(httpd_req_t *req) {
  return send_json(req, "200 OK", settings_to_json(s_config));
}

#define TAKE(expr, field_label)                                              \
  do {                                                                       \
    const char *problem_ = (expr);                                           \
    if (problem_ != NULL && problem_[0] != '\0') {                           \
      snprintf(error, sizeof(error), "%s: %s", field_label, problem_);       \
      goto invalid;                                                          \
    }                                                                        \
  } while (0)

static esp_err_t settings_post_handler(httpd_req_t *req) {
  if (s_restart_pending) return send_error(req, "409 Conflict", "The clock is restarting");

  cJSON *body = read_json_body(req);
  if (body == NULL) return ESP_OK;

  static const char *const SHARING[] = { "single", "shared", NULL };
  static const char *const LANGUAGES[] = { "en", "nl", NULL };
  static const char *const FACES[] = { "digital", "analog", NULL };
  static const char *const TIME_FORMATS[] = { "12", "24", NULL };
  static const char *const UNITS[] = { "metric", "imperial", NULL };

  device_config_t *next = malloc(sizeof(*next));
  if (next == NULL) { cJSON_Delete(body); return send_error(req, "500 Internal Server Error", "Out of memory"); }
  *next = *s_config;

  char error[96] = "";
  char location_query[DEVICE_CONFIG_STR_LEN] = "";
  bool location_given = false;
  bool location_changed = false;
  bool device_id_given = false;   // an id set earlier on the screen is not re-checked
  const cJSON *item = NULL;
  cJSON *json = NULL;
  esp_err_t err = ESP_OK;

  TAKE(take_string(body, "roomName", next->room_name, sizeof(next->room_name), false, NULL), "Room name");
  TAKE(take_string(body, "deviceId", next->device_id, sizeof(next->device_id), false, &device_id_given), "Device ID");
  if (device_id_given && !valid_device_id(next->device_id)) {
    snprintf(error, sizeof(error), "Device ID: use letters, digits, '-', '_' or '.'");
    goto invalid;
  }
  TAKE(take_choice(body, "messageSharing", next->message_sharing, sizeof(next->message_sharing), SHARING), "House messages");
  TAKE(take_choice(body, "language", next->language, sizeof(next->language), LANGUAGES), "Language");
  TAKE(take_choice(body, "clockFace", next->default_clock_face, sizeof(next->default_clock_face), FACES), "Home screen");
  TAKE(take_choice(body, "timeFormat", next->time_format, sizeof(next->time_format), TIME_FORMATS), "Time format");
  TAKE(take_choice(body, "units", next->units, sizeof(next->units), UNITS), "Units");
  TAKE(take_string(body, "nightShiftStart", next->night_shift_start, sizeof(next->night_shift_start), false, NULL), "Night shift start");
  TAKE(take_string(body, "nightShiftEnd", next->night_shift_end, sizeof(next->night_shift_end), false, NULL), "Night shift end");
  if (!valid_hhmm(next->night_shift_start) || !valid_hhmm(next->night_shift_end)) {
    snprintf(error, sizeof(error), "Night shift: use HH:MM");
    goto invalid;
  }
  TAKE(take_string(body, "location", location_query, sizeof(location_query), true, &location_given), "Location");

  item = cJSON_GetObjectItemCaseSensitive(body, "leadingZero");
  if (cJSON_IsBool(item)) next->leading_zero_12h = cJSON_IsTrue(item);
  item = cJSON_GetObjectItemCaseSensitive(body, "nightShift");
  if (cJSON_IsBool(item)) next->night_shift_enabled = cJSON_IsTrue(item);
  cJSON_Delete(body);
  body = NULL;

  // Same rule as the settings screen: shared messages need a unique id.
  if (strcasecmp(next->message_sharing, "shared") == 0 && strcasecmp(next->device_id, "clock-esp32-p4") == 0) {
    snprintf(error, sizeof(error), "Set a unique device ID before enabling shared house messages");
    goto invalid;
  }

  // A new location is resolved to timezone + coordinates, like on the screen.
  location_changed = location_given && location_query[0] != '\0'
    && (strcmp(location_query, s_config->location) != 0 || !s_config->location_ready);
  if (location_given && location_query[0] == '\0' && !s_config->location_ready) {
    snprintf(error, sizeof(error), "Location: enter a city, like Amsterdam");
    goto invalid;
  }
  if (location_changed) {
    if (!connectivity_is_wifi_connected()) {
      snprintf(error, sizeof(error), "Location: no internet connection for the lookup");
      goto invalid;
    }
    location_lookup_result_t result;
    memset(&result, 0, sizeof(result));
    ESP_LOGI(TAG, "Looking up location '%s'", location_query);
    err = location_lookup_resolve(location_query, &result);   // blocks this request for a few seconds
    if (err == ESP_ERR_NOT_FOUND) {
      snprintf(error, sizeof(error), "Location not found. Try \"City, Country\".");
      goto invalid;
    }
    if (err != ESP_OK) {
      snprintf(error, sizeof(error), "Location lookup failed (%s), try again", esp_err_to_name(err));
      free(next);
      return send_error(req, "502 Bad Gateway", error);
    }
    strlcpy(next->location, result.location[0] != '\0' ? result.location : location_query, sizeof(next->location));
    strlcpy(next->timezone, result.timezone, sizeof(next->timezone));
    strlcpy(next->latitude, result.latitude, sizeof(next->latitude));
    strlcpy(next->longitude, result.longitude, sizeof(next->longitude));
    next->location_ready = true;
  }

  err = device_config_save(next);
  if (err != ESP_OK) {
    snprintf(error, sizeof(error), "Saving failed: %s", esp_err_to_name(err));
    free(next);
    return send_error(req, "500 Internal Server Error", error);
  }

  ESP_LOGI(TAG, "Settings saved from the web page, restarting");
  schedule_restart();
  json = settings_to_json(next);   // restartPending is now true
  free(next);
  return send_json(req, "200 OK", json);

invalid:
  cJSON_Delete(body);
  free(next);
  return send_error(req, "400 Bad Request", error);
}

#undef TAKE

// --- /api/alarm --------------------------------------------------------------

static cJSON *alarm_to_json(void) {
  alarm_config_t alarm;
  audio_alarm_get(&alarm);
  cJSON *json = cJSON_CreateObject();
  if (json == NULL) return NULL;
  cJSON_AddBoolToObject(json, "enabled", alarm.enabled);
  cJSON_AddNumberToObject(json, "hour", alarm.hour);
  cJSON_AddNumberToObject(json, "minute", alarm.minute);
  cJSON_AddStringToObject(json, "type", alarm.type == ALARM_TYPE_RADIO ? "radio" : "beep");
  cJSON_AddNumberToObject(json, "volume", audio_alarm_get_volume());
  cJSON_AddBoolToObject(json, "ringing", audio_alarm_is_ringing());
  return json;
}

static esp_err_t alarm_get_handler(httpd_req_t *req) {
  return send_json(req, "200 OK", alarm_to_json());
}

static esp_err_t alarm_post_handler(httpd_req_t *req) {
  cJSON *body = read_json_body(req);
  if (body == NULL) return ESP_OK;

  alarm_config_t alarm;
  audio_alarm_get(&alarm);
  bool alarm_changed = false;

  const cJSON *item = cJSON_GetObjectItemCaseSensitive(body, "enabled");
  if (cJSON_IsBool(item)) { alarm.enabled = cJSON_IsTrue(item); alarm_changed = true; }

  item = cJSON_GetObjectItemCaseSensitive(body, "hour");
  if (item != NULL) {
    if (!cJSON_IsNumber(item) || item->valueint < 0 || item->valueint > 23) {
      cJSON_Delete(body);
      return send_error(req, "400 Bad Request", "Hour must be 0-23");
    }
    alarm.hour = (uint8_t) item->valueint;
    alarm_changed = true;
  }
  item = cJSON_GetObjectItemCaseSensitive(body, "minute");
  if (item != NULL) {
    if (!cJSON_IsNumber(item) || item->valueint < 0 || item->valueint > 59) {
      cJSON_Delete(body);
      return send_error(req, "400 Bad Request", "Minute must be 0-59");
    }
    alarm.minute = (uint8_t) item->valueint;
    alarm_changed = true;
  }
  item = cJSON_GetObjectItemCaseSensitive(body, "type");
  if (cJSON_IsString(item)) {
    if (strcasecmp(item->valuestring, "radio") == 0) alarm.type = ALARM_TYPE_RADIO;
    else if (strcasecmp(item->valuestring, "beep") == 0) alarm.type = ALARM_TYPE_BEEP;
    else { cJSON_Delete(body); return send_error(req, "400 Bad Request", "Type must be beep or radio"); }
    alarm_changed = true;
  }
  item = cJSON_GetObjectItemCaseSensitive(body, "volume");
  int volume = -1;
  if (item != NULL) {
    if (!cJSON_IsNumber(item) || item->valueint < 0 || item->valueint > 100) {
      cJSON_Delete(body);
      return send_error(req, "400 Bad Request", "Volume must be 0-100");
    }
    volume = item->valueint;
  }
  cJSON_Delete(body);

  if (alarm_changed && audio_alarm_set(&alarm) != ESP_OK) {
    return send_error(req, "500 Internal Server Error", "Saving the alarm failed");
  }
  if (volume >= 0) {
    audio_alarm_set_volume((uint8_t) volume, true);   // the arc on the screen follows within a second
  }
  if (alarm_changed) {
    ESP_LOGI(TAG, "Alarm set from the web page: %02u:%02u %s %s", alarm.hour, alarm.minute,
             alarm.enabled ? "ON" : "OFF", alarm.type == ALARM_TYPE_RADIO ? "RADIO" : "BEEP");
  }
  return send_json(req, "200 OK", alarm_to_json());
}

// --- registration ------------------------------------------------------------

esp_err_t settings_web_register(httpd_handle_t server, const device_config_t *config) {
  if (server == NULL || config == NULL) return ESP_ERR_INVALID_STATE;
  s_config = config;

  const httpd_uri_t handlers[] = {
    { .uri = "/api/settings", .method = HTTP_GET,  .handler = settings_get_handler },
    { .uri = "/api/settings", .method = HTTP_POST, .handler = settings_post_handler },
    { .uri = "/api/alarm",    .method = HTTP_GET,  .handler = alarm_get_handler },
    { .uri = "/api/alarm",    .method = HTTP_POST, .handler = alarm_post_handler },
  };
  for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
    esp_err_t err = httpd_register_uri_handler(server, &handlers[i]);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Registering %s failed: %s", handlers[i].uri, esp_err_to_name(err));
      return err;
    }
  }
  ESP_LOGI(TAG, "Settings API ready");
  return ESP_OK;
}
