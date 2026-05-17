# Waveshare ESP32-S3-Touch-AMOLED-1.75C

Round 1.75" AMOLED watch-form-factor dev board with CO5300 QSPI display (466×466), CST9217 touch, ES8311 audio codec + ES7210 echo-cancellation AEC, AXP2101 PMIC, QMI8658 6-axis IMU, dual-microphone array, and an on-board speaker. ESP32-S3R8 (16 MB Flash, 8 MB Octal PSRAM).

## Skill

For hardware details, pinout, and conventions, invoke the `waveshare-amoled-175c` skill (entry: `.claude/skills/waveshare-amoled-175c/SKILL.md`).

## Repo Structure

```
devices/amoled_175c/
├── .claude/skills/waveshare-amoled-175c/   # Device skill
├── lib/amoled_175c_hw/                     # Pin definitions (display + I2S helpers TBD)
├── projects/                               # PlatformIO projects (empty)
├── firmware/                               # Factory firmware archive
└── docs/
    ├── schematics/                         # Schematic PDF from vendor repo
    ├── datasheets/                         # (to be added: CO5300, CST9217, ES7210, AXP2101…)
    └── demo-code/                          # (to be added from waveshareteam repo when needed)
```

Shared code may live in `../../shared/lib/` in the future — the CO5300 panel will likely need a new driver (the existing `esp_lcd_sh8601` is for SH8601, used by AMOLED 1.8). Decide on factorization when we get a second CO5300-based device.

## Gotchas

- **`PIN_LCD_RST` and `PIN_TP_RST` both = GPIO 2** in the vendor `pin_config.h`. Unusual but probably intentional (coordinated reset of LCD + touch). Validate against `docs/schematics/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf` before relying on either as an independent reset.
- **Audio I2S bus is shared** between ES8311 (playback codec, DOUT GPIO 8) and ES7210 (mic-array AEC, DIN GPIO 10). MCK / BCK / WS are common. Mind the bus arbitration when configuring I2S in software.
- **No SD card** on this board, unlike the AMOLED 1.8 — projects ported from there need to drop SDMMC.
- **No 3.5 mm jack** — audio output goes directly to the on-board speaker via `PIN_PA_EN` (GPIO 46).
- **Different display driver** than the AMOLED 1.8 (CO5300 vs SH8601) — no shared init / driver yet.

## Next steps

- First PlatformIO project (e.g. `Basic_Blink`) once we have a CO5300 driver wired into the monorepo. The Waveshare demo code (`waveshareteam/ESP32-S3-Touch-AMOLED-1.75C`, `examples/Arduino-v3.3.5`) is the natural starting point — its `01_HelloWorld` uses `Arduino_GFX_Library` with an `Arduino_CO5300` class.
- Factory firmware (`ESP32-S3-Touch-AMOLED-1.75C-FactoryOnly-260114.bin`, ~33.5 MB) is archived in [`firmware/`](firmware/) with the restore procedure in [`firmware/README.md`](firmware/README.md).
