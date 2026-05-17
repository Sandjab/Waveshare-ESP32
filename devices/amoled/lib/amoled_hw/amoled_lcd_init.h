#pragma once

#include "esp_lcd_sh8601.h"

// SH8601 init sequence for the Waveshare ESP32-S3-Touch-AMOLED-1.8.
//
// Verbatim copy of Waveshare's working ESP-IDF init (from
// docs/demo-code/ESP-IDF/05_LVGL_WITH_RAM/main/example_qspi_with_ram.c),
// minus DISPON (0x29) which esp_lcd_panel_disp_on_off(panel, true)
// will issue from amoled_display_init() after this table.
//
// We need this *custom* init because the vendor_specific_init_default
// that ships inside the shared esp_lcd_sh8601 driver:
//   - omits SLPOUT (0x11), so the panel stays asleep after reset and
//     the screen never lights up,
//   - and doesn't set CASET / RASET explicitly to 0..367 / 0..447, so
//     the addressable window doesn't exactly match the visible area
//     of this 368×448 panel and the picture comes out vertically
//     asymmetric (top band visibly shorter than the bottom band).
//
// Sequence:
//   1. 0x11 SLPOUT  + 120 ms wait
//   2. 0x44 STES = 0x01D1   tear-effect scanline (465)
//   3. 0x35 TEON  = 0x00    tearing-effect line ON
//   4. 0x53 WCTRLD1 = 0x20  brightness control on
//   5. 0x2A CASET = 0..0x16F (0..367) — full width
//   6. 0x2B RASET = 0..0x1BF (0..447) — full height
//   7. 0x51 brightness = 0  (panel ready, screen still dark)
//   8. 0x51 brightness = 0xFF (max) — issued after DISPON
//
// DISPON (0x29) is sent by `esp_lcd_panel_disp_on_off(panel, true)` in
// amoled_display_init(), between the brightness-zero and brightness-max
// commands here is fine because esp_lcd_panel_init runs the full table
// before disp_on_off is called.

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, NULL,                                0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1},             2,   0},
    {0x35, (uint8_t[]){0x00},                   1,   0},
    {0x53, (uint8_t[]){0x20},                   1,  10},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4,   0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4,   0},
    {0x51, (uint8_t[]){0x00},                   1,  10},
    {0x51, (uint8_t[]){0xFF},                   1,   0},
};
