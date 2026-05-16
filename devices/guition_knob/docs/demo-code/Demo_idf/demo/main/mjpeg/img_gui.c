#include "stdio.h"
#include "esp_log.h"
#include "lvgl.h"

#include "mjpeg_fram.h"
#include "mjpeg_gui.h"
#include "sd_card.h"

#define TAG     "IMG"
#define IMG_PATH    "/sdcard/pic"

static const char (*image_filename)[256] = NULL;
static int file_cnt = 0;
static int file_max = 0;
static lv_obj_t *img = NULL;
static lv_timer_t *timer = NULL;

static void img_timer_callback(lv_timer_t *tmr)
{
    file_cnt++;
    if(file_cnt > file_max)
        file_cnt = 0;
    char name[64];
    snprintf(name,sizeof(name),"A:/pic/%s",image_filename[file_cnt]);
    lv_img_set_src(img,name);
}

void img_gui()
{
    file_max = sdcard_filelist(IMG_PATH,&image_filename);
    ESP_LOGI(TAG,"file name : %s",image_filename[0]);
    if(file_max > 20)
        file_max = 20;

    img = lv_img_create(lv_scr_act());
    char name[64];
    snprintf(name,sizeof(name),"A:/pic/%s",image_filename[0]);
    lv_img_set_src(img,name);
    timer = lv_timer_create(img_timer_callback,5 * 1000,NULL);
}

