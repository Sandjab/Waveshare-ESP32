---
name: waveshare-knob
description: Development reference for Waveshare ESP32-S3-Knob-Touch-LCD-1.8. Use when working with this device, its dual-MCU architecture, display (ST77916 QSPI), touch (CST816), haptics (DRV2605), audio, or PlatformIO setup.
---

# Waveshare ESP32-S3-Knob-Touch-LCD-1.8

## Quick Reference Card

| Spec | Value |
|---|---|
| **Main MCU** | ESP32-S3R8 — Xtensa dual-core 240 MHz, WiFi + BLE 5 |
| **Secondary MCU** | ESP32-U4WDH — Xtensa dual-core 240 MHz, BT Classic + BLE |
| **Flash / PSRAM** | 16 MB Flash (Quad SPI) / 8 MB PSRAM (Octal SPI) |
| **Display** | 1.8" IPS LCD, 360x360, ST77916 QSPI, 262K colors, 600 cd/m² |
| **Touch** | CST816 capacitive, I2C |
| **Haptics** | DRV2605 + LRA motor, I2C (0x5A) |
| **Audio Out** | PCM5100A DAC stereo (muxed: GPIO0 selects S3 or ESP32) |
| **Audio In** | PDM microphone (I2S on ESP32-S3) |
| **Encoder** | Rotary encoder on ESP32-S3 (+ 2nd encoder on ESP32) |
| **Storage** | MicroSD (SDMMC 4-wire) |
| **USB** | Type-C, CH445P switch (MCU select by plug orientation) |
| **Power** | USB 5V / Li-ion 3.7V MX1.25 (800mAh optional), charger intégré |
| **Framework** | PlatformIO (ESP-IDF + Arduino) |

## GPIO Quick Reference (ESP32-S3)

| Function | GPIOs | Interface |
|---|---|---|
| Display (ST77916) | CLK:13 D0:15 D1:16 D2:17 D3:18 CS:14 RST:21 BL:47 | QSPI 80MHz |
| Touch (CST816) | SDA:11 SCL:12 INT:9 RST:10 | I2C |
| Haptics (DRV2605) | SDA:11 SCL:12 | I2C (0x5A) |
| Rotary Encoder | A:8 B:7 | GPIO |
| Microphone | CLK:45 DATA:46 | I2S PDM RX |
| SD Card (SDMMC) | CMD:3 CLK:4 D0:5 D1:6 D2:42 D3:2 | SDMMC 4-wire |
| Audio I2S out | BCLK:39 WS:40 DOUT:41 | I2S STD → PCM5100A |
| Inter-MCU UART | TX:43 RX:44 | UART (↔ ESP32 GPIO16/17) |
| Battery ADC | GPIO 1 (ADC1_CH0) | ADC (divider 1:1) |
| Audio mux / BOOT | GPIO 0 | HIGH=S3 controls DAC |

## Flash (PlatformIO)

```bash
# Build + upload via USB
pio run -t upload

# Monitor serial
pio device monitor -b 115200
```

## Progressive Disclosure Index

For deeper information, consult these files in order:

1. **[esp32-platform.md](esp32-platform.md)** — PlatformIO setup, toolchain, common libraries (reusable for any ESP32-S3 project)
2. **[device-hardware.md](device-hardware.md)** — Dual-MCU architecture, complete pinout, ICs, display init, power
3. **[resources.md](resources.md)** — Official links, datasheets, community projects, repos

> **Convention** : `esp32-platform.md` contains generic ESP32-S3/PlatformIO knowledge. `device-hardware.md` contains everything specific to this Waveshare device.
