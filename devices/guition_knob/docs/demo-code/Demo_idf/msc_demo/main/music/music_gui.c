#include "lvgl.h"
#include "esp_log.h"
#include "music_gui.h"


static void music_btn_event(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *label = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_VALUE_CHANGED){
        bool status = lv_obj_has_state(obj,LV_STATE_CHECKED);
        if(status){
            lv_label_set_text(label,"play");
            play_music();
        } else {
            lv_label_set_text(label,"pause");
            pause_music();
        } 
    }

}

void play_music_gui(void)
{
    lv_obj_t *screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen,360,360);

    lv_obj_t *btn = lv_btn_create(screen);
    lv_obj_set_size(btn,100,50);
    lv_obj_add_flag(btn,LV_OBJ_FLAG_CHECKABLE);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label,"play");
    lv_obj_center(label);
    lv_obj_center(btn);

    lv_obj_add_event_cb(btn,music_btn_event,LV_EVENT_VALUE_CHANGED,(void  *)label);
}