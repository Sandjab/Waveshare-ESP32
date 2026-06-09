# Waveshare ESP32-S3-ePaper-1.54G

Carte AIoT ESP32-S3 avec écran e-paper 1.54" 200×200 **4 couleurs** (noir/blanc/jaune/rouge), codec audio ES8311 (speaker MX1.25 + micro), capteur température/humidité SHTC3, RTC PCF85063, slot TF et connecteur batterie Li-ion.

Produit : [waveshare.com/esp32-s3-epaper-1.54g.htm](https://www.waveshare.com/esp32-s3-epaper-1.54g.htm) — Wiki : [docs.waveshare.com/ESP32-S3-ePaper-1.54G](https://docs.waveshare.com/ESP32-S3-ePaper-1.54G)

## Projets

| Projet | Description | Périphériques utilisés |
|--------|-------------|------------------------|
| [Basic_Blink](projects/Basic_Blink/) | LED verte clignotante (GPIO 3, active LOW) | LED onboard |

## Structure

```
devices/epaper_154g/
├── lib/epaper_154g_hw/              # Pins (epaper154g_pins.h)
├── projects/
│   └── Basic_Blink/
├── docs/
│   ├── demo-code/                   # Demo vendor (Arduino 3.2.0, ESP-IDF 5.5.1, XiaoZhi)
│   └── schematics/                  # Schéma PDF
└── firmware/                        # Firmware usine + restauration
```

## Build et flash

Depuis la racine du monorepo :

```powershell
# Windows
.\build.ps1 epaper_154g Basic_Blink                   # Build
.\build.ps1 epaper_154g Basic_Blink -Upload -Monitor  # Build + flash + moniteur
```

```bash
# macOS / Linux
./build.sh epaper_154g Basic_Blink                    # Build
./build.sh epaper_154g Basic_Blink --upload --monitor # Build + flash + moniteur
```

Le firmware usine expose un port série CDC natif : pas de séquence BOOT manuelle nécessaire pour le premier flash.
