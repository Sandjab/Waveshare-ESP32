#pragma once

#include <Wire.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "esp_lcd_sh8601.h"
#include "amoled_pins.h"
#include "amoled_lcd_init.h"

// Initialize the SH8601 QSPI AMOLED on the ESP32-S3-Touch-AMOLED-1.8.
//
// AMOLED specifics vs the ST77916-based Knob/Guition:
//   - The LCD reset / power-enable signals are routed via the XCA9554
//     I/O expander on the I2C bus, NOT a direct GPIO. We bring P0..P2
//     and P6 LOW for 20 ms then HIGH before talking to the panel —
//     same sequence the Waveshare 13_LVGL_Widgets demo runs in setup().
//     Without it the panel stays dark even after a power cycle.
//   - We still pass `reset_gpio_num = -1` to the esp_lcd_sh8601 driver
//     because the SoC has no direct RST line; the toggle above does
//     the equivalent job via the expander.
//   - No PWM backlight (AMOLED is self-emitting). Brightness is set
//     via SH8601 command 0x51 (Write Display Brightness, 0..255).
//   - No custom init command table: the shared esp_lcd_sh8601 driver
//     already carries a `vendor_specific_init_default` for the real
//     SH8601 part (the ST77916 boards override it; we use the
//     default here by leaving vendor_config = nullptr).

// Toggle XCA9554 P0..P2 + P6 LOW then HIGH to release the AMOLED panel's
// reset / power-enable lines. Registers per the TCA9554 / XCA9554 datasheet:
// 0x01 = output port, 0x03 = configuration (0 = output, 1 = input).
static inline void amoled_xca9554_lcd_reset() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // Configure P0, P1, P2, P6 as outputs (0 = output for those bits;
    // 0xB8 = 0b10111000, bits 0/1/2/6 cleared).
    Wire.beginTransmission(0x20);
    Wire.write(0x03);
    Wire.write(0xB8);
    Wire.endTransmission();

    // Drive them LOW (panel in reset / unpowered).
    Wire.beginTransmission(0x20);
    Wire.write(0x01);
    Wire.write(0x00);
    Wire.endTransmission();
    delay(20);

    // Drive them HIGH (release reset / enable LCD rails).
    Wire.beginTransmission(0x20);
    Wire.write(0x01);
    Wire.write(0xFF);
    Wire.endTransmission();
    delay(100);
}
//
// Usage:
//   esp_lcd_panel_handle_t panel = amoled_display_init();
//   esp_lcd_panel_handle_t panel = amoled_display_init(flush_ready_cb, &ctx);
//
static inline esp_lcd_panel_handle_t amoled_display_init(
        esp_lcd_panel_io_color_trans_done_cb_t on_flush_ready = NULL,
        void *user_ctx = NULL) {

    // 0. Release LCD reset / enable rails via XCA9554 expander.
    amoled_xca9554_lcd_reset();

    // 1. SPI bus (QSPI)
    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        PIN_LCD_CLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
        LCD_H_RES * LCD_V_RES * LCD_BPP / 8
    );
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. Panel IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        PIN_LCD_CS, on_flush_ready, user_ctx
    );
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    // 3. Panel with our SH8601 init (must include SLPOUT — the driver's
    //    vendor_specific_init_default omits it and leaves the panel asleep).
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BPP,
        .vendor_config = &vendor_config,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    return panel_handle;
}
