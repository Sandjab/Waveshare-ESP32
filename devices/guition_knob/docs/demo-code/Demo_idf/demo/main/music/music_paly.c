
#include <stdio.h>
#include "esp_check.h"
#include "audio_player.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "sd_card.h"
#include "pinconfig.h"

#define BSP_I2S_GPIO_CFG       \
    {                          \
        .mclk = -1,  \
        .bclk = PIN_I2S_BCK,  \
        .ws = PIN_I2S_WS,    \
        .dout = PIN_I2S_DO,  \
        .din = -1,   \
        .invert_flags = {      \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv = false,   \
        },                     \
    }

#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }

#define TAG             "music_play"
#define MUSIC_SHUTDOWN      (BIT0)
#define MUSIC_PATH      "/sdcard/music"

static const char (*music_filename)[256] = NULL;
static int file_cnt = 1;
static int file_max = 0;
static bool play_init = false;
static i2s_chan_handle_t i2s_tx_chan;
static i2s_chan_handle_t i2s_rx_chan;
static EventGroupHandle_t music_event;

static esp_err_t bsp_i2s_write(void * audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ret = i2s_channel_write(i2s_tx_chan, (char *)audio_buffer, len, bytes_written, timeout_ms);
    return ret;
}

static esp_err_t bsp_i2s_reconfig_clk(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t)bits_cfg, (i2s_slot_mode_t)ch),
        .gpio_cfg = BSP_I2S_GPIO_CFG,
    };

    ret |= i2s_channel_disable(i2s_tx_chan);
    ret |= i2s_channel_reconfig_std_clock(i2s_tx_chan, &std_cfg.clk_cfg);
    ret |= i2s_channel_reconfig_std_slot(i2s_tx_chan, &std_cfg.slot_cfg);
    ret |= i2s_channel_enable(i2s_tx_chan);
    return ret;
}

static esp_err_t audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting) {
    ESP_LOGI(TAG, "mute setting %d", setting);
    return ESP_OK;
}

static void audio_player_callback(audio_player_cb_ctx_t *ctx)
{
    if(ctx->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN){
        ESP_LOGI(TAG,"music shutdown");
         xEventGroupSetBits(music_event,MUSIC_SHUTDOWN); 
    } else if(ctx->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_PAUSE){
        ESP_LOGI(TAG,"music pause");
    } else if(ctx->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_PLAYING) {
        ESP_LOGI(TAG,"music playing");
    }
}

static void play_music_task(void *param)
{
    esp_err_t ret = ESP_OK;
    char *file_name = (char *)heap_caps_malloc(512,MALLOC_CAP_SPIRAM);
    while (1)
    {
        snprintf(file_name,512,"/sdcard/music/%s",music_filename[file_cnt]);
        FILE *fp = fopen(file_name, "rb");
        if(fp == NULL){
            ESP_LOGE(TAG,"open file failed");
        }
        ESP_LOGI(TAG,"play: %s",music_filename[file_cnt]);
        file_cnt++;
        if(file_cnt > file_max)
            file_cnt = 1;
        
        ESP_ERROR_CHECK(audio_player_play(fp));
        if(!play_init){
            ESP_ERROR_CHECK(audio_player_pause());
            play_init = true;
        }

        EventBits_t ev = xEventGroupWaitBits(music_event,MUSIC_SHUTDOWN,pdTRUE,pdFALSE,portMAX_DELAY);
        if(ev & MUSIC_SHUTDOWN)
        {
            fclose(fp);
            memset(file_name,0,512);
            xEventGroupClearBits(music_event,MUSIC_SHUTDOWN); 
        }
    }

    if(file_name != NULL){
        free(file_name);
        file_name = NULL;
    }
    vTaskDelete(NULL);
}

static esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config, i2s_chan_handle_t *tx_channel, i2s_chan_handle_t *rx_channel)
{
    /* Setup I2S peripheral */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, tx_channel, rx_channel));

    /* Setup I2S channels */
    const i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(22050);
    const i2s_std_config_t *p_i2s_cfg = &std_cfg_default;
    if (i2s_config != NULL) {
        p_i2s_cfg = i2s_config;
    }

    if (tx_channel != NULL) {
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(*tx_channel, p_i2s_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(*tx_channel));
    }
    if (rx_channel != NULL) {
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(*rx_channel, p_i2s_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(*rx_channel));
    }

    /* Setup power amplifier pin */
    const gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(PIN_PA_CTRL),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(PIN_PA_CTRL,1);

    return ESP_OK;
}

void music_init(void){
    music_event = xEventGroupCreate();
    xEventGroupClearBits(music_event,MUSIC_SHUTDOWN); 
    file_max = sdcard_filelist(MUSIC_PATH,&music_filename);
    ESP_LOGI(TAG,"file name : %s",music_filename[0]);
    if(file_max > 20)
        file_max = 20;

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = BSP_I2S_GPIO_CFG,
    };
    esp_err_t ret = bsp_audio_init(&std_cfg, &i2s_tx_chan, &i2s_rx_chan);

    audio_player_config_t config = { .mute_fn = audio_mute_function,
                                     .write_fn = bsp_i2s_write,
                                     .clk_set_fn = bsp_i2s_reconfig_clk,
                                     .priority = 5,
                                     .coreID = 0 };
    ret = audio_player_new(config);
    if(ret != ESP_OK){
        ESP_LOGE(TAG,"create audio player failed,error(%s)",esp_err_to_name(ret));
        return;
    }
    ret = audio_player_callback_register(audio_player_callback, NULL);
    if(ret != ESP_OK){
        ESP_LOGE(TAG,"create audio player failed,error(%s)",esp_err_to_name(ret));
        return;
    }

    xTaskCreatePinnedToCore(play_music_task,"mjpeg",4096,NULL,4,NULL,1);
}

void play_music(void)
{
    ESP_ERROR_CHECK(audio_player_resume());
}

void pause_music(void)
{
    ESP_ERROR_CHECK(audio_player_pause());
}