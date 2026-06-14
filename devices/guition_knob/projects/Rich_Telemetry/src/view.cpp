#include "view.h"
#include "color.h"
#include "nav_input.h"
#include <lvgl.h>
#include <string.h>
#include <cstdio>

static lv_obj_t* s_page_cont[MAX_PAGES];
static lv_obj_t* s_widget[MAX_PAGES][MAX_PLACEMENTS_PER_PAGE];
static lv_obj_t* s_sub1  [MAX_PAGES][MAX_PLACEMENTS_PER_PAGE];
static lv_obj_t* s_sub2  [MAX_PAGES][MAX_PLACEMENTS_PER_PAGE];
static lv_obj_t* s_dots = nullptr;

static const lv_align_t ALIGN_MAP[] = {
    LV_ALIGN_CENTER, LV_ALIGN_TOP_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_LEFT_MID,
    LV_ALIGN_RIGHT_MID, LV_ALIGN_TOP_LEFT, LV_ALIGN_TOP_RIGHT, LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_RIGHT
};

static const lv_font_t* pick_font(uint16_t px) {
    if (px >= 28) return &lv_font_montserrat_28;
    if (px >= 20) return &lv_font_montserrat_20;
    return &lv_font_montserrat_14;
}

const char* view_default_layout() {
    return
      "{\"title\":\"Claude\",\"background\":\"#0B0B0F\",\"nav\":{\"wrap\":true},"
      "\"components\":{"
        "\"w5h\":{\"type\":\"ring\",\"color\":\"#38BDF8\",\"pill\":true,\"countdown\":true},"
        "\"w7d\":{\"type\":\"ring\",\"color\":\"#A78BFA\",\"pill\":true,\"countdown\":true},"
        "\"led\":{\"type\":\"led_ring\"},\"buzz\":{\"type\":\"sound\"}},"
      "\"pages\":[{\"name\":\"usage\",\"place\":["
        "{\"ref\":\"w5h\",\"radius\":176,\"thickness\":16,\"gap_deg\":70},"
        "{\"ref\":\"w7d\",\"radius\":141,\"thickness\":16,\"gap_deg\":70}]}]}";
}

// Place les labels d'une couronne : légende dans l'ouverture du bas, pastille sur le haut
// de la bande. Offset = rayon - épaisseur (bord interne de la bande) -> suit le diamètre.
// À rappeler après chaque set_text : LVGL recalcule alors le centrage sur la taille réelle
// du label (sinon il grandit vers la droite depuis sa position posée à vide -> décalé).
static void ring_place_labels(lv_obj_t* arc, lv_obj_t* cap, lv_obj_t* pill, const Placement& q) {
    int off = q.radius - q.thickness;
    if (cap)  lv_obj_align_to(cap,  arc, LV_ALIGN_CENTER, 0,  off);   // bas
    if (pill) lv_obj_align_to(pill, arc, LV_ALIGN_CENTER, 0, -off);   // haut
}

static void build_ring(lv_obj_t* parent, Component& c, Placement& q,
                       lv_obj_t** main, lv_obj_t** cap, lv_obj_t** pill) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, q.radius * 2, q.radius * 2);
    lv_obj_center(arc);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(arc, 90 + q.gap_deg / 2, 90 - q.gap_deg / 2);
    lv_arc_set_range(arc, c.vmin, c.vmax);
    lv_obj_set_style_arc_width(arc, q.thickness, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, q.thickness, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1F2937), LV_PART_MAIN);
    *main = arc;

    *cap = lv_label_create(parent);
    lv_obj_set_style_text_font(*cap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(*cap, lv_color_hex(c.color), 0);
    lv_label_set_text(*cap, "");

    if (c.pill) {
        *pill = lv_label_create(parent);
        lv_obj_set_style_bg_opa(*pill, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(*pill, lv_color_hex(c.color), 0);
        lv_obj_set_style_text_color(*pill, lv_color_hex(0x04121A), 0);
        lv_obj_set_style_radius(*pill, 13, 0);
        lv_obj_set_style_pad_hor(*pill, 8, 0); lv_obj_set_style_pad_ver(*pill, 3, 0);
        lv_label_set_text(*pill, "0%");
    }
    ring_place_labels(arc, *cap, c.pill ? *pill : nullptr, q);
}

// Swipe -> navigation. L'objet ecran persiste a travers les rebuilds (lv_obj_clean
// ne supprime que ses enfants), donc on n'enregistre le callback gesture qu'une fois.
static Dashboard* s_dash_for_gesture = nullptr;
static void gesture_cb(lv_event_t* e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (!s_dash_for_gesture) return;
    // Seuls les swipes latéraux naviguent : droite = suivant, gauche = précédent.
    // Haut/bas volontairement ignorés (réservés à une future page de config par swipe haut).
    if (dir == LV_DIR_RIGHT)     nav_goto_dir(s_dash_for_gesture, +1);
    else if (dir == LV_DIR_LEFT) nav_goto_dir(s_dash_for_gesture, -1);
}

void view_rebuild(Dashboard* d) {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(d->background), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    s_dash_for_gesture = d;
    static bool s_gesture_cb_added = false;
    if (!s_gesture_cb_added) {
        lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, nullptr);
        s_gesture_cb_added = true;
    }
    memset(s_widget, 0, sizeof(s_widget));
    memset(s_sub1, 0, sizeof(s_sub1)); memset(s_sub2, 0, sizeof(s_sub2));

    for (int p = 0; p < d->page_count; p++) {
        lv_obj_t* cont = lv_obj_create(scr);
        lv_obj_remove_style_all(cont);
        lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
        s_page_cont[p] = cont;

        for (int i = 0; i < d->pages[p].place_count; i++) {
            Placement& q = d->pages[p].places[i];
            Component& c = d->components[q.comp_index];
            switch (c.type) {
                case COMP_RING:
                    build_ring(cont, c, q, &s_widget[p][i], &s_sub1[p][i], &s_sub2[p][i]);
                    break;
                case COMP_LABEL:
                case COMP_READOUT: {
                    lv_obj_t* l = lv_label_create(cont);
                    lv_obj_set_style_text_font(l, pick_font(c.font), 0);
                    lv_obj_set_style_text_color(l, lv_color_hex(c.color), 0);
                    lv_label_set_text(l, "");
                    lv_obj_align(l, ALIGN_MAP[q.anchor], q.dx, q.dy);
                    s_widget[p][i] = l;
                    break;
                }
                case COMP_BAR: {
                    lv_obj_t* b = lv_bar_create(cont);
                    lv_obj_set_size(b, q.width ? q.width : 200, q.height ? q.height : 16);
                    lv_bar_set_range(b, c.vmin, c.vmax);
                    lv_obj_set_style_bg_color(b, lv_color_hex(c.color), LV_PART_INDICATOR);
                    lv_obj_align(b, ALIGN_MAP[q.anchor], q.dx, q.dy);
                    s_widget[p][i] = b;
                    break;
                }
                default: break;
            }
        }
    }
    // points indicateurs (au-dessus des conteneurs de page)
    if (s_dots) { lv_obj_del(s_dots); s_dots = nullptr; }
    if (d->page_count > 1) {
        s_dots = lv_obj_create(scr);
        lv_obj_remove_style_all(s_dots);
        lv_obj_set_size(s_dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(s_dots, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(s_dots, 8, 0);
        lv_obj_align(s_dots, LV_ALIGN_BOTTOM_MID, 0, -10);
        for (int p = 0; p < d->page_count; p++) {
            lv_obj_t* dot = lv_obj_create(s_dots);
            lv_obj_remove_style_all(dot);
            lv_obj_set_size(dot, 9, 9);
            lv_obj_set_style_radius(dot, 5, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(dot, lv_color_hex(0x374151), 0);
        }
    }
    view_show_page(d, d->active_page);
    d->layout_dirty = false;
    for (int i = 0; i < d->comp_count; i++) d->components[i].dirty = true;
    view_sync(d);
}

void view_show_page(Dashboard* d, int idx) {
    if (idx < 0 || idx >= d->page_count) return;
    d->active_page = idx;
    for (int p = 0; p < d->page_count; p++) {
        if (p == idx) lv_obj_clear_flag(s_page_cont[p], LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag(s_page_cont[p], LV_OBJ_FLAG_HIDDEN);
    }
    if (s_dots) {
        uint32_t n = lv_obj_get_child_cnt(s_dots);
        for (uint32_t p = 0; p < n; p++)
            lv_obj_set_style_bg_color(lv_obj_get_child(s_dots, p),
                lv_color_hex((int)p == idx ? 0xE5E7EB : 0x374151), 0);
    }
}

void view_sync(Dashboard* d) {
    for (int p = 0; p < d->page_count; p++) {
        for (int i = 0; i < d->pages[p].place_count; i++) {
            Placement& q = d->pages[p].places[i];
            Component& c = d->components[q.comp_index];
            if (!c.dirty) continue;
            lv_obj_t* w = s_widget[p][i];
            if (!w) continue;
            switch (c.type) {
                case COMP_LABEL:
                case COMP_READOUT:
                    lv_label_set_text(w, c.vstr);
                    break;
                case COMP_BAR:
                    lv_bar_set_value(w, c.value, LV_ANIM_OFF);
                    break;
                case COMP_RING: {
                    uint32_t col = threshold_color(c.thresholds, c.threshold_count, c.value, c.color);
                    lv_obj_set_style_arc_color(w, lv_color_hex(col), LV_PART_INDICATOR);
                    lv_arc_set_value(w, c.value);
                    if (s_sub1[p][i]) lv_label_set_text(s_sub1[p][i], c.caption);
                    if (s_sub2[p][i]) {
                        char pb[8]; snprintf(pb, sizeof(pb), "%ld%%", (long)c.value);
                        lv_label_set_text(s_sub2[p][i], pb);
                        lv_obj_set_style_bg_color(s_sub2[p][i], lv_color_hex(col), 0);
                    }
                    ring_place_labels(w, s_sub1[p][i], s_sub2[p][i], q);  // re-centre après set_text
                    break;
                }
                default: break;
            }
        }
    }
    for (int i = 0; i < d->comp_count; i++) d->components[i].dirty = false;
    d->values_dirty = false;
}
