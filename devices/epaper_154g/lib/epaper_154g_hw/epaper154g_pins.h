// epaper154g_pins.h — GPIO definitions for Waveshare ESP32-S3-ePaper-1.54G
// Source of truth: vendor demo code (docs/demo-code/ESP-IDF/08_BATT_PWR_Test/main/user_config.h
// + Arduino examples). Demo code is authoritative for GPIOs (repo convention).

#pragma once

// --- E-Paper panel (SPI2, driver EPD_1in54g — 200x200, 4 colors B/W/Y/R) ---
#define PIN_EPD_DC      10
#define PIN_EPD_CS      11
#define PIN_EPD_SCK     12
#define PIN_EPD_MOSI    13
#define PIN_EPD_RST     9
#define PIN_EPD_BUSY    8

#define EPD_WIDTH       200
#define EPD_HEIGHT      200

// 2-bit color codes used by the vendor EPD_1in54g driver
#define EPD_COLOR_BLACK   0x0
#define EPD_COLOR_WHITE   0x1
#define EPD_COLOR_YELLOW  0x2
#define EPD_COLOR_RED     0x3

// --- Power rails (vendor board_power_bsp; note inverted logic on EPD/audio) ---
#define PIN_EPD_PWR     6   // LOW = panel powered, HIGH = off
#define PIN_AUDIO_PWR   42  // LOW = audio rail on, HIGH = off (PA_EN in Arduino demo)
#define PIN_VBAT_PWR    17  // HIGH = battery rail hold on, LOW = off

// --- Audio (ES8311 codec, I2C addr 0x18 + I2S) ---
#define PIN_I2S_MCLK    14
#define PIN_I2S_BCLK    15
#define PIN_I2S_LRCK    38
#define PIN_I2S_DOUT    45  // ESP32 -> ES8311 (playback)
#define PIN_I2S_DIN     16  // ES8311 -> ESP32 (mic record)
#define PIN_PA_CTRL     46  // HIGH = speaker amp output enabled

// --- I2C bus (shared: RTC + temp/humidity + audio codec) ---
#define PIN_I2C_SDA     47
#define PIN_I2C_SCL     48
#define I2C_ADDR_PCF85063  0x51  // RTC
#define I2C_ADDR_SHTC3     0x70  // temp/humidity sensor
#define I2C_ADDR_ES8311    0x18  // audio codec

// --- TF card (SDMMC 1-bit) ---
#define PIN_SD_CLK      39
#define PIN_SD_CMD      41
#define PIN_SD_D0       40

// --- Battery monitor ---
#define PIN_BAT_ADC     4   // ADC1_CH3, 12 dB atten (voltage divider on VBAT)

// --- Buttons ---
#define PIN_BTN_BOOT    0   // BOOT strap, also deep-sleep wakeup in vendor demo
#define PIN_BTN_PWR     18

// --- LED ---
#define PIN_LED_GREEN   3   // active LOW (LED_ON = 0 in vendor demo)
