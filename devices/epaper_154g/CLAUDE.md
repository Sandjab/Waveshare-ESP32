# Waveshare ESP32-S3-ePaper-1.54G — e-paper 4 couleurs

Carte AIoT ESP32-S3 avec écran e-paper 1.54" 200×200 **4 couleurs (noir/blanc/jaune/rouge)**, codec audio ES8311 (speaker + mic), capteur température/humidité SHTC3, RTC PCF85063, slot TF, connecteurs batterie Li-ion et speaker MX1.25.

Skill associé : `waveshare-epaper-154g` ([.claude/skills/waveshare-epaper-154g/SKILL.md](.claude/skills/waveshare-epaper-154g/SKILL.md)).

## Repo Structure

```
devices/epaper_154g/
├── lib/epaper_154g_hw/              # GPIO defs (epaper154g_pins.h)
├── projects/
│   └── Basic_Blink/                 # LED verte clignotante (GPIO 3)
├── docs/
│   ├── demo-code/                   # Demo vendor (Arduino 3.2.0, ESP-IDF 5.5.1, XiaoZhi)
│   └── schematics/                  # Schéma PDF
└── firmware/                        # Firmware usine + procédure de restauration
```

## Hardware (vérifié)

- **SoC** : ESP32-S3-PICO-1 (LGA56), flash 8 MB + PSRAM 8 MB embarquées — confirmé par `esptool flash_id` sur l'exemplaire physique.
- **PSRAM en mode octal (OPI)** : le sdkconfig vendor impose `CONFIG_SPIRAM_MODE_OCT` et la config Arduino IDE vendor demande « OPI PSRAM » ⇒ PlatformIO : `board_build.arduino.memory_type = qio_opi`, `flash_size = 8MB`, partitions `default_8MB.csv`.
- **USB CDC natif exposé par le firmware usine** : contrairement au Guition, le port série (`/dev/cu.usbmodem*`) est disponible out-of-the-box — pas de séquence BOOT manuelle pour le premier flash.

## Gotchas

- **Rails de puissance en logique inversée** : `PIN_EPD_PWR` (GPIO 6) et `PIN_AUDIO_PWR` (GPIO 42) actifs **LOW** ; `PIN_VBAT_PWR` (GPIO 17) actif **HIGH** (maintien de l'alim batterie). Cf. `board_power_bsp` vendor. Sortie audio : `PIN_AUDIO_PWR` LOW **et** `PIN_PA_CTRL` (GPIO 46) HIGH.
- **LED verte GPIO 3 active LOW** (`LED_ON = 0` dans la démo vendor).
- **Refresh e-paper lent** : ~20 s full refresh, ~15 s fast refresh (4 couleurs oblige). Pas d'animation possible — UI statique uniquement. Le driver vendor est `EPD_1in54g` (2 bits/pixel, codes couleur 0x0=noir 0x1=blanc 0x2=jaune 0x3=rouge) ; le contrôleur du panel n'est pas nommé dans le code vendor.
- **SD en SDMMC 1-bit** (CLK 39, CMD 41, D0 40) — pas de mode 4-bit câblé dans la démo.
- **Schéma nommé `ESP32-S3-Touch-ePaper-1.54-Schematic.pdf`** : le repo GitHub dédié au 1.54G ne fournit que ce PDF — PCB vraisemblablement partagé avec la variante Touch (non vérifié ; notre carte G n'a pas de touch dans ses démos).
- **Pas de bouton RESET dédié documenté** : boutons BOOT (GPIO 0, aussi wakeup deep-sleep dans la démo 11_RTC_Sleep) et PWR (GPIO 18).
- **Le firmware usine exige une carte SD** : c'est un build XiaoZhi v2.0.1 (mode « PhotoPainter », observé sur les logs série 2026-06-09). Sans TF card montable, `sdcard_bsp` timeout (`0x107`) → `init Failure` → `app_main()` retourne **sans rien afficher** — écran figé sur l'image précédente (l'e-paper retient son image hors tension). Insérer une TF FAT32 puis reset.

## Restauration du firmware usine

Binaire archivé dans [`firmware/`](firmware/) (image merged, offset `0x0`, vérifiée : magic `0xE9` à 0x0 + table de partitions à 0x8000). Procédure : [`firmware/README.md`](firmware/README.md).

## Démo code vendor

- `docs/demo-code/Arduino/` — Arduino-ESP32 3.2.0 : ADC, RTC, SHTC3, SD, WiFi AP/STA, audio ES8311, e-paper (`08_E_paper_test`, avec `GUI_Paint` et fonts)
- `docs/demo-code/ESP-IDF/` — ESP-IDF 5.5.1 : mêmes briques en composants BSP (+ `09_E_Paper_Test` avec LVGL)
- `docs/demo-code/XiaoZhi/` — assistant vocal XiaoZhi (sources zip + fonts)
