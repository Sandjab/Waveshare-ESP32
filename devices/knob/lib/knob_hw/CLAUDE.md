# knob_hw — Device-Specific Library

## Files

| File | Role |
|------|------|
| `knob_pins.h` | GPIO definitions (LCD, encoder, I2C, backlight, etc.) |
| `knob_lcd_init.h` | ST77916 init command sequence (vendor-specific register table) |
| `knob_display.h` | One-liner display init: SPI bus + panel IO + ST77916 + backlight (header-only) |
| `knob_lvgl.h` | One-liner LVGL init: display + double-buffer + driver + tick timer (header-only) |
| `bidi_switch_knob.h` | Encoder driver API (timer-polled, debounced — not quadrature) |
| `bidi_switch_knob.c` | Encoder driver implementation |

## Init pattern

- **Raw display** (no LVGL): `#include "knob_display.h"` → `knob_display_init()`
- **LVGL projects**: `#include "knob_lvgl.h"` → `knob_lvgl_init()` (calls `knob_display_init()` internally)

`knob_display.h` and `knob_lvgl.h` are header-only (no .cpp). They use `static inline` functions to avoid multiple-definition issues.
