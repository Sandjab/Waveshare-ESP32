#include <Arduino.h>
#include "guition_lvgl.h"
#include "touch_cst816.h"
#include "audio_engine.h"
#include "ui.h"
#include "encoder_input.h"

// Rich_Audio — banc d'essai de sons sur le Guition JC3636K718.
//  - Liste tactile (LVGL) pour parcourir et déclencher les sons (Harp, Ping).
//  - Encodeur rotatif = volume maître.
//  - Moteur audio dans une tâche FreeRTOS dédiée (cœur 0), découplé de LVGL.

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nGuition JC3636K718 - Rich_Audio");

    guition_lvgl_init();    // display + LVGL (lv_init)
    touch_begin();          // CST816 -> indev tactile LVGL (no-op si absent)
    audio_begin();          // I2S + tâche audio (ampli muet jusqu'au premier son)
    ui_build();             // construit l'UI à partir du registre de sons
    encoder_begin();        // volume via encodeur

    lv_timer_handler();
}

void loop() {
    encoder_tick();
    ui_tick();
    lv_timer_handler();
    delay(5);
}
