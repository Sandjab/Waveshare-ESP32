#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "lvgl.h"
#include "knob_gui.h"

static bool knob_init = false;

static void switch_evnet_cb(lv_event_t *e)
{
    if(!knob_init){
        knob_init = true;
        xTaskCreatePinnedToCore(knob_hid_init,"Knob",8192,NULL,5,NULL,1);
    }
}

static void screen_btn_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(!knob_init)
        return;
    
    if(code == LV_EVENT_LONG_PRESSED){
        surface_dial_report(DIAL_PRESS);
    } else if(code == LV_EVENT_PRESSED) {
        surface_dial_report(DIAL_PRESS);
    }   else if(code == LV_EVENT_RELEASED) {
        surface_dial_report(DIAL_RELEASE);
    }
}

void knob_hid_gui(void)
{
    lv_obj_t *obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj,360,360);
    lv_obj_add_flag(obj,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj,screen_btn_event,LV_EVENT_ALL,NULL);

    lv_obj_t *btn = lv_switch_create(obj);
    lv_obj_set_size(btn,100,50);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn,switch_evnet_cb,LV_EVENT_VALUE_CHANGED,NULL);
}