#include "gps_nmea.h"
#include <string.h>
#include <stdlib.h>
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "gps_nmea";

#define GPS_UART_BUF_SIZE 1024
static char nmea_line[128];
static int nmea_pos = 0;
static gps_position_t last_position = {0};
static bool has_fix = false;

static double nmea_to_decimal(const char *nmea_coord, char hemisphere) {
  if (!nmea_coord || strlen(nmea_coord) < 4) return 0.0;
  double deg = 0, min = 0;
  int deg_len = (strchr(nmea_coord, '.') - nmea_coord) - 2;
  if (deg_len < 1) deg_len = 2;
  char deg_str[8] = {0};
  strncpy(deg_str, nmea_coord, deg_len);
  deg = atof(deg_str);
  min = atof(nmea_coord + deg_len);
  double decimal = deg + (min / 60.0);
  if (hemisphere == 'S' || hemisphere == 'W') decimal = -decimal;
  return decimal;
}

static void parse_rmc(const char *line) {
  // $GPRMC,time,status,lat,N/S,lon,E/W,...
  char buf[128];
  strncpy(buf, line, sizeof(buf) - 1);
  char *token = strtok(buf, ",");
  int field = 0;
  char lat_str[16] = {0}, lon_str[16] = {0};
  char lat_hem = 0, lon_hem = 0;
  char status = 'V';

  while (token) {
    switch (field) {
      case 2: status = token[0]; break;
      case 3: strncpy(lat_str, token, sizeof(lat_str) - 1); break;
      case 4: lat_hem = token[0]; break;
      case 5: strncpy(lon_str, token, sizeof(lon_str) - 1); break;
      case 6: lon_hem = token[0]; break;
    }
    token = strtok(NULL, ",");
    field++;
  }

  if (status == 'A' && lat_str[0] && lon_str[0]) {
    last_position.latitude = nmea_to_decimal(lat_str, lat_hem);
    last_position.longitude = nmea_to_decimal(lon_str, lon_hem);
    last_position.valid = true;
    has_fix = true;
    strncpy(last_position.raw_rmc, line, sizeof(last_position.raw_rmc) - 1);
    ESP_LOGI(TAG, "GPS fix: %.6f, %.6f", last_position.latitude, last_position.longitude);
  }
}

esp_err_t gps_nmea_init(int uart_num, int tx_pin, int rx_pin, int baud_rate) {
  const uart_config_t uart_config = {
    .baud_rate = baud_rate,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };
  ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(uart_num, GPS_UART_BUF_SIZE * 2, 0, 0, NULL, 0));
  ESP_LOGI(TAG, "GPS UART initialized (UART%d, TX=%d, RX=%d, %d baud)", uart_num, tx_pin, rx_pin, baud_rate);
  return ESP_OK;
}

void gps_nmea_process(void) {
  uint8_t data[64];
  int len = uart_read_bytes(UART_NUM_1, data, sizeof(data), 20 / portTICK_PERIOD_MS);
  for (int i = 0; i < len; i++) {
    char c = data[i];
    if (c == '\n' || c == '\r') {
      if (nmea_pos > 6 && nmea_line[0] == '$') {
        nmea_line[nmea_pos] = 0;
        if (strncmp(nmea_line, "$GPRMC", 6) == 0 || strncmp(nmea_line, "$GNRMC", 6) == 0) {
          parse_rmc(nmea_line);
        }
      }
      nmea_pos = 0;
    } else if (nmea_pos < sizeof(nmea_line) - 1) {
      nmea_line[nmea_pos++] = c;
    }
  }
}

bool gps_nmea_get_position(gps_position_t *out) {
  if (!has_fix || !out) return false;
  *out = last_position;
  return true;
}

bool gps_nmea_has_fix(void) {
  return has_fix;
}
