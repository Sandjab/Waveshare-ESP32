# ESP32-S3-ePaper-1.54G — Resources

## En ligne

| Ressource | URL |
|---|---|
| Wiki (EN) | https://docs.waveshare.com/ESP32-S3-ePaper-1.54G |
| Wiki (CN) | https://docs.waveshare.net/ESP32-S3-ePaper-1.54G |
| Produit | https://www.waveshare.com/esp32-s3-epaper-1.54g.htm |
| GitHub vendor (dédié G) | https://github.com/waveshareteam/ESP32-S3-ePaper-1.54G |
| GitHub variante N/B + Touch | https://github.com/waveshareteam/ESP32-S3-ePaper-1.54 |

## Local

| Ressource | Chemin |
|---|---|
| Demo Arduino (Arduino-ESP32 3.2.0) | `devices/epaper_154g/docs/demo-code/Arduino/` — 01_ADC, 02_RTC, 03_SHTC3, 04_SD, 05/06_WiFi, 07_Audio_out, 08_E_paper_test |
| Demo ESP-IDF (5.5.1) | `devices/epaper_154g/docs/demo-code/ESP-IDF/` — mêmes briques en composants BSP + 09_E_Paper_Test (LVGL) + 08_BATT_PWR (dont `user_config.h`, **référence pinout**) |
| XiaoZhi (assistant vocal) | `devices/epaper_154g/docs/demo-code/XiaoZhi/` |
| Schéma | `devices/epaper_154g/docs/schematics/ESP32-S3-Touch-ePaper-1.54-Schematic.pdf` |
| Firmware usine | `devices/epaper_154g/firmware/ESP32-S3-ePaper-1.54G.bin` (merged, offset 0x0 — voir `firmware/README.md`) |

## Variantes produit (ne pas confondre)

- **ESP32-S3-ePaper-1.54** : noir/blanc, V1 (S3FH4R2, 4MB/2MB) et V2 (PICO-1-N8R8, 8MB/8MB).
- **ESP32-S3-Touch-ePaper-1.54** : noir/blanc + touch FT6336.
- **ESP32-S3-ePaper-1.54G** (la nôtre) : 4 couleurs, PICO-1-N8R8, pas de touch dans les démos vendor.
