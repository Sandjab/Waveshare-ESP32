# Device Hardware — Waveshare ESP32-S3-Knob-Touch-LCD-1.8

## Dual-MCU Architecture

This device has two independent MCUs on a single board:

```
                    ┌─────────────────────────────┐
                    │       USB Type-C             │
                    │     ┌───────────┐            │
                    │     │  CH445P   │            │
                    │     │ USB Switch│            │
                    │     └─┬───────┬─┘            │
                    │       │       │              │
               ┌────┴───────┴┐  ┌──┴──────────┐   │
               │  ESP32-S3R8 │  │ ESP32-U4WDH │   │
               │  (Primary)  │  │ (Secondary) │   │
               │             │  │             │   │
               │ • Display   │  │ • BT Classic│   │
               │ • Touch     │  │ • Encoder 2 │   │
               │ • Encoder 1 │  │             │   │
               │ • Haptics   │  │  PCM5100A   │   │
               │ • Mic       │  │  DAC (muxed │   │
               │ • SD Card   │  │  GPIO0 sel) │   │
               │ • WiFi+BLE  │  │             │   │
               └─────────────┘  └─────────────┘   │
                    └─────────────────────────────┘
```

### ESP32-S3R8 (Primary MCU)

- **SoC** : ESP32-S3R8 — Xtensa LX7 dual-core @ 240 MHz
- **Flash** : 16 MB external (Quad SPI)
- **PSRAM** : 8 MB (Octal SPI, integrated in R8 package)
- **Connectivity** : WiFi 802.11 b/g/n (2.4 GHz) + Bluetooth 5 LE
- **Peripherals managed** : display, touch, rotary encoder, haptics, microphone, SD card, battery ADC
- **USB** : Native USB-OTG (CDC/JTAG)

### ESP32-U4WDH (Secondary MCU)

- **SoC** : ESP32-U4WDH — Xtensa LX6 dual-core @ 240 MHz
- **Flash** : 4 MB (integrated in package)
- **Connectivity** : WiFi + Bluetooth Classic (A2DP) + BLE
- **Peripherals managed** : PCM5100A DAC (I2S), 2nd rotary encoder
- **Primary use case** : Bluetooth audio sink/source, speaker output

### CH445P USB Switch

- 4P2T analog switch for USB Type-C
- Selects which MCU is connected to USB based on plug orientation
- Allows flashing/monitoring either MCU without hardware jumpers
- **ESP32-S3 side**: VID:PID = `303A:1001` (Espressif USB CDC)
- **ESP32 side**: VID:PID = `1A86:7523` (CH340 UART bridge)
- COM port changes on each replug — always specify `--upload-port COMxx`
- PermissionError after flash is normal (USB re-enumerates during hard reset)

### Inter-MCU Communication — UART

| Signal | ESP32-S3 GPIO | ESP32 GPIO | Direction |
|---|---|---|---|
| TX → ESP32 | 43 | 16 (RX) | S3 → ESP32 |
| RX ← ESP32 | 44 | 17 (TX) | ESP32 → S3 |

Standard UART link between the two MCUs. No hardware flow control lines observed on schematic.

---

## Complete Pinout — ESP32-S3

### Display — ST77916 (QSPI)

| Signal | GPIO | Notes |
|---|---|---|
| CLK | 13 | QSPI clock, 80 MHz |
| D0 (SIO0) | 15 | Data line 0 |
| D1 (SIO1) | 16 | Data line 1 |
| D2 (SIO2) | 17 | Data line 2 |
| D3 (SIO3) | 18 | Data line 3 |
| CS | 14 | Chip select (active low) |
| RST | 21 | Hardware reset (active low) |
| Backlight | 47 | PWM via LEDC channel |

### Touch — CST816 (I2C address 0x15)

| Signal | GPIO | Notes |
|---|---|---|
| SDA | 11 | I2C data (shared bus) |
| SCL | 12 | I2C clock, 300 kHz (shared bus) |
| INT | 9 | Interrupt (active low), polled in demo |
| RST | 10 | Hardware reset (active low) |

### Haptics — DRV2605

| Signal | GPIO | Notes |
|---|---|---|
| SDA | 11 | I2C shared with touch |
| SCL | 12 | I2C shared with touch |

- I2C address: **0x5A** (fixed, not configurable)
- Motor type: **LRA** (Linear Resonant Actuator)

### Rotary Encoder (Primary)

| Signal | GPIO |
|---|---|
| A (CLK) | 8 |
| B (DT) | 7 |

### Microphone (PDM) — MSM261D3526H1CPM

| Signal | GPIO | Notes |
|---|---|---|
| CLK | 45 | PDM clock |
| DATA | 46 | PDM data |

> PDM mode uses only CLK + DATA (2 pins). Demo code confirms GPIO 45/46 only.

### SD Card (SDMMC 4-wire)

| Signal | GPIO | Notes |
|---|---|---|
| CMD | 3 | Command line |
| CLK | 4 | Clock |
| D0 | 5 | Data line 0 |
| D1 | 6 | Data line 1 |
| D2 | 42 | Data line 2 |
| D3 | 2 | Data line 3 |

> Confirmed by demo code (`02_SD_Card`). Uses `SDMMC_FREQ_HIGHSPEED` (40 MHz), 4-bit native SDMMC host.

### Audio I2S Output (ESP32-S3 → PCM5100A)

| Signal | GPIO | Notes |
|---|---|---|
| BCLK | 39 | I2S bit clock |
| WS/LRCK | 40 | I2S word select |
| DOUT | 41 | I2S data out |

> Confirmed by demo code (`07_Audio_Test`). No MCLK needed (PCM5100A has internal PLL).
> GPIO 0 set HIGH selects ESP32-S3 as DAC controller (audio mux). The ESP32 secondary also has I2S lines to the DAC (see secondary pinout) — GPIO 0 switches between the two.

### Other

| Signal | GPIO | Notes |
|---|---|---|
| Battery ADC | GPIO 1 (ADC1_CH0) | Divider 10K/10K → V_bat = ADC × 2 × 3.3/4095 |
| Audio mux / BOOT | GPIO 0 | OUTPUT HIGH = S3 controls PCM5100A; also BOOT button |

### Inter-MCU UART (ESP32-S3 side)

| Signal | GPIO | Notes |
|---|---|---|
| TX → ESP32 | 43 | UART to secondary MCU |
| RX ← ESP32 | 44 | UART from secondary MCU |

### Remaining Unknowns

- **Audio amplifier IC** : visible on schematic but reference illegible (possibly MAX98357 or SGM8903)
- **PCM5100A XSMT** : soft mute pin destination (ESP32 GPIO or fixed pull-up?) not identified
- **LED software-controllable** : LED4 is charge indicator driven by charger IC — no software-controllable status LED found
- **SD card detect** : socket SWITCH pin exists but GPIO not identified

---

## Pinout — ESP32 (Secondary)

> These pins are on the ESP32-U4WDH, **not** the ESP32-S3. From schematic analysis (not code-confirmed — no ESP32 demo code available).

| Function | Signal | GPIO | Notes |
|---|---|---|---|
| PCM5100A I2S | BCK | 5 | I2S bit clock |
| | WS/LRCK | 25 | I2S word select |
| | DATA | 26 | I2S data out |
| Encoder 2 | A | 18 | Second rotary encoder |
| | B | 19 | Second rotary encoder |
| UART to S3 | TX | 17 | Inter-MCU UART |
| | RX | 16 | Inter-MCU UART |
| BOOT | GPIO0 | 0 | Enter download mode |

---

## ICs and Interfaces

| IC | Function | Interface | Details |
|---|---|---|---|
| **ST77916** | LCD driver | QSPI (4-wire data) | 360x360, 262K colors, 600 cd/m² |
| **CST816** | Capacitive touch | I2C (0x15) | Single-point, gesture support |
| **DRV2605LDGS** | Haptic driver | I2C (0x5A) | LRA motor, 123 waveform effects |
| **PCM5100APW** | Stereo DAC | I2S | 32-bit/384kHz, separate 3V3_DAC rail, muxed between both MCUs |
| **MSM261D3526H1CPM** | MEMS PDM mic | I2S PDM | On ESP32-S3 |
| **CH445P** | USB analog switch | — | 4P2T, selects MCU via USB-C orientation |

---

## Display — ST77916 Details

### Specifications

- **Size** : 1.8" diagonal
- **Resolution** : 360 x 360 pixels (round area usable, square panel)
- **Panel** : IPS, 262K colors (18-bit RGB666)
- **Brightness** : 600 cd/m²
- **Interface** : QSPI (not standard SPI, not RGB parallel)
- **Clock** : up to 80 MHz
- **Driver IC** : ST77916

### ESP-IDF / Driver Notes

> **Important:** The Waveshare demo code uses the `esp_lcd_sh8601` driver (a generic QSPI panel driver) with a custom ST77916-specific init command table (~80 register writes). Do NOT use the default SH8601 init — the custom Waveshare init sequence is required.

- **Color depth** : 16-bit RGB565 with byte swap (`LV_COLOR_16_SWAP 1`)
- **Color order** : RGB, may need `esp_lcd_panel_invert_color(panel, true)`
- **QSPI mode** : SPI2_HOST, 4 data lines (D0-D3), `quad_mode = true`, `lcd_cmd_bits = 32`
- **Backlight** : PWM on GPIO 47 via LEDC (timer 3, 50 kHz, 8-bit resolution, channel 1)
- **LVGL buffer** : double-buffered DMA, 360 × 36 lines (V_RES/10), 2-pixel alignment required (rounder callback)
- **LVGL tick** : 2 ms via `esp_timer`
- **Rotation** : write 0x60 to register 0x36 for 90-degree rotation

---

## Power

### Sources

| Source | Voltage | Connector |
|---|---|---|
| USB Type-C | 5V | USB-C port |
| Li-ion battery | 3.7–4.2V | MX1.25 2-pin connector |

### Battery

- Capacity: 800 mAh (optional, not included)
- Charging: integrated charger IC (charges from USB)
- Monitoring: GPIO 1 (ADC1_CH0) via voltage divider (R62=10K / R63=10K, ratio 1:1)
- Formula: V_bat = ADC_reading × 2 × (3.3 / 4095)

### Power Notes

- Device can run on USB only (no battery required)
- Battery voltage readable via ADC for battery percentage estimation
- No hardware power switch — controlled via USB connection or software deep sleep

---

## Important Caveats

### Guition JC3636W518 vs Waveshare

The Guition JC3636W518 is a similar device based on the same display/MCU but with **different pin assignments**:
- Guition touch I2C: GPIO 7/8 (vs Waveshare GPIO 11/12)
- Other pins may differ

Do NOT use Guition pinouts for this Waveshare board. Always verify which board the code/config targets.

### I2C Bus Sharing

GPIO 11 (SDA) and GPIO 12 (SCL) are shared between:
- CST816 touch controller
- DRV2605 haptic driver

Both devices are on the same I2C bus. Ensure proper bus arbitration and no address conflicts (CST816 and DRV2605 have different addresses).
