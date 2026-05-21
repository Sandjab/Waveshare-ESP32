#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_BLOOMS = 4;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int R_MAX    = 175;
static constexpr uint32_t LIFE_MS = 600;

static lv_obj_t* circles[N_BLOOMS];
static uint32_t  born_at[N_BLOOMS] = { 0 };
static uint16_t  hue_at[N_BLOOMS]  = { 0 };
static int       next_slot = 0;

static int dominant_band(const float* bands, int n) {
    int best = 0; float v = bands[0];
    for (int i = 1; i < n; i++) if (bands[i] > v) { v = bands[i]; best = i; }
    return best;
}

static void viz_init() {
    for (int i = 0; i < N_BLOOMS; i++) {
        circles[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(circles[i]);
        lv_obj_set_size(circles[i], 1, 1);
        lv_obj_align(circles[i], LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(circles[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(circles[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(circles[i], 4, 0);
        lv_obj_set_style_border_opa(circles[i], LV_OPA_TRANSP, 0);
        born_at[i] = 0;
    }
    next_slot = 0;
}

static void viz_render(const AudioFrame& af) {
    if (af.transient) {
        int band = dominant_band(af.bands, 13);
        hue_at[next_slot]  = (uint16_t)((float)band / 13 * 280.0f);
        born_at[next_slot] = af.t_ms;
        next_slot = (next_slot + 1) % N_BLOOMS;
    }

    for (int i = 0; i < N_BLOOMS; i++) {
        if (born_at[i] == 0) continue;
        uint32_t age = af.t_ms - born_at[i];
        if (age > LIFE_MS) {
            lv_obj_set_style_border_opa(circles[i], LV_OPA_TRANSP, 0);
            born_at[i] = 0;
            continue;
        }
        float t = (float)age / LIFE_MS;     // 0..1
        int   r = (int)(t * R_MAX);
        lv_obj_set_size(circles[i], r * 2, r * 2);
        lv_obj_align(circles[i], LV_ALIGN_CENTER, 0, 0);
        uint8_t opa = (uint8_t)((1.0f - t) * LV_OPA_COVER);
        lv_obj_set_style_border_color(circles[i],
            lv_color_hsv_to_rgb(hue_at[i], 100, 100), 0);
        lv_obj_set_style_border_opa(circles[i], opa, 0);
    }

    // Anneau : flash blanc sur transient, sinon noir
    uint8_t v = af.transient ? 255 : 0;
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) rgb_ring_set(i, v, v, v);
}

static void viz_deinit() {
    for (int i = 0; i < N_BLOOMS; i++) {
        if (circles[i]) { lv_obj_del(circles[i]); circles[i] = nullptr; }
        born_at[i] = 0;
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_BEAT_BLOOM = {
    "Beat Bloom", viz_init, viz_render, viz_deinit
};
