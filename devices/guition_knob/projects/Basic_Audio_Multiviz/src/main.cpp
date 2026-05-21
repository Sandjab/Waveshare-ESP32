#include <Arduino.h>
#include "guition_lvgl.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Basic_Audio_Multiviz scaffolding OK");
    guition_lvgl_init(72);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
}

void loop() {
    lv_timer_handler();
    delay(5);
}
