#pragma once

// --- LCD (QSPI ST77916) ---
#define PIN_LCD_CS    14
#define PIN_LCD_CLK   13
#define PIN_LCD_D0    15
#define PIN_LCD_D1    16
#define PIN_LCD_D2    17
#define PIN_LCD_D3    18
#define PIN_LCD_RST   21
#define PIN_LCD_BL    47

#define LCD_H_RES     360
#define LCD_V_RES     360
#define LCD_BPP       16

// --- Rotary Encoder ---
#define PIN_ENC_A     8
#define PIN_ENC_B     7

// --- I2C (shared: CST816 touch + DRV2605 haptics) ---
#define PIN_I2C_SDA   11
#define PIN_I2C_SCL   12
