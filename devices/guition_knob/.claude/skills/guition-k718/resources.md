# Resources — Guition JC3636K718

## Vendor (Guition)

| Resource | URL |
|---|---|
| Guition AliExpress store | https://guition.aliexpress.com/store/ |
| Product (typical listings) | Search "JC3636K718" on AliExpress or Guition resellers |
| Vendor ZIP shipped with the board | Contains schematics, Arduino + IDF demo, factory firmware (`9-Burn/Burn operation instructions/JC3636K718_V1.1.bin`) |

> Guition does **not** have a public wiki like Waveshare does. The vendor ZIP shipped with the board (and reposted in `docs/demo-code/` + `docs/instructions/` + `firmware/`) is the only authoritative source.

## Local Documentation (in this repo)

| Path | Contents |
|---|---|
| [`docs/datasheets/`](../../../docs/datasheets/) | ESP32-S3R8 datasheet, PCM5100A datasheet, ST77916 init `.INI` |
| [`docs/demo-code/Demo_arduino/`](../../../docs/demo-code/Demo_arduino/) | Vendor Arduino examples (display, encoder, touch, audio, RGB ring, SD…) |
| [`docs/demo-code/Demo_idf/`](../../../docs/demo-code/Demo_idf/) | Vendor ESP-IDF examples — note `main/led_strip/` for the RMT-based WS2812 driver and `main/device/pinconfig.h` for the authoritative pinout |
| [`docs/instructions/`](../../../docs/instructions/) | Vendor user manual + "Getting started" PDFs |
| [`docs/schematics/`](../../../docs/schematics/) | Schematics (PDF) — `JC3636K718.pdf` + `JC3636K718_P.pdf` |
| [`docs/dimensions/`](../../../docs/dimensions/) | Mechanical dimensions |
| [`firmware/`](../../../firmware/) | **Factory firmware** `JC3636K718_V1.1.bin` (12 MB merged @ 0x0) — observed working. Restore procedure in [`firmware/README.md`](../../../firmware/README.md) |

## IC Datasheets

| IC | Source |
|---|---|
| **ST77916** (LCD driver) | https://dl.espressif.com/AE/esp-iot-solution/ST77916_SPEC_V1.0.pdf — also Espressif component: https://components.espressif.com/components/espressif/esp_lcd_st77916 |
| **PCM5100APW** (audio DAC) | `docs/datasheets/pcm5100a.pdf` |
| **NS4150B** (speaker amp) | Generic Class-D amp, datasheet on Nsiway / Aliexpress vendor pages |
| **CST816** (touch, unused so far) | Identical part as the Waveshare Knob — datasheet in `devices/knob/docs/datasheets/CST816D_datasheet_En_V1.3.pdf` if needed |
| **WS2812** (RGB) | Standard part, WorldSemi datasheet widely available online |
| **ESP32-S3R8** (MCU) | `docs/datasheets/ESP32-S3R8_规格书.PDF` (Chinese) — Espressif EN version: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf |

## Libraries (used in current projects)

| Library | Use |
|---|---|
| **Espressif `esp_lcd_sh8601`** (shared via `shared/lib/qspi_panel/`) | QSPI panel framework — wraps the ST77916 init table from `guition_lcd_init.h` |
| **LVGL v8.3.x** (managed via PlatformIO) | UI framework (`Basic_LVGL_Meter` uses `lv_meter`) |
| **Adafruit NeoPixel** | RGB ring driver via Arduino — pulled only by projects that include `rgb_ring.h` (header-only wrapper) |
| **TalkiePCM** | LPC robotic TTS (`Basic_Audio_Talkie`) |

> See each project's `platformio.ini` for the exact `lib_deps`.

## Inter-Device Comparison

Three devices in this monorepo carry the **same ST77916 driver** but are otherwise distinct boards. Easy to confuse — keep the differences in mind:

| Aspect | Guition JC3636K718 (this) | Waveshare ESP32-S3-Knob-Touch-LCD-1.8 | Guition JC3636K518 / W518 (other knobs) |
|---|---|---|---|
| LCD CLK / CS | 11 / 12 | 13 / 14 | different again |
| Encoder A / B | 2 / 1 | 8 / 7 | different |
| I2C SDA / SCL | 9 / 10 | 11 / 12 | different |
| Touch INT / RST | 7 / 8 | 9 / 10 | different |
| SD CMD / CLK / D0–D3 | 38 / 39 / 40-41-48-47 | 3 / 4 / 5-6-42-2 | different |
| Audio mux (BOOT vs DAC select) | GPIO 0 = **WS2812 data** | GPIO 0 = audio mux (S3 vs ESP32) | varies |
| Haptics (DRV2605) | absent | present | depends on model |
| **RGB ring** | **present, 13 LEDs on GPIO 0** | absent | depends on model |
| Architecture | Single MCU | Dual MCU (S3 + ESP32) | varies |

References in this repo:
- Waveshare Knob skill: `devices/knob/.claude/skills/waveshare-knob/SKILL.md`
- Repo-root note about ST77916 sharing and possible factorization: `[[project-outstanding-followups]]` (memory)

## Community References

Mostly indirect — the K718 has a smaller online footprint than its siblings.

| Resource | Note |
|---|---|
| [nkinnan/Waveshare-ESP32-S3-Knob-Touch-LCD-1.8_and_Guition-K5-Knob-Series-JC3636K518](https://github.com/nkinnan/Waveshare-ESP32-S3-Knob-Touch-LCD-1.8_and_Guition-K5-Knob-Series-JC3636K518) | ESPHome config for the **K518** (not K718) — useful for the *shape* of an ESPHome integration, but pinout will differ |
| [waveshareteam/ESP32-display-support](https://github.com/waveshareteam/ESP32-display-support) | Generic ST77916 / SH8601 component code, reusable |
| [Espressif `esp_lcd_st77916` component](https://components.espressif.com/components/espressif/esp_lcd_st77916) | Native ST77916 panel component — alternative to the SH8601-as-generic-QSPI approach we currently use |
