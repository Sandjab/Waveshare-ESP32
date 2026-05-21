#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static lv_obj_t* disc  = nullptr;
static lv_obj_t* flash = nullptr;
static uint32_t  flash_until = 0;
static float     hue_deg = 0;

static void viz_init() {
    disc = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(disc);
    lv_obj_set_size(disc, 60, 60);
    lv_obj_align(disc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);

    flash = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(flash);
    lv_obj_set_size(flash, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(flash, 0, 0);
    lv_obj_set_style_bg_color(flash, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(flash, LV_OPA_TRANSP, 0);
    flash_until = 0;
}

static void viz_render(const AudioFrame& af) {
    hue_deg += 0.5f;  // rotation lente
    if (hue_deg >= 360) hue_deg -= 360;

    int size = 30 + (int)(af.rms * 280);  // 30..310 px
    lv_obj_set_size(disc, size, size);
    lv_obj_align(disc, LV_ALIGN_CENTER, 0, 0);
    lv_color_t col = lv_color_hsv_to_rgb((uint16_t)hue_deg, 100, 100);
    lv_obj_set_style_bg_color(disc, col, 0);

    if (af.transient) flash_until = af.t_ms + 80;
    if (af.t_ms < flash_until) {
        lv_obj_set_style_bg_opa(flash, LV_OPA_50, 0);
    } else {
        lv_obj_set_style_bg_opa(flash, LV_OPA_TRANSP, 0);
    }

    uint32_t rgb = lv_color_to32(col);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >>  8) & 0xFF;
    uint8_t b =  rgb        & 0xFF;
    uint8_t v = (uint8_t)(af.rms * 255.0f);
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) {
        rgb_ring_set(i, (r * v) / 255, (g * v) / 255, (b * v) / 255);
    }
}

static void viz_deinit() {
    if (disc)  { lv_obj_del(disc);  disc  = nullptr; }
    if (flash) { lv_obj_del(flash); flash = nullptr; }
    rgb_ring_clear();
}

extern const Visualizer VIZ_PEAK_METER = {
    "Peak Meter", viz_init, viz_render, viz_deinit
};
