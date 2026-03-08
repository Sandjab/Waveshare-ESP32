#include <Arduino.h>
#include "knob_lvgl.h"
#include "bidi_switch_knob.h"

// ---- Globals ----
static esp_lcd_panel_handle_t panel_handle = NULL;
static volatile int32_t enc_position = 0;
static int32_t last_count = 0;
static lv_obj_t *count_label = NULL;

// ---- Setup ----
void setup() {
    Serial.begin(115200);
    Serial.println("Basic_Encoder: Counter");

    panel_handle = knob_lvgl_init();

    // UI — single centered counter label
    Serial.println("Build UI...");
    count_label = lv_label_create(lv_scr_act());
    lv_obj_align(count_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(count_label, lv_color_black(), 0);
    lv_label_set_text(count_label, " 0000");

    // Encoder (bidi_switch_knob driver, timer-polled)
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
