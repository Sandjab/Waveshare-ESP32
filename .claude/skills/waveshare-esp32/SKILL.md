---
name: waveshare-esp32
description: Common ESP32-S3 platform reference for all Waveshare devices in this monorepo. Use when working with PlatformIO setup, toolchain, common libraries, or adding a new device.
---

# Waveshare ESP32-S3 — Common Platform Reference

## Supported Devices

| Device | Directory | Display | Resolution | Key ICs | Skill |
|---|---|---|---|---|---|
| **ESP32-S3-Knob-Touch-LCD-1.8** (Waveshare) | `devices/knob/` | ST77916 IPS LCD | 360×360 | CST816, DRV2605, PCM5100A | `waveshare-knob` |
| **ESP32-S3-Touch-AMOLED-1.8** (Waveshare) | `devices/amoled/` | SH8601 AMOLED | 368×448 | FT3168, ES8311, AXP2101, QMI8658, PCF85063 | `waveshare-amoled` |
| **ESP32-S3-Touch-AMOLED-1.75C** (Waveshare) | `devices/amoled_175c/` | CO5300 AMOLED (round) | 466×466 | CST9217, ES8311+ES7210, AXP2101, QMI8658 | `waveshare-amoled-175c` |
| **JC3636K718** (Guition) | `devices/guition_knob/` | ST77916 IPS LCD | 360×360 | PCM5100A, NS4150B, 13× WS2812 RGB ring | `guition-k718` |
| **ESP32-S3-ePaper-1.54G** (Waveshare) | `devices/epaper_154g/` | e-paper 4 couleurs (SPI) | 200×200 | ES8311, SHTC3, PCF85063, ETA6098 | `waveshare-epaper-154g` |

All three QSPI display devices share `shared/lib/qspi_panel/esp_lcd_sh8601` (the panel framework is QSPI-generic; the ST77916 init lives in each device's `lib/<name>_hw/`).

## Monorepo Structure

```
Waveshare/
├── shared/lib/qspi_panel/     # Common QSPI display driver
├── devices/knob/              # Waveshare Knob (skill: waveshare-knob)
├── devices/amoled/            # Waveshare AMOLED (skill: waveshare-amoled)
├── devices/guition_knob/      # Guition JC3636K718
├── build.ps1                  # Build (Windows / PowerShell)
└── build.sh                   # Build (macOS / Linux / bash)
```

## Per-Device Skills

Each device has its own skill with hardware-specific details:
- **waveshare-knob** — `devices/knob/.claude/skills/waveshare-knob/SKILL.md`
- **waveshare-amoled** — `devices/amoled/.claude/skills/waveshare-amoled/SKILL.md`
- **waveshare-amoled-175c** — `devices/amoled_175c/.claude/skills/waveshare-amoled-175c/SKILL.md`
- **guition-k718** — `devices/guition_knob/.claude/skills/guition-k718/SKILL.md`
- **waveshare-epaper-154g** — `devices/epaper_154g/.claude/skills/waveshare-epaper-154g/SKILL.md`

## Platform Reference

For PlatformIO setup, toolchain, ESP-IDF, common libraries (LVGL, display drivers, sensors), and debugging:

→ **[esp32-platform.md](esp32-platform.md)**

## Adding a New Device

Template:
```
devices/<name>/
├── .claude/skills/<skill-name>/   # Optional (Waveshare devices have one)
│   ├── SKILL.md                   # Quick ref + GPIO + progressive disclosure
│   ├── device-hardware.md         # Architecture, pinout, ICs
│   └── resources.md               # Wiki, GitHub, datasheets
├── CLAUDE.md                      # Always — conventions, gotchas, skill ref
├── README.md                      # Build commands (both flavors)
├── lib/<name>_hw/
│   ├── <name>_pins.h              # GPIO definitions
│   └── CLAUDE.md                  # Lib contents
├── projects/                      # PlatformIO projects
├── docs/                          # demo-code, datasheets, schematics
└── firmware/                      # Factory firmware .bin + restore README
```

Add the device row to the root `CLAUDE.md` table; both `build.ps1` and `build.sh` will auto-discover it via `devices/<name>/projects/`.
