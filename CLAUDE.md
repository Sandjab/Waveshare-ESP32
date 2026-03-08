# Waveshare ESP32-S3 Monorepo

Monorepo for Waveshare ESP32-S3 devices — shared platform, per-device projects and skills.

## Devices

| Device | Directory | Skill |
|---|---|---|
| ESP32-S3-Knob-Touch-LCD-1.8 | `devices/knob/` | `waveshare-knob` |
| ESP32-S3-Touch-AMOLED-1.8 | `devices/amoled/` | `waveshare-amoled` |

## Structure

```
Waveshare/
├── .claude/skills/waveshare-esp32/   # Common ESP32-S3 platform skill
├── shared/lib/qspi_panel/           # QSPI display driver (used by both devices)
├── devices/
│   ├── knob/                         # Knob device (projects, lib, docs, skills)
│   └── amoled/                       # AMOLED device (projects, lib, docs, skills)
├── build.ps1                         # Build script: .\build.ps1 <device> [project] [-Upload]
└── inbox/                            # Staging area (gitignored)
```

## Conventions

- **Shared code** goes in `shared/lib/`. Each device's `platformio.ini` references it via `lib_extra_dirs`.
- **Device-specific code** stays in `devices/<name>/lib/<name>_hw/`.
- **Demo code is authoritative for GPIOs** — `docs/demo-code/` takes precedence over schematics.
- **`inbox/`** is a temporary drop zone for unprocessed material (gitignored).
- **Skills** : common platform in `.claude/skills/waveshare-esp32/`, device-specific in `devices/<name>/.claude/skills/`.

## Build

```powershell
.\build.ps1 knob Basic_Blink         # Build specific project
.\build.ps1 knob Basic_Blink -Upload # Build + flash
.\build.ps1 amoled                   # Build all AMOLED projects
.\build.ps1 -ListDevices             # Show available devices
```

## LVGL Documentation

The demo code uses LVGL v8.3–8.4. For up-to-date docs, use Context7:
- `/websites/lvgl_io_8_4` — best version match
- `/websites/lvgl_io_master` — latest docs (highest coverage)
