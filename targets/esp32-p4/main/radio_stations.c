#include "radio_stations.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

static const char *TAG = "radio_stations";
static const char *NVS_NAMESPACE = "radio";
static const char *NVS_KEY_LIST = "list";

// Saved as a compact JSON blob: {"v":1,"s":[["name","url"],...]}. With 16
// stations that is at most ~4 KB; the nvs partition is 24 KB, so the limits in
// radio_stations.h are deliberately modest.
#define STORAGE_VERSION 1

// Built-in default list (the stations that used to be hard-coded). Order
// matters: index 0 is the default station.
static const struct { const char *name; const char *url; } DEFAULT_STATIONS[] = {
  { "Radio 538", "http://25483.live.streamtheworld.com/RADIO538.mp3" },
  { "3FM",       "http://icecast.omroep.nl/3fm-sb-mp3" },
  { "Radio 10",  "http://playerservices.streamtheworld.com/api/livestream-redirect/RADIO10.mp3" },
  { "Skyradio",  "http://playerservices.streamtheworld.com/api/livestream-redirect/SKYRADIO.mp3" },
  { "Radio 2",   "http://icecast.omroep.nl/radio2-sb-mp3" },
  { "Qmusic",    "http://stream.qmusic.nl/qmusic/mp3" },
  { "Slam",      "http://stream.slam.nl/slam_mp3" },
};
#define DEFAULT_COUNT (sizeof(DEFAULT_STATIONS) / sizeof(DEFAULT_STATIONS[0]))

static radio_station_t s_stations[RADIO_STATIONS_MAX];
static size_t s_count = 0;
static volatile uint32_t s_generation = 0;
static SemaphoreHandle_t s_mutex = NULL;

static void lock(void)   { xSemaphoreTake(s_mutex, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_mutex); }

static void load_defaults_locked(void) {
  memset(s_stations, 0, sizeof(s_stations));
  s_count = 0;
  for (size_t i = 0; i < DEFAULT_COUNT && i < RADIO_STATIONS_MAX; i++) {
    strlcpy(s_stations[i].name, DEFAULT_STATIONS[i].name, sizeof(s_stations[i].name));
    strlcpy(s_stations[i].url, DEFAULT_STATIONS[i].url, sizeof(s_stations[i].url));
    s_count++;
  }
}

// --- validation ------------------------------------------------------------

static bool has_prefix_ci(const char *s, const char *prefix) {
  for (; *prefix; s++, prefix++) {
    if (tolower((unsigned char) *s) != *prefix) return false;
  }
  return true;
}

static bool ends_with_ci(const char *s, const char *suffix) {
  size_t ls = strlen(s), lx = strlen(suffix);
  if (lx > ls) return false;
  return strncasecmp(s + ls - lx, suffix, lx) == 0;
}

// Copies `src` into `dst` without leading/trailing whitespace.
static size_t copy_trimmed(char *dst, size_t dst_size, const char *src) {
  while (*src && isspace((unsigned char) *src)) src++;
  size_t len = strlen(src);
  while (len > 0 && isspace((unsigned char) src[len - 1])) len--;
  if (len >= dst_size) return len;   // caller treats >= dst_size as too long
  memcpy(dst, src, len);
  dst[len] = '\0';
  return len;
}

static const char *validate_and_copy(const cJSON *item, radio_station_t *out) {
  const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
  const cJSON *url = cJSON_GetObjectItemCaseSensitive(item, "url");
  if (!cJSON_IsString(name) || !cJSON_IsString(url)) return "Each station needs a name and a URL";

  memset(out, 0, sizeof(*out));
  size_t name_len = copy_trimmed(out->name, sizeof(out->name), name->valuestring);
  if (name_len == 0) return "Station name is empty";
  if (name_len >= sizeof(out->name)) return "Station name is too long";

  size_t url_len = copy_trimmed(out->url, sizeof(out->url), url->valuestring);
  if (url_len == 0) return "Stream URL is empty";
  if (url_len >= sizeof(out->url)) return "Stream URL is too long";
  if (!has_prefix_ci(out->url, "http://") && !has_prefix_ci(out->url, "https://")) {
    return "Stream URL must start with http:// or https://";
  }
  for (const char *p = out->url; *p; p++) {
    if (isspace((unsigned char) *p)) return "Stream URL contains spaces";
  }
  // Playlists and HLS cannot be played directly; the page resolves them via
  // Radio Browser (url_resolved), so a playlist URL here is a user mistake.
  if (ends_with_ci(out->url, ".m3u8")) return "HLS streams (.m3u8) are not supported";
  if (ends_with_ci(out->url, ".m3u") || ends_with_ci(out->url, ".pls")) {
    return "Playlist URLs (.m3u/.pls) are not supported, use the stream URL inside it";
  }
  return NULL;
}

// --- NVS ---------------------------------------------------------------------

static esp_err_t save_locked(void) {
  cJSON *root = cJSON_CreateObject();
  cJSON *list = cJSON_AddArrayToObject(root, "s");
  if (root == NULL || list == NULL) { cJSON_Delete(root); return ESP_ERR_NO_MEM; }
  cJSON_AddNumberToObject(root, "v", STORAGE_VERSION);
  for (size_t i = 0; i < s_count; i++) {
    cJSON *pair = cJSON_CreateArray();
    cJSON_AddItemToArray(pair, cJSON_CreateString(s_stations[i].name));
    cJSON_AddItemToArray(pair, cJSON_CreateString(s_stations[i].url));
    cJSON_AddItemToArray(list, pair);
  }
  char *blob = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (blob == NULL) return ESP_ERR_NO_MEM;

  nvs_handle_t nvs;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
  if (err == ESP_OK) {
    err = nvs_set_blob(nvs, NVS_KEY_LIST, blob, strlen(blob));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
  }
  free(blob);
  if (err != ESP_OK) ESP_LOGE(TAG, "Saving station list failed: %s", esp_err_to_name(err));
  return err;
}

static bool load_locked(void) {
  nvs_handle_t nvs;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) return false;
  size_t size = 0;
  esp_err_t err = nvs_get_blob(nvs, NVS_KEY_LIST, NULL, &size);
  char *blob = (err == ESP_OK && size > 0) ? calloc(size + 1, 1) : NULL;
  if (blob != NULL) err = nvs_get_blob(nvs, NVS_KEY_LIST, blob, &size);
  nvs_close(nvs);
  if (blob == NULL || err != ESP_OK) { free(blob); return false; }

  cJSON *root = cJSON_Parse(blob);
  free(blob);
  const cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "s");
  if (!cJSON_IsArray(list)) { cJSON_Delete(root); return false; }

  radio_station_t loaded[RADIO_STATIONS_MAX];
  size_t count = 0;
  const cJSON *pair;
  cJSON_ArrayForEach(pair, list) {
    if (count >= RADIO_STATIONS_MAX) break;
    const cJSON *name = cJSON_GetArrayItem(pair, 0);
    const cJSON *url = cJSON_GetArrayItem(pair, 1);
    if (!cJSON_IsString(name) || !cJSON_IsString(url)) continue;
    memset(&loaded[count], 0, sizeof(loaded[count]));
    strlcpy(loaded[count].name, name->valuestring, sizeof(loaded[count].name));
    strlcpy(loaded[count].url, url->valuestring, sizeof(loaded[count].url));
    count++;
  }
  cJSON_Delete(root);

  // An empty saved list is valid (the user removed everything).
  memcpy(s_stations, loaded, count * sizeof(loaded[0]));
  s_count = count;
  return true;
}

// --- public API --------------------------------------------------------------

esp_err_t radio_stations_init(void) {
  if (s_mutex == NULL) {
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) return ESP_ERR_NO_MEM;
  }
  lock();
  if (load_locked()) {
    ESP_LOGI(TAG, "Loaded %u saved stations", (unsigned) s_count);
  } else {
    load_defaults_locked();
    ESP_LOGI(TAG, "No saved stations, using %u defaults", (unsigned) s_count);
  }
  s_generation++;
  unlock();
  return ESP_OK;
}

size_t radio_stations_count(void) {
  return s_count;
}

bool radio_stations_get(size_t index, radio_station_t *out) {
  if (out == NULL || s_mutex == NULL) return false;
  lock();
  bool ok = index < s_count;
  if (ok) *out = s_stations[index];
  unlock();
  return ok;
}

int radio_stations_find_url(const char *url) {
  if (url == NULL || url[0] == '\0' || s_mutex == NULL) return -1;
  int found = -1;
  lock();
  for (size_t i = 0; i < s_count; i++) {
    if (strcmp(s_stations[i].url, url) == 0) { found = (int) i; break; }
  }
  unlock();
  return found;
}

uint32_t radio_stations_generation(void) {
  return s_generation;
}

esp_err_t radio_stations_replace_from_json(const cJSON *array, const char **out_error) {
  const char *dummy;
  if (out_error == NULL) out_error = &dummy;
  *out_error = NULL;
  if (s_mutex == NULL) { *out_error = "Not ready"; return ESP_ERR_INVALID_STATE; }
  if (!cJSON_IsArray(array)) { *out_error = "Expected a list of stations"; return ESP_ERR_INVALID_ARG; }
  if (cJSON_GetArraySize(array) > RADIO_STATIONS_MAX) {
    *out_error = "Too many stations";
    return ESP_ERR_INVALID_SIZE;
  }

  // Validate everything into a scratch copy first: all or nothing.
  radio_station_t *next = calloc(RADIO_STATIONS_MAX, sizeof(radio_station_t));
  if (next == NULL) { *out_error = "Out of memory"; return ESP_ERR_NO_MEM; }
  size_t count = 0;
  const cJSON *item;
  cJSON_ArrayForEach(item, array) {
    const char *problem = validate_and_copy(item, &next[count]);
    if (problem != NULL) {
      free(next);
      *out_error = problem;
      return ESP_ERR_INVALID_ARG;
    }
    count++;
  }

  lock();
  memcpy(s_stations, next, sizeof(s_stations));
  s_count = count;
  esp_err_t err = save_locked();
  s_generation++;
  unlock();
  free(next);
  if (err != ESP_OK) *out_error = "Saving to flash failed";
  return err;
}

esp_err_t radio_stations_reset_to_defaults(void) {
  if (s_mutex == NULL) return ESP_ERR_INVALID_STATE;
  lock();
  load_defaults_locked();
  esp_err_t err = save_locked();
  s_generation++;
  unlock();
  return err;
}

cJSON *radio_stations_to_json(void) {
  cJSON *root = cJSON_CreateObject();
  cJSON *list = root ? cJSON_AddArrayToObject(root, "stations") : NULL;
  if (list == NULL) { cJSON_Delete(root); return NULL; }
  lock();
  for (size_t i = 0; i < s_count; i++) {
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "name", s_stations[i].name);
    cJSON_AddStringToObject(entry, "url", s_stations[i].url);
    cJSON_AddItemToArray(list, entry);
  }
  unlock();
  cJSON_AddNumberToObject(root, "max", RADIO_STATIONS_MAX);
  cJSON_AddNumberToObject(root, "nameMax", RADIO_STATION_NAME_MAX - 1);
  cJSON_AddNumberToObject(root, "urlMax", RADIO_STATION_URL_MAX - 1);
  return root;
}
