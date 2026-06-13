#include <Arduino.h>
#include "guition_lvgl.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nGuition JC3636K718 - Rich_Telemetry (stub)");
    guition_lvgl_init();
}

void loop() {
    lv_timer_handler();
    delay(5);
}
