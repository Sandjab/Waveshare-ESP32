---
name: waveshare-epaper-154g
description: Development reference for Waveshare ESP32-S3-ePaper-1.54G (200×200 4-color e-paper, ES8311 audio, SHTC3, PCF85063 RTC, battery). Use when working with this device, its e-paper display (EPD_1in54g driver), power rails, audio codec, sensors, deep-sleep, or restoring the factory firmware.
---

# Waveshare ESP32-S3-ePaper-1.54G

## Quick Reference Card

| Spec | Value |
|---|---|
| **MCU** | ESP32-S3-PICO-1 (LGA56) — Xtensa LX7 dual-core 240 MHz, WiFi + BLE 5 |
| **Flash / PSRAM** | 8 MB Flash (QIO) / 8 MB PSRAM (**OPI**) — embarquées dans le package, vérifié `esptool flash_id` |
| **Display** | 1.54" e-paper 200×200, **4 couleurs** (noir/blanc/jaune/rouge), SPI, driver vendor `EPD_1in54g` |
| **Refresh** | ~20 s full, ~15 s fast — UI statique uniquement |
| **Audio** | ES8311 codec (I2C 0x18) : speaker MX1.25 (via ampli, ctrl GPIO 46) + micro |
| **Sensors** | SHTC3 temp/humidité (I2C 0x70), PCF85063 RTC (I2C 0x51) |
| **Storage** | TF card slot (SDMMC 1-bit) |
| **Power** | USB-C 5V / Li-ion MX1.25 (ETA6098 charger, VBAT sense ADC GPIO 4) |
| **USB** | Type-C, CDC natif exposé par le firmware usine (pas de BOOT dance au 1er flash) |
| **Framework** | PlatformIO Arduino (`qio_opi`, 8MB, `default_8MB.csv`) ; vendor : Arduino-ESP32 3.2.0 / ESP-IDF 5.5.1 |

## GPIO Quick Reference

| Function | GPIOs | Notes |
|---|---|---|
| E-Paper (SPI2) | DC:10 CS:11 SCK:12 MOSI:13 RST:9 BUSY:8 | codes couleur 2 bits : 0x0 noir, 0x1 blanc, 0x2 jaune, 0x3 rouge |
| Power rails | EPD_PWR:6 (**LOW=on**) AUDIO_PWR:42 (**LOW=on**) VBAT_PWR:17 (HIGH=on) | logique inversée sur EPD/audio |
| Audio I2S (ES8311) | MCLK:14 BCLK:15 LRCK:38 DOUT:45 DIN:16 PA_CTRL:46 | speaker : AUDIO_PWR LOW **et** PA_CTRL HIGH |
| I2C | SDA:47 SCL:48 | PCF85063 0x51, SHTC3 0x70, ES8311 0x18 |
| TF card (SDMMC 1-bit) | CLK:39 CMD:41 D0:40 | |
| Battery ADC | GPIO 4 | ADC1_CH3, atten 12 dB |
| Buttons | BOOT:0 PWR:18 | BOOT = wakeup deep-sleep dans la démo vendor |
| LED verte | GPIO 3 | **active LOW** |
| UART | TX:43 RX:44 | |

## Flash (from monorepo root)

```bash
./build.sh epaper_154g <ProjectName>            # macOS / Linux
./build.sh epaper_154g <ProjectName> --upload   # + flash
```

```powershell
.\build.ps1 epaper_154g <ProjectName>           # Windows
.\build.ps1 epaper_154g <ProjectName> -Upload   # + flash
```

## Progressive Disclosure Index

1. **[esp32-platform.md](../../../../.claude/skills/waveshare-esp32/esp32-platform.md)** — PlatformIO setup, toolchain, common libraries (reusable across all monorepo devices)
2. **[device-hardware.md](device-hardware.md)** — Architecture, pinout complet, rails de puissance, chaîne audio, e-paper
3. **[resources.md](resources.md)** — Wiki, GitHub vendor, demo code local, firmware usine
