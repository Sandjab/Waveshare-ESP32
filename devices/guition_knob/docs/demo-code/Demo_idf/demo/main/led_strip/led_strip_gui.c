#include "esp_log.h"
#include "esp_err.h"

#include "lvgl.h"
#include "led_strip_gui.h"
#include "led_strip_init.h"

#define TAG         "led strip gui"

static void set_led_strip_color_cb(lv_event_t *e);

static lv_obj_t *screen1 = NULL;
static lv_obj_t *rgb_colorwheel = NULL;
static lv_obj_t *label1 = NULL;


void led_strip_gui()
{
    screen1 = lv_obj_create(NULL);
    lv_obj_set_size(screen1,360,360);
    
    label1 = lv_label_create(screen1);
    lv_label_set_text(label1,"Long press to change color");
    lv_obj_align(label1,LV_ALIGN_TOP_MID,0,10);

    rgb_colorwheel = lv_colorwheel_create(screen1,true);
    lv_obj_center(rgb_colorwheel);
    lv_obj_add_event_cb(rgb_colorwheel,set_led_strip_color_cb,LV_EVENT_VALUE_CHANGED,NULL);

    lv_scr_load(screen1);
}

static void set_led_strip_color_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED){
        lv_colorwheel_mode_t mode = lv_colorwheel_get_color_mode(obj);
        if(mode == LV_COLORWHEEL_MODE_SATURATION){
            lv_color_hsv_t color = lv_colorwheel_get_hsv(obj);
            // lv_color16_t color2 = lv_color_hsv_to_rgb(color.h,color.s,color.v);
            // uint16_t green = color2.ch.green_h << 3 | color2.ch.green_l;
            // _led_strip_set_pixel_rgb(0,color2.ch.red,green,color2.ch.blue);
            _led_strip_set_pixel_hsv(0,color.h,color.s,color.v);
        } else {
            lv_color16_t color = lv_colorwheel_get_rgb(obj);
            uint16_t green = color.ch.green_h << 3 | color.ch.green_l;
            _led_strip_set_pixel_rgb(0,color.ch.red,green,color.ch.blue);
        }
        
    }
}