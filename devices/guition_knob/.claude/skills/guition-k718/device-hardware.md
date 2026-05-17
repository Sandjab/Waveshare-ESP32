# Device Hardware — Guition JC3636K718

## Single-MCU Architecture

Single ESP32-S3 with rich on-board peripherals — no secondary MCU (unlike the Waveshare Knob).

```
                ┌─────────────────────────────────────┐
                │          USB Type-C                  │
                │                                      │
                │  ┌─────────────────────────────┐     │
                │  │       ESP32-S3R8             │     │
                │  │       (Single MCU)           │     │
                │  │                              │     │
                │  │ • LCD (ST77916 QSPI)         │     │
                │  │ • Encoder (bidi switch)      │     │
                │  │ • Audio I2S → PCM5100A DAC   │     │
                │  │              → NS4150B amp   │     │
                │  │ • Microphone (I2S in)        │     │
                │  │ • SD (SDMMC 4-wire)          │     │
                │  │ • RGB ring (13× WS2812 GRB)  │     │
                │  │ • Touch (CST816 I2C, unused) │     │
                │  │ • WiFi + BLE 5               │     │
                │  └─────────────────────────────┘     │
                └─────────────────────────────────────┘
```

### ESP32-S3R8

- **SoC** : ESP32-S3R8 — Xtensa LX7 dual-core @ 240 MHz
- **Flash** : 16 MB external (Quad SPI)
- **PSRAM** : 8 MB (Octal SPI, integrated in the R8 package)
- **Connectivity** : WiFi 802.11 b/g/n (2.4 GHz) + Bluetooth 5 LE
- **USB** : Native USB-OTG (CDC/JTAG) — exposed once our firmware is flashed

---

## Complete Pinout

Authoritative source: [`lib/guition_knob_hw/guition_pins.h`](../../../lib/guition_knob_hw/guition_pins.h). Cross-checked against `docs/demo-code/Demo_idf/demo/main/device/pinconfig.h`.

### Display — ST77916 (QSPI)

| Signal | GPIO | Notes |
|---|---|---|
| CLK | 11 | QSPI clock |
| CS | 12 | Chip select (active low) |
| D0–D3 | 13, 14, 15, 16 | Data lines |
| RST | 17 | Hardware reset (active low) |
| TE | 18 | Tear-effect — **exposed on this board** (not on Waveshare Knob); currently unused in our projects |
| Backlight | 21 | PWM via LEDC (50 kHz, 8-bit, see `guition_display.h`) |

### Rotary Encoder

| Signal | GPIO | Notes |
|---|---|---|
| A (in code) | 1 | **GPIO swapped vs vendor silkscreen** (see below) |
| B (in code) | 2 | |

- **Not quadrature** — two independent contacts. Use the `bidi_switch_knob` driver in `lib/guition_knob_hw/` (timer-polled at 3 ms, debounced). Same driver as the Waveshare Knob.
- **Phases A/B swapped vs silkscreen / vendor `pinconfig.h`** : the vendor labels GPIO 2 = phase A and GPIO 1 = phase B, but with that mapping the `bidi_switch_knob` driver counts down on CW rotation (because the driver convention is A → `+1`, B → `-1`, calibrated against the Waveshare Knob). Confirmed by cross-test on 2026-05-17: with the swap, CW rotation increments correctly on both the Waveshare Knob and this Guition. The swap is applied in [`guition_pins.h`](../../../lib/guition_knob_hw/guition_pins.h) rather than in the driver, so the driver stays generic and shared across devices.

### Touch — CST816 (header-exposed)

| Signal | GPIO | Notes |
|---|---|---|
| SDA | 9 | I2C bus (shared with DRV2605 haptics) |
| SCL | 10 | I2C bus (shared with DRV2605 haptics) |
| INT | 7 | Touch interrupt |
| RST | 8 | Touch reset |

> Wired on the board but **not exercised** in any project we have today. The vendor demo code (`docs/demo-code/Demo_arduino/`) does use it — port from there if you need touch.

### Haptics — DRV2605 + LRA

| Signal | GPIO | Notes |
|---|---|---|
| SDA | 9 | I2C bus (shared with CST816 touch) |
| SCL | 10 | I2C bus (shared with CST816 touch) |

- I2C address: **0x5A** (DRV2605 standard, fixed)
- Motor: **LRA** (Linear Resonant Actuator) — visible on the schematic as `LRA_N` / `LRA_P` pads driven by the DRV2605.
- The schematic also defines `HAPTIC_TRIG` and `HAPTIC_EN` nets but **the vendor `pinconfig.h` does not assign GPIOs to them**, so for now they are best treated as "to be re-derived from the schematic" if a project needs hardware enable or PWM-trigger mode. The standard I2C "internal trigger" mode (Mode 4 etc.) should work without them.
- **The vendor demo does not exercise the haptics** — we initially documented the Guition as "no DRV2605", which was wrong. The IC is on the PCB.

### Audio I2S Output

| Signal | GPIO | Notes |
|---|---|---|
| BCLK | 3 | I2S bit clock |
| WS / LRCK | 45 | I2S word select |
| DOUT | 42 | I2S data out → PCM5100A |
| PA enable | 46 | HIGH = NS4150B amplifier ON. **Not** DAC mute (see audio chain below) |

### Microphone I2S Input

| Signal | GPIO |
|---|---|
| SCK | 5 |
| DATA | 4 |

### SD Card (SDMMC 4-wire)

| Signal | GPIO |
|---|---|
| CMD | 38 |
| CLK | 39 |
| D0 | 40 |
| D1 | 41 |
| D2 | 48 |
| D3 | 47 |

### Battery Monitor

| Signal | GPIO | Notes |
|---|---|---|
| Battery sense | 6 | DAC pin reused as ADC input — divider on board, formula not yet characterized |

### RGB Ring — 13× WS2812 (GRB)

| Signal | GPIO | Notes |
|---|---|---|
| Data | 0 | **Also BOOT strap pin** — see gotcha below |

- LED count : 13, color order : **GRB** (NEO_GRB)
- Driver wrapper : [`rgb_ring.h`](../../../lib/guition_knob_hw/rgb_ring.h) — header-only around `Adafruit_NeoPixel` (Arduino). Native ESP-IDF reference using RMT + `led_strip` component is in `docs/demo-code/Demo_idf/demo/main/led_strip/`.
- Brightness default : 64/255 in `rgb_ring_init()` — at full white, 13 LEDs ≈ 780 mA, well above the USB 500 mA budget. Don't crank to 255 on bus power.

---

## ICs and Interfaces

| IC | Function | Interface | Details |
|---|---|---|---|
| **ST77916** | LCD driver | QSPI (4-wire data) | 360×360, IPS, with TE output |
| **PCM5100APW** | Stereo audio DAC | I2S | 32-bit/384 kHz, `XSMT` tied to 3V3 (always unmuted) |
| **NS4150B** | Mono Class-D speaker amp | analog in from DAC | Enable on GPIO 46; output drives onboard speaker |
| **CST816** | Capacitive touch | I2C | Header-wired, unused in our projects so far |
| **DRV2605LDGSR** | Haptic driver | I2C (0x5A) | Drives an on-board LRA (linear resonant actuator). Same shared I2C bus as CST816. Unused by the vendor demo. |
| **WS2812** ×13 | Addressable RGB LEDs | 1-wire data (GPIO 0) | GRB color order |

---

## Display — ST77916 Details

- **Resolution** : 360 × 360 px (round usable area, square panel)
- **Panel** : IPS color
- **Interface** : QSPI (not standard SPI, not RGB parallel)
- **Driver framework** : the Espressif `esp_lcd_sh8601` panel component is used as a generic QSPI host with an ST77916-specific init command table in [`guition_lcd_init.h`](../../../lib/guition_knob_hw/guition_lcd_init.h).
- **Init sequence** : verified byte-identical (181 / 185 entries) to the Waveshare Knob's `knob_lcd_init.h`. The 4 diverging entries are control opcodes with no parameters — net behavior is the same. Candidate for factorization into `shared/lib/qspi_panel/` if a 3rd ST77916 device appears.
- **TE pin** : on GPIO 18, available on this board but not on the Waveshare Knob. Currently not used (LVGL flush relies on DMA done callback).
- **Backlight** : LEDC PWM on GPIO 21 (50 kHz, 8-bit, default duty 255).
- **One-liner init** : `guition_display_init()` (raw) or `guition_lvgl_init(buf_height = 36)` (LVGL with double-buffered DMA + rounder + 2 ms tick). See `lib/guition_knob_hw/guition_display.h` and `guition_lvgl.h`.

---

## Audio Chain

```
ESP32-S3 ─I2S─▶ PCM5100A ──▶ NS4150B ─▶ Onboard speaker
                  DAC      (amp,         (3.5 mm jack detect switch
                            GPIO 46)      auto-cuts speaker when
                                          headphones inserted)
                            │
                            └──▶ 3.5 mm jack (line out from DAC,
                                  bypasses amp)
```

Key points :

- **PCM5100A `XSMT` is pulled to 3V3 on the board** — the DAC is **always unmuted**. Software muting via DAC pin is not possible.
- **GPIO 46 enables the speaker amplifier (NS4150B), not the DAC**. HIGH = amp ON, LOW = silent (speaker only).
- **Jack detect (CN3 — PJ-342)** : inserting headphones flips a mechanical switch on the jack that **opens the NS4150B input path**, so the speaker is automatically cut. The headphones receive the line-out level directly from the DAC.
- **Haptics ARE present** (DRV2605 + LRA on the shared I2C bus, I2C address 0x5A). The vendor demo does not use them, so an early version of this skill mistakenly listed "no DRV2605". Confirmed against `docs/schematics/JC3636K718.pdf` (nets `DRV2605LDGSR`, `LRA_N`, `LRA_P`, `HAPTIC_SDA`, `HAPTIC_SCL`).

---

## Factory USB Anomaly (first flash)

The board ships running the **Guition vendor firmware**, which exposes a custom USB device — **not** a serial CDC port :

- `VID:PID = 303A:4001` (sometimes `4002`)
- Device strings: `ESP USB DEVICE` / `N7 Workshop`
- Probably HID-class for driving the on-screen menu

Consequences :

- `pio device list` shows nothing usable
- `build.sh` / `build.ps1` auto-detect (looking for `303A:1001` Espressif CDC) fails
- `esptool` cannot DTR/RTS-reset into download mode

Workaround — force the ROM bootloader manually :

1. Hold **BOOT** button (GPIO 0)
2. Plug USB (or press **RESET** briefly if already plugged)
3. Release BOOT

The board then re-enumerates as `VID:PID = 303A:1001` (`USB JTAG/serial debug unit`) with a `/dev/cu.usbmodem*` (or `COM*`) port, and `./build.sh guition_knob <project> --upload` works normally.

After our firmware (built with `-DARDUINO_USB_CDC_ON_BOOT=1`) is in place, CDC stays exposed across resets — the BOOT dance is **only needed the very first time** (and to go back to vendor firmware, see [`firmware/README.md`](../../../firmware/README.md)).

> The vendor firmware also has a **USB Mass Storage mode** (~503 MB FAT32 volume). It is **not** the default at boot — there is an on-screen menu entry ("reboot to MSC") that switches into it. So we don't trip over it accidentally.

---

## Power

| Source | Voltage | Connector |
|---|---|---|
| USB Type-C | 5 V | USB-C port |
| Li-ion battery | 3.7–4.2 V | On-board connector |

Battery voltage is sensed on GPIO 6 (DAC pin reused as ADC input). The exact divider ratio and conversion formula are not yet characterized in our code — check the schematic `docs/schematics/JC3636K718.pdf` if a project needs to report battery %.

---

## Important Caveats

- **Same display IC as the Waveshare Knob (ST77916), but entirely different pinout.** Don't reuse the Waveshare `knob_pins.h` or `knob_lcd_init.h` paths from a Guition project (init sequence is byte-identical but pins differ).
- **Not a Guition K5 / JC3636K518 / JC3636W518.** These are sibling Guition models with different pinouts (the community has more material on the K518; references to "Guition Knob" online often mean K518, not our K718).
- **GPIO 0 dual role** : BOOT strap + WS2812 data. WS2812 idle = LOW, so as long as `rgb_ring_init()` is called from `setup()` (post-boot), there is no conflict. Don't drive GPIO 0 from a pre-boot strap source.
- **No haptics, no Waveshare USB switch** : projects ported from the Knob that rely on DRV2605 or on the CH445P USB orientation switch don't transfer 1:1.
- **TE pin exposed** : if a project needs tear-free updates, we already have GPIO 18 wired — the Waveshare Knob doesn't.
