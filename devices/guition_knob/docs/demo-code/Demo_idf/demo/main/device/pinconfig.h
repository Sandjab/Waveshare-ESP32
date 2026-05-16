#ifndef __PINCONFIG_H
#define __PINCONFIG_H

#define PIN_SDMMC_SCK           GPIO_NUM_39
#define PIN_SDMMC_CMD           GPIO_NUM_38
#define PIN_SDMMC_D0            GPIO_NUM_40
#define PIN_SDMMC_D1            GPIO_NUM_41
#define PIN_SDMMC_D2            GPIO_NUM_48
#define PIN_SDMMC_D3            GPIO_NUM_47


#define TOUCH_HOST              I2C_NUM_0
#define PIN_TP_SDA              GPIO_NUM_9
#define PIN_TP_SCL              GPIO_NUM_10
#define PIN_TP_INT              GPIO_NUM_7
#define PIN_TP_RST              GPIO_NUM_8

#define LCD_H_RES               360
#define LCD_V_RES               360
#define LCD_BIT_PER_PIXEL       16
#define LCD_HOST                SPI2_HOST

#define PIN_LCD_SCL             GPIO_NUM_11
#define PIN_LCD_CS              GPIO_NUM_12
#define PIN_LCD_D0              GPIO_NUM_13
#define PIN_LCD_D1              GPIO_NUM_14
#define PIN_LCD_D2              GPIO_NUM_15
#define PIN_LCD_D3              GPIO_NUM_16
#define PIN_LCD_RST             GPIO_NUM_17
#define PIN_LCD_TE              GPIO_NUM_18
#define PIN_LCD_BLK             GPIO_NUM_21


#define PIN_PA_CTRL             GPIO_NUM_46
#define PIN_I2S_BCK             GPIO_NUM_3
#define PIN_I2S_WS              GPIO_NUM_45
#define PIN_I2S_DO              GPIO_NUM_42

#define PIN_MIC_SCK             GPIO_NUM_5
#define PIN_MIC_DATA            GPIO_NUM_4

#define PIN_BAT_DAC             GPIO_NUM_6

#define PIN_RGB_DATA            GPIO_NUM_0

#define GPIO_KNOB_A             GPIO_NUM_2
#define GPIO_KNOB_B             GPIO_NUM_1



#endif