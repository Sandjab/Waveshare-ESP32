#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_DRV2605.h>
#include "guition_lvgl.h"
#include "bidi_switch_knob.h"
#include "rgb_ring.h"

Adafruit_NeoPixel rgb_ring(RGB_RING_LED_COUNT, PIN_RGB_DATA, NEO_GRB + NEO_KHZ800);

// Port du Hue_Encoder Knob -> Guition K718. Code identique sauf les deux
// includes device-specific (guition_lvgl.h + guition_pins.h via le header).
// L'I2C bus du Guition (SDA:9, SCL:10) est partage entre CST816 touch (0x15)
// et DRV2605 haptics (0x5A), donc une seule init Wire suffit pour les deux.

// ---- Hue step per encoder count ----
#define HUE_STEP      5

// ---- Globals ----
static esp_lcd_panel_handle_t panel_handle = NULL;

static Adafruit_DRV2605 drv;
static bool drv_ok = false;

static bool haptic_on = true;
static bool touch_down = false;
static uint32_t last_toggle_ms = 0;
static lv_obj_t *haptic_icon = NULL;
#define CST816_ADDR        0x15
#define TOGGLE_COOLDOWN_MS 200

// ---- Encoder (bidi_switch_knob driver) ----
static volatile int32_t enc_position = 0;

static lv_obj_t *bg_obj = NULL;
static lv_obj_t *dot_obj = NULL;
static lv_obj_t *hex_label = NULL;
static lv_obj_t *count_label = NULL;

static int16_t hue = 0;
static int32_t last_count = 0;

// ---- HSV to RGB (S=255, V=255) ----
static void hsv_to_rgb(uint16_t h, uint8_t &r, uint8_t &g, uint8_t &b) {
    h = h % 360;
    uint16_t sector = h / 60;
    uint16_t frac = (h % 60) * 255 / 60;
    uint8_t q = 255 - frac;
    uint8_t t = frac;

    switch (sector) {
        case 0:  r = 255; g = t;   b = 0;   break;
        case 1:  r = q;   g = 255; b = 0;   break;
        case 2:  r = 0;   g = 255; b = t;   break;
        case 3:  r = 0;   g = q;   b = 255; break;
        case 4:  r = t;   g = 0;   b = 255; break;
        default: r = 255; g = 0;   b = q;   break;
    }
}

// ---- Perceived brightness (BT.601) ----
static bool is_dark(uint8_t r, uint8_t g, uint8_t b) {
    return (r * 299 + g * 587 + b * 114) < 128000;
}

// ---- Touch active (CST816 poll) ----
static bool touch_active() {
    uint8_t buf[7];
    Wire.beginTransmission(CST816_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom(CST816_ADDR, 7) != 7) return false;
    for (int i = 0; i < 7; i++) buf[i] = Wire.read();
    return buf[2] > 0;
}

// ---- Haptic tick ----
static void haptic_tick() {
    if (!drv_ok || !haptic_on) return;
    drv.setWaveform(0, 17);  // Strong Click 1
    drv.setWaveform(1, 0);
    drv.go();
}

// ---- Update display for current hue ----
static void update_color() {
    uint8_t r, g, b;
    hsv_to_rgb(hue, r, g, b);

    // Background
    lv_obj_set_style_bg_color(bg_obj, lv_color_make(r, g, b), 0);

    // Auto-contrast: white on dark, black on light
    bool dark = is_dark(r, g, b);
    lv_color_t fg = dark ? lv_color_white() : lv_color_black();
    lv_obj_set_style_bg_color(dot_obj, fg, 0);
    lv_obj_set_style_text_color(hex_label, fg, 0);
    lv_obj_set_style_text_color(count_label, fg, 0);
    if (haptic_icon) lv_obj_set_style_text_color(haptic_icon, fg, 0);

    // Update hex string
    char hex[10];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    lv_label_set_text(hex_label, hex);

    // Mirror the screen color onto the 13-LED ring
    rgb_ring_set_all(r, g, b);
    rgb_ring_show();
}

// ---- Setup ----
void setup() {
    Serial.begin(115200);
    Serial.println("Guition JC3636K718 — Hue_Encoder: Hue Wheel + Haptic Toggle");

    panel_handle = guition_lvgl_init();

    // RGB ring (mirrors screen color). 128/255 ≈ 50 %, well under USB 500 mA.
    rgb_ring_init(128);

    // Build UI
    Serial.println("Build UI...");

    // Full-screen background
    bg_obj = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(bg_obj);
    lv_obj_set_size(bg_obj, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_bg_opa(bg_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bg_obj, lv_color_make(255, 0, 0), 0);

    // Center dot (semi-transparent contrast circle)
    dot_obj = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(dot_obj);
    lv_obj_set_size(dot_obj, 100, 100);
    lv_obj_align(dot_obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(dot_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot_obj, LV_OPA_40, 0);
    lv_obj_set_style_bg_color(dot_obj, lv_color_white(), 0);

    // Hex label
    hex_label = lv_label_create(lv_scr_act());
    lv_obj_align(hex_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(hex_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(hex_label, lv_color_white(), 0);
    lv_label_set_text(hex_label, "#FF0000");

    // Counter label (bottom of screen)
    count_label = lv_label_create(lv_scr_act());
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(count_label, lv_color_white(), 0);
    lv_label_set_text(count_label, "0");

    // Haptic icon (top of screen)
    haptic_icon = lv_label_create(lv_scr_act());
    lv_obj_align(haptic_icon, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_text_font(haptic_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(haptic_icon, lv_color_white(), 0);
    lv_label_set_text(haptic_icon, LV_SYMBOL_VOLUME_MID);

    // Paint screen + ring with the initial hue (0 = red)
    update_color();

    // Encoder (bidi_switch_knob driver, timer-polled)
    Serial.println("Init encoder...");
    knob_config_t enc_cfg = {
        .gpio_encoder_a = PIN_ENC_A,
        .gpio_encoder_b = PIN_ENC_B,
    };
    knob_handle_t knob = iot_knob_create(&enc_cfg);
    iot_knob_register_cb(knob, KNOB_RIGHT, [](void *, void *) {
        enc_position = enc_position + 1;
    }, NULL);
    iot_knob_register_cb(knob, KNOB_LEFT, [](void *, void *) {
        enc_position = enc_position - 1;
    }, NULL);

    // I2C bus (shared: CST816 touch + DRV2605 haptics)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // DRV2605 haptics
    Serial.println("Init DRV2605...");
    if (drv.begin()) {
        drv.useLRA();
        drv.selectLibrary(6);
        drv.setMode(DRV2605_MODE_INTTRIG);
        drv_ok = true;
        Serial.println("DRV2605 OK (LRA, library 6)");
    } else {
        Serial.println("DRV2605 not found — haptics disabled");
    }

    Serial.println("Ready. Rotate the knob! Tap screen to toggle haptics.");
}

// ---- Loop ----
void loop() {
    int32_t count = enc_position;
    if (count != last_count) {
        int32_t diff = count - last_count;
        last_count = count;

        hue = (hue + (int)(diff * HUE_STEP)) % 360;
        if (hue < 0) hue += 360;

        update_color();
        haptic_tick();

        char buf[16];
        snprintf(buf, sizeof(buf), "%ld", (long)count);
        lv_label_set_text(count_label, buf);

        Serial.printf("Hue: %d  Count: %ld\n", hue, (long)count);
    }

    // Touch toggle: haptic on/off on release
    bool now_touched = touch_active();
    if (touch_down && !now_touched) {
        uint32_t now = millis();
        if (now - last_toggle_ms > TOGGLE_COOLDOWN_MS) {
            haptic_on = !haptic_on;
            lv_label_set_text(haptic_icon,
                haptic_on ? LV_SYMBOL_VOLUME_MID : LV_SYMBOL_MUTE);
            last_toggle_ms = now;
            Serial.printf("Haptic: %s\n", haptic_on ? "ON" : "OFF");
        }
    }
    touch_down = now_touched;

    lv_timer_handler();
    delay(5);
}
