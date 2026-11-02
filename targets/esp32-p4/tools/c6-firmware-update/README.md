# C6 Wi-Fi co-processor firmware update

The Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C reaches Wi-Fi through an on-board
ESP32-C6 running ESP-Hosted. Boards ship with old factory firmware on the C6,
which the clock reports at boot as:

```
W transport: Version mismatch: Host [2.13.0] > Co-proc [0.0.0] ==> Upgrade co-proc to avoid RPC timeouts
```

This one-off tool runs on the P4 and updates the C6 over the existing SDIO link
(no wires, no soldering). It is Espressif's `host_performs_slave_ota` example
from esp_hosted 2.13.1 (Apache-2.0), set up for this board:

- `partitions.csv` keeps the clock's `nvs` exactly where it is, so Wi-Fi,
  location, radio stations and alarm survive;
- LittleFS method: the C6 image is built into the tool;
- extra check: the image must be built for the ESP32-C6 in DIO flash mode.

The C6 must run the **same esp_hosted version as the clock firmware** (now
2.13.1, see `../../dependencies.lock` and `../../main/idf_component.yml`).

## 1. Build the C6 firmware (esp_hosted 2.13.1)

Outside the repo, in an ESP-IDF 5.5 shell:

```
idf.py create-project-from-example "espressif/esp_hosted==2.13.1:slave"
cd slave
idf.py set-target esp32c6
idf.py build
```

Do not change the flash mode to QIO. Copy the result into this tool:

```
copy build\network_adapter.bin <repo>\targets\esp32-p4\tools\c6-firmware-update\components\ota_littlefs\slave_fw_bin\
```

## 2. Run the tool on the clock

```
cd targets\esp32-p4\tools\c6-firmware-update
idf.py set-target esp32p4
idf.py build flash monitor
```

Expected log, roughly:

```
Slave firmware version: 0.0.0          (or "Could not get slave firmware version")
Firmware verified - Size: ... Version: 2.13.1
Starting OTA from LittleFS ...
... OTA completed ...
Restarting host to resync with slave...
... Versions compatible - OTA not required
```

After the restart the tool checks again and stops with "OTA not required".

## 3. Put the clock firmware back

```
cd ..\..
idf.py build flash monitor
```

The `Version mismatch` warning at boot should be gone.

## If something goes wrong

- The C6 writes the new image to its spare slot, so a failed transfer leaves
  the old firmware running. Run the tool again.
- Transfer errors half way: set `CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=10000`
  in `sdkconfig.defaults`, delete `sdkconfig` and `build`, and retry.
- Last resort is flashing the C6 over its own UART (test pads on the board).
