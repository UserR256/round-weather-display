#pragma once

#include <stdbool.h>
#include "esp_err.h"

// Simple GPS NMEA parser interface
// Connect a GPS module (e.g. NEO-6M / NEO-M8N) to a free UART

typedef struct {
  double latitude;
  double longitude;
  bool valid;
  char raw_rmc[128];
} gps_position_t;

// Initialize GPS UART (call once at startup)
esp_err_t gps_nmea_init(int uart_num, int tx_pin, int rx_pin, int baud_rate);

// Process incoming NMEA data (call regularly, e.g. from a task)
void gps_nmea_process(void);

// Get latest valid position (returns true if a fresh fix is available)
bool gps_nmea_get_position(gps_position_t *out);

// Check if we currently have a valid GPS fix
bool gps_nmea_has_fix(void);
