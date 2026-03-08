# Hue_Encoder - HSV Color Wheel

Rotate the knob to cycle through HSV hues — the full screen fills with the current color, with a center dot showing the hex code (e.g. `#FF8000`). Auto-contrast switches text between white and black based on perceived brightness. Tap the screen to toggle haptic feedback on/off.

## What it tests

- LVGL v8.4 via `knob_lvgl.h` (one-liner init with double-buffered DMA flush)
- Rotary encoder via `bidi_switch_knob` driver (timer-polled, not interrupt-based)
- DRV2605 haptics (LRA mode, library 6)
- CST816 touch (raw I2C polling for tap-to-toggle)
- Auto-contrast text (BT.601 perceived brightness)

## Build & flash

```powershell
.\build.ps1 knob Hue_Encoder           # build
.\build.ps1 knob Hue_Encoder -Upload   # build + flash
```
