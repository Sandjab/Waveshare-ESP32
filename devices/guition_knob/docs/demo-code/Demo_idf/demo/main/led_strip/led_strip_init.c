#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

#include "driver/ledc.h"
#include "led_strip_init.h"
#include "pinconfig.h"


#define TAG     "led strip"

#define LED_STRIP_USE_DMA  0

#if LED_STRIP_USE_DMA
// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT 13
#define LED_STRIP_MEMORY_BLOCK_WORDS 1024 // this determines the DMA block size
#else
// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT 13
#define LED_STRIP_MEMORY_BLOCK_WORDS 0 // let the driver choose a proper memory block size automatically
#endif // LED_STRIP_USE_DMA

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

static led_strip_handle_t led_strip = NULL;



esp_err_t led_strip_init(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = PIN_RGB_DATA, // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_COUNT,      // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,        // LED strip model
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color order of the strip: GRB
        .flags = {
            .invert_out = false, // don't invert the output signal
        }
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .mem_block_symbols = LED_STRIP_MEMORY_BLOCK_WORDS, // the memory block size used by the RMT channel
        .flags = {
            .with_dma = LED_STRIP_USE_DMA,     // Using DMA can improve performance when driving more LEDs
        }
    };

    // LED Strip object handle
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");

    return ESP_OK;
}

esp_err_t _led_strip_deinit()
{
    if(led_strip != NULL){
        led_strip_del(led_strip);
        led_strip = NULL;
    }
    return ESP_OK;
}

esp_err_t _led_strip_set_pixel_rgb(uint32_t index, uint32_t red, uint32_t green, uint32_t blue)
{
    if(index != 0){
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, index, red, green, blue));
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    } else {
        for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, red, green, blue));
        }
        /* Refresh the strip to send data */
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    }
    return ESP_OK;
}
esp_err_t _led_strip_set_pixel_rgbw(uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white)
{
    if(index != 0){
        ESP_ERROR_CHECK(led_strip_set_pixel_rgbw(led_strip, index, red, green, blue,white));
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    } else {
        for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
                ESP_ERROR_CHECK(led_strip_set_pixel_rgbw(led_strip, i, red, green, blue,white));
        }
        /* Refresh the strip to send data */
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    }
    return ESP_OK;
}

esp_err_t _led_strip_set_pixel_hsv(uint32_t index, uint16_t hue, uint8_t saturation, uint8_t value)
{
    if(index != 0){
        ESP_ERROR_CHECK(led_strip_set_pixel_hsv(led_strip, index, hue, saturation, value));
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    } else {
        for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
                ESP_ERROR_CHECK(led_strip_set_pixel_hsv(led_strip, i, hue, saturation, value));
        }
        /* Refresh the strip to send data */
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    }

    return ESP_OK;
}

esp_err_t _led_strip_clear()
{
    return led_strip_clear(led_strip);
}
