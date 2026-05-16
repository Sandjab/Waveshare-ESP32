# guition_knob_hw — Device-Specific Library

## Files

| File | Role |
|------|------|
| `guition_pins.h` | GPIO definitions (LCD, encoder, I2C, audio, mic, SD, **PIN_RGB_DATA=0** / 13 WS2812) |
| `guition_lcd_init.h` | ST77916 init command sequence (identique au Knob Waveshare — vérifié) |
| `guition_display.h` | One-liner display init: SPI bus + panel IO + ST77916 + backlight (header-only) |
| `guition_lvgl.h` | One-liner LVGL init: display + double-buffer + driver + tick timer (header-only) |
| `bidi_switch_knob.h/.c` | Encoder driver — copie carbone du driver Knob (timer-polled 3ms, debounce, non-quadrature) |

## Init pattern

- **Raw display** (no LVGL): `#include "guition_display.h"` → `guition_display_init()`
- **LVGL projects**: `#include "guition_lvgl.h"` → `guition_lvgl_init()`

## RGB ring (à venir)

L'anneau de 13 LEDs WS2812 (GRB, data sur GPIO 0) n'est **pas encore** câblé dans cette lib. Le driver attend qu'un premier projet `Basic_RGB_Ring` (ou équivalent) le requière. Référence d'implémentation : `../../docs/demo-code/Demo_idf/demo/main/led_strip/` (ESP-IDF `led_strip` + RMT).

Attention GPIO 0 = BOOT strap. WS2812 idle = low, donc envoyer la première trame uniquement après boot complet pour éviter l'entrée en download mode.
