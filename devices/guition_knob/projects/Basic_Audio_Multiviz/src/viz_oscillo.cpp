#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_POINTS = 512;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr int AMPLITUDE = 140;  // ± pixels autour du centre

static lv_obj_t*   line = nullptr;
static lv_point_t  pts[N_POINTS];
static lv_style_t  style;

static void viz_init() {
    lv_style_init(&style);
    lv_style_set_line_width(&style, 2);
    lv_style_set_line_color(&style, lv_color_make(0, 255, 80));  // vert phosphor
    lv_style_set_line_rounded(&style, true);

    for (int i = 0; i < N_POINTS; i++) {
        pts[i].x = (int)((float)i / (N_POINTS - 1) * (LCD_H_RES - 1));
        pts[i].y = CENTER_Y;
    }
    line = lv_line_create(lv_scr_act());
    lv_line_set_points(line, pts, N_POINTS);
    lv_obj_add_style(line, &style, 0);
}

static void viz_render(const AudioFrame& af) {
    // PDM mic a un offset DC : on retire la moyenne avant scaling, sinon
    // augmenter le gain décale la ligne de base au lieu d'étirer le signal.
    int32_t sum = 0;
    for (int i = 0; i < N_POINTS; i++) sum += af.wave[i];
    int16_t dc = (int16_t)(sum / N_POINTS);

    for (int i = 0; i < N_POINTS; i++) {
        int32_t s = (int32_t)af.wave[i] - dc;
        int dy = (int)((float)s * AMPLITUDE / 1500.0f);
        if (dy > AMPLITUDE)  dy = AMPLITUDE;
        if (dy < -AMPLITUDE) dy = -AMPLITUDE;
        pts[i].y = CENTER_Y + dy;
    }
    lv_line_set_points(line, pts, N_POINTS);

    uint8_t v = (uint8_t)(af.rms * 255.0f);
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        rgb_ring_set(i, 0, v, (uint8_t)(v / 4));
    }
}

static void viz_deinit() {
    if (line) { lv_obj_del(line); line = nullptr; }
    rgb_ring_clear();
}

extern const Visualizer VIZ_OSCILLO = {
    "Oscilloscope", viz_init, viz_render, viz_deinit
};
