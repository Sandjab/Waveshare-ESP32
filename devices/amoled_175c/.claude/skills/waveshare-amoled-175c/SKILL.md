---
name: waveshare-amoled-175c
description: Development reference for Waveshare ESP32-S3-Touch-AMOLED-1.75C (round 1.75" watch form factor). Use when working with this device, its AMOLED display (CO5300 QSPI 466×466), touch (CST9217), audio (ES8311 playback + ES7210 mic-array AEC), power (AXP2101), IMU (QMI8658), or restoring the factory firmware.
---

# Waveshare ESP32-S3-Touch-AMOLED-1.75C

## Quick Reference Card

| Spec | Value |
|---|---|
| **MCU** | ESP32-S3R8 — Xtensa LX7 dual-core 240 MHz, WiFi + BLE 5 |
| **Flash / PSRAM** | 16 MB Flash (Quad SPI) / 8 MB PSRAM (Octal SPI) |
| **Display** | 1.75" round AMOLED, **466×466**, **CO5300** QSPI |
| **Touch** | **CST9217** capacitive, I2C (shared bus) |
| **Audio Out** | ES8311 codec → on-board speaker (PA enable on GPIO 46). **No 3.5 mm jack.** |
| **Audio In** | Dual-microphone array via **ES7210** echo-cancellation AEC, I2S in |
| **Power** | AXP2101 PMIC, I2C — USB-C 5 V / Li-ion |
| **IMU** | QMI8658 6-axis (accel + gyro), I2C |
| **Storage** | **No SD card** on this board |
| **USB** | Type-C, native USB CDC |
| **Buttons** | 2 side buttons (PWR, BOOT) |
| **Framework** | PlatformIO (Arduino expected) |

## GPIO Quick Reference (from `amoled_175c_pins.h`)

| Function | GPIOs | Notes |
|---|---|---|
| Display (CO5300) | CLK:38 CS:12 D0:4 D1:5 D2:6 D3:7 RST:2 | QSPI. **RST shared with touch RST** per vendor pinconfig — to validate |
| Touch (CST9217) | SDA:15 SCL:14 INT:11 RST:2 | I2C; RST shared with LCD RST |
| Audio out (ES8311) | DOUT:8 | I2S, shares MCK/BCK/WS with ES7210 |
| Audio in (ES7210) | MCK:16 BCK:9 WS:45 DIN:10 | Mic-array AEC; shares I2S bus with ES8311 |
| Speaker PA | PA:46 | HIGH = amp enabled |
| I2C bus | SDA:15 SCL:14 | Shared : CST9217, AXP2101, QMI8658, ES8311-ctrl, ES7210-ctrl |

## Flash (from monorepo root)

```bash
./build.sh amoled_175c <Project>            # macOS / Linux (once a project exists)
./build.sh amoled_175c <Project> --upload   # + flash
```

```powershell
.\build.ps1 amoled_175c <Project>           # Windows
.\build.ps1 amoled_175c <Project> -Upload
```

## Progressive Disclosure Index

1. **[esp32-platform.md](../../../../.claude/skills/waveshare-esp32/esp32-platform.md)** — PlatformIO setup, toolchain, common libraries (reusable across all monorepo devices)
2. **[device-hardware.md](device-hardware.md)** — Architecture, complete pinout, ICs, audio chain, watch-form-factor specifics
3. **[resources.md](resources.md)** — Official links (vendor repo + wiki), datasheets, factory firmware

## Status

- **Basic_Blink** works (green/red full-screen). Display driver wired through the shared `esp_lcd_sh8601` generic QSPI host with a CO5300 `init_cmds` table.
- **Display offset** : the visible 466×466 area is offset by 6 columns inside the CO5300 RAM (4-col `set_gap(6, 0)` in `amoled_175c_display_init()`). Confirmed against the Waveshare `Arduino_CO5300` demo which uses `col_offset1 = 6`.
- **Native orientation** : USB-on-top is correct out of the box (MADCTL = 0x00 default).
- **Touch / audio / IMU / PMIC** : not yet exercised — see device-hardware.md for I2C addresses and audio bus layout.
