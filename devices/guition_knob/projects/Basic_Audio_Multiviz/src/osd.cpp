#include "osd.h"

#include <Arduino.h>
#include "lvgl.h"

static lv_obj_t* container = nullptr;
static lv_obj_t* label     = nullptr;
static int       total     = 0;
static uint32_t  shown_at  = 0;
static bool      active    = false;

static constexpr uint32_t HOLD_MS   = 800;
static constexpr uint32_t FADE_MS   = 400;
static constexpr uint32_t TOTAL_MS  = HOLD_MS + FADE_MS;

void osd_init(int total_vizs) {
    total = total_vizs;

    // Container fond noir semi-transparent sur layer top
    container = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, 220, 40);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_radius(container, 8, 0);
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(container, 6, 0);
    lv_obj_set_style_border_width(container, 0, 0);

    label = lv_label_create(container);
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "");

    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
}

void osd_show(int viz_index, const char* viz_name) {
    if (!container) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "%d/%d  -  %s", viz_index + 1, total, viz_name);
    lv_label_set_text(label, buf);
    lv_obj_set_style_bg_opa(container, LV_OPA_80, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
    shown_at = millis();
    active   = true;
}

void osd_tick() {
    if (!active || !container) return;
    uint32_t dt = millis() - shown_at;
    if (dt >= TOTAL_MS) {
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
        active = false;
        return;
    }
    if (dt > HOLD_MS) {
        // fade out linéaire 80%..0
        float t = (float)(dt - HOLD_MS) / FADE_MS;  // 0..1
        uint8_t bg_opa  = (uint8_t)((1.0f - t) * LV_OPA_80);
        uint8_t txt_opa = (uint8_t)((1.0f - t) * LV_OPA_COVER);
        lv_obj_set_style_bg_opa(container, bg_opa, 0);
        lv_obj_set_style_text_opa(label, txt_opa, 0);
    }
}
