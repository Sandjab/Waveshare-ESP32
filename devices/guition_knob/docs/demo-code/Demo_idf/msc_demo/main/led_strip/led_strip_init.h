#ifndef __LED_STRIP_INIT_H
#define __LED_STRIP_INIT_H

#include "esp_err.h"

esp_err_t led_strip_init(void);
esp_err_t _led_strip_set_pixel_rgb(uint32_t index, uint32_t red, uint32_t green, uint32_t blue);
esp_err_t _led_strip_set_pixel_rgbw(uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white);
esp_err_t _led_strip_set_pixel_hsv(uint32_t index, uint16_t hue, uint8_t saturation, uint8_t value);
esp_err_t _led_strip_clear();
esp_err_t _led_strip_deinit();
#endif