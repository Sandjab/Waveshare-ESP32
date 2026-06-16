#include "view.h"
#include "color.h"
#include "nav_input.h"
#include <lvgl.h>
#include <string.h>
#include <cstdio>
#include <math.h>
#include "format.h"

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
    if (px >= 48) return &lv_font_montserrat_48;
    if (px >= 36) return &lv_font_montserrat_36;
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

// Place les labels d'une couronne autour de son ouverture. L'ouverture est centrée
// sur l'angle (90 + start_angle) en convention LVGL (0=droite, 90=bas, horaire).
// - cap (légende/countdown) : DANS l'ouverture.
// - slot2 : soit la pastille (à l'opposé de l'ouverture), soit la lecture centrale
//   (au centre géométrique) selon slot2_center.
// À rappeler après chaque set_text (LVGL recentre sur la taille réelle du label).
static void ring_place_labels(lv_obj_t* arc, lv_obj_t* cap, lv_obj_t* slot2,
                              const Placement& q, bool slot2_center) {
    const float DEG2RAD = 0.01745329252f;
    int r = q.radius - q.thickness;
    if (cap) {
        float a = (90 + q.start_angle) * DEG2RAD;          // dans l'ouverture
        lv_obj_align_to(cap, arc, LV_ALIGN_CENTER,
                        (int)roundf(r * cosf(a)), (int)roundf(r * sinf(a)));
    }
    if (slot2) {
        if (slot2_center) {
            lv_obj_align_to(slot2, arc, LV_ALIGN_CENTER, 0, 0);   // centre
        } else {
            int rp = q.radius - q.thickness / 2;           // pill centrée sur l'épaisseur de la bande
            float a = (270 + q.start_angle) * DEG2RAD;     // opposé de l'ouverture
            lv_obj_align_to(slot2, arc, LV_ALIGN_CENTER,
                            (int)roundf(rp * cosf(a)), (int)roundf(rp * sinf(a)));
        }
    }
}

static void build_ring(lv_obj_t* parent, Component& c, Placement& q,
                       lv_obj_t** main, lv_obj_t** cap, lv_obj_t** pill) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, q.radius * 2, q.radius * 2);
    lv_obj_center(arc);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(arc, 90 + q.start_angle + q.gap_deg / 2,
                              90 + q.start_angle - q.gap_deg / 2);
    lv_arc_set_range(arc, c.vmin, c.vmax);
    lv_obj_set_style_arc_width(arc, q.thickness, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, q.thickness, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1F2937), LV_PART_MAIN);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);   // bord externe de la bande au bord du widget → milieu exact = radius - thickness/2 (sinon le padding par défaut décale la pill)
    *main = arc;

    *cap = lv_label_create(parent);
    lv_obj_set_style_text_font(*cap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(*cap, lv_color_hex(c.color), 0);
    lv_label_set_text(*cap, "");

    if (c.center_pct) {                       // lecture centrale (prioritaire sur la pastille)
        *pill = lv_label_create(parent);
        lv_obj_set_style_text_font(*pill, pick_font(c.font), 0);
        lv_obj_set_style_text_color(*pill, lv_color_hex(c.color), 0);
        lv_label_set_text(*pill, "");
    } else if (c.pill) {                       // pastille de pourcentage
        *pill = lv_label_create(parent);
        lv_obj_set_style_text_font(*pill, &lv_font_montserrat_14, 0);
        lv_obj_set_style_bg_opa(*pill, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(*pill, lv_color_hex(c.color), 0);
        lv_obj_set_style_text_color(*pill, lv_color_hex(0x04121A), 0);
        lv_obj_set_style_radius(*pill, 13, 0);
        lv_obj_set_style_border_width(*pill, 1, 0);                       // contour noir 1px : détache la pill d'un anneau plein de même couleur
        lv_obj_set_style_border_color(*pill, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_opa(*pill, LV_OPA_COVER, 0);
        // Hauteur de la pill = max(hauteur actuelle, thickness+4) → déborde la bande de ≥2px en haut/bas.
        // Obtenue par padding vertical symétrique (garde le % centré) ; plancher 3 = hauteur actuelle.
        int lh = lv_font_get_line_height(&lv_font_montserrat_14);
        int pv = (q.thickness + 4 - lh) / 2;
        if (pv < 3) pv = 3;
        lv_obj_set_style_pad_hor(*pill, 8, 0); lv_obj_set_style_pad_ver(*pill, pv, 0);
        lv_label_set_text(*pill, "0%");
    }
    ring_place_labels(arc, *cap, (c.center_pct || c.pill) ? *pill : nullptr, q, c.center_pct);
}

// build/sync extraits des anciens switch de view_rebuild/view_sync, à l'identique.
// Signature commune : 3 slots LVGL (main + 2 sous-objets) car ring/bar sont multi-objets.
static void build_text(lv_obj_t* parent, Component& c, Placement& q,
                       lv_obj_t** main, lv_obj_t**, lv_obj_t**) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, pick_font(c.font), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(c.color), 0);
    lv_label_set_text(l, "");
    lv_obj_align(l, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = l;
}
static void build_bar(lv_obj_t* parent, Component& c, Placement& q,
                      lv_obj_t** main, lv_obj_t** sub1, lv_obj_t**) {
    lv_obj_t* b = lv_bar_create(parent);
    lv_obj_set_size(b, q.width ? q.width : 200, q.height ? q.height : 16);
    lv_bar_set_range(b, c.vmin, c.vmax);
    lv_obj_set_style_bg_color(b, lv_color_hex(c.color), LV_PART_INDICATOR);
    lv_obj_align(b, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = b;
    if (c.label[0]) {
        lv_obj_t* bl = lv_label_create(parent);
        lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(bl, lv_color_hex(0x9AA0AA), 0);
        lv_label_set_text(bl, c.label);
        lv_obj_align_to(bl, b, LV_ALIGN_OUT_TOP_MID, 0, -6);
        *sub1 = bl;
    }
}

static void sync_label(Component& c, Placement&, lv_obj_t* w, lv_obj_t*, lv_obj_t*) {
    lv_label_set_text(w, c.vstr);
}
static void sync_readout(Component& c, Placement&, lv_obj_t* w, lv_obj_t*, lv_obj_t*) {
    if (c.label[0]) {
        char rb[TEXT_LEN * 2];
        snprintf(rb, sizeof(rb), "%s %s", c.label, c.vstr);
        lv_label_set_text(w, rb);
    } else {
        lv_label_set_text(w, c.vstr);
    }
}
static void sync_bar(Component& c, Placement&, lv_obj_t* w, lv_obj_t*, lv_obj_t*) {
    lv_bar_set_value(w, c.value, LV_ANIM_OFF);
}
static void sync_ring(Component& c, Placement& q, lv_obj_t* w, lv_obj_t* sub1, lv_obj_t* sub2) {
    uint32_t col = threshold_color(c.thresholds, c.threshold_count, c.value, c.color);
    lv_obj_set_style_arc_color(w, lv_color_hex(col), LV_PART_INDICATOR);
    lv_arc_set_value(w, c.value);
    if (sub1) lv_label_set_text(sub1, c.caption);
    if (sub2) {
        if (c.center_pct) {
            char cb[24]; format_value((double)c.value, c.unit, cb, sizeof(cb));
            lv_label_set_text(sub2, cb);
            uint32_t ccol = c.center_color_set ? c.center_color : col;  // surcharge explicite, sinon suit le seuil
            lv_obj_set_style_text_color(sub2, lv_color_hex(ccol), 0);
        } else {
            char pb[8]; snprintf(pb, sizeof(pb), "%ld%%", (long)c.value);
            lv_label_set_text(sub2, pb);
            lv_obj_set_style_bg_color(sub2, lv_color_hex(col), 0);
        }
    }
    ring_place_labels(w, sub1, sub2, q, c.center_pct);
}

// --- chart : l'historique vit dans le modèle (Component.hist) ; build crée le widget,
// sync mirroir hist -> y_points (lv_chart_set_next_value n'est PAS idempotent). ---
static void build_chart(lv_obj_t* parent, Component& c, Placement& q,
                        lv_obj_t** main, lv_obj_t**, lv_obj_t**) {
    lv_obj_t* chart = lv_chart_create(parent);
    lv_obj_set_size(chart, q.width ? q.width : 200, q.height ? q.height : 100);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    int n = c.chart_points;
    if (n > CHART_MAX_POINTS) n = CHART_MAX_POINTS;
    if (n < 1) n = 1;
    lv_chart_set_point_count(chart, n);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, c.vmin, c.vmax);
    lv_chart_add_series(chart, lv_color_hex(c.color), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_align(chart, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = chart;
}
static void sync_chart(Component& c, Placement&, lv_obj_t* chart, lv_obj_t*, lv_obj_t*) {
    lv_chart_series_t* ser = lv_chart_get_series_next(chart, NULL);   // pas de stockage : on relit la 1re série
    if (!ser) return;
    int n = c.chart_points;
    if (n > CHART_MAX_POINTS) n = CHART_MAX_POINTS;
    if (n < 1) n = 1;
    for (int i = 0; i < n; i++)
        ser->y_points[i] = (i < c.hist_count) ? c.hist[i] : LV_CHART_POINT_NONE;
    lv_chart_refresh(chart);
}

// --- meter : jauge à aiguille ; thresholds réutilisés en zones d'arc.
// Handle aiguille stocké dans le slot sub1 (pas de getter d'indicateur côté lv_meter). ---
static void build_meter(lv_obj_t* parent, Component& c, Placement& q,
                        lv_obj_t** main, lv_obj_t** sub1, lv_obj_t**) {
    lv_obj_t* meter = lv_meter_create(parent);
    int sz = q.width ? q.width : 160;
    lv_obj_set_size(meter, sz, q.height ? q.height : sz);
    lv_meter_scale_t* scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 21, 2, 8, lv_color_hex(0x4B5563));
    lv_meter_set_scale_major_ticks(meter, scale, 5, 3, 12, lv_color_hex(0x9CA3AF), 10);
    lv_meter_set_scale_range(meter, scale, c.vmin, c.vmax, 270, 135);   // arc 270° ouvert en bas
    // zones d'arc depuis thresholds : bande i = (prev, limit[i]] couleur i ; prev démarre à vmin
    int prev = c.vmin;
    for (int i = 0; i < c.threshold_count; i++) {
        lv_meter_indicator_t* arc = lv_meter_add_arc(meter, scale, 5, lv_color_hex(c.thresholds[i].color), 0);
        lv_meter_set_indicator_start_value(meter, arc, prev);
        lv_meter_set_indicator_end_value(meter, arc, (int)c.thresholds[i].limit);
        prev = (int)c.thresholds[i].limit;
    }
    lv_meter_indicator_t* needle = lv_meter_add_needle_line(meter, scale, 4, lv_color_hex(c.color), -10);
    lv_meter_set_indicator_value(meter, needle, c.value);
    lv_obj_align(meter, ALIGN_MAP[q.anchor], q.dx, q.dy);
    *main = meter;
    *sub1 = (lv_obj_t*)(void*)needle;     // handle aiguille pour sync (cast opaque, jamais déréférencé en lv_obj_t)
}
static void sync_meter(Component& c, Placement&, lv_obj_t* meter, lv_obj_t* sub1, lv_obj_t*) {
    lv_meter_indicator_t* needle = (lv_meter_indicator_t*)(void*)sub1;
    if (needle) lv_meter_set_indicator_value(meter, needle, c.value);
}

// Vtable vue indexée par CompType. Types physiques (led_ring/sound) : build/sync = nullptr
// (rendus par leur tick dédié -> le moteur les saute). label/readout partagent build_text.
struct ViewVTable {
    void (*build)(lv_obj_t* parent, Component& c, Placement& q,
                  lv_obj_t** main, lv_obj_t** sub1, lv_obj_t** sub2);
    void (*sync)(Component& c, Placement& q,
                 lv_obj_t* main, lv_obj_t* sub1, lv_obj_t* sub2);
};
static const ViewVTable VIEW[] = {
    /* COMP_NONE     */ { nullptr,    nullptr      },
    /* COMP_LABEL    */ { build_text, sync_label   },
    /* COMP_READOUT  */ { build_text, sync_readout },
    /* COMP_BAR      */ { build_bar,  sync_bar     },
    /* COMP_RING     */ { build_ring, sync_ring    },
    /* COMP_LED_RING */ { nullptr,    nullptr      },
    /* COMP_SOUND    */ { nullptr,    nullptr      },
    /* COMP_CHART    */ { build_chart, sync_chart },
    /* COMP_METER    */ { build_meter, sync_meter },
};
static_assert(sizeof(VIEW) / sizeof(VIEW[0]) == COMP_COUNT,
              "VIEW desync avec CompType : ajoute la ligne du nouveau type");

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
    s_dots = nullptr;  // freed by lv_obj_clean above; drop stale pointer
    lv_obj_set_style_bg_color(scr, lv_color_hex(d->background), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    s_dash_for_gesture = d;
    static bool s_gesture_cb_added = false;
    if (!s_gesture_cb_added) {
        lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, nullptr);
        s_gesture_cb_added = true;
    }
    memset(s_page_cont, 0, sizeof(s_page_cont));
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
            if ((unsigned)c.type < COMP_COUNT && VIEW[c.type].build)
                VIEW[c.type].build(cont, c, q, &s_widget[p][i], &s_sub1[p][i], &s_sub2[p][i]);
        }
    }
    // points indicateurs (au-dessus des conteneurs de page)
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
            if ((unsigned)c.type < COMP_COUNT && VIEW[c.type].sync)
                VIEW[c.type].sync(c, q, w, s_sub1[p][i], s_sub2[p][i]);
        }
    }
    for (int i = 0; i < d->comp_count; i++) d->components[i].dirty = false;
    d->values_dirty = false;
}
