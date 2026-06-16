#include "ui.h"
#include <lvgl.h>
#include <stdio.h>
#include "audio_engine.h"
#include "sounds.h"
#include "config.h"

// Palette (ASCII-only pour les polices Montserrat embarquées).
static const lv_color_t COL_BG     = LV_COLOR_MAKE(0x0B, 0x0B, 0x0F);
static const lv_color_t COL_ACCENT = LV_COLOR_MAKE(0x38, 0xBD, 0xF8);
static const lv_color_t COL_TEXT   = LV_COLOR_MAKE(0xE5, 0xE7, 0xEB);
static const lv_color_t COL_DIM    = LV_COLOR_MAKE(0x64, 0x74, 0x8B);

#define MAX_CAT_DOTS 16

static lv_obj_t* s_arc       = nullptr;
static lv_obj_t* s_title     = nullptr;
static lv_obj_t* s_list      = nullptr;
static lv_obj_t* s_vol_label = nullptr;
static lv_obj_t* s_dots[MAX_CAT_DOTS];
static int       s_dot_count = 0;

static int  s_category  = 0;     // catégorie (page) affichée
static int  s_nav_req   = 0;     // catégorie demandée par un swipe (appliquée dans ui_tick)
static bool s_nav_dirty = false; // un swipe attend d'être appliqué

// Tap sur un item -> joue le son (index global passé en user_data).
static void item_event_cb(lv_event_t* e) {
    int gi = (int)(intptr_t)lv_event_get_user_data(e);
    audio_play(gi);
}

// Swipe latéral -> change de catégorie. On NE rebâtit PAS la liste ici (on
// supprimerait le bouton en cours de traitement) : on note la demande et ui_tick
// l'applique dans la boucle principale. Convention du repo (cf. Rich_Telemetry) :
// droite = suivant, gauche = précédent.
static void gesture_cb(lv_event_t*) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    int n = categories_count();
    if (n <= 1) return;
    if (dir == LV_DIR_RIGHT)      { s_nav_req = (s_category + 1) % n;     s_nav_dirty = true; }
    else if (dir == LV_DIR_LEFT)  { s_nav_req = (s_category + n - 1) % n; s_nav_dirty = true; }
}

// Reconstruit la liste + le titre + les points pour la catégorie c.
static void show_category(int c) {
    const SoundCategory* cat = category_get(c);
    if (!cat) return;
    s_category = c;

    lv_obj_clean(s_list);                                  // retire les boutons précédents
    for (int i = 0; i < cat->count; i++) {
        int gi = cat->start + i;
        lv_obj_t* btn = lv_list_add_btn(s_list, nullptr, sounds_name(gi));
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_20, 0);
        lv_obj_set_style_bg_color(btn, COL_BG, 0);
        lv_obj_set_style_text_color(btn, COL_TEXT, 0);
        lv_obj_add_event_cb(btn, item_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)gi);
    }
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);            // remonte en haut de la liste

    lv_label_set_text(s_title, cat->name);
    for (int i = 0; i < s_dot_count; i++) {
        lv_obj_set_style_bg_color(s_dots[i], i == c ? COL_ACCENT : COL_DIM, 0);
    }
}

void ui_build() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Arc de volume au pourtour (indicateur seul).
    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, 348, 348);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    lv_arc_set_range(s_arc, 0, AUDIO_VOL_STEPS);
    lv_obj_remove_style(s_arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, COL_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, COL_ACCENT, LV_PART_INDICATOR);

    // Liste des sons (centre), scroll vertical uniquement (laisse l'horizontale au swipe).
    s_list = lv_list_create(scr);
    lv_obj_set_size(s_list, 224, 188);
    lv_obj_align(s_list, LV_ALIGN_CENTER, 0, 6);
    lv_obj_set_style_bg_color(s_list, COL_BG, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);

    // Titre de catégorie (en haut).
    s_title = lv_label_create(scr);
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_title, COL_TEXT, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 38);

    // Rangée de points indicateurs de page (une par catégorie).
    int n = categories_count();
    s_dot_count = (n > MAX_CAT_DOTS) ? MAX_CAT_DOTS : n;
    const int SP = 16;
    int x0 = -((s_dot_count - 1) * SP) / 2;
    for (int i = 0; i < s_dot_count; i++) {
        lv_obj_t* d = lv_obj_create(scr);
        lv_obj_set_size(d, 8, 8);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(d, 0, 0);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(d, LV_ALIGN_TOP_MID, x0 + i * SP, 18);
        s_dots[i] = d;
    }

    // Libellé de volume (en bas).
    s_vol_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_vol_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_vol_label, COL_ACCENT, 0);
    lv_obj_align(s_vol_label, LV_ALIGN_BOTTOM_MID, 0, -34);

    // Navigation par swipe (callback sur l'écran, ajouté une seule fois).
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, nullptr);

    show_category(0);
    ui_set_volume(audio_get_volume());
}

void ui_set_volume(int step) {
    if (s_arc) lv_arc_set_value(s_arc, step);
    if (s_vol_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "Vol %d/%d", step, AUDIO_VOL_STEPS);
        lv_label_set_text(s_vol_label, buf);
    }
}

void ui_tick() {
    if (!s_nav_dirty) return;
    s_nav_dirty = false;
    show_category(s_nav_req);     // rebuild différé, hors du callback gesture
}
