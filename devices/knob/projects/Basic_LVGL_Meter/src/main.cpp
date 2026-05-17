#include <Arduino.h>
#include "knob_lvgl.h"
#include "bidi_switch_knob.h"

// Cadran lv_meter 0..100 pilote par l'encoder — portage 1:1 de la demo
// Guition (devices/guition_knob/projects/Basic_LVGL_Meter) pour comparer
// le sens de rotation : CW doit faire monter l'aiguille. Si le Guition
// l'inverse, c'est que le pin map Guition cable A/B a l'envers de la
// convention Waveshare (a fixer dans guition_pins.h, pas ici).

static constexpr int VALUE_MIN = 0;
static constexpr int VALUE_MAX = 100;

static esp_lcd_panel_handle_t panel;
static knob_handle_t          knob;
static volatile int           value = 30;     // demarre a 30%

static lv_obj_t              *meter;
static lv_meter_indicator_t  *needle;
static lv_meter_indicator_t  *arc_lo;
static lv_meter_indicator_t  *arc_md;
static lv_meter_indicator_t  *arc_hi;
static lv_obj_t              *value_label;

static void build_ui() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);

    meter = lv_meter_create(lv_scr_act());
    lv_obj_set_size(meter, 340, 340);
    lv_obj_center(meter);
    lv_obj_set_style_bg_opa(meter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(meter, 0, 0);

    lv_meter_scale_t *scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, scale, 8, 4, 16, lv_color_black(), 14);
    lv_meter_set_scale_range(meter, scale, VALUE_MIN, VALUE_MAX, 270, 135);

    // 3 arcs colores empiles : bleu (0-50), jaune (50-80), rouge (80-100)
    arc_lo = lv_meter_add_arc(meter, scale, 8, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter, arc_lo, 0);
    lv_meter_set_indicator_end_value(meter, arc_lo, 50);
    arc_md = lv_meter_add_arc(meter, scale, 8, lv_palette_main(LV_PALETTE_AMBER), 0);
    lv_meter_set_indicator_start_value(meter, arc_md, 50);
    lv_meter_set_indicator_end_value(meter, arc_md, 80);
    arc_hi = lv_meter_add_arc(meter, scale, 8, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, arc_hi, 80);
    lv_meter_set_indicator_end_value(meter, arc_hi, 100);

    // Aiguille
    needle = lv_meter_add_needle_line(meter, scale, 5, lv_palette_main(LV_PALETTE_RED), -20);
    lv_meter_set_indicator_value(meter, needle, value);

    // Valeur au centre
    value_label = lv_label_create(meter);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(value_label, lv_color_black(), 0);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", value);
    lv_label_set_text(value_label, buf);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Waveshare Knob — Basic_LVGL_Meter (port from Guition)");

    panel = knob_lvgl_init();

    build_ui();

    knob_config_t kc = { .gpio_encoder_a = PIN_ENC_A, .gpio_encoder_b = PIN_ENC_B };
    knob = iot_knob_create(&kc);
    iot_knob_clear_count_value(knob);

    Serial.printf("Valeur initiale : %d (encoder pour faire bouger l'aiguille)\n", value);
}

void loop() {
    int delta = iot_knob_get_count_value(knob);
    if (delta != 0) {
        iot_knob_clear_count_value(knob);
        int new_val = value + delta;
        if (new_val < VALUE_MIN) new_val = VALUE_MIN;
        if (new_val > VALUE_MAX) new_val = VALUE_MAX;
        if (new_val != value) {
            value = new_val;
            lv_meter_set_indicator_value(meter, needle, value);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", value);
            lv_label_set_text(value_label, buf);
            Serial.printf("Valeur : %d\n", value);
        }
    }

    lv_timer_handler();
    delay(5);
}
