# Waveshare ESP32-S3-Touch-AMOLED-1.8

Documentation and development repo for the Waveshare ESP32-S3-Touch-AMOLED-1.8 device.

## Skill

For hardware details, pinout, GPIO table, framework setup, and flash commands, invoke the `waveshare-amoled` skill. It is the primary reference for this project.

## Repo Structure

```
devices/amoled/
├── .claude/skills/waveshare-amoled/   # Device skill (3 files — SKILL.md is entry point)
├── lib/amoled_hw/                     # Device-specific lib (pins)
├── projects/                          # PlatformIO projects (empty — to be created)
├── docs/
│   ├── demo-code/                     # Waveshare demo code (Arduino 16 examples + ESP-IDF 6 examples)
│   ├── schematics/                    # Schematic extracts
│   └── product.pdf                    # Product datasheet
```

Shared code lives in `../../shared/lib/` (QSPI driver `esp_lcd_sh8601`).

## Conventions

- **Demo code is authoritative for GPIOs.** `docs/demo-code/` takes precedence over schematics.
- **All I2C devices share one bus** — GPIO 15 (SDA) / 14 (SCL): touch, PMIC, IMU, RTC, codec, expander.

## Gotchas

- **No display RST pin** — reset is handled via the XCA9554 I/O expander, not a direct GPIO.
- **No backlight PWM** — AMOLED is self-emitting; brightness is controlled via SH8601 display commands.
- **SD card is 1-wire** — uses SDMMC 1-bit mode (not 4-wire like the Knob). Power is controlled via XCA9554 P7.
- **AXP2101 PMIC** — the `XPOWERS_CHIP_AXP2101` define is required. Use XPowersLib for battery/charging management.
- **PA enable** — GPIO 46 must be set HIGH to enable the speaker amplifier for audio output.
- **Single-MCU, USB-C orientation indifferent.** Unlike the Waveshare Knob (dual-MCU, see [`../knob/CLAUDE.md`](../knob/CLAUDE.md)), this board exposes the same ESP32-S3 on both USB-C orientations. Same MAC, same port, native USB-CDC (`cu.usbmodem*`, VID:303A PID:1001). Confirmed 2026-05-17.

## Next steps

Factory firmware (Waveshare `FactoryXiaozhi_250805`, ~16 MB) is archived in [`firmware/`](firmware/) with the restore procedure documented in [`firmware/README.md`](firmware/README.md). Merged image @ `0x0` validated 2026-05-17.
