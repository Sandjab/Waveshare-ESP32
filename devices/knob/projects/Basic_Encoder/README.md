# Basic_Encoder - Hue Wheel

Rotate the encoder to sweep through HSV hues. The full screen updates via LVGL, with a center dot showing the hex color code and a haptic tick on each step.

## What it tests

- LVGL v8.4 with double-buffered DMA flush
- Rotary encoder (GPIO interrupt, half-quad decoding)
- DRV2605 haptic feedback (LRA, Strong Click waveform)
- Auto-contrast text (white on dark backgrounds, black on light)

## Build & flash

```
cd projects/Basic_Encoder
pio run                          # build
pio run -t upload --upload-port COMxx  # flash
```
