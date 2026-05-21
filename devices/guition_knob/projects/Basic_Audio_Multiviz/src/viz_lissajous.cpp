#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_POINTS = 256;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int RADIUS   = 150;

static lv_obj_t*  line = nullptr;
static lv_point_t pts[N_POINTS];
static lv_style_t style;
static float      hue_deg = 0;

static void viz_init() {
    lv_style_init(&style);
    lv_style_set_line_width(&style, 2);
    lv_style_set_line_color(&style, lv_color_make(0, 220, 255));  // cyan
    lv_style_set_line_rounded(&style, true);

    for (int i = 0; i < N_POINTS; i++) {
        pts[i].x = CENTER_X;
        pts[i].y = CENTER_Y;
    }
    line = lv_line_create(lv_scr_act());
    lv_line_set_points(line, pts, N_POINTS);
    lv_obj_add_style(line, &style, 0);
}

static void viz_render(const AudioFrame& af) {
    for (int i = 0; i < N_POINTS; i++) {
        int16_t x = af.wave[2 * i];
        int16_t y = af.wave[2 * i + 1];
        pts[i].x = CENTER_X + (int)((float)x * RADIUS / 16000.0f);
        pts[i].y = CENTER_Y + (int)((float)y * RADIUS / 16000.0f);
    }
    lv_line_set_points(line, pts, N_POINTS);

    // Anneau : dégradé HSV qui tourne
    hue_deg += 1.5f;
    if (hue_deg >= 360) hue_deg -= 360;
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t h = (uint16_t)((hue_deg + i * (360.0f / RGB_RING_LED_COUNT)) * 65535.0f / 360.0f);
        rgb_ring_set_hsv(i, h, 255, 200);
    }
}

static void viz_deinit() {
    if (line) { lv_obj_del(line); line = nullptr; }
    rgb_ring_clear();
}

extern const Visualizer VIZ_LISSAJOUS = {
    "Lissajous", viz_init, viz_render, viz_deinit
};
