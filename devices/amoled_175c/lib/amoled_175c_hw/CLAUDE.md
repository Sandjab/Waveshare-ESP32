# amoled_175c_hw — Device-Specific Library

## Files

| File | Role |
|------|------|
| `amoled_175c_pins.h` | GPIO definitions (LCD QSPI, shared I2C bus, audio I2S, PA enable) |
| `amoled_175c_lcd_init.h` | CO5300 init command sequence (derived from `Arduino_CO5300.h` in the Waveshare repo) |
| `amoled_175c_display.h` | One-liner display init (header-only): SPI bus + panel IO + CO5300 init + 6-col `set_gap` to align the visible 466×466 area inside the CO5300 RAM |

## Init pattern

- **Raw display** (no LVGL): `#include "amoled_175c_display.h"` → `amoled_175c_display_init()`

The shared `esp_lcd_sh8601` driver from `shared/lib/qspi_panel/` is reused as a generic QSPI host (same trick used for the ST77916 boards) — we just inject our CO5300 `init_cmds` via `sh8601_vendor_config_t`.

## Specifics vs AMOLED 1.8

- **Different display IC** : CO5300 (1.75C) vs SH8601 (1.8). Both go through the generic QSPI host in `shared/lib/qspi_panel/esp_lcd_sh8601.c`, but with different `init_cmds` tables.
- **Different touch IC** : CST9217 (1.75C) vs FT3168 (1.8), both on the shared I2C bus.
- **Audio chain** : ES8311 codec + **ES7210** AEC for the dual-mic array. The 1.8 has just ES8311.
- **No XCA9554 I/O expander** — the LCD reset uses a direct GPIO (`PIN_LCD_RST = 2`), shared with `PIN_TP_RST` per the vendor pin_config.
- **No SD card** on this device.
- **6-column offset** — the visible panel starts at column 6 within the CO5300 RAM; applied via `esp_lcd_panel_set_gap(panel, 6, 0)` in `amoled_175c_display_init()`.

## Specifics vs AMOLED 1.8

- **Different display IC** : CO5300 (1.75C) vs SH8601 (1.8). Driver layer not yet shared.
- **Different touch IC** : CST9217 (1.75C) vs FT3168 (1.8), but both on the same I2C bus pattern.
- **Audio chain** : ES8311 codec + **ES7210** echo-cancellation AEC for the dual-mic array. The 1.8 has just an ES8311.
- **No XCA9554 I/O expander** — the LCD reset uses a direct GPIO (PIN_LCD_RST = 2), reportedly shared with touch RST. No expander to drive on init.
- **No SD card slot** on this device, unlike the 1.8.
