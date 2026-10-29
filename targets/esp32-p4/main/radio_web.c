#include "radio_web.h"

#include <stdlib.h>
#include <string.h>

#include "audio_alarm.h"
#include "cJSON.h"
#include "esp_log.h"
#include "radio_stations.h"

static const char *TAG = "radio_web";

// 16 stations * (name + url + JSON overhead) stays well under this.
#define RADIO_MAX_HTTP_BODY 8192

static const char *radio_state_name(radio_state_t state) {
  switch (state) {
    case RADIO_STATE_CONNECTING:   return "connecting";
    case RADIO_STATE_PLAYING:      return "playing";
    case RADIO_STATE_FAILED:       return "failed";
    case RADIO_STATE_AUDIO_FAILED: return "audio_failed";
    case RADIO_STATE_OFF:
    default:                       return "off";
  }
}

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

// Parses the request body as JSON. On failure it has already answered the request.
static cJSON *read_json_body(httpd_req_t *req) {
  if (req->content_len <= 0) { send_error(req, "400 Bad Request", "Empty request"); return NULL; }
  if (req->content_len > RADIO_MAX_HTTP_BODY) { send_error(req, "400 Bad Request", "Request too large"); return NULL; }
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

// List + what the clock is doing right now.
static cJSON *stations_response(void) {
  cJSON *json = radio_stations_to_json();
  if (json == NULL) return NULL;
  cJSON_AddNumberToObject(json, "selected", (double) audio_radio_get_station());
  cJSON_AddStringToObject(json, "state", radio_state_name(audio_radio_get_state()));
  return json;
}

static void current_url(char *out, size_t out_size) {
  radio_station_t station;
  out[0] = '\0';
  if (audio_radio_station(audio_radio_get_station(), &station)) strlcpy(out, station.url, out_size);
}

static esp_err_t stations_get_handler(httpd_req_t *req) {
  return send_json(req, "200 OK", stations_response());
}

static esp_err_t stations_post_handler(httpd_req_t *req) {
  cJSON *body = read_json_body(req);
  if (body == NULL) return ESP_OK;

  char previous[RADIO_STATION_URL_MAX];
  current_url(previous, sizeof(previous));

  const char *problem = NULL;
  esp_err_t err = radio_stations_replace_from_json(cJSON_GetObjectItemCaseSensitive(body, "stations"), &problem);
  cJSON_Delete(body);
  if (err != ESP_OK) {
    return send_error(req, err == ESP_ERR_NO_MEM || err == ESP_FAIL ? "500 Internal Server Error" : "400 Bad Request",
                      problem != NULL ? problem : "Saving failed");
  }

  audio_radio_stations_changed(previous);
  ESP_LOGI(TAG, "Station list updated (%u stations)", (unsigned) radio_stations_count());
  return send_json(req, "200 OK", stations_response());
}

static esp_err_t stations_reset_handler(httpd_req_t *req) {
  char previous[RADIO_STATION_URL_MAX];
  current_url(previous, sizeof(previous));
  if (radio_stations_reset_to_defaults() != ESP_OK) {
    return send_error(req, "500 Internal Server Error", "Saving to flash failed");
  }
  audio_radio_stations_changed(previous);
  return send_json(req, "200 OK", stations_response());
}

static esp_err_t play_handler(httpd_req_t *req) {
  cJSON *body = read_json_body(req);
  if (body == NULL) return ESP_OK;

  if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(body, "stop"))) {
    cJSON_Delete(body);
    audio_radio_stop();
    return send_json(req, "200 OK", stations_response());
  }

  const cJSON *index = cJSON_GetObjectItemCaseSensitive(body, "index");
  if (!cJSON_IsNumber(index) || index->valuedouble < 0) {
    cJSON_Delete(body);
    return send_error(req, "400 Bad Request", "Missing station index");
  }
  size_t station = (size_t) index->valuedouble;
  cJSON_Delete(body);

  if (audio_radio_set_station(station) != ESP_OK) return send_error(req, "404 Not Found", "No such station");
  if (audio_radio_get_state() == RADIO_STATE_OFF ||
      audio_radio_get_state() == RADIO_STATE_FAILED ||
      audio_radio_get_state() == RADIO_STATE_AUDIO_FAILED) {
    esp_err_t err = audio_radio_start();   // when already playing, set_station switched the stream
    if (err == ESP_ERR_INVALID_STATE) {
      return send_error(req, "409 Conflict", audio_alarm_is_ringing()
                        ? "The alarm is using the audio right now"
                        : "Radio is still stopping, try again");
    }
    if (err != ESP_OK) return send_error(req, "404 Not Found", "No stations");
  }
  return send_json(req, "200 OK", stations_response());
}

esp_err_t radio_web_register(httpd_handle_t server) {
  if (server == NULL) return ESP_ERR_INVALID_STATE;

  // More specific URIs first: the server uses wildcard matching.
  const httpd_uri_t handlers[] = {
    { .uri = "/api/radio/stations/reset", .method = HTTP_POST, .handler = stations_reset_handler },
    { .uri = "/api/radio/stations",       .method = HTTP_GET,  .handler = stations_get_handler },
    { .uri = "/api/radio/stations",       .method = HTTP_POST, .handler = stations_post_handler },
    { .uri = "/api/radio/play",           .method = HTTP_POST, .handler = play_handler },
  };
  for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
    esp_err_t err = httpd_register_uri_handler(server, &handlers[i]);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Registering %s failed: %s", handlers[i].uri, esp_err_to_name(err));
      return err;
    }
  }
  ESP_LOGI(TAG, "Radio API ready");
  return ESP_OK;
}
