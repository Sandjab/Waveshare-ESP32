#include <Arduino.h>
#include "lvgl.h"
#include "guition_pins.h"
#include "rgb_ring.h"
#include "viz_api.h"

static constexpr int N_COLS    = 18;
static constexpr int COL_W     = LCD_H_RES / N_COLS;  // 20 px
static constexpr int CHAR_H    = 18;
static constexpr int CHARS_PER_COL = LCD_V_RES / CHAR_H + 1;  // ~21

static lv_obj_t*  cols[N_COLS];
static lv_style_t col_styles[N_COLS];
static float      head_y[N_COLS];   // 0..LCD_V_RES (position de la "tête")
static float      vel[N_COLS];      // px/frame
// Chaque entrée stocke CHARS_PER_COL caractères séparés par '\n' + nul terminal.
static char       buf[N_COLS][CHARS_PER_COL * 2 + 1];

static char random_char() {
    static const char alpha[] = "0123456789ABCDEFGHJKLMNPRSTUVWXYZ";
    return alpha[esp_random() % (sizeof(alpha) - 1)];
}

static void viz_init() {
    for (int i = 0; i < N_COLS; i++) {
        lv_style_init(&col_styles[i]);
        lv_style_set_text_font(&col_styles[i], &lv_font_montserrat_14);
        lv_style_set_text_color(&col_styles[i], lv_color_make(0, 255, 80));
        lv_style_set_text_line_space(&col_styles[i], 4);

        cols[i] = lv_label_create(lv_scr_act());
        lv_obj_add_style(cols[i], &col_styles[i], 0);
        lv_obj_set_pos(cols[i], i * COL_W + 2, -CHAR_H * (esp_random() % CHARS_PER_COL));
        head_y[i] = -CHAR_H * (esp_random() % CHARS_PER_COL);
        vel[i]    = 1.0f + (esp_random() % 100) / 100.0f;

        for (int j = 0; j < CHARS_PER_COL; j++) {
            buf[i][j * 2]     = random_char();
            buf[i][j * 2 + 1] = '\n';
        }
        buf[i][CHARS_PER_COL * 2 - 1] = '\0';
        lv_label_set_text(cols[i], buf[i]);
    }
}

static void viz_render(const AudioFrame& af) {
    float high = 0;
    for (int i = 8; i < 13; i++) high += af.bands[i];
    high /= 5.0f;
    float global_speed = 0.5f + high * 6.0f;  // 0.5..6.5

    for (int i = 0; i < N_COLS; i++) {
        head_y[i] += vel[i] * global_speed;
        if (head_y[i] > LCD_V_RES + CHAR_H * CHARS_PER_COL) {
            head_y[i] = -CHAR_H * CHARS_PER_COL;
            for (int j = 0; j < CHARS_PER_COL; j++) {
                buf[i][j * 2] = random_char();
            }
            lv_label_set_text(cols[i], buf[i]);
        }
        lv_obj_set_pos(cols[i], i * COL_W + 2, (int)(head_y[i] - CHAR_H * CHARS_PER_COL));
    }

    uint8_t v = (uint8_t)(high * 255.0f);
    for (int i = 0; i < RGB_RING_LED_COUNT; i++) rgb_ring_set(i, 0, v, 0);
}

static void viz_deinit() {
    for (int i = 0; i < N_COLS; i++) {
        if (cols[i]) { lv_obj_del(cols[i]); cols[i] = nullptr; }
    }
    rgb_ring_clear();
}

extern const Visualizer VIZ_MATRIX_RAIN = {
    "Matrix Rain", viz_init, viz_render, viz_deinit
};
