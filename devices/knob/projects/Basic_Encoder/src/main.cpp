#include <Arduino.h>
#include <lvgl.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_sh8601.h"
#include "knob_pins.h"
#include "knob_lcd_init.h"
#include "bidi_switch_knob.h"

#define BUF_HEIGHT 36

// ---- Globals ----
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static volatile int32_t enc_position = 0;
static int32_t last_count = 0;
static lv_obj_t *count_label = NULL;

// ---- LVGL callbacks ----
static bool notify_flush_ready(esp_lcd_panel_io_handle_t io,
                               esp_lcd_panel_io_event_data_t *edata,
                               void *user_ctx) {
    lv_disp_flush_ready((lv_disp_drv_t *)user_ctx);
    return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                           lv_color_t *color_map) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1, color_map);
}

static void lvgl_rounder_cb(lv_disp_drv_t *disp_drv, lv_area_t *area) {
    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;
}

// ---- Setup ----
void setup() {
    Serial.begin(115200);
    Serial.println("Basic_Encoder: Counter");

    // 1. Display init (QSPI bus)
    Serial.println("Init SPI bus...");
    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        PIN_LCD_CLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
        LCD_H_RES * LCD_V_RES * LCD_BPP / 8
    );
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. Panel IO
    Serial.println("Create panel IO...");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        PIN_LCD_CS, notify_flush_ready, &disp_drv
    );
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    // 3. Panel with vendor config
    Serial.println("Create panel...");
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
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // 4. Backlight
    Serial.println("Backlight on...");
    ledcAttach(PIN_LCD_BL, 50000, 8);
    ledcWrite(PIN_LCD_BL, 255);

    // 5. LVGL init
    Serial.println("Init LVGL...");
    lv_init();

    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(
        LCD_H_RES * BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
        LCD_H_RES * BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 && buf2);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * BUF_HEIGHT);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.rounder_cb = lvgl_rounder_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_drv_register(&disp_drv);

    // LVGL tick timer (2ms)
    const esp_timer_create_args_t tick_args = {
        .callback = [](void *) { lv_tick_inc(2); },
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 2000));

    // 6. UI — single centered counter label
    Serial.println("Build UI...");
    count_label = lv_label_create(lv_scr_act());
    lv_obj_align(count_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(count_label, lv_color_black(), 0);
    lv_label_set_text(count_label, " 0000");

    // 7. Encoder (bidi_switch_knob driver, timer-polled)
    Serial.println("Init encoder...");
    knob_config_t enc_cfg = {
        .gpio_encoder_a = PIN_ENC_A,
        .gpio_encoder_b = PIN_ENC_B,
    };
    knob_handle_t knob = iot_knob_create(&enc_cfg);
    iot_knob_register_cb(knob, KNOB_RIGHT, [](void *, void *) {
        enc_position = enc_position + 1;
    }, NULL);
    iot_knob_register_cb(knob, KNOB_LEFT, [](void *, void *) {
        enc_position = enc_position - 1;
    }, NULL);

    Serial.println("Ready. Rotate the knob!");
}

// ---- Loop ----
void loop() {
    int32_t count = enc_position;
    if (count != last_count) {
        last_count = count;

        int32_t display_val = count;
        if (display_val > 9999) display_val = 9999;
        if (display_val < -9999) display_val = -9999;

        char buf[8];
        if (display_val == 0) snprintf(buf, sizeof(buf), " 0000");
        else if (display_val > 0) snprintf(buf, sizeof(buf), "+%04ld", (long)display_val);
        else snprintf(buf, sizeof(buf), "-%04ld", (long)(-display_val));

        lv_label_set_text(count_label, buf);
        Serial.printf("Count: %ld\n", (long)count);
    }

    lv_timer_handler();
    delay(5);
}
