# Device Hardware — Waveshare ESP32-S3-Touch-AMOLED-1.8

## Single-MCU Architecture

Unlike the Knob (dual-MCU), this device uses a single ESP32-S3 with rich peripherals managed via I2C.

```
                ┌─────────────────────────────────────┐
                │          USB Type-C                  │
                │                                      │
                │  ┌─────────────────────────────┐     │
                │  │       ESP32-S3R8             │     │
                │  │       (Single MCU)           │     │
                │  │                              │     │
                │  │ • AMOLED (SH8601 QSPI)      │     │
                │  │ • Touch (FT3168 I2C)         │     │
                │  │ • Audio (ES8311 codec)       │     │
                │  │ • Power (AXP2101 PMIC)       │     │
                │  │ • IMU (QMI8658)              │     │
                │  │ • RTC (PCF85063)             │     │
                │  │ • I/O expander (XCA9554)     │     │
                │  │ • SD card (SDMMC 1-wire)     │     │
                │  │ • WiFi + BLE 5               │     │
                │  └─────────────────────────────┘     │
                └─────────────────────────────────────┘
```

### ESP32-S3R8

- **SoC** : ESP32-S3R8 — Xtensa LX7 dual-core @ 240 MHz
- **Flash** : 16 MB external (Quad SPI)
- **PSRAM** : 8 MB (Octal SPI, integrated in R8 package)
- **Connectivity** : WiFi 802.11 b/g/n (2.4 GHz) + Bluetooth 5 LE
- **USB** : Native USB-OTG (CDC/JTAG)

---

## Complete Pinout — ESP32-S3

### Display — SH8601 (QSPI AMOLED)

| Signal | GPIO | Notes |
|---|---|---|
| CLK | 11 | QSPI clock |
| D0 (SIO0) | 4 | Data line 0 |
| D1 (SIO1) | 5 | Data line 1 |
| D2 (SIO2) | 6 | Data line 2 |
| D3 (SIO3) | 7 | Data line 3 |
| CS | 12 | Chip select (active low) |

> **No RST pin** — display reset is handled via the XCA9554 I/O expander.
> **No backlight pin** — AMOLED is self-emitting (brightness via SH8601 commands).

### Touch — FT3168 (I2C address 0x38)

| Signal | GPIO | Notes |
|---|---|---|
| SDA | 15 | I2C data (shared bus) |
| SCL | 14 | I2C clock (shared bus) |
| INT | 21 | Interrupt (active low) |

### Audio — ES8311 Codec

| Signal | GPIO | Notes |
|---|---|---|
| MCLK | 16 | Master clock |
| BCK | 9 | I2S bit clock |
| WS | 45 | I2S word select |
| DI | 10 | I2S data in (microphone) |
| DO | 8 | I2S data out (speaker) |
| PA | 46 | Power amplifier enable |

- I2C control address: **0x18** (on shared I2C bus)
- Supports both speaker output and microphone input

### SD Card (SDMMC 1-wire)

| Signal | GPIO | Notes |
|---|---|---|
| CLK | 2 | Clock |
| CMD | 1 | Command |
| D0 | 3 | Data line 0 |

> **1-wire mode only** (not 4-wire like the Knob). SD power is controlled via XCA9554 expander pin 7.

### I2C Bus (shared)

All I2C peripherals share GPIO 15 (SDA) / GPIO 14 (SCL):

| Device | I2C Address | Function |
|---|---|---|
| FT3168 | 0x38 | Capacitive touch |
| AXP2101 | 0x34 | Power management IC |
| QMI8658 | 0x6B | 6-axis IMU (accel + gyro) |
| PCF85063 | 0x51 | Real-time clock |
| ES8311 | 0x18 | Audio codec (control) |
| XCA9554 | 0x20 | I/O expander |

---

## ICs and Interfaces

| IC | Function | Interface | Details |
|---|---|---|---|
| **SH8601** | AMOLED driver | QSPI (4-wire data) | 368x448, self-emitting |
| **FT3168** | Capacitive touch | I2C (0x38) | Single/multi-point |
| **ES8311** | Audio codec | I2S + I2C (0x18) | Speaker + mic, with PA enable |
| **AXP2101** | Power management | I2C (0x34) | Multi-rail PMIC, battery charger, ADC |
| **QMI8658** | 6-axis IMU | I2C (0x6B) | Accelerometer + gyroscope |
| **PCF85063** | Real-time clock | I2C (0x51) | Low-power RTC with alarm |
| **XCA9554** | I/O expander | I2C (0x20) | 8-bit GPIO: LCD ctrl, SD power, PMU INT |

---

## Display — SH8601 Details

### Specifications

- **Size** : 1.8" diagonal
- **Resolution** : 368 x 448 pixels (rectangular)
- **Panel** : AMOLED (self-emitting, no backlight needed)
- **Interface** : QSPI (4-wire data)
- **Driver IC** : SH8601

### Driver Notes

- Uses `Arduino_SH8601` from Arduino_GFX library in demo code
- Same QSPI panel framework as Knob (`shared/lib/qspi_panel/esp_lcd_sh8601`)
- Brightness control via display commands (not PWM backlight)
- Color depth: 16-bit RGB565
- Reset handled via XCA9554 I/O expander (not direct GPIO)

---

## XCA9554 I/O Expander

| Pin | Direction | Function |
|---|---|---|
| P0 | Output | LCD control |
| P1 | Output | LCD control |
| P2 | Output | LCD control |
| P4 | Input | Backlight / display status |
| P5 | Input | PMU interrupt (AXP2101) |
| P7 | Output | SD card power control |

---

## Power — AXP2101

### Features

- Multi-output PMIC with integrated battery charger
- ADC for battery voltage, current, temperature monitoring
- Multiple LDO and DCDC outputs for subsystem power
- Interrupt output via XCA9554 P5

### Sources

| Source | Voltage | Connector |
|---|---|---|
| USB Type-C | 5V | USB-C port |
| Li-ion battery | 3.7–4.2V | On-board connector |

---

## Key Differences from Knob

| Aspect | Knob | AMOLED |
|---|---|---|
| Architecture | Dual-MCU (ESP32-S3 + ESP32) | Single-MCU (ESP32-S3) |
| Display | IPS LCD 360x360 (ST77916) | AMOLED 368x448 (SH8601) |
| Display reset | Direct GPIO 21 | Via XCA9554 expander |
| Backlight | PWM GPIO 47 | None (AMOLED self-emitting) |
| Touch | CST816 (0x15) | FT3168 (0x38) |
| Audio | PCM5100A DAC (no mic codec) | ES8311 full codec (speaker + mic) |
| Power management | Basic charger IC | AXP2101 PMIC |
| IMU | None | QMI8658 6-axis |
| RTC | None | PCF85063 |
| Encoder | Rotary encoder | None |
| Haptics | DRV2605 + LRA | None |
| SD card | SDMMC 4-wire | SDMMC 1-wire |
| USB switch | CH445P (dual-MCU) | Direct (single-MCU) |
