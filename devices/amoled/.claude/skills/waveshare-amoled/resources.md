# Resources — Waveshare ESP32-S3-Touch-AMOLED-1.8

## Official Waveshare

| Resource | URL |
|---|---|
| **Wiki** (primary reference) | https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8 |
| **Product page** | https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm |
| **Schematic + datasheets** | Downloadable from the wiki page (Documents section) |
| **Demo code** (Arduino + ESP-IDF) | Downloadable from the wiki page (Demo section) |

> The wiki is the authoritative source for schematics, pin assignments, and demo code.

## GitHub waveshareteam

> https://github.com/waveshareteam — 79 public repos.

### Most Relevant Repos

| Repo | Why it matters |
|---|---|
| [ESP32-S3-Touch-AMOLED-1.8](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8) | **Official repo** — Arduino + ESP-IDF examples, pin_config.h |
| [ESP32-display-support](https://github.com/waveshareteam/ESP32-display-support) | Resource hub. `AMOLED-Products/ESP32-S3-Touch-AMOLED-1.8/` has SensorLib, esp_lcd_sh8601, examples |
| [Waveshare-ESP32-components](https://github.com/waveshareteam/Waveshare-ESP32-components) | ESP-IDF component registry. Contains `display/lcd/esp_lcd_sh8601/` (QSPI driver) |
| [ESP32-AIChats](https://github.com/waveshareteam/ESP32-AIChats) | Voice/AI (xiaozhi-esp32) — supports AMOLED-1.8. LVGL + audio |

## IC Datasheets & Components

| Component | Local datasheet | Notes |
|---|---|---|
| **SH8601** | `../../../shared/docs/datasheets/SH8601A0_DataSheet_Preliminary_V0.0_UCS_191107_1_.pdf` | QSPI AMOLED driver |
| **FT3168** | `docs/datasheets/FT3168.pdf` | Focaltech capacitive touch controller |
| **ES8311** | `docs/datasheets/ES8311.DS.pdf` + `docs/datasheets/ES8311.user.Guide.pdf` | Everest Semiconductor audio codec |
| **AXP2101** | `docs/datasheets/X-power-AXP2101_SWcharge_V1.0.pdf` | X-Powers PMIC (battery management + multi-rail) |
| **QMI8658** | `docs/datasheets/QMI8658C.pdf` | QST 6-axis IMU (accelerometer + gyroscope) |
| **PCF85063** | `docs/datasheets/PCF85063A.pdf` | NXP low-power real-time clock |
| **XCA9554 / TCA9554** | — | I2C 8-bit I/O expander |

## Libraries

| Library | Purpose | Notes |
|---|---|---|
| **Arduino_GFX** | Display driver | `Arduino_SH8601` class for QSPI AMOLED |
| **Arduino_DriveBus** | I2C/SPI bus abstraction | Used by Arduino_GFX |
| **LVGL** | UI framework | v8.4.0, demo uses custom UI projects |
| **XPowersLib** | AXP2101 PMIC driver | Battery, charging, ADC |
| **SensorLib** | QMI8658 IMU driver | Accel + gyro |
| **Adafruit_XCA9554** | I/O expander driver | LCD ctrl, SD power |
