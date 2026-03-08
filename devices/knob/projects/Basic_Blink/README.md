# Basic_Blink - Minimal Screen Blink

Validates the QSPI display pipeline. Alternates the full 360x360 screen between green and red every second.

## What it tests

- Display init via `knob_display.h` (one-liner: SPI bus, panel IO, ST77916, backlight)
- Strip-based drawing (360x36 px strips, DMA buffer)

## Build & flash

```powershell
.\build.ps1 knob Basic_Blink           # build
.\build.ps1 knob Basic_Blink -Upload   # build + flash
```
