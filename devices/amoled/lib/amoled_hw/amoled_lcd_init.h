#pragma once

#include "esp_lcd_sh8601.h"

// SH8601 init sequence for the Waveshare ESP32-S3-Touch-AMOLED-1.8.
//
// Derived from Waveshare's Arduino_SH8601 demo (sh8601_init_operations in
// docs/demo-code/Arduino/libraries/GFX_Library_for_Arduino/src/display/
// Arduino_SH8601.h). We need this *custom* init because the
// vendor_specific_init_default that ships inside the shared
// esp_lcd_sh8601 driver omits SLPOUT (0x11) — without it the panel stays
// in sleep mode after reset and the screen never lights up, even with
// DISPON (0x29) issued afterwards by esp_lcd_panel_disp_on_off.
//
// Sequence:
//   1. 0x11 SLPOUT  + 120 ms wait (mandatory to exit sleep)
//   2. 0x13 NORON   normal display mode
//   3. 0x20 INVOFF  no color inversion
//   4. 0x3A PIXFMT = 0x05   16 bit/pixel (RGB565)
//   5. 0x53 WCTRLD1 = 0x28  Brightness Control On + Display Dimming On
//   6. 0x51 WDBRIGHTNESSVALNOR = 0xFF (max brightness)
//   7. 0x55 WCE = 0x00      Sunlight Readability Enhancement off
//
// DISPON (0x29) is sent by `esp_lcd_panel_disp_on_off(panel, true)` in
// amoled_display_init(), so it is not in this table.

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, NULL,                 0, 120},
    {0x13, NULL,                 0,   0},
    {0x20, NULL,                 0,   0},
    {0x3A, (uint8_t[]){0x05},    1,   0},
    {0x53, (uint8_t[]){0x28},    1,   0},
    {0x51, (uint8_t[]){0xFF},    1,   0},
    {0x55, (uint8_t[]){0x00},    1,  10},
};
