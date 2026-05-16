#include "stdio.h"
#include "esp_log.h"
#include "lvgl.h"

#include "mjpeg_fram.h"
#include "mjpeg_gui.h"
#include "sd_card.h"

#define TAG             "mjpeg"
#define MJPEG_PATH      "/sdcard/mjpeg"


static const char (*mjpeg_filename)[256] = NULL;
static int file_cnt = 0;
static int file_max = 0;
static lv_obj_t *screen = NULL;
static lv_timer_t *timer = NULL;


void lv_timer_cb(lv_timer_t *t)
{
    static jpeg_frame_data_t frame_data = {0,0};
    if(frame_data.frame)
    {
        free(frame_data.frame);
        frame_data.frame = NULL;
        frame_data.len = 0;
    }
    //获取一帧jpg图像
    jpeg_frame_get_one(&frame_data);
    if(frame_data.len)
    {
        static lv_img_dsc_t mjpeg_img_dsc;
        memset(&mjpeg_img_dsc,0,sizeof(lv_img_dsc_t));
        // uint16_t mjpeg_width = 0;
        // uint16_t mjpeg_height = 0;
        // int mjpeg_data_size;

        mjpeg_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        mjpeg_img_dsc.data = frame_data.frame;
        mjpeg_img_dsc.data_size = frame_data.len;
        // mjpeg_img_dsc.header.h = mjpeg_height;
        // mjpeg_img_dsc.header.w = mjpeg_width;
        lv_img_set_src(screen,&mjpeg_img_dsc);
    }
    else
    {
        file_cnt++;
        if(file_cnt > file_max)
            file_cnt = 0;
        jpeg_frame_start(mjpeg_filename[file_cnt]);
    }
}

void mjepg_gui()
{
    jpeg_frame_cfg_t  frame_cfg = 
    {
        .buff_size = 100*1024,
    };
    jpeg_frame_config(&frame_cfg);

    file_max = sdcard_filelist(MJPEG_PATH,&mjpeg_filename);
    ESP_LOGI(TAG,"file name : %s",mjpeg_filename[0]);
    if(file_max > 20)
        file_max = 20;
    
    screen = lv_img_create(lv_scr_act());
    timer = lv_timer_create(lv_timer_cb,40,NULL);
} 