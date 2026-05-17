# Device Hardware — Waveshare ESP32-S3-Touch-AMOLED-1.75C

## Single-MCU Architecture

Single ESP32-S3 with rich on-board peripherals managed via the shared I2C bus and a single I2S bus for audio in + out.

```
                ┌─────────────────────────────────────┐
                │          USB Type-C                  │
                │                                      │
                │  ┌─────────────────────────────┐     │
                │  │       ESP32-S3R8             │     │
                │  │       (Single MCU)           │     │
                │  │                              │     │
                │  │ • AMOLED (CO5300 QSPI 466²)  │     │
                │  │ • Touch (CST9217 I2C)        │     │
                │  │ • Speaker (ES8311 + PA)      │     │
                │  │ • Mic array (ES7210 AEC)     │     │
                │  │ • PMIC (AXP2101)             │     │
                │  │ • IMU (QMI8658)              │     │
                │  │ • 2 side buttons (PWR/BOOT)  │     │
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

Authoritative source: [`lib/amoled_175c_hw/amoled_175c_pins.h`](../../../lib/amoled_175c_hw/amoled_175c_pins.h). Cross-checked against the vendor `pin_config.h` shipped in the Waveshare Arduino demo on GitHub (`waveshareteam/ESP32-S3-Touch-AMOLED-1.75C`).

### Display — CO5300 (QSPI)

| Signal | GPIO | Notes |
|---|---|---|
| CLK | 38 | QSPI clock |
| CS | 12 | Chip select |
| D0–D3 | 4, 5, 6, 7 | Data lines |
| RST | 2 | **Shared with touch RST** per vendor pinconfig — see Caveats |

### Touch — CST9217

| Signal | GPIO | Notes |
|---|---|---|
| SDA | 15 | I2C bus (shared) |
| SCL | 14 | I2C bus (shared) |
| INT | 11 | Touch interrupt |
| RST | 2 | Shared with LCD RST |

### Audio I2S (shared bus, ES8311 + ES7210)

| Signal | GPIO | Notes |
|---|---|---|
| MCLK | 16 | Master clock (common to both codecs) |
| BCLK | 9 | Bit clock (common) |
| WS / LRCK | 45 | Word select (common) |
| DOUT | 8 | Speaker out (ES8311 playback) |
| DIN | 10 | Mic array in (ES7210 AEC) |
| PA enable | 46 | HIGH = speaker amp ON |

### I2C bus (shared)

All I2C peripherals on SDA:15 / SCL:14 — exact addresses to confirm at first I2C scan :

| Device | Typical address | Function |
|---|---|---|
| CST9217 | varies (CST family ~0x15) | Capacitive touch |
| AXP2101 | 0x34 | PMIC |
| QMI8658 | 0x6B | 6-axis IMU |
| ES8311 | 0x18 | Playback codec ctrl |
| ES7210 | 0x40 | Mic-array AEC ctrl |

---

## ICs and Interfaces

| IC | Function | Interface | Notes |
|---|---|---|---|
| **CO5300** | AMOLED driver | QSPI | 466×466 round; no driver in `shared/lib/` yet — derive from `Arduino_CO5300` |
| **CST9217** | Capacitive touch | I2C | Different family from CST816 (used on Knob/Guition) and FT3168 (used on AMOLED 1.8) |
| **ES8311** | Audio playback codec | I2S + I2C | Same chip as AMOLED 1.8 |
| **ES7210** | Mic-array echo-cancellation AEC | I2S + I2C | New on this board (vs AMOLED 1.8) — handles the dual-mic array |
| **AXP2101** | PMIC | I2C | Same as AMOLED 1.8 |
| **QMI8658** | 6-axis IMU | I2C | Same as AMOLED 1.8 |

---

## Key Differences vs other monorepo devices

| Aspect | AMOLED 1.75C (this) | AMOLED 1.8 | Knob / Guition |
|---|---|---|---|
| Display IC | CO5300 (round 466²) | SH8601 (368×448) | ST77916 (360×360) |
| Touch IC | CST9217 | FT3168 | CST816 |
| Audio chain | ES8311 + ES7210 AEC + speaker | ES8311 + speaker | PCM5100A DAC → jack (Knob) or speaker (Guition) |
| Mics | Dual array | Single | Single (Guition only) |
| I/O expander | None | XCA9554 (drives LCD reset + SD power) | None |
| SD card | **None** | SDMMC 1-wire | SDMMC 4-wire |
| 3.5 mm jack | **None** (speaker only) | None | Yes (Knob) / No (Guition) |
| LCD RST | Direct GPIO 2 (shared with touch RST) | Via XCA9554 | Direct GPIO |
| Encoder | None | None | Yes |
| RGB ring | None | None | Yes (Guition only) |
| Haptics | None mentioned | None | DRV2605 (both Knob and Guition) |

---

## Caveats

- **`PIN_LCD_RST` and `PIN_TP_RST` both map to GPIO 2** in the vendor `pin_config.h`. Verify on the schematic at [`docs/schematics/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf`](../../../docs/schematics/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf) whether this is a deliberate coordinated reset or a vendor typo — before relying on either signal independently.
- **Audio I2S is shared between two codecs** (ES8311 playback + ES7210 AEC). Software setup must time-multiplex correctly. The Waveshare demos `06_ES7210` and `07_ES8311` are the natural reference.
- **No SD card** — projects ported from the AMOLED 1.8 that use `SDMMC` must drop those calls.
- **No 3.5 mm jack** — `PIN_PA_EN` (GPIO 46) controls the on-board speaker amp directly.
- **Round display (466×466)** — UIs should respect the circular visible area; corners of the square frame may be partially clipped.
