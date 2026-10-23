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
