#pragma once
#include <stdint.h>
#include <stddef.h>
#include "config.h"

enum CompType { COMP_NONE, COMP_LABEL, COMP_READOUT, COMP_BAR, COMP_RING, COMP_LED_RING, COMP_SOUND };
enum LedMode  { LED_OFF, LED_SOLID, LED_PROGRESS, LED_SPINNER, LED_BLINK, LED_BREATHE };
enum Anchor   { A_CENTER, A_TOP_MID, A_BOTTOM_MID, A_LEFT_MID, A_RIGHT_MID,
                A_TOP_LEFT, A_TOP_RIGHT, A_BOTTOM_LEFT, A_BOTTOM_RIGHT };

struct Threshold { float limit; uint32_t color; };

struct Component {
    char     id[ID_LEN];
    CompType type;

    // --- config (style/donnees, sans position) ---
    char     label[TEXT_LEN];
    char     unit[8];
    char     text[TEXT_LEN];
    uint32_t color;
    int32_t  vmin, vmax;
    bool     pill, center_pct, countdown;
    Threshold thresholds[MAX_THRESHOLDS];
    int      threshold_count;
    uint16_t font;
    uint8_t  led_brightness_cfg;

    // --- etat (modifie par /update) ---
    int32_t  value;
    char     vstr[TEXT_LEN];
    uint32_t reset_in_s;
    char     caption[CAPTION_LEN];
    LedMode  led_mode; uint32_t led_color; uint8_t led_value, led_brightness; uint16_t led_period_ms;
    bool     snd_pending; uint16_t snd_tone; uint16_t snd_ms; char snd_name[12];

    bool     dirty;
};

struct Placement {
    int     comp_index;
    Anchor  anchor; int16_t dx, dy; int16_t width, height;
    int16_t radius, thickness, gap_deg, start_angle;
};

struct Page {
    char      name[ID_LEN];
    Placement places[MAX_PLACEMENTS_PER_PAGE];
    int       place_count;
};

struct Dashboard {
    char      title[TEXT_LEN];
    uint32_t  background;
    bool      nav_wrap;
    Component components[MAX_COMPONENTS];
    int       comp_count;
    Page      pages[MAX_PAGES];
    int       page_count;
    int       active_page;
    bool      layout_dirty;
    bool      values_dirty;
};

int  dash_find(const Dashboard* d, const char* id);
bool dash_set_layout(Dashboard* d, const char* json, char* err, size_t errn);
int  dash_apply_update(Dashboard* d, const char* json, char* unknown_csv, size_t n);
void dash_tick_countdown(Dashboard* d, uint32_t elapsed_s);
