# amoled_175c_hw — Device-Specific Library

## Files

| File | Role |
|------|------|
| `amoled_175c_pins.h` | GPIO definitions (LCD QSPI, shared I2C bus, audio I2S, PA enable) |

## To be added when needed

- `amoled_175c_display.h` — one-liner display init for the CO5300 QSPI AMOLED. We don't have a CO5300 driver in `shared/lib/qspi_panel/` yet (the existing one is `esp_lcd_sh8601` which the AMOLED 1.8 reuses); the 1.75C will likely need a new driver or a fork. Defer until the first display project.
- `amoled_175c_lcd_init.h` — CO5300 init command sequence, to be extracted from the Waveshare demo at `docs/demo-code/Arduino-v3.3.5/libraries/GFX_Library_for_Arduino/src/display/Arduino_CO5300.*` (or equivalent).

## Specifics vs AMOLED 1.8

- **Different display IC** : CO5300 (1.75C) vs SH8601 (1.8). Driver layer not yet shared.
- **Different touch IC** : CST9217 (1.75C) vs FT3168 (1.8), but both on the same I2C bus pattern.
- **Audio chain** : ES8311 codec + **ES7210** echo-cancellation AEC for the dual-mic array. The 1.8 has just an ES8311.
- **No XCA9554 I/O expander** — the LCD reset uses a direct GPIO (PIN_LCD_RST = 2), reportedly shared with touch RST. No expander to drive on init.
- **No SD card slot** on this device, unlike the 1.8.
