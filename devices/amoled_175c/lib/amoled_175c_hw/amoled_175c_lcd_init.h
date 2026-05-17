#pragma once

#include "esp_lcd_sh8601.h"

// CO5300 init sequence for the Waveshare ESP32-S3-Touch-AMOLED-1.75C.
//
// Derived from Waveshare's Arduino_CO5300 demo (co5300_init_operations
// in devices/amoled_175c/docs/demo-code .../GFX_Library_for_Arduino/src/
// display/Arduino_CO5300.h), trimmed to the bytes we actually need.
//
// We re-use the shared `esp_lcd_sh8601` driver as a generic QSPI host
// (same trick we apply for the ST77916 boards). The driver already
// sends MADCTL (0x36) and COLMOD (0x3A) before iterating this table,
// using madctl_val=0x00 and colmod_val=0x55 (16bpp) by default, so we
// don't repeat them here.
//
// Sequence:
//   1. 0x11 SLPOUT          + 120 ms wait
//   2. 0xFE = 0x00          register-bank / page select (CO5300 quirk)
//   3. 0xC4 SPIMODECTL=0x80 force the panel into QSPI mode
//   4. 0x53 WCTRLD1 = 0x20  brightness control on
//   5. 0x63 HBM brightness = 0xFF
//   6. 0x51 brightness = 0xD0 (normal mode)
//   7. 0x58 WCE = 0x00      contrast / sunlight readability off
//
// DISPON (0x29) is sent by `esp_lcd_panel_disp_on_off(panel, true)` in
// amoled_175c_display_init() — not in this table.

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, NULL,                  0, 120},
    {0xFE, (uint8_t[]){0x00},     1,   0},
    {0xC4, (uint8_t[]){0x80},     1,   0},
    {0x53, (uint8_t[]){0x20},     1,   0},
    {0x63, (uint8_t[]){0xFF},     1,   0},
    {0x51, (uint8_t[]){0xD0},     1,   0},
    {0x58, (uint8_t[]){0x00},     1,  10},
};
