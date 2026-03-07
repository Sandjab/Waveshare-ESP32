# Test01 - Minimal Screen Blink

Validates the QSPI display pipeline. Alternates the full 360x360 screen between green and red every second.

## What it tests

- SPI bus init (QSPI, SPI2_HOST)
- ST77916 panel init via `esp_lcd_sh8601` driver + custom init sequence
- PWM backlight on GPIO 47
- Strip-based drawing (360x36 px strips, DMA buffer)

## Build & flash

```
cd projects/Test01
pio run                          # build
pio run -t upload --upload-port COMxx  # flash
```
