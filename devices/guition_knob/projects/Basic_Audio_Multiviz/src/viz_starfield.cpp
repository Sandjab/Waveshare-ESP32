#include <Arduino.h>
#include <math.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_PARTS  = 24;
static constexpr int CENTER_X = LCD_H_RES / 2;
static constexpr int CENTER_Y = LCD_V_RES / 2;
static constexpr float R_MAX  = 175.0f;

static lv_obj_t* parts[N_PARTS];
static float     r_pos[N_PARTS];  // 0..R_MAX
static float     ang[N_PARTS];    // radians
static uint32_t  boost_until = 0;

static void viz_init() {
    for (int i = 0; i < N_PARTS; i++) {
        parts[i] = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(parts[i]);
        lv_obj_set_size(parts[i], 6, 6);
        lv_obj_set_style_radius(parts[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(parts[i], LV_OPA_COVER, 0);
        ang[i]   = (float)i * 2 * (float)M_PI / N_PARTS;
        r_pos[i] = (float)(i * 7 % (int)R_MAX);  // étalé au départ
        uint16_t hue = (uint16_t)((float)i / N_PARTS * 360.0f);
        lv_obj_set_style_bg_color(parts[i],
            lv_color_hsv_to_rgb(hue, 100, 100), 0);
    }
}

static void viz_render(const AudioFrame& af) {
    if (af.transient) boost_until = af.t_ms + 200;
    float bass = af.bands[0] + af.bands[1] + af.bands[2];
    float speed = 1.0f + bass * 6.0f;  // ~1..3 px/frame
    if (af.t_ms < boost_until) speed *= 1.5f;

    for (int i = 0; i < N_PARTS; i++) {
        r_pos[i] += speed;
        if (r_pos[i] > R_MAX) r_pos[i] = 0;
        int x = CENTER_X + (int)(r_pos[i] * cosf(ang[i])) - 3;
        int y = CENTER_Y + (int)(r_pos[i] * sinf(ang[i])) - 3;
        lv_obj_set_pos(parts[i], x, y);
    }

    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        uint16_t hue = (uint16_t)((float)i / RGB_RING_LED_COUNT * 0.78f * 65535.0f);
        rgb_ring_set_hsv(i, hue, 255, (uint8_t)(af.bands[i] * 255.0f));
    }
}

static void viz_deinit() {
    for (int i = 0; i < N_PARTS; i++) {
        if (parts[i]) { lv_obj_del(parts[i]); parts[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_STARFIELD = {
    "Starfield", viz_init, viz_render, viz_deinit
};
