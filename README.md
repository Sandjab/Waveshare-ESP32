# Waveshare ESP32-S3 Monorepo

Development repo for Waveshare ESP32-S3 devices.

## Devices

| | Device | Description | Display |
|---|--------|-------------|---------|
| <img src="devices/knob/docs/images/ESP32-S3-Knob-Touch-LCD-1.8.jpg" width="80"> | [Knob](devices/knob/) | [ESP32-S3-Knob-Touch-LCD-1.8](https://www.waveshare.com/esp32-s3-knob-touch-lcd-1.8.htm) — Rotary knob with round IPS LCD 360x360 | ST77916 QSPI |
| <img src="devices/amoled/docs/images/ESP32-S3-Touch-AMOLED-1.8.png" width="80"> | [AMOLED](devices/amoled/) | [ESP32-S3-Touch-AMOLED-1.8](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm) — AMOLED touch watch 368x448 | SH8601 QSPI |

## Structure

```
Waveshare/
├── shared/lib/qspi_panel/     # Common QSPI display driver
├── devices/
│   ├── knob/                  # Knob: projects, lib, docs
│   └── amoled/                # AMOLED: projects, lib, docs
└── build.ps1                  # Build script
```

## Build

Prérequis : [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/installation.html)
- **Windows** → [docs/install/windows.md](docs/install/windows.md)
- **macOS** → [docs/install/macos.md](docs/install/macos.md)

### Windows (PowerShell)

```powershell
.\build.ps1 knob Basic_Blink                   # Build
.\build.ps1 knob Basic_Blink -Upload           # Build + flash (autodetect port)
.\build.ps1 knob Basic_Blink -Upload -Monitor  # Build + flash + monitor série
.\build.ps1 knob -Clean                        # Clean + rebuild tous les projets du device
.\build.ps1 -ListDevices                       # Lister les devices disponibles
```

### macOS / Linux (bash)

```bash
./build.sh knob Basic_Blink                    # Build
./build.sh knob Basic_Blink --upload           # Build + flash (autodetect port)
./build.sh knob Basic_Blink --upload --monitor # Build + flash + monitor série
./build.sh knob --clean                        # Clean + rebuild tous les projets du device
./build.sh --list-devices                      # Lister les devices disponibles
```
