# GPS Support (Experimental)

This adds a basic NMEA GPS parser so the Round Weather Display can use coordinates from an external GPS module instead of a manually entered location.

## Hardware

Connect a GPS module (NEO-6M, NEO-M8N, etc.) to a free UART:

- GPS TX → ESP32-P4 RX (example: GPIO 17)
- GPS RX → ESP32-P4 TX (example: GPIO 18)
- VCC → 3.3V or 5V (depending on module)
- GND → GND

## Integration status

- `gps_nmea.c` / `gps_nmea.h` — basic NMEA RMC parser
- Not yet fully integrated into the weather client or UI
- You still need to call `gps_nmea_init()` and `gps_nmea_process()` from `main.c` or a task

## Next steps (if you want full automatic location)

1. Call `gps_nmea_init(UART_NUM_1, 18, 17, 9600);` in `main.c`
2. Periodically call `gps_nmea_process()`
3. When `gps_nmea_has_fix()` returns true, copy the coordinates into `device_config` and trigger a weather refresh

This is a starting point. Full automatic switching between manual location and GPS requires additional changes in `device_config` and the weather update path.
