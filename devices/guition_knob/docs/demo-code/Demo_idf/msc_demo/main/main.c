#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"
#include "lv_demos.h"
#include "esp_lv_adapter.h"
#include "lcd_init.h"
#include "backlight.h"
#include "led_strip_init.h"
#include "led_strip_gui.h"
#include "mjpeg_gui.h"
#include "sd_card.h"
#include "music_gui.h"
#include "msc_gui.h"


#define TAG     "main"

//将需要的例子修改为1，一次只建议开启一个
#define LV_DEMO_WIDGETS     0       //lvgl demo
#define LED_STRIP_DEMO      0       //灯带
#define KNOB_DEMO           0       //旋钮控制电脑,,长按屏幕唤醒Windows wheel，旋转旋钮切换，点击屏幕确认
#define MJPEG_DEMO          0       //播放sd卡里的mjpeg视频
#define IMAGE_DEMO          0       //显示SD卡中的图片
#define MUSIC_DEMO          0       //播放sd卡中的音乐,本例子使用的mp3解码库可能解码失败部分音频，如果打印大量的"mp3: status error -6“,建议跳过或者使用其他的解码库
#define MSC_DEMO            1       //U盘模式

void app_main(void)
{
    lcd_init();

    lcd_backlight_init();
    lcd_backlight_on();

    sdcard_mount();

    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to lock LVGL for UI creation");
        return;
    }
#if LV_DEMO_WIDGETS
    lv_demo_widgets();
#endif

#if LED_STRIP_DEMO
    led_strip_init();
    led_strip_gui();
#endif

#if MJPEG_DEMO
    mjepg_gui();
#endif

#if IMAGE_DEMO
    img_gui();
#endif

#if MUSIC_DEMO
    music_init();
    play_music_gui();
#endif

#if MSC_DEMO
    msc_gui();
#endif

    esp_lv_adapter_unlock();
}
