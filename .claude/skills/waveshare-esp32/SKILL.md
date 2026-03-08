---
name: waveshare-esp32
description: Common ESP32-S3 platform reference for all Waveshare devices in this monorepo. Use when working with PlatformIO setup, toolchain, common libraries, or adding a new device.
---

# Waveshare ESP32-S3 — Common Platform Reference

## Supported Devices

| Device | Directory | Display | Resolution | Key ICs |
|---|---|---|---|---|
| **ESP32-S3-Knob-Touch-LCD-1.8** | `devices/knob/` | ST77916 IPS LCD | 360x360 | CST816, DRV2605, PCM5100A |
| **ESP32-S3-Touch-AMOLED-1.8** | `devices/amoled/` | SH8601 AMOLED | 368x448 | FT3168, ES8311, AXP2101, QMI8658, PCF85063 |

Both devices use the same QSPI display framework (`shared/lib/qspi_panel/esp_lcd_sh8601`).

## Monorepo Structure

```
Waveshare/
├── shared/lib/qspi_panel/     # Common QSPI display driver
├── devices/knob/              # Knob device (skill: waveshare-knob)
├── devices/amoled/            # AMOLED device (skill: waveshare-amoled)
└── build.ps1                  # Build script (-Device param)
```

## Per-Device Skills

Each device has its own skill with hardware-specific details:
- **waveshare-knob** — `devices/knob/.claude/skills/waveshare-knob/SKILL.md`
- **waveshare-amoled** — `devices/amoled/.claude/skills/waveshare-amoled/SKILL.md`

## Platform Reference

For PlatformIO setup, toolchain, ESP-IDF, common libraries (LVGL, display drivers, sensors), and debugging:

→ **[esp32-platform.md](esp32-platform.md)**

## Adding a New Device

Template:
```
devices/<name>/
├── .claude/skills/waveshare-<name>/
│   ├── SKILL.md              # Quick ref + GPIO + progressive disclosure
│   ├── device-hardware.md    # Architecture, pinout, ICs
│   └── resources.md          # Wiki, GitHub, datasheets
├── CLAUDE.md                 # Ref skill + conventions
├── lib/<name>_hw/
│   └── <name>_pins.h
├── projects/                 # PlatformIO projects
└── docs/demo-code/           # Waveshare demo code (Arduino + ESP-IDF)
```

Add the device to root `CLAUDE.md` and `build.ps1` will auto-discover it.
