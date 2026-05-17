# amoled_hw — Device-Specific Library

## Files

| File | Role |
|------|------|
| `amoled_pins.h` | GPIO definitions (LCD QSPI, touch I2C, audio I2S, SD SDMMC, PA enable) |
| `amoled_display.h` | One-liner display init for the SH8601 QSPI AMOLED (header-only) |

## Init pattern

- **Raw display**: `#include "amoled_display.h"` → `amoled_display_init()`

`amoled_display.h` is header-only (`static inline`), matching the pattern used by `knob_display.h` and `guition_display.h`.

## Specifics vs Knob / Guition

- **No direct LCD RST GPIO** — the reset signal is routed via the XCA9554 I/O expander (I2C 0x20, pins P0..P2). `amoled_display_init()` currently passes `reset_gpio_num = -1` and relies on the panel's power-on reset, which is enough for first-light projects. Future LVGL-class projects may want to drive XCA9554 explicitly for deterministic resets.
- **No PWM backlight** — AMOLED is self-emitting. Brightness is set via SH8601 command `0x51` (Write Display Brightness, 0..255), inside `amoled_display_init()`.
- **No custom init command table** — unlike the ST77916 boards, we use the SH8601 driver's `vendor_specific_init_default` (built into `shared/lib/qspi_panel/esp_lcd_sh8601.c`).
