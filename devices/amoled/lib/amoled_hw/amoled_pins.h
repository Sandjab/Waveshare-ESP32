#pragma once

// --- LCD (QSPI SH8601 AMOLED) ---
#define PIN_LCD_CS    12
#define PIN_LCD_CLK   11
#define PIN_LCD_D0    4
#define PIN_LCD_D1    5
#define PIN_LCD_D2    6
#define PIN_LCD_D3    7

#define LCD_H_RES     368
#define LCD_V_RES     448
#define LCD_BPP       16

// --- I2C (shared: FT3168 touch, AXP2101, QMI8658, PCF85063, ES8311 ctrl, XCA9554) ---
#define PIN_I2C_SDA   15
#define PIN_I2C_SCL   14

// --- Touch (FT3168) ---
#define PIN_TP_INT    21

// --- Audio (ES8311 codec) ---
#define PIN_I2S_MCK   16
#define PIN_I2S_BCK   9
#define PIN_I2S_WS    45
#define PIN_I2S_DI    10
#define PIN_I2S_DO    8
#define PIN_PA_EN     46

// --- SD Card (SDMMC 1-wire) ---
#define SD_CLK_PIN    2
#define SD_CMD_PIN    1
#define SD_D0_PIN     3
