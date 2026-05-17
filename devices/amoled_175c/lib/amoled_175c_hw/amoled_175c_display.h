#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "esp_lcd_sh8601.h"
#include "amoled_175c_pins.h"
#include "amoled_175c_lcd_init.h"

// Initialize the CO5300 QSPI AMOLED on the ESP32-S3-Touch-AMOLED-1.75C.
//
// Specifics vs the AMOLED 1.8 helper:
//   - Direct LCD reset on PIN_LCD_RST (GPIO 2). No XCA9554 expander on
//     this board. Note that the vendor pin_config.h assigns the same
//     GPIO 2 to PIN_TP_RST — the reset toggle below will therefore
//     reset both the panel and the touch controller in one shot.
//   - No PWM backlight (AMOLED is self-emitting). Brightness is set
//     via CO5300 cmd 0x51 inside amoled_175c_lcd_init.h.
//   - We pass our own init_cmds (CO5300 sequence) through
//     sh8601_vendor_config_t — the shared esp_lcd_sh8601 driver is a
//     generic QSPI host, not SH8601-specific.
//
// Usage:
//   esp_lcd_panel_handle_t panel = amoled_175c_display_init();
//   esp_lcd_panel_handle_t panel = amoled_175c_display_init(flush_ready_cb, &ctx);

static inline esp_lcd_panel_handle_t amoled_175c_display_init(
        esp_lcd_panel_io_color_trans_done_cb_t on_flush_ready = NULL,
        void *user_ctx = NULL) {

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

    // 3. Panel with our CO5300 init via vendor_config
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BPP,
        .vendor_config = &vendor_config,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Panel visible area is offset by 6 columns within the CO5300 RAM.
    // The Waveshare HelloWorld demo passes col_offset1 = 6 (row_offset = 0)
    // to Arduino_CO5300's constructor — same effect via esp_lcd's set_gap.
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 6, 0));

    return panel_handle;
}
