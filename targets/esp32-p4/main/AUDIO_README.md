# Audio Beep (Experimental)

Simple sine-wave beep using the onboard ES8311 codec and speaker on the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C.

## Pins used
- MCLK = GPIO13
- BCLK = GPIO12
- LRCK = GPIO10
- DOUT = GPIO9
- PA enable = GPIO53

## Usage
```c
audio_beep_init();
audio_beep_play(1000, 500);  // 1 kHz for 500 ms
audio_beep_deinit();
```

## Notes
- This is a minimal implementation. Full ES8311 codec initialization (I2C) may be needed for reliable volume control.
- Next steps: proper ES8311 driver, volume control, and later internet radio.
- The speaker must be connected to the PH2.0 connector.

## Radio stations
The station list is no longer hard-coded. It lives in `radio_stations.c`
(saved in NVS, namespace `radio`, max 16 stations) and is edited from the
clock's own web page (`http://<clock-ip>/`, panel "Radio Stations"):

- search stations via radio-browser.info (by name and/or country; pick only a
  country to get its most popular MP3 stations), or paste a stream URL;
- reorder, remove, play on the clock, and "Save list";
- "Restore defaults" brings back the built-in Dutch list.

Only MP3 streams play (minimp3). Playlist (.m3u/.pls) and HLS (.m3u8) URLs are
rejected; search results use Radio Browser's `url_resolved`, which is the
stream behind a playlist. API: see `radio_web.h`.
