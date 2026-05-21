#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_RINGS  = 8;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int R_BASE   = 30;
static constexpr int R_STEP   = 20;

// Indices des bandes utilisées (réparties dans le spectre)
static const int band_idx[N_RINGS] = { 0, 1, 3, 5, 7, 9, 11, 12 };

static lv_obj_t* arcs[N_RINGS];
static float     hue_deg = 0;

static void viz_init() {
    for (int i = 0; i < N_RINGS; i++) {
        arcs[i] = lv_arc_create(lv_scr_act());
        int r = R_BASE + i * R_STEP;
        lv_obj_set_size(arcs[i], r * 2, r * 2);
        lv_obj_center(arcs[i]);
        lv_arc_set_bg_angles(arcs[i], 0, 360);
        lv_arc_set_angles(arcs[i], 0, 360);
        lv_obj_remove_style(arcs[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_color(arcs[i], lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arcs[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arcs[i], 4, LV_PART_INDICATOR);
    }
}

static void viz_render(const AudioFrame& af) {
    hue_deg += 0.8f;
    if (hue_deg >= 360) hue_deg -= 360;

    for (int i = 0; i < N_RINGS; i++) {
        float b = af.bands[band_idx[i]];
        int width = 2 + (int)(b * 30);  // 2..32 px
        lv_obj_set_style_arc_width(arcs[i], width, LV_PART_INDICATOR);

        float h = hue_deg + i * (360.0f / N_RINGS);
        if (h >= 360) h -= 360;
        lv_obj_set_style_arc_color(arcs[i],
            lv_color_hsv_to_rgb((uint16_t)h, 100, (uint8_t)(40 + b * 60)),
            LV_PART_INDICATOR);
    }

    // Anneau LED : wash hue qui rotate
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hh = (uint16_t)((hue_deg + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, hh, 255, 180);
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_RINGS; i++) {
        if (arcs[i]) { lv_obj_del(arcs[i]); arcs[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_TUNNEL = {
    "Tunnel", viz_init, viz_render, viz_deinit
};
