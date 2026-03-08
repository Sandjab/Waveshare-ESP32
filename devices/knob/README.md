# Waveshare ESP32-S3-Knob-Touch-LCD-1.8

<img src="docs/ESP32-S3-Knob-Touch-LCD-1.8.jpg" width="200" alt="ESP32-S3-Knob-Touch-LCD-1.8">

Repo de dev pour le [Waveshare ESP32-S3-Knob-Touch-LCD-1.8](https://www.waveshare.com/esp32-s3-knob-touch-lcd-1.8.htm) - un device rotatif avec ecran rond 360x360, encoder, touch, haptics et audio.

## Projets

| Projet | Description | Peripheriques utilises |
|--------|-------------|----------------------|
| [Test01](projects/Test01/) | Ecran clignotant (vert/rouge) | Display QSPI, backlight PWM |
| [Test02](projects/Test02/) | Roue de teintes HSV | Display + LVGL, encoder, DRV2605 haptics |
| [Test03](projects/Test03/) | Lecteur SD USB (mass storage) | SD card SDMMC, USB-OTG MSC |

## Structure

```
devices/knob/
├── lib/
│   └── knob_hw/                 # Lib device-specific (pins, init LCD)
├── projects/
│   ├── Test01/                  # Ecran clignotant
│   ├── Test02/                  # Roue de teintes
│   └── Test03/                  # Lecteur SD USB
└── docs/
    ├── demo-code/               # Code demo Waveshare (ESP-IDF + Arduino)
    └── schematics/              # Schemas (5 pages PNG)
```

Les projets utilisent `lib/knob_hw/` (pins, init LCD) et `shared/lib/qspi_panel/` (driver QSPI `esp_lcd_sh8601`).

## Build et flash

Depuis la racine du monorepo :

```powershell
.\build.ps1 knob                           # Build tous les projets Knob
.\build.ps1 knob Test01                    # Build Test01 seul
.\build.ps1 knob Test02 -Upload            # Build + flash (autodetect port)
.\build.ps1 knob Test01 -Upload -Port COM13 -Monitor
.\build.ps1 knob Test01 -Flash             # Flash sans rebuild
.\build.ps1 knob -Clean                    # Clean + rebuild tout
```
