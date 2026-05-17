---
name: waveshare-amoled
description: Development reference for Waveshare ESP32-S3-Touch-AMOLED-1.8. Use when working with this device, its AMOLED display (SH8601 QSPI), touch (FT3168), audio (ES8311), power (AXP2101), IMU (QMI8658), or RTC (PCF85063).
---

# Waveshare ESP32-S3-Touch-AMOLED-1.8

## Quick Reference Card

| Spec | Value |
|---|---|
| **MCU** | ESP32-S3R8 — Xtensa dual-core 240 MHz, WiFi + BLE 5 |
| **Flash / PSRAM** | 16 MB Flash (Quad SPI) / 8 MB PSRAM (Octal SPI) |
| **Display** | 1.8" AMOLED, 368x448, SH8601 QSPI |
| **Touch** | FT3168 capacitive, I2C (0x38) |
| **Audio** | ES8311 codec (I2S + I2C ctrl), speaker + mic, PA on GPIO 46 |
| **Power** | AXP2101 PMIC, I2C (0x34) — USB 5V / Li-ion |
| **IMU** | QMI8658 6-axis (accel + gyro), I2C (0x6B) |
| **RTC** | PCF85063 real-time clock, I2C (0x51) |
| **I/O Expander** | XCA9554 (TCA9554), I2C (0x20) — LCD ctrl, PMU INT, SD power |
| **Storage** | MicroSD (SDMMC 1-wire) |
| **USB** | Type-C, native USB CDC |
| **Framework** | PlatformIO (ESP-IDF + Arduino) |

## GPIO Quick Reference (ESP32-S3)

| Function | GPIOs | Interface |
|---|---|---|
| Display (SH8601) | CLK:11 D0:4 D1:5 D2:6 D3:7 CS:12 | QSPI |
| Touch (FT3168) | SDA:15 SCL:14 INT:21 | I2C (0x38) |
| Audio (ES8311) | MCK:16 BCK:9 WS:45 DI:10 DO:8 PA:46 | I2S + I2C (0x18) |
| I2C bus | SDA:15 SCL:14 | Shared: touch, PMIC, IMU, RTC, codec, expander |
| SD Card (SDMMC) | CLK:2 CMD:1 D0:3 | SDMMC 1-wire |
| AXP2101 PMIC | via I2C (0x34) | Power management |
| QMI8658 IMU | via I2C (0x6B) | 6-axis accel + gyro |
| PCF85063 RTC | via I2C (0x51) | Real-time clock |
| XCA9554 expander | via I2C (0x20) | LCD ctrl, SD power, PMU INT |

## Flash (PlatformIO)

```bash
# Build + upload via USB
pio run -t upload

# Monitor serial
pio device monitor -b 115200
```

## Progressive Disclosure Index

For deeper information, consult these files in order:

1. **[esp32-platform.md](../../../../.claude/skills/waveshare-esp32/esp32-platform.md)** — PlatformIO setup, toolchain, common libraries (reusable for any ESP32-S3 project)
2. **[device-hardware.md](device-hardware.md)** — Single-MCU architecture, complete pinout, ICs, display, power (AXP2101), audio (ES8311)
3. **[resources.md](resources.md)** — Official links, datasheets, community projects, repos

> **Convention** : `esp32-platform.md` contains generic ESP32-S3/PlatformIO knowledge. `device-hardware.md` contains everything specific to this Waveshare device.
