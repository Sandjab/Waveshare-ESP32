# Waveshare ESP32-S3 Monorepo

Monorepo for Waveshare ESP32-S3 devices — shared platform, per-device projects and skills.

## Devices

| Device | Directory | Skill |
|---|---|---|
| ESP32-S3-Knob-Touch-LCD-1.8 | `devices/knob/` | `waveshare-knob` |
| ESP32-S3-Touch-AMOLED-1.8 | `devices/amoled/` | `waveshare-amoled` |
| ESP32-S3-Touch-AMOLED-1.75C | `devices/amoled_175c/` | `waveshare-amoled-175c` |
| Guition JC3636K718 | `devices/guition_knob/` | `guition-k718` |
| ESP32-S3-ePaper-1.54G | `devices/epaper_154g/` | `waveshare-epaper-154g` |

## Structure

```
Waveshare/
├── .claude/skills/waveshare-esp32/   # Common ESP32-S3 platform skill
├── shared/lib/qspi_panel/           # QSPI display driver (used by both devices)
├── devices/
│   ├── knob/                         # Waveshare Knob (projects, lib, docs, firmware, skills)
│   ├── amoled/                       # Waveshare AMOLED 1.8 (projects, lib, docs, firmware, skills)
│   ├── amoled_175c/                  # Waveshare AMOLED 1.75C — round (lib, docs, firmware, skills)
│   ├── guition_knob/                 # Guition JC3636K718 (projects, lib, docs, firmware, skills)
│   └── epaper_154g/                  # Waveshare ePaper 1.54G — 4 couleurs (projects, lib, docs, firmware, skills)
├── build.ps1                         # Build script (Windows / PowerShell)
├── build.sh                          # Build script (macOS / Linux / bash)
├── tools/device_mac.py               # MAC ↔ device_dir inventory helper
├── devices.local.yaml                # Per-user device inventory (gitignored)
└── inbox/                            # Staging area (gitignored)
```

## Conventions

- **Shared code** goes in `shared/lib/`. Each device's `platformio.ini` references it via `lib_extra_dirs`.
- **Device-specific code** stays in `devices/<name>/lib/<name>_hw/`.
- **Demo code is authoritative for GPIOs** — `docs/demo-code/` takes precedence over schematics.
- **Factory firmware** goes in `devices/<name>/firmware/` (sibling of `docs/`, not inside it), with a `README.md` documenting source, version, format, offset, and restore command. See `devices/guition_knob/firmware/` for the canonical layout.
- **Display orientation** : canonical orientation places the **USB connector on the side of the device that keeps the cable out of the way during use** :
  - Round 1.8" devices with USB on a short edge (Knob, Guition) : **USB at the top** when the screen is read upright. Knob `MADCTL = 0xC0`, Guition `MADCTL = 0x00` (panels mounted 180° apart in their housings).
  - Rectangular AMOLED 1.8" with USB on a long edge : **USB on the right**. The current `amoled_lcd_init.h` leaves `MADCTL` at the panel default (effectively `0x00`); the exact value to satisfy "USB on the right" is to be validated with the first oriented demo (text or LVGL widget) — see the outstanding-followups memory.
- **`inbox/`** is a temporary drop zone for unprocessed material (gitignored).
- **Skills** : common platform in `.claude/skills/waveshare-esp32/`, device-specific in `devices/<name>/.claude/skills/`.
- **Device identity check** : several devices in this monorepo share `VID:303A PID:1001`, so `build.sh` / `build.ps1` cannot tell them apart by port alone — flashing the wrong board is easy. The per-user inventory `devices.local.yaml` (gitignored) maps each physical MAC to a `device_dir`. `tools/device_mac.py check <device_dir>` runs before every `--upload` / `--flash` and aborts on mismatch (override with `--no-device-check` / `-NoDeviceCheck`). Pass `auto` instead of a device name (`./build.sh auto Hue_Encoder --upload`) to let the script identify the connected device from its MAC. To enrol a new device : plug it in, run `python3 tools/device_mac.py scan`, copy the printed MAC into the right entry of `devices.local.yaml`.

## Build

Prérequis PlatformIO : voir [docs/install/windows.md](docs/install/windows.md) ou [docs/install/macos.md](docs/install/macos.md).

Windows (PowerShell) :

```powershell
.\build.ps1 knob Basic_Blink         # Build specific project
.\build.ps1 knob Basic_Blink -Upload # Build + flash
.\build.ps1 amoled                   # Build all AMOLED projects
.\build.ps1 -ListDevices             # Show available devices
```

macOS / Linux (bash) :

```bash
./build.sh knob Basic_Blink          # Build specific project
./build.sh knob Basic_Blink --upload # Build + flash
./build.sh amoled                    # Build all AMOLED projects
./build.sh --list-devices            # Show available devices
```

## LVGL Documentation

The demo code uses LVGL v8.3–8.4. For up-to-date docs, use Context7:
- `/websites/lvgl_io_8_4` — best version match
- `/websites/lvgl_io_master` — latest docs (highest coverage)
