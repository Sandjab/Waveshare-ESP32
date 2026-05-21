#include <Arduino.h>
#include <math.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_SEGS    = 8;
static constexpr int N_LINES   = 5;  // par segment
static constexpr int CENTER_X  = LCD_H_RES / 2;
static constexpr int CENTER_Y  = LCD_V_RES / 2;
static constexpr int R_MAX     = 170;

static lv_obj_t*  lines[N_SEGS * N_LINES];
static lv_point_t pts[N_SEGS * N_LINES][2];
static lv_style_t styles[N_SEGS * N_LINES];
static float      hue_deg = 0;

static void viz_init() {
    for (int s = 0; s < N_SEGS; s++) {
        for (int l = 0; l < N_LINES; l++) {
            int i = s * N_LINES + l;
            lv_style_init(&styles[i]);
            lv_style_set_line_width(&styles[i], 2);
            lv_style_set_line_color(&styles[i], lv_color_white());
            lv_style_set_line_rounded(&styles[i], true);

            lines[i] = lv_line_create(lv_scr_act());
            pts[i][0].x = CENTER_X;
            pts[i][0].y = CENTER_Y;
            pts[i][1].x = CENTER_X;
            pts[i][1].y = CENTER_Y;
            lv_line_set_points(lines[i], pts[i], 2);
            lv_obj_add_style(lines[i], &styles[i], 0);
        }
    }
}

static void viz_render(const AudioFrame& af) {
    hue_deg += 1.2f;
    if (hue_deg >= 360) hue_deg -= 360;

    float bass = af.bands[0] + af.bands[1];  // 0..2
    float amp  = 0.2f + 0.8f * (bass / 2.0f);  // 0.2..1.0

    for (int s = 0; s < N_SEGS; s++) {
        float seg_ang = (float)s * 2 * (float)M_PI / N_SEGS;
        for (int l = 0; l < N_LINES; l++) {
            int i = s * N_LINES + l;
            float r0 = (float)(l + 1) / (N_LINES + 1) * R_MAX * amp;
            float r1 = r0 + 24;
            float a0 = seg_ang;
            float a1 = seg_ang + (float)M_PI / N_SEGS * (0.5f + 0.5f * sinf(af.t_ms * 0.001f + l));

            pts[i][0].x = CENTER_X + (int)(r0 * cosf(a0));
            pts[i][0].y = CENTER_Y + (int)(r0 * sinf(a0));
            pts[i][1].x = CENTER_X + (int)(r1 * cosf(a1));
            pts[i][1].y = CENTER_Y + (int)(r1 * sinf(a1));
            lv_line_set_points(lines[i], pts[i], 2);

            float h = hue_deg + l * 40;
            if (h >= 360) h -= 360;
            lv_obj_set_style_line_color(lines[i],
                lv_color_hsv_to_rgb((uint16_t)h, 100, 100), 0);
        }
    }

    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hh = (uint16_t)((hue_deg + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, hh, 255, 180);
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_SEGS * N_LINES; i++) {
        if (lines[i]) { lv_obj_del(lines[i]); lines[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_KALEIDOSCOPE = {
    "Kaleidoscope", viz_init, viz_render, viz_deinit
};
