# Resources — Waveshare ESP32-S3-Knob-Touch-LCD-1.8

## Official Waveshare

| Resource | URL |
|---|---|
| **Wiki** (primary reference) | https://www.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8 |
| **Product page** | https://www.waveshare.com/esp32-s3-knob-touch-lcd-1.8.htm |
| **Schematic + datasheets** | Downloadable from the wiki page (Documents section) |
| **Demo code** (Arduino + ESP-IDF) | Downloadable from the wiki page (Demo section) |
| **Demo ZIP** | https://files.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8/ESP32-S3-Knob-Touch-LCD-1.8-Demo.zip |
| **Firmware BIN** | https://files.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8/ESP32-S3-Knob-Touch-LCD-1.8-BIN.zip |
| **Schematics ZIP** | https://files.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8/ESP32-S3-Knob-Touch-LCD-1.8-schematic.zip |

> The wiki is the authoritative source for schematics, pin assignments, and demo code.
> **No dedicated GitHub repo exists** for the Knob — only the wiki ZIP downloads above.

## GitHub waveshareteam

> https://github.com/waveshareteam — 79 public repos. None dedicated to the Knob, but several share components.

### Most Relevant Repos

| Repo | Stars | Why it matters |
|---|---|---|
| [ESP32-S3-Touch-LCD-1.85C](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C) | 0 | **Closest sibling device** — same ST77916, same 360x360, same CST816. Has `Display_ST77916.h`, `esp_lcd_st77916.h`, `Touch_CST816.cpp`. 8 Arduino examples. |
| [Waveshare-ESP32-components](https://github.com/waveshareteam/Waveshare-ESP32-components) | 47 | ESP-IDF component registry. Contains `display/lcd/esp_lcd_sh8601/` (the QSPI driver our Knob uses). BSPs in `bsp/`. |
| [ESP32-display-support](https://github.com/waveshareteam/ESP32-display-support) | 31 | Resource hub for ESP32 display boards. `AMOLED-Products/ESP32-S3-Touch-AMOLED-1.8/` has SensorLib (DRV2605, CST816), esp_lcd_sh8601, examples. |
| [ESP32-AIChats](https://github.com/waveshareteam/ESP32-AIChats) | 59 | Voice/AI (xiaozhi-esp32) for Waveshare boards. Supports 31 boards (not Knob yet, but AMOLED-1.8 yes). LVGL + audio — portage potential. |
| [ESP32-S3-PhotoPainter](https://github.com/waveshareteam/ESP32-S3-PhotoPainter) | 29 | Contains `knob.h`/`knob.cc` — C++ wrapper around Espressif `iot_knob.h` (rotary encoder). Board def for `esp32-s3-touch-lcd-1.85c` in xiaozhi. |

### Key Files to Look At

- `ESP32-S3-Touch-LCD-1.85C/Arduino/examples/01_lvgl_example/Display_ST77916.h` — ST77916 init sequence (compare with Knob demo)
- `Waveshare-ESP32-components/display/lcd/esp_lcd_sh8601/` — the QSPI component our display driver wraps
- `ESP32-display-support/AMOLED-Products/ESP32-S3-Touch-AMOLED-1.8/` — SensorLib for DRV2605 + CST816

## IC Datasheets & Components

| Component | Resource |
|---|---|
| **ST77916 datasheet** | https://dl.espressif.com/AE/esp-iot-solution/ST77916_SPEC_V1.0.pdf |
| **esp_lcd_st77916** (ESP-IDF component) | https://components.espressif.com/components/espressif/esp_lcd_st77916 |
| **DRV2605** datasheet | Available from TI (Texas Instruments) |
| **PCM5100A** datasheet | Available from TI (Texas Instruments) |
| **CST816** | Available from Hynitron |
| **CH445P** datasheet | Available from WCH (Jiangsu Qin Heng) |

## Libraries

| Library | Version | Purpose | Notes |
|---|---|---|---|
| **LVGL** | v8.4.0 | UI framework | Recommended for this display |
| **ESP32_Display_Panel** | latest | Display abstraction | Espressif official, supports ST77916 QSPI |
| **LovyanGFX** | latest | Display driver | Community, good QSPI support |
| **TFT_eSPI** | latest | Display driver | May need patches for ST77916 QSPI |
| **SensorLib** | v0.3.1 | DRV2605, sensors | Haptic feedback driver |
| **Adafruit DRV2605** | latest | Haptic feedback | Alternative DRV2605 library |

## Community Projects & Configs

### ESPHome

| Resource | URL |
|---|---|
| ESPHome config (detailed) | https://github.com/nkinnan/Waveshare-ESP32-S3-Knob-Touch-LCD-1.8_and_Guition-K5-Knob-Series-JC3636K518 |
| ESPHome device page | https://devices.esphome.io/devices/esp32s3-1.8-inch-jc3636k518c/ |
| ESPHome discussion | https://github.com/orgs/esphome/discussions/3253 |

### Tasmota

| Resource | URL |
|---|---|
| Tasmota discussion | https://github.com/arendst/Tasmota/discussions/23737 |

### Notable Third-Party Projects

| Author | Repo | Description |
|---|---|---|
| **Volos Projects** | [Knob18Meters](https://github.com/VolosR/Knob18Meters) | System monitoring dashboard |
| **iHayri1** | [ESP32-S3-1.8inch-Knob-Display](https://github.com/ihayri/ESP32-S3-1.8inch-Knob-Display-Development-Board) | Combination lock UI with rotary encoder |
| **Muness** | [roon-knob](https://github.com/muness/roon-knob) | Hi-Fi music player (Roon/LMS) remote |
| **That Project** | [lvgl_kawaii_face](https://github.com/0015/lvgl_kawaii_face) | LVGL animated face (17+ expressions) |

## Pre-Flashed Demo Applications

The device ships with these demo apps accessible via a menu:

- AIDA64 — PC hardware monitoring display
- Music Player — Bluetooth audio player
- MJPEG Player — Video playback from SD card
- Picture Album — Image viewer from SD card
- Theme Clock — Animated clock faces
- Spectrum Analyzer — Audio visualization
- BT Music — Bluetooth A2DP audio
- Text Reader — Text file reader from SD card
- HID Volume Control — USB HID volume knob

## Key Tips from Community

- The ST77916 requires a **custom init sequence** — the default ESP-IDF component init may not work. Use the init from Waveshare's demo code.
- The **CH445P USB switch** means you must flip the USB-C cable to access the other MCU for flashing.
- **PSRAM allocation** is critical for display buffers on a 360x360 display — always enable and use PSRAM.
- The Guition JC3636W518 looks similar but has **different pinouts** — don't mix configurations.
- For LVGL, use **double buffering with PSRAM** for smooth animations on this display size.
