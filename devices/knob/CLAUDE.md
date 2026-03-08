# Waveshare ESP32-S3-Knob-Touch-LCD-1.8

Documentation and development repo for the Waveshare ESP32-S3-Knob-Touch-LCD-1.8 device.

## Skill

For hardware details, pinout, GPIO table, framework setup, and flash commands, invoke the `waveshare-knob` skill. It is the primary reference for this project.

## Repo Structure

```
devices/knob/
├── .claude/skills/waveshare-knob/   # Device skill (3 files — SKILL.md is entry point)
├── lib/knob_hw/                     # Device-specific lib (see below)
│   ├── knob_pins.h                  # GPIO definitions
│   ├── knob_lcd_init.h              # ST77916 init command sequence
│   ├── knob_display.h               # One-liner display init (header-only)
│   ├── knob_lvgl.h                  # One-liner LVGL init (header-only, calls display_init)
│   ├── bidi_switch_knob.h/.c        # Encoder driver (timer-polled, debounced)
├── projects/
│   ├── Basic_Blink/                 # Ecran clignotant (raw display)
│   ├── Basic_Encoder/               # Compteur rotatif ±9999 (LVGL + encoder)
│   ├── Hue_Encoder/                 # Roue de teintes HSV (LVGL + encoder + haptics + touch)
│   └── Basic_SD_OTG/                # Lecteur SD USB
├── docs/
│   ├── demo-code/                   # Waveshare demo code (ESP-IDF + Arduino, 8 examples each)
│   └── schematics/                  # 5 schematic pages (PNG)
```

Shared code lives in `../../shared/lib/` (QSPI driver `esp_lcd_sh8601`).

### Display / LVGL helpers

`knob_display.h` and `knob_lvgl.h` are header-only helpers that eliminate boilerplate:

- **`knob_display_init()`** — raw display init (SPI bus, panel IO, ST77916, backlight). Use for non-LVGL projects (e.g. Basic_Blink).
- **`knob_lvgl_init()`** — full LVGL setup (calls `knob_display_init()` internally, then configures double-buffered DMA, display driver, rounder, tick timer). Use for any LVGL project.

## Encoder

The rotary encoder is **not quadrature** — it has two independent contacts (Phase A on GPIO 8, Phase B on GPIO 7). Use the `bidi_switch_knob` driver in `lib/knob_hw/` (timer-polled at 3ms with debounce). Do **not** use interrupt-based quadrature decoding — it won't work with this hardware. Reference demo: `docs/demo-code/Arduino/examples/04_Encoder_Test/`.

## Conventions

- **Demo code is authoritative for GPIOs.** `docs/demo-code/` takes precedence over schematics when there is a conflict. Schematics have been wrong for SD card GPIOs in the past.
- **Schematics** in `docs/schematics/` are reference-only PNGs (5 pages covering LCD/power, ESP32-S3, ESP32 secondary, peripherals, DAC).

## LVGL Documentation

The demo code uses LVGL v8.3.11. For up-to-date docs, use Context7:
- `/websites/lvgl_io_8_4` — best version match (v8.4, closest to v8.3.11)
- `/websites/lvgl_io_master` — latest docs (6400 snippets, highest coverage)
- Official docs: https://docs.lvgl.io

## Gotchas

- **Not a Guition JC3636W518.** Similar form factor, completely different pinouts. Do not mix configurations.
- **Display driver mismatch.** The QSPI framework uses `esp_lcd_sh8601`, but the actual panel IC is an **ST77916** with a custom init sequence. Don't assume SH8601 registers apply.
- **GPIO 0 dual role.** It serves as both BOOT strap pin and audio mux control (HIGH = S3 controls PCM5100A DAC).
