#pragma once

// Waveshare ESP32-S3-Touch-AMOLED-1.75C — pin map.
// Cross-checked against the vendor pin_config.h shipped in
// docs/demo-code/Arduino-v3.3.5/libraries/Mylibrary/pin_config.h
// (and the GitHub source waveshareteam/ESP32-S3-Touch-AMOLED-1.75C).
//
// Gotcha to validate on first flash : the vendor `pin_config.h` assigns
// GPIO 2 to BOTH `LCD_RESET` and `TP_RST`. Probably a coordinated reset
// (panel + touch driven together), but unusual — confirm with the
// schematic at docs/schematics/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf
// before relying on it for either signal.

// --- LCD (QSPI CO5300, 466x466 round AMOLED) ---
#define PIN_LCD_CS    12
#define PIN_LCD_CLK   38
#define PIN_LCD_D0    4
#define PIN_LCD_D1    5
#define PIN_LCD_D2    6
#define PIN_LCD_D3    7
#define PIN_LCD_RST   2     // shared with PIN_TP_RST per vendor pinconfig — see note above

#define LCD_H_RES     466
#define LCD_V_RES     466
#define LCD_BPP       16

// --- I2C shared bus (touch + AXP2101 + QMI8658 + ES8311 ctrl + ES7210 ctrl) ---
#define PIN_I2C_SDA   15
#define PIN_I2C_SCL   14

// --- Touch (CST9217) ---
#define PIN_TP_INT    11
#define PIN_TP_RST    2     // see PIN_LCD_RST note

// --- Audio I2S (shared by ES8311 playback codec + ES7210 mic-array AEC) ---
#define PIN_I2S_MCK   16
#define PIN_I2S_BCK   9
#define PIN_I2S_WS    45    // LRCK
#define PIN_I2S_DI    10    // microphones via ES7210
#define PIN_I2S_DO    8     // speaker via ES8311
#define PIN_PA_EN     46    // speaker amp enable (HIGH = on)
