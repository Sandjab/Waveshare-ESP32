# Waveshare ESP32-S3-Knob-Touch-LCD-1.8

Repo de dev pour le [Waveshare ESP32-S3-Knob-Touch-LCD-1.8](https://www.waveshare.com/esp32-s3-knob-touch-lcd-1.8.htm) - un device rotatif avec ecran rond 360x360, encoder, touch, haptics et audio.

## Projets

| Projet | Description | Peripheriques utilises |
|--------|-------------|----------------------|
| [Test01](projects/Test01/) | Ecran clignotant (vert/rouge) | Display QSPI, backlight PWM |
| [Test02](projects/Test02/) | Roue de teintes HSV | Display + LVGL, encoder, DRV2605 haptics |
| [Test03](projects/Test03/) | Lecteur SD USB (mass storage) | SD card SDMMC, USB-OTG MSC |

## Structure

```
Knob/
├── build.ps1                    # Script build/flash/monitor
├── lib/
│   └── knob_hw/                 # Lib partagee (driver QSPI, pins, init LCD)
├── projects/
│   ├── Test01/                  # Ecran clignotant
│   ├── Test02/                  # Roue de teintes
│   └── Test03/                  # Lecteur SD USB
└── docs/
    ├── demo-code/               # Code demo Waveshare (ESP-IDF + Arduino)
    └── schematics/              # Schemas (5 pages PNG)
```

Les projets partagent `lib/knob_hw/` qui contient le driver QSPI (`esp_lcd_sh8601`), les definitions de pins (`knob_pins.h`) et la sequence d'init ST77916 (`knob_lcd_init.h`).

## Build et flash

### Prerequis

- [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/installation.html) installe dans `~/.platformio/`
- Le cable USB-C branche cote ESP32-S3 (retourner le cable si le CH340 est detecte)

### Usage

```powershell
.\build.ps1                                # Build tous les projets
.\build.ps1 Test01                         # Build Test01 seul
.\build.ps1 Test02 -Upload                 # Build + flash (autodetect port)
.\build.ps1 Test01 -Upload -Port COM13     # Build + flash sur port specifique
.\build.ps1 Test01 -Flash                  # Flash sans rebuild
.\build.ps1 Test01 -Upload -Monitor        # Build + flash + monitor serie
.\build.ps1 -Clean                         # Clean + rebuild tout
```

### Parametres

| Parametre    | Description                                                  |
|-------------|--------------------------------------------------------------|
| `Projects`  | Un ou plusieurs noms de projet (ex: `Test01 Test02`). Defaut: tous |
| `-Upload`   | Build puis flash sur la board                                |
| `-Flash`    | Flash le dernier firmware sans rebuild (esptool direct)      |
| `-Port`     | Port COM explicite (ex: `COM13`). Defaut: autodetection      |
| `-Monitor`  | Ouvre le monitor serie apres flash (Ctrl-C pour quitter)     |
| `-Clean`    | Clean avant de rebuild                                       |

### Autodetection du port

Sans `-Port`, le script detecte automatiquement le port :

1. **ESP32-S3** - VID:PID `303A:1001` (USB CDC natif)
2. **CH340** - VID:PID `1A86:7523` (fallback, avec warning de retourner le cable USB-C)

### Notes

- Le `PermissionError` apres un flash est **normal** - l'USB re-enumerate pendant le hard reset.
- `-Flash` utilise esptool directement (instantane, necessite au moins un build prealable).
- `-Upload` passe par PlatformIO, qui ne rebuild que si les sources ont change.
