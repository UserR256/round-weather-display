# ESP32-P4 Target

This target is the native firmware path for the Waveshare `ESP32-P4-WIFI6-Touch-LCD-3.4C` board family.

It is intentionally separate from the Raspberry Pi runtime, but it follows the same shared product contract in:

- `../../shared/spec/product-spec.md`

## Current Scope

This target includes:

- A real ESP-IDF project layout, with WiFi via ESP-Hosted over SDIO to a companion ESP32-C6
- Analog, digital, forecast, and message screens with Pi-matched layout/behavior
- Live weather fetch (Open-Meteo) with icon/condition mapping
- On-device WiFi provisioning (network scan + on-screen keyboard) and location setup/geocoding, driven by a touch UI
- House messaging: the full JSON API (`/api/messages`, `/api/message-targets`, `/api/message-runtime`, ack/deactivate/sync) plus UDP hub discovery/election for shared mode across multiple clocks
- A web page at `http://<device-ip>/` (or `/messages`) with three tabs: **Messages** (the composer, same as the Pi's `/messages` page), **Settings** (the settings screen plus alarm and volume, via `/api/settings` and `/api/alarm`), and **Radio** (station list, via `/api/radio/...`)
- Alarm (beep or internet radio) and an internet radio screen with a volume arc; MP3 streams only (minimp3)
- A user-editable radio station list (max 16, stored in NVS), searchable via radio-browser.info on the web page
- English and Dutch UI (`main/i18n*.{c,h,inc}`)
- NVS-backed device config with boot-count persistence
- A `scripts/setup.sh` one-command install/update flow (see below)

Known gaps:

- No wireless OTA firmware updates yet -- updating means re-running `scripts/setup.sh` with the board plugged in over USB
- The prebuilt firmware in `docs/firmware/` (browser installer) is still a chip revision v1.x build and won't install on v3.x boards
- The web page is English only, also when the clock is set to Dutch

## Chip revision (v1.x vs v3.x)

Current Waveshare boards ship with an ESP32-P4 **v3.x** chip (the boot log says `chip revision: v3.2`). `sdkconfig.defaults` targets v3.x, which needs ESP-IDF v5.5.3 or newer. One firmware image can't support both v1.x and v3.x. For an old v1.x board, set these in your local `sdkconfig` (or via `idf.py menuconfig` → Component config → Hardware Settings → Chip revision):

```
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
```

A v1.x build on a v3.x chip fails to flash with `requires chip revision in range [v1.0 - v1.99]`.

`sdkconfig.defaults` only applies when there is no `sdkconfig` yet. After changing it, delete `build/` and `sdkconfig` and run `idf.py set-target esp32p4` (the target is pinned in the defaults, so a fresh build doesn't fall back to plain `esp32`).

## Wi-Fi co-processor (ESP32-C6) firmware

Wi-Fi runs on the board's ESP32-C6 via ESP-Hosted. Boards ship with old factory firmware on the C6, which shows at boot as:

```
W transport: Version mismatch: Host [2.13.0] > Co-proc [0.0.0] ==> Upgrade co-proc to avoid RPC timeouts
```

`tools/c6-firmware-update/` updates the C6 over SDIO from the P4 (no extra wiring) and keeps the clock's settings. See its README. Host and C6 should run the same esp_hosted major.minor, so `main/idf_component.yml` pins esp_hosted to `~2.13.1`; bump both together.

## Setup / Update

From the repo root, with the board connected over USB:

```bash
targets/esp32-p4/scripts/setup.sh
```

This is both the first-time setup path and the update path. It's safe to re-run any time: it pulls the latest source (skipping the pull if you have uncommitted local changes, so nothing gets clobbered), installs ESP-IDF `v5.5.5` if it isn't present yet (one-time, several GB), builds, detects the board's serial port (prompting you to pick if more than one is attached), and flashes. It'll also ask if you want to open the serial monitor afterward.

Choosing hardware target `2` in the root `setup.sh` delegates straight to this script.

## Manual build/flash (for iterative dev work)

If you're actively working on the firmware and don't want the full setup script's git-pull/IDF-install checks every time:

```bash
source targets/esp32-p4/scripts/activate-idf.sh
targets/esp32-p4/scripts/set-target.sh
targets/esp32-p4/scripts/build.sh
targets/esp32-p4/scripts/flash.sh /dev/tty.usbmodemXXXX
targets/esp32-p4/scripts/monitor.sh /dev/tty.usbmodemXXXX
```

## Verified Baseline

- Waveshare recommends `ESP-IDF` for the `ESP32-P4-WIFI6-Touch-LCD-XC` series.
- Waveshare documents the `3.4C` as the default `800x800` round display variant in the `XC` BSP.
- Waveshare recommends `ESP-IDF v5.5.0` or newer for the `ESP32-P4-WIFI6-Touch-LCD` line; this project is pinned to `v5.5.5`.

Sources:

- <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-XC>
- <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-X/Development-Environment-Setup-IDF>
- <https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_xc/versions/3.0.0/readme>

## Project Layout

- `CMakeLists.txt`
  Top-level ESP-IDF project file
- `sdkconfig.defaults`
  Default project settings for the 3.4C display variant
- `main/idf_component.yml`
  Managed component dependencies
- `main/main.c`
  Firmware entrypoint
- `main/app_ui.*`
  Native LVGL UI: analog/digital/forecast/message screens and on-device setup flow
- `main/connectivity.*`
  WiFi station connect, scanning, and time sync
- `main/device_config.*`
  NVS-backed config (WiFi, location, room/device id, message sharing mode, display prefs)
- `main/location_lookup.*`
  On-device Open-Meteo geocoding for the setup UI
- `main/message_service.*`
  House messaging JSON API, UDP hub discovery/election, and the served composer page
- `main/web/index.html`
  The clock's web page (tabs: Messages, Settings, Radio), served by `message_service.c` and embedded into the firmware image
- `tools/c6-firmware-update/`
  One-off tool that updates the ESP-Hosted firmware on the board's ESP32-C6 Wi-Fi chip over SDIO (fixes the `Co-proc [0.0.0]` version warning); see its README
- `main/settings_web.*`
  Settings and alarm API for the Settings tab (`/api/settings`, `/api/alarm`)
- `main/radio_web.*`, `main/radio_stations.*`
  Radio API for the Radio tab (`/api/radio/...`) and the user-editable station list in NVS
- `main/weather_client.*`
  Open-Meteo forecast fetch
- `main/bsp_shims.h`
  Small forward declarations for BSP backlight helpers documented upstream
- `scripts/`
  Shell helpers: `setup.sh` (install/update/build/flash), plus `activate-idf.sh`, `set-target.sh`, `build.sh`, `flash.sh`, `monitor.sh` for manual use

## Memory notes

Internal, DMA-capable RAM is the tight resource, not PSRAM. While the radio plays, the I2S DMA buffers take ~16 KB of it, and the hardware AES engine then couldn't get its DMA bounce buffer, so every HTTPS request failed. `sdkconfig.defaults` therefore uses software AES (`CONFIG_MBEDTLS_HARDWARE_AES=n`) and puts mbedTLS buffers in PSRAM (`CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`). The log prints the low-water mark once a minute:

```
heap: periodic up 600s radio=playing | internal free … (min …) | DMA free … (min …, largest …)
```

Keep DMA `min` comfortably above ~15-20 KB. The weather client logs why a fetch fails (HTTP status, truncated body, missing field, out of memory), and `Radio start requested by:` shows whether the screen, the web page, or the alarm started the radio.

## Notes

- The project is set up to use the Waveshare `XC` BSP with the `3.4C` round display as the intended default.
- Screen layout constants are derived from the Pi reference (`targets/pi/public/style.css` and `targets/pi/public/renderer/clock.js`) to keep both targets visually matched.
