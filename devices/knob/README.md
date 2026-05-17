# Waveshare ESP32-S3-Knob-Touch-LCD-1.8

<img src="docs/images/ESP32-S3-Knob-Touch-LCD-1.8.jpg" width="200" alt="ESP32-S3-Knob-Touch-LCD-1.8">

Repo de dev pour le [Waveshare ESP32-S3-Knob-Touch-LCD-1.8](https://www.waveshare.com/esp32-s3-knob-touch-lcd-1.8.htm) - un device rotatif avec ecran rond 360x360, encoder, touch, haptics et audio.

## Projets

| Projet | Description | Peripheriques utilises |
|--------|-------------|----------------------|
| [Basic_Blink](projects/Basic_Blink/) | Ecran clignotant (vert/rouge) | Display QSPI, backlight PWM |
| [Basic_Encoder](projects/Basic_Encoder/) | Compteur rotatif ±9999 | Display + LVGL, encoder |
| [Basic_LVGL_Meter](projects/Basic_LVGL_Meter/) | Cadran type compteur de vitesse 0-100 piloté par encoder (portage du Guition) | Display + LVGL `lv_meter`, encoder |
| [Hue_Encoder](projects/Hue_Encoder/) | Roue de teintes HSV + toggle haptics | Display + LVGL, encoder, DRV2605 haptics, CST816 touch |
| [Basic_SD_OTG](projects/Basic_SD_OTG/) | Lecteur SD USB (mass storage) | SD card SDMMC, USB-OTG MSC |

## Structure

```
devices/knob/
├── lib/
│   └── knob_hw/                 # Lib device-specific (pins, display, LVGL, encoder)
├── projects/
│   ├── Basic_Blink/             # Ecran clignotant
│   ├── Basic_Encoder/           # Compteur rotatif
│   ├── Basic_LVGL_Meter/        # Cadran lv_meter piloté par encoder
│   ├── Hue_Encoder/             # Roue de teintes HSV
│   └── Basic_SD_OTG/            # Lecteur SD USB
└── docs/
    ├── demo-code/               # Code demo Waveshare (ESP-IDF + Arduino)
    └── schematics/              # Schemas (5 pages PNG)
```

Les projets utilisent `lib/knob_hw/` (pins, init display via `knob_display.h`, init LVGL via `knob_lvgl.h`, encoder) et `shared/lib/qspi_panel/` (driver QSPI `esp_lcd_sh8601`).

## Build et flash

> ⚠️ **Switch USB CH445P** : la prise Type-C sélectionne **l'ESP32-S3 ou l'ESP32 secondaire** selon l'orientation du câble. Si `esptool` répond `This chip is ESP32 not ESP32-S3`, retourner le câble (1 clic). Voir le skill `waveshare-knob` pour le détail.

Depuis la racine du monorepo :

```powershell
# Windows
.\build.ps1 knob                                       # Build tous les projets Knob
.\build.ps1 knob Basic_Blink                           # Build Basic_Blink seul
.\build.ps1 knob Basic_Encoder -Upload                 # Build + flash (auto-detect port)
.\build.ps1 knob Basic_Blink -Upload -Port COM13 -Monitor
.\build.ps1 knob Basic_Blink -Flash                    # Flash sans rebuild
.\build.ps1 knob -Clean                                # Clean + rebuild tout
```

```bash
# macOS / Linux
./build.sh knob                                        # Build tous les projets Knob
./build.sh knob Basic_Blink                            # Build Basic_Blink seul
./build.sh knob Basic_Encoder --upload                 # Build + flash (auto-detect port)
./build.sh knob Basic_Blink --upload --port /dev/cu.usbmodem* --monitor
./build.sh knob Basic_Blink --flash                    # Flash sans rebuild
./build.sh knob --clean                                # Clean + rebuild tout
```
