# Resources — Waveshare ESP32-S3-Touch-AMOLED-1.75C

## Official Waveshare

| Resource | URL |
|---|---|
| **Wiki** | https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75C |
| **Documentation Platform** | https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75C |
| **Product page** | https://www.waveshare.com/esp32-s3-touch-amoled-1.75c.htm |
| **GitHub repo** (Arduino + ESP-IDF demos) | https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C |
| **Schematic** (archived locally) | [`docs/schematics/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf`](../../../docs/schematics/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf) |
| **Factory firmware** (archived locally) | [`devices/amoled_175c/firmware/`](../../../firmware/) — `FactoryOnly-260114.bin` (~32 MB) with restore procedure |

## GitHub Demos to Mine

In `waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/examples/Arduino-v3.3.5/examples/`:

| Demo | What to extract |
|---|---|
| `01_HelloWorld` | Minimal CO5300 init using `Arduino_CO5300` (in `libraries/GFX_Library_for_Arduino`) |
| `02_GFX_AsciiTable` | Basic drawing + font usage |
| `03_LVGL_AXP2101_ADC_Data` | AXP2101 over I2C, LVGL display |
| `04_LVGL_QMI8658_ui` | QMI8658 IMU readings + LVGL |
| `05_LVGL_Widgets` | Full LVGL example (with full I2C device scan / init) |
| `06_ES7210` | Mic-array AEC capture |
| `07_ES8311` | Playback codec |

In `waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/examples/ESP-IDF-v5.5/`:

- Mirror IDF demos for the same peripherals. Useful when porting to our `esp_lcd_*` style.

## IC Datasheets

To be fetched into `docs/datasheets/` on first need :

| IC | Notes |
|---|---|
| **CO5300** | AMOLED driver. Datasheet not yet in this repo. |
| **CST9217** | Touch controller. Hangzhou Hynitron Semiconductor — CST9xx family. |
| **ES8311** | Audio codec. Already in `devices/amoled/docs/datasheets/ES8311.DS.pdf`. |
| **ES7210** | 4-channel ADC for mic arrays. Everest Semi. |
| **AXP2101** | X-Powers PMIC. Already in `devices/amoled/docs/datasheets/X-power-AXP2101_SWcharge_V1.0.pdf`. |
| **QMI8658** | QST 6-axis IMU. Already in `devices/amoled/docs/datasheets/QMI8658C.pdf`. |

## Libraries (used in vendor demos)

| Library | Purpose |
|---|---|
| **Arduino_GFX** (`moononournation/GFX Library for Arduino`) | Display driver — provides `Arduino_CO5300` |
| **Arduino_DriveBus** | I2C/SPI bus abstraction used by Arduino_GFX |
| **LVGL** | UI framework (vendor demos use v8.x) |
| **XPowersLib** | AXP2101 PMIC driver |
| **SensorLib** | QMI8658 IMU driver |

## Inter-Device Comparison

Two AMOLED devices now in this monorepo :

| Aspect | AMOLED 1.75C (this) | AMOLED 1.8 |
|---|---|---|
| Display IC | CO5300 | SH8601 |
| Resolution | 466×466 round | 368×448 rectangular |
| Touch IC | CST9217 | FT3168 |
| Audio | ES8311 + **ES7210 AEC** + speaker | ES8311 + speaker |
| Microphones | Dual array | Single |
| I/O expander | None | XCA9554 |
| SD card | No | SDMMC 1-wire |

Driver factorization in `shared/lib/qspi_panel/` will need attention once we ship a project on the 1.75C — the current `esp_lcd_sh8601` is reused for the SH8601 (AMOLED 1.8) and as a generic QSPI host for the ST77916 (Knob, Guition), but CO5300 is yet another driver IC.
