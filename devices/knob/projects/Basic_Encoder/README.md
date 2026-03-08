# Basic_Encoder - Counter

Minimal encoder test: rotate the knob to increment/decrement a counter displayed as a signed 4-digit number (e.g. `+0042`, `-0007`), blue on black, centered on screen.

## What it tests

- LVGL v8.4 with double-buffered DMA flush
- Rotary encoder via `bidi_switch_knob` driver (timer-polled, not interrupt-based)
- Large text rendering (`montserrat_48` + zoom)

## Build & flash

```powershell
.\build.ps1 knob Basic_Encoder           # build
.\build.ps1 knob Basic_Encoder -Upload   # build + flash
```
