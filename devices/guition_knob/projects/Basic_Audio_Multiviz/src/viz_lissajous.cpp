#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_POINTS = 256;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int RADIUS   = 150;
// Mic mono → on ne peut pas faire un vrai XY scope. On simule en décalant y
// dans le temps : un 1 kHz @ 16 kHz sample rate déphase de 90° pour DELAY=4.
static constexpr int DELAY    = 8;

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
    // DC removal sur la fenêtre utile (N_POINTS + DELAY samples) — sinon
    // l'offset PDM décale toute la figure et amplifier le gain l'éjecte.
    int32_t sum = 0;
    int n_used = N_POINTS + DELAY;
    for (int i = 0; i < n_used; i++) sum += af.wave[i];
    int16_t dc = (int16_t)(sum / n_used);

    for (int i = 0; i < N_POINTS; i++) {
        int32_t x = (int32_t)af.wave[i]         - dc;
        int32_t y = (int32_t)af.wave[i + DELAY] - dc;
        int dx = (int)((float)x * RADIUS / 2000.0f);
        int dy = (int)((float)y * RADIUS / 2000.0f);
        if (dx >  RADIUS) dx =  RADIUS;
        if (dx < -RADIUS) dx = -RADIUS;
        if (dy >  RADIUS) dy =  RADIUS;
        if (dy < -RADIUS) dy = -RADIUS;
        pts[i].x = CENTER_X + dx;
        pts[i].y = CENTER_Y + dy;
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
