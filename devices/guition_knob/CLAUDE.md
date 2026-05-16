# Guition JC3636K718 — Knob avec anneau RGB

Carte rotative 1.8" 360×360 (ST77916 QSPI) avec encoder, audio DAC PCM5100A, micro I2S, SD card, et **anneau 13 WS2812 (GRB) sur GPIO 0** — fonctionnellement très proche du Waveshare ESP32-S3-Knob-Touch-LCD-1.8, mais avec un pinout entièrement différent et l'anneau RGB en plus.

## Repo Structure

```
devices/guition_knob/
├── lib/guition_knob_hw/             # GPIO defs, display, LVGL, encoder
├── projects/
│   └── Basic_Blink/                 # Ecran clignotant (raw display)
└── docs/
    ├── datasheets/                  # ST77916 INI, PCM5100A pdf, ESP32-S3R8 pdf
    ├── demo-code/                   # Code demo Guition (Arduino + ESP-IDF, dont led_strip)
    ├── dimensions/                  # Photos cotes mécaniques
    ├── instructions/                # PDFs vendeur (utilisation, getting started)
    ├── schematics/                  # JC3636K718.pdf + JC3636K718_P.pdf
    └── images/                      # (à compléter)
```

Shared : `shared/lib/qspi_panel/` (driver `esp_lcd_sh8601` — déjà réutilisé par Knob).

## Différences clés vs Waveshare Knob

| Aspect | Waveshare Knob | Guition K718 |
|---|---|---|
| LCD CS / CLK / D0-D3 / RST / BL | 14 / 13 / 15-18 / 21 / 47 | 12 / 11 / 13-16 / 17 / 21 |
| Encoder A / B | 8 / 7 | **2 / 1** |
| I2C SDA / SCL | 11 / 12 | **9 / 10** |
| Touch INT / RST | non exposés | 7 / 8 |
| SD CMD / CLK / D0-D3 | 3 / 4 / 5-6-42-2 | **38 / 39 / 40-41-48-47** |
| Audio I2S BCK / WS / DO / Mute | (n/a dans Basic_*) | 3 / 45 / 42 / 46 |
| Mic SCK / Data | (n/a) | 5 / 4 |
| Battery monitor | — | 6 (DAC) |
| **RGB ring data** | — | **0** (13 WS2812 GRB) |
| Haptic DRV2605 | présent (I2C) | **absent** |

## Init ST77916

La séquence d'init est **identique** entre Waveshare et Guition (vérifié 181/185 entrées byte-identiques contre le `.INI` Guition). `guition_lcd_init.h` est une copie du `knob_lcd_init.h`. Si un jour on veut factoriser, candidat naturel pour `shared/lib/`.

## Gotchas

- **GPIO 0 dual role** : BOOT strap + RGB ring data. WS2812 idle = low ⇒ pas de conflit tant qu'on n'écrit pas avant la fin du boot.
- **Pas de DRV2605** : tout port direct des projets `Hue_Encoder` du Knob qui utilisent les haptics ne marchera pas tel quel — il faut désactiver / supprimer ces appels.
- **Pinout 100% différent du Knob Waveshare** malgré l'IC ST77916 commun.
- **Pas un Guition JC3636W518** non plus (notre repo en parlait à propos du Knob). K718 et W518 sont deux modèles Guition distincts avec des pinouts différents.

## First flash — mode download manuel obligatoire

À la sortie d'usine, la carte tourne le **firmware vendor Guition** qui expose un USB custom (`VID:PID=303A:4001` ou `4002`, "ESP USB DEVICE" / "N7 Workshop") — probablement HID pour piloter leur menu/écran. Il n'expose **pas** de port série CDC, donc :
- `pio device list` ne voit aucun port utilisable
- l'auto-detect de `build.sh`/`build.ps1` (qui cherche `VID:PID=303A:1001`) échoue
- esptool ne peut pas piloter d'auto-reset DTR/RTS

Solution : forcer le **ROM bootloader** de l'ESP32-S3 en mode download manuel :

1. Maintenir **BOOT** appuyé
2. Brancher le câble USB (ou appuyer brièvement sur **RESET** si déjà branché)
3. Relâcher BOOT

La carte ré-énumère alors en `VID:PID=303A:1001` ("USB JTAG_serial debug unit" / Espressif) avec un `/dev/cu.usbmodem*` (ou `COM*`) exposé. À partir de là, `./build.sh guition_knob <projet> --upload` (ou `.\build.ps1 ... -Upload`) trouve le port et flashe normalement.

Note : le firmware vendor a aussi un **mode USB Mass Storage** (monté en FAT32 503 MB sur `/Volumes/NO NAME` côté Mac, `COM*` MSC côté Windows), mais ce n'est **pas** le mode par défaut au boot — il faut l'activer explicitement via une entrée de menu (« reboot to MSC ») sur l'écran. Donc on ne tombe dessus que volontairement.

Une fois notre firmware en place (avec `-DARDUINO_USB_CDC_ON_BOOT=1` dans `platformio.ini`), le CDC est exposé en permanence et l'auto-reset esptool fonctionne — la procédure manuelle n'est plus nécessaire pour les flashs suivants.

### Restauration du firmware vendor

Le binaire d'usine `JC3636K718_V1.1.bin` est archivé dans [`docs/firmware/`](docs/firmware/). Procédure complète de re-flash (commande esptool, durée, état post-flash, etc.) : [`docs/firmware/README.md`](docs/firmware/README.md).

## LVGL Documentation

Comme pour le Knob : Context7 `/websites/lvgl_io_8_4` ou `/websites/lvgl_io_master`.
