#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_WAVES = 6;
static constexpr int R_MAX   = 180;
static constexpr uint32_t LIFE_MS = 2000;
static constexpr uint32_t IDLE_EMIT_MS = 400;

static lv_obj_t* arcs[N_WAVES];
static uint32_t  born_at[N_WAVES] = { 0 };
static uint16_t  hue_at[N_WAVES]  = { 0 };
static int       next_slot = 0;
static uint32_t  last_emit = 0;
static float     hue_emit  = 0;

static void emit_wave(uint32_t now) {
    born_at[next_slot] = now;
    hue_at[next_slot]  = (uint16_t)hue_emit;
    hue_emit += 47;
    if (hue_emit >= 360) hue_emit -= 360;
    next_slot = (next_slot + 1) % N_WAVES;
    last_emit = now;
}

static void viz_init() {
    for (int i = 0; i < N_WAVES; i++) {
        arcs[i] = lv_arc_create(lv_scr_act());
        lv_obj_set_size(arcs[i], 1, 1);
        lv_obj_center(arcs[i]);
        lv_arc_set_bg_angles(arcs[i], 0, 360);
        lv_arc_set_angles(arcs[i], 0, 360);
        lv_obj_remove_style(arcs[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_opa(arcs[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arcs[i], LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(arcs[i], 6, LV_PART_INDICATOR);
        born_at[i] = 0;
    }
    next_slot = 0;
    last_emit = 0;
}

static void viz_render(const AudioFrame& af) {
    if (af.transient || (af.t_ms - last_emit) > IDLE_EMIT_MS) {
        emit_wave(af.t_ms);
    }

    for (int i = 0; i < N_WAVES; i++) {
        if (born_at[i] == 0) continue;
        uint32_t age = af.t_ms - born_at[i];
        if (age > LIFE_MS) {
            lv_obj_set_style_arc_opa(arcs[i], LV_OPA_TRANSP, LV_PART_INDICATOR);
            born_at[i] = 0;
            continue;
        }
        float t = (float)age / LIFE_MS;
        int   r = (int)(t * R_MAX);
        lv_obj_set_size(arcs[i], r * 2, r * 2);
        lv_obj_center(arcs[i]);
        uint8_t opa = (uint8_t)((1.0f - t) * LV_OPA_COVER);
        lv_obj_set_style_arc_color(arcs[i],
            lv_color_hsv_to_rgb(hue_at[i], 100, 100), LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(arcs[i], opa, LV_PART_INDICATOR);
    }

    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hh = (uint16_t)((hue_emit + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, hh, 255, 180);
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_WAVES; i++) {
        if (arcs[i]) { lv_obj_del(arcs[i]); arcs[i] = nullptr; }
        born_at[i] = 0;
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_G_WAVE = {
    "G-Wave", viz_init, viz_render, viz_deinit
};
