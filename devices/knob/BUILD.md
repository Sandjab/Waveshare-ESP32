# Build

## Prerequis

- [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/installation.html) installe dans `~/.platformio/`
- Le cable USB-C branche cote ESP32-S3 (retourner le cable si le CH340 est detecte)

## Usage

```powershell
.\build.ps1                                # Build tous les projets
.\build.ps1 Test01                         # Build Test01 seul
.\build.ps1 Test02 -Upload                 # Build + flash (autodetect port)
.\build.ps1 Test01 -Upload -Port COM13     # Build + flash sur port specifique
.\build.ps1 Test01 -Flash                  # Flash sans rebuild
.\build.ps1 Test01 -Upload -Monitor        # Build + flash + monitor serie
.\build.ps1 -Clean                         # Clean + rebuild tout
```

## Parametres

| Parametre    | Description                                                  |
|-------------|--------------------------------------------------------------|
| `Projects`  | Un ou plusieurs noms de projet (ex: `Test01 Test02`). Defaut: tous |
| `-Upload`   | Build puis flash sur la board                                |
| `-Flash`    | Flash le dernier firmware sans rebuild (esptool direct)      |
| `-Port`     | Port COM explicite (ex: `COM13`). Defaut: autodetection      |
| `-Monitor`  | Ouvre le monitor serie apres flash (Ctrl-C pour quitter)     |
| `-Clean`    | Clean avant de rebuild                                       |

## Autodetection du port

Quand `-Upload`, `-Flash` ou `-Monitor` est utilise sans `-Port`, le script detecte automatiquement le port en cherchant dans cet ordre :

1. **ESP32-S3** - VID:PID `303A:1001` (USB CDC natif)
2. **CH340** - VID:PID `1A86:7523` (fallback, avec warning de retourner le cable USB-C)

Si aucun device n'est trouve, le script demande de brancher la board ou de specifier `-Port`.

## Structure

```
Knob/
├── build.ps1                    # Ce script
├── lib/
│   └── knob_hw/                 # Librairie partagee (driver + pins + init LCD)
└── projects/
    ├── Test01/                  # Minimal screen blink
    │   ├── platformio.ini       # lib_extra_dirs = ../../lib
    │   └── src/main.cpp
    └── Test02/                  # Hue wheel (LVGL + encoder + haptics)
        ├── platformio.ini       # lib_extra_dirs = ../../lib
        └── src/
            ├── main.cpp
            └── lv_conf.h
```

Les projets partagent `lib/knob_hw/` qui contient le driver QSPI (`esp_lcd_sh8601`), les definitions de pins (`knob_pins.h`) et la sequence d'init ST77916 (`knob_lcd_init.h`).

## Notes

- Le `PermissionError` affiche apres un flash est **normal** - l'USB re-enumerate pendant le hard reset, le flash a bien fonctionne.
- `-Flash` utilise esptool directement (pas PlatformIO), ce qui est instantane. Il faut avoir fait au moins un build avant.
- `-Upload` passe par PlatformIO, qui ne rebuild que si les sources ont change.
