#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_STRIPES = 13;
static constexpr int STRIPE_H  = LCD_V_RES / N_STRIPES;  // ~27 px

static lv_obj_t* stripes[N_STRIPES];
static float     hue_deg = 0;

static void viz_init() {
    for (int i = 0; i < N_STRIPES; i++) {
        stripes[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(stripes[i]);
        lv_obj_set_size(stripes[i], LCD_H_RES, STRIPE_H + 1);
        lv_obj_set_pos(stripes[i], 0, i * STRIPE_H);
        lv_obj_set_style_bg_opa(stripes[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(stripes[i], lv_color_black(), 0);
        lv_obj_set_style_radius(stripes[i], 0, 0);
    }
}

static void viz_render(const AudioFrame& af) {
    hue_deg += 0.7f;
    if (hue_deg >= 360) hue_deg -= 360;

    for (int i = 0; i < N_STRIPES; i++) {
        float h = hue_deg + i * (360.0f / N_STRIPES);
        if (h >= 360) h -= 360;
        uint8_t val = (uint8_t)(20 + af.bands[i] * 235.0f);
        lv_obj_set_style_bg_color(stripes[i],
            lv_color_hsv_to_rgb((uint16_t)h, 100, val), 0);
    }

    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hh = (uint16_t)((hue_deg + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, hh, 255, (uint8_t)(af.bands[i] * 255.0f));
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_STRIPES; i++) {
        if (stripes[i]) { lv_obj_del(stripes[i]); stripes[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_GEISS_STRIPES = {
    "Geiss Stripes", viz_init, viz_render, viz_deinit
};
