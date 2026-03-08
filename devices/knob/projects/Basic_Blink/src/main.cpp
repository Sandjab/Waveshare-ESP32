#include <Arduino.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "esp_lcd_sh8601.h"
#include "knob_pins.h"
#include "knob_lcd_init.h"

#define STRIP_HEIGHT  36
#define STRIP_PIXELS  (LCD_H_RES * STRIP_HEIGHT)

// --- RGB565 byte-swapped colors (big-endian for ST77916) ---
#define COLOR_GREEN   0xE007
#define COLOR_RED     0x00F8

// --- Globals ---
static esp_lcd_panel_handle_t panel_handle = NULL;
static uint16_t *strip_buf = NULL;

void fill_screen(uint16_t color) {
    for (int i = 0; i < STRIP_PIXELS; i++) {
        strip_buf[i] = color;
    }
    for (int y = 0; y < LCD_V_RES; y += STRIP_HEIGHT) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + STRIP_HEIGHT, strip_buf);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Basic_Blink: Minimal Screen Blink");

    // 1. Init SPI bus (QSPI)
    Serial.println("Init SPI bus...");
    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        PIN_LCD_CLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
        LCD_H_RES * LCD_V_RES * LCD_BPP / 8
    );
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. Create panel IO
    Serial.println("Create panel IO...");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        PIN_LCD_CS, NULL, NULL
    );
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    // 3. Create panel with vendor config
    Serial.println("Create panel...");
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BPP,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));

    // 4. Reset + init
    Serial.println("Panel reset...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    Serial.println("Panel init...");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // 5. Backlight on (PWM)
    Serial.println("Backlight on...");
    ledcAttach(PIN_LCD_BL, 50000, 8);
    ledcWrite(PIN_LCD_BL, 255);

    // 6. Allocate strip buffer in DMA-capable memory
    strip_buf = (uint16_t *)heap_caps_malloc(STRIP_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(strip_buf);

    Serial.println("Init complete. Starting blink loop.");
}

void loop() {
    Serial.println("GREEN");
    fill_screen(COLOR_GREEN);
    delay(1000);

    Serial.println("RED");
    fill_screen(COLOR_RED);
    delay(1000);
}
