#pragma once

// Finding the clock on the home network, and resetting it.
//
// Discovery: the clock gets a host name derived from its room name
// ("weerklok-slaapkamer", or "weerklok" while the room name is the default).
// That name is used for DHCP (so the router lists the clock by name) and for
// mDNS, so the web page opens at http://weerklok-slaapkamer.local/.
//
// Factory reset: erases all settings (Wi-Fi, location, stations, alarm,
// messages) and restarts into the setup screens. Triggered from the settings
// screen, or by holding the board's BOOT button for 10 seconds while the clock
// runs.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device_config.h"
#include "esp_err.h"

#define DISCOVERY_HOSTNAME_MAX 32

// Host name for this config (lower case, a-z 0-9 and '-').
void discovery_hostname(const device_config_t *config, char *out, size_t out_size);

// Call before Wi-Fi connects (sets the DHCP host name on the station netif).
void discovery_set_dhcp_hostname(void *esp_netif_sta, const device_config_t *config);

// Starts mDNS (host name + http service). Safe to call more than once.
esp_err_t discovery_start_mdns(const device_config_t *config);

// "http://weerklok-slaapkamer.local/" (always available, even before Wi-Fi).
void discovery_local_url(char *out, size_t out_size);

// "http://192.168.1.80/" when connected, else false.
bool discovery_ip_url(char *out, size_t out_size);

// Erases all settings and restarts. Returns only on failure.
void device_factory_reset(void);

// Runs the reset from its own task after `delay_ms`, so the UI can show a
// message first.
void device_factory_reset_async(uint32_t delay_ms);

// Call once a second. Starts a factory reset after the BOOT button has been
// held for 10 seconds (only once it has been seen released after boot).
// Returns the number of seconds it has been held (0 when released), so the
// caller can show a countdown.
uint32_t device_reset_button_poll(void);
