#include "esp_log.h"
#include "lvgl.h"
#include "msc_gui.h"

static bool _msc_init = false;

static void switch_evnet_cb(lv_event_t *e)
{
    if(!_msc_init){
        _msc_init = true;
        msc_init();
    }
}


void msc_gui(void)
{
    lv_obj_t *obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj,360,360);
    lv_obj_add_flag(obj,LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn = lv_switch_create(obj);
    lv_obj_set_size(btn,100,50);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn,switch_evnet_cb,LV_EVENT_VALUE_CHANGED,NULL);
}