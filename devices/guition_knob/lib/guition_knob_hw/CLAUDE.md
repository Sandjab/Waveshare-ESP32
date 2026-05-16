# guition_knob_hw — Device-Specific Library

## Files

| File | Role |
|------|------|
| `guition_pins.h` | GPIO definitions (LCD, encoder, I2C, audio, mic, SD, **PIN_RGB_DATA=0** / 13 WS2812) |
| `guition_lcd_init.h` | ST77916 init command sequence (identique au Knob Waveshare — vérifié) |
| `guition_display.h` | One-liner display init: SPI bus + panel IO + ST77916 + backlight (header-only) |
| `guition_lvgl.h` | One-liner LVGL init: display + double-buffer + driver + tick timer (header-only) |
| `bidi_switch_knob.h/.c` | Encoder driver — copie carbone du driver Knob (timer-polled 3ms, debounce, non-quadrature) |
| `rgb_ring.h` | Helpers anneau 13×WS2812 GRB (header-only, wrappe `Adafruit_NeoPixel`) |

## Init pattern

- **Raw display** (no LVGL): `#include "guition_display.h"` → `guition_display_init()`
- **LVGL projects**: `#include "guition_lvgl.h"` → `guition_lvgl_init()`

## RGB ring (`rgb_ring.h`)

Header-only car il dépend de `Adafruit_NeoPixel` — pour ne pas obliger les projets sans anneau (genre `Basic_Blink`) à pull cette lib_dep. Pour utiliser :

1. `#include "rgb_ring.h"` dans le sketch
2. Définir l'instance globale (l'extern est déclaré dans le header) :
   ```cpp
   Adafruit_NeoPixel rgb_ring(RGB_RING_LED_COUNT, PIN_RGB_DATA, NEO_GRB + NEO_KHZ800);
   ```
3. Ajouter `lib_deps = adafruit/Adafruit NeoPixel` dans `platformio.ini`
4. Appeler `rgb_ring_init(brightness)` dans `setup()` ; puis `rgb_ring_set*` + `rgb_ring_show()` dans `loop()`

Attention GPIO 0 = BOOT strap. WS2812 idle = low, donc envoyer la première trame uniquement après boot complet pour éviter l'entrée en download mode (c'est ce que fait `rgb_ring_init`, appelé depuis `setup()` donc post-boot).

Référence d'implémentation native ESP-IDF (RMT + `led_strip` component) : `../../docs/demo-code/Demo_idf/demo/main/led_strip/`. On a choisi `Adafruit_NeoPixel` côté Arduino pour cohérence avec l'écosystème ESP32 Arduino.
