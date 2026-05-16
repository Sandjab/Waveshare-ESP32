#ifndef __BACKLIGHT_H
#define __BACKLIGHT_H

#include "esp_err.h"

esp_err_t lcd_backlight_init();
esp_err_t lcd_backlight_set(int brightness_percent);
esp_err_t lcd_backlight_on();
esp_err_t lcd_backlight_off();

#endif