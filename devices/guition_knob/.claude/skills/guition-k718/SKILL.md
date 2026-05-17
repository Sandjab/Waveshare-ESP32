---
name: guition-k718
description: Development reference for Guition JC3636K718 (ESP32-S3, round 1.8" 360×360 + encoder + RGB ring). Use when working with this device, its display (ST77916 QSPI), audio chain (PCM5100A DAC + NS4150B amp), 13× WS2812 ring on GPIO 0, encoder, microphone, DRV2605 + LRA haptics, or restoring the factory firmware.
---

# Guition JC3636K718

## Quick Reference Card

| Spec | Value |
|---|---|
| **MCU** | ESP32-S3R8 — Xtensa LX7 dual-core 240 MHz, WiFi + BLE 5 |
| **Flash / PSRAM** | 16 MB Flash (Quad SPI) / 8 MB PSRAM (Octal SPI) |
| **Display** | 1.8" IPS LCD, 360×360, ST77916 QSPI, with TE pin |
| **Touch** | CST816 capacitive, I2C (0x15) on the shared SDA:9 / SCL:10 bus — exercised by `Hue_Encoder` |
| **Haptics** | DRV2605 + LRA, I2C (0x5A) on the same shared bus — exercised by `Hue_Encoder` (Adafruit_DRV2605, INTTRIG mode, no GPIO enable/trig needed in practice) |
| **Audio Out** | PCM5100APW DAC stereo → NS4150B mono Class-D amp → onboard speaker + 3.5 mm jack (auto-switch via jack detect) |
| **Audio In** | I2S microphone (SCK:5, DATA:4) |
| **Encoder** | Rotary encoder, **bidi switch** (not quadrature — same driver as Waveshare Knob) |
| **RGB ring** | 13× WS2812 GRB on GPIO 0 |
| **Storage** | MicroSD (SDMMC 4-wire) |
| **USB** | Type-C, native USB CDC (after our firmware; vendor firmware exposes USB-MSC instead) |
| **Power** | USB 5V / Li-ion (battery voltage on GPIO 6 DAC) |
| **Framework** | PlatformIO (Arduino preferred — driver lib uses `ledcAttach`) |

## GPIO Quick Reference

| Function | GPIOs | Notes |
|---|---|---|
| Display (ST77916) | CLK:11 CS:12 D0:13 D1:14 D2:15 D3:16 RST:17 TE:18 BL:21 | QSPI, TE exposed (unused), BL via LEDC PWM |
| Touch (CST816) | SDA:9 SCL:10 INT:7 RST:8 | I2C, not wired in any project yet |
| Rotary Encoder | A:1 B:2 | bidi_switch, timer-polled 3 ms — **swapped vs vendor silkscreen** to fix CW direction (see device-hardware.md) |
| Audio I2S out | BCLK:3 WS:45 DOUT:42 PA:46 | PA HIGH = NS4150B amp on (not DAC mute) |
| Microphone I2S | SCK:5 DATA:4 | I2S in |
| SD Card (SDMMC 4-wire) | CMD:38 CLK:39 D0:40 D1:41 D2:48 D3:47 | |
| Battery monitor | GPIO 6 | DAC pin used as ADC input |
| **RGB ring** | **GPIO 0** | 13× WS2812 GRB — **also BOOT strap** |

## Flash (from monorepo root)

```bash
./build.sh guition_knob <ProjectName>            # macOS / Linux
./build.sh guition_knob <ProjectName> --upload   # + flash
```

```powershell
.\build.ps1 guition_knob <ProjectName>           # Windows
.\build.ps1 guition_knob <ProjectName> -Upload   # + flash
```

> **First flash from vendor firmware**: manual download mode required (hold BOOT + plug USB + release BOOT). Vendor firmware exposes `VID:PID=303A:4001` (USB MSC opt-in), no CDC → no auto-reset. See `device-hardware.md` for details.
>
> **After our firmware is in place**, the native USB-CDC stays exposed and `--upload` works without the BOOT dance.

## Progressive Disclosure Index

1. **[esp32-platform.md](../../../../.claude/skills/waveshare-esp32/esp32-platform.md)** — PlatformIO setup, toolchain, common libraries (reusable across all monorepo devices)
2. **[device-hardware.md](device-hardware.md)** — Architecture, complete pinout, ICs, audio chain, RGB ring details, factory USB anomaly
3. **[resources.md](resources.md)** — Local datasheets, demo code references, inter-device comparison, factory firmware archive

> **Convention** : `esp32-platform.md` is generic ESP32-S3/PlatformIO. `device-hardware.md` is everything specific to this Guition board.
