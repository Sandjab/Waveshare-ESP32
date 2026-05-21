#include <Arduino.h>
#include <math.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_BANDS  = 13;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int BAR_RMIN = 40;
static constexpr int BAR_RMAX = 172;
static constexpr int BAR_WIDTH = 16;

static lv_obj_t   *bars[N_BANDS];
static lv_point_t  bar_pts[N_BANDS][2];
static lv_style_t  bar_styles[N_BANDS];

static void viz_init() {
    for (int i = 0; i < N_BANDS; i++) {
        float ang = -90.0f + (360.0f * i / N_BANDS);
        float r   = ang * (float)M_PI / 180.0f;
        float c = cosf(r), s = sinf(r);

        bar_pts[i][0].x = CENTER_X + (int)(BAR_RMIN * c);
        bar_pts[i][0].y = CENTER_Y + (int)(BAR_RMIN * s);
        bar_pts[i][1].x = CENTER_X + (int)((BAR_RMIN + 4) * c);
        bar_pts[i][1].y = CENTER_Y + (int)((BAR_RMIN + 4) * s);

        uint16_t hue = (uint16_t)((float)i / N_BANDS * 280.0f);  // 0..280°
        lv_color_t col = lv_color_hsv_to_rgb(hue, 100, 100);

        lv_style_init(&bar_styles[i]);
        lv_style_set_line_width(&bar_styles[i], BAR_WIDTH);
        lv_style_set_line_color(&bar_styles[i], col);
        lv_style_set_line_rounded(&bar_styles[i], true);

        bars[i] = lv_line_create(lv_scr_act());
        lv_line_set_points(bars[i], bar_pts[i], 2);
        lv_obj_add_style(bars[i], &bar_styles[i], 0);
    }
}

static void viz_render(const AudioFrame& af) {
    for (int i = 0; i < N_BANDS; i++) {
        float ang = -90.0f + (360.0f * i / N_BANDS);
        float r   = ang * (float)M_PI / 180.0f;
        float c = cosf(r), s = sinf(r);

        int len = BAR_RMIN + (int)((BAR_RMAX - BAR_RMIN) * af.bands[i]);
        bar_pts[i][1].x = CENTER_X + (int)(len * c);
        bar_pts[i][1].y = CENTER_Y + (int)(len * s);
        lv_line_set_points(bars[i], bar_pts[i], 2);

        uint16_t hue = (uint16_t)((float)i / N_BANDS * 0.78f * 65535.0f);
        uint8_t  val = (uint8_t)(af.bands[i] * 255.0f);
        rgb_ring_set_hsv(i, hue, 255, val);
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_BANDS; i++) {
        if (bars[i]) { lv_obj_del(bars[i]); bars[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_SPECTRUM_RADIAL = {
    "Spectrum Radial", viz_init, viz_render, viz_deinit
};
