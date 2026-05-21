#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_BARS    = 13;
static constexpr int BAR_W     = 22;
static constexpr int GAP       = 4;
static constexpr int BAR_H_MAX = 300;
static constexpr int BASELINE_Y = LCD_V_RES - 30;  // bas de l'écran

static lv_obj_t* bars[N_BARS];
static lv_obj_t* caps[N_BARS];
static float     cap_y_norm[N_BARS] = { 0 };
static float     cap_vel[N_BARS]    = { 0 };

static void viz_init() {
    int total_w = N_BARS * BAR_W + (N_BARS - 1) * GAP;
    int x0      = (LCD_H_RES - total_w) / 2;

    for (int i = 0; i < N_BARS; i++) {
        int x = x0 + i * (BAR_W + GAP);

        bars[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(bars[i]);
        lv_obj_set_size(bars[i], BAR_W, 4);
        lv_obj_set_pos(bars[i], x, BASELINE_Y - 4);
        lv_obj_set_style_radius(bars[i], 3, 0);
        uint16_t hue = (uint16_t)((float)i / N_BARS * 280.0f);
        lv_obj_set_style_bg_color(bars[i], lv_color_hsv_to_rgb(hue, 100, 100), 0);
        lv_obj_set_style_bg_opa(bars[i], LV_OPA_COVER, 0);

        caps[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(caps[i]);
        lv_obj_set_size(caps[i], BAR_W, 3);
        lv_obj_set_pos(caps[i], x, BASELINE_Y - 4);
        lv_obj_set_style_bg_color(caps[i], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(caps[i], LV_OPA_COVER, 0);
    }
}

static void viz_render(const AudioFrame& af) {
    int x0 = (LCD_H_RES - (N_BARS * BAR_W + (N_BARS - 1) * GAP)) / 2;
    for (int i = 0; i < N_BARS; i++) {
        float h_norm = af.bands[i];
        int   h_px   = (int)(h_norm * BAR_H_MAX);
        if (h_px < 4) h_px = 4;

        lv_obj_set_size(bars[i], BAR_W, h_px);
        lv_obj_set_pos(bars[i], x0 + i * (BAR_W + GAP), BASELINE_Y - h_px);

        // Cap : suit le haut de la barre, retombe à 0.6 px/frame quand la barre descend
        if (h_norm > cap_y_norm[i]) {
            cap_y_norm[i] = h_norm;
            cap_vel[i]    = 0;
        } else {
            cap_vel[i]    += 0.0008f;  // gravité
            cap_y_norm[i] -= cap_vel[i];
            if (cap_y_norm[i] < 0) { cap_y_norm[i] = 0; cap_vel[i] = 0; }
        }
        int cap_y = BASELINE_Y - (int)(cap_y_norm[i] * BAR_H_MAX) - 3;
        lv_obj_set_pos(caps[i], x0 + i * (BAR_W + GAP), cap_y);

        uint16_t hue = (uint16_t)((float)i / N_BARS * 0.78f * 65535.0f);
        rgb_ring_set_hsv(i, hue, 255, (uint8_t)(h_norm * 255.0f));
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_BARS; i++) {
        if (bars[i]) { lv_obj_del(bars[i]); bars[i] = nullptr; }
        if (caps[i]) { lv_obj_del(caps[i]); caps[i] = nullptr; }
        cap_y_norm[i] = 0;
        cap_vel[i]    = 0;
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_SPECTRUM_BARS = {
    "Spectrum Bars", viz_init, viz_render, viz_deinit
};
