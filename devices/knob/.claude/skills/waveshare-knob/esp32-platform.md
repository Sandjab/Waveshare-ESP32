# ESP32-S3 / ESP32 Platform — PlatformIO Reference

This file is reusable for any ESP32-S3 project using PlatformIO.

## PlatformIO Setup

### Typical `platformio.ini` for ESP32-S3

> **IMPORTANT**: The stock `platform = espressif32` bundles Arduino 2.x / ESP-IDF 4.4, which
> lacks QSPI panel IO APIs (`quad_mode`, `rgb_ele_order`). Use **pioarduino** for Arduino 3.x / ESP-IDF 5.1:

```ini
[env:esp32s3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/51.03.07/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200

; PSRAM (8MB Octal)
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv

build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM
```

This gives: Arduino 3.0.7, ESP-IDF 5.1, toolchain-xtensa-esp32s3 12.2.0.
LED PWM API: `ledcAttach(pin, freq, resolution)` (Arduino 3.x — replaces `ledcSetup`+`ledcAttachPin`).

### Dual Framework (ESP-IDF + Arduino)

```ini
[env:esp32s3_idf]
platform = espressif32
board = esp32-s3-devkitc-1
framework = espidf, arduino
platform_packages =
    framework-arduinoespressif32
```

### Custom Board Definition

If needed, create `boards/waveshare_knob.json`:
```json
{
  "build": {
    "mcu": "esp32s3",
    "f_cpu": "240000000L",
    "flash_mode": "qio",
    "psram_type": "opi",
    "arduino": {
      "memory_type": "qio_opi"
    }
  },
  "connectivity": ["wifi", "bluetooth"],
  "frameworks": ["arduino", "espidf"],
  "name": "Waveshare ESP32-S3-Knob-Touch-LCD-1.8",
  "upload": {
    "flash_size": "16MB",
    "maximum_ram_size": 327680,
    "maximum_size": 16777216
  },
  "url": "https://www.waveshare.com/esp32-s3-knob-touch-lcd-1.8.htm",
  "vendor": "Waveshare"
}
```

### Partition Schemes

Common partition layouts for 16MB flash:

| Scheme | App | SPIFFS/LittleFS | OTA |
|---|---|---|---|
| `default_16MB.csv` | 6.5MB | 6.5MB | No |
| `app3M_fat9M_16MB.csv` | 3MB | 9MB FAT | No |
| Custom OTA | 2x 4MB | 4MB | Yes |

## Toolchain

### esptool Commands

```bash
# Erase flash completely
esptool.py --chip esp32s3 erase_flash

# Flash firmware manually
esptool.py --chip esp32s3 --port COM3 --baud 921600 \
    write_flash -z 0x0 firmware.bin
```

### Serial Monitor

```bash
# PlatformIO monitor
pio device monitor -b 115200

# With filters (esp32 crash decoder)
pio device monitor -b 115200 -f esp32_exception_decoder
```

### OTA Updates

```ini
; platformio.ini addition
upload_protocol = espota
upload_port = 192.168.x.x
```

## ESP-IDF Under PlatformIO

### Supported Versions

| PlatformIO espressif32 | ESP-IDF Version | Arduino Core |
|---|---|---|
| pioarduino 51.x | ESP-IDF 5.1.x | Arduino 3.0.7 (recommended) |
| stock 6.x | ESP-IDF 4.4.x | Arduino 2.0.x (missing QSPI APIs) |
| stock 5.x | ESP-IDF 4.4.x | Arduino 2.0.x |

### menuconfig

```bash
pio run -t menuconfig
```

Key settings for this class of device:
- `Component config > ESP PSRAM` : Enable PSRAM, Octal mode
- `Component config > SPI Flash` : QIO mode, 80MHz
- `Component config > ESP32S3-specific` : USB CDC on boot

### ESP-IDF Components

Install components via `idf_component.yml` or PlatformIO lib:

```yaml
# idf_component.yml
dependencies:
  espressif/esp_lcd_st77916: "^2.0.2"
  lvgl/lvgl: "^8.4.0"
```

## Common Libraries

### LVGL (v8.4.0)

```ini
; platformio.ini
lib_deps =
    lvgl/lvgl@^8.4.0

build_flags =
    -DLV_CONF_INCLUDE_SIMPLE
    -DLV_TICK_PERIOD_MS=5
```

Key LVGL config (`lv_conf.h`):
- `LV_COLOR_DEPTH 16`
- `LV_COLOR_16_SWAP 1` (for SPI/QSPI displays)
- `LV_MEM_SIZE (64 * 1024)` (or use PSRAM: `LV_MEM_CUSTOM 1`)
- `LV_DISP_DEF_REFR_PERIOD 16` (60fps target)
- `LV_INPUT_DEV` : enable pointer + encoder

### Display Libraries

| Library | Notes |
|---|---|
| **ESP32_Display_Panel** | Espressif official, supports ST77916 QSPI natively |
| **LovyanGFX** | Community, fast, good QSPI support |
| **TFT_eSPI** | Popular but ST77916 QSPI support may require patches |

### Sensor/Peripheral Libraries

| Library | Version | Use |
|---|---|---|
| **SensorLib** | v0.3.1 | DRV2605 haptics, various sensors |
| **Adafruit DRV2605** | latest | Alternative DRV2605 driver |

## Flash / Upload

### USB Modes

| Mode | Description | Usage |
|---|---|---|
| **UART** | Via USB-UART bridge | Default upload, reliable |
| **USB-CDC** | Native USB on ESP32-S3 | Serial + upload, needs `ARDUINO_USB_CDC_ON_BOOT=1` |
| **USB-JTAG** | Native USB debug | JTAG debugging via USB |

### Download Mode (Manual)

1. Hold **BOOT** button (GPIO 0)
2. Press **RESET**
3. Release **BOOT**
4. Upload firmware

Some boards auto-enter download mode via DTR/RTS.

### Upload Baud Rates

- Default: 460800
- Fast: 921600
- Reliable fallback: 115200

## Debugging

### JTAG (Built-in USB)

```ini
; platformio.ini
debug_tool = esp-builtin
debug_init_break = tbreak setup
```

ESP32-S3 has built-in USB-JTAG — no external probe needed.

### Serial Debug Log Levels

```cpp
// Set in code
esp_log_level_set("*", ESP_LOG_VERBOSE);
esp_log_level_set("wifi", ESP_LOG_WARN);

// Or via menuconfig
// Component config > Log output > Default log verbosity
```

```ini
; platformio.ini
build_flags =
    -DCORE_DEBUG_LEVEL=4    ; 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose
```

### Common Debug Tips

- **Crash backtrace** : use `monitor_filters = esp32_exception_decoder`
- **PSRAM issues** : check `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`
- **Display blank** : verify QSPI pin order, check backlight GPIO, confirm init sequence
- **Touch not responding** : check I2C pullups, verify INT/RST GPIOs, scan I2C bus
- **USB CDC no output** : ensure `ARDUINO_USB_CDC_ON_BOOT=1`, wait for USB enumeration after reset
