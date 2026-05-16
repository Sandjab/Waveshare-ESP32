#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "mjpeg_fram.h"
#include "esp_log.h"

#define JPEG_SOI 0xFFD8  //jpeg start
#define JPEG_EOI 0xFFD9  //jpeg end

#define MIN_READ_BUFF_SIZE 4096
#define BUFF_SIZE 80*1024

#define START_GET_EV    (BIT0)
#define STOP_GET_EV    (BIT1)

static const char *TAG = "mjpeg_frame";

static EventGroupHandle_t mjpeg_event;
static QueueHandle_t mjpeg_data_queue;

static jpeg_frame_cfg_t  s_mjpeg_cfg;
static int mjpeg_inited = 0;        //初始化标志
static int mjpeg_started = 0;       //已经启动解析标志

void jpeg_frame_config(jpeg_frame_cfg_t* cfg)
{
    if (cfg->buff_size < MIN_READ_BUFF_SIZE)
        return;
    memcpy(&s_mjpeg_cfg,cfg,sizeof(jpeg_frame_cfg_t));
    if(!mjpeg_event)
        mjpeg_event = xEventGroupCreate();
    if(!mjpeg_data_queue)
        mjpeg_data_queue = xQueueCreate(5,sizeof(jpeg_frame_data_t));
    mjpeg_inited = 1;
}

static void jpeg_export_frame_task(void *param);

void jpeg_frame_start(const char* filename)
{
    if (!mjpeg_inited)
        return;
    if(mjpeg_started)
        return;
    xEventGroupClearBits(mjpeg_event,START_GET_EV | START_GET_EV);
    static char jpeg_filename[256];
    snprintf(jpeg_filename,sizeof(jpeg_filename),"/sdcard/mjpeg/%s",filename);
    mjpeg_started = 1;
    xTaskCreatePinnedToCore(jpeg_export_frame_task,"mjpeg",4096,jpeg_filename,4,NULL,1);
}

static void jpeg_export_frame_task(void *param)
{
    char *filename = (char*)param;
    ESP_LOGI(TAG,"play mjpeg %s",filename);
    FILE* f = fopen(filename, "rb");
    int frame_cnt = 0;      //用于统计解析出来的帧数
    int jpeg_started = 0;
    if (f == NULL)
    {
        printf("Can't open file:%s\r\n",filename);
        goto mjpeg_task_del;
    }
    uint8_t* read_buff = (uint8_t*)malloc(s_mjpeg_cfg.buff_size);
    if (!read_buff)
    {
        goto mjpeg_task_del;
    }
    size_t read_bytes = 0;          //每次实际读取文件的字节数
    uint8_t* frame_buff = NULL;     //截取到的JPG帧
    size_t frame_write_index = 0;   //当前的JPG帧的长度
    size_t frame_buff_total_len = s_mjpeg_cfg.buff_size;    //分配给JPG帧的内存大小，刚开始用单次读回来数据长度大小
    while ((read_bytes = fread(read_buff, 1, s_mjpeg_cfg.buff_size, f) )> 0)
    {
        int soi_index = 0;      //SOI的下标
        for (int i = 0; i < read_bytes - 1; i++)
        {
            uint16_t oi_flags = (read_buff[i] << 8 )+ read_buff[i+1];   //查找标志
            if (!jpeg_started && oi_flags == JPEG_SOI)      //读回来的数据中检索到JPEG起始标志
            {
                soi_index = i;
                jpeg_started = 1;
            }
            else if(jpeg_started && oi_flags == JPEG_EOI)   //读回来的数据中检索到JPEG结束标志
            {
                int write_len = (i - soi_index + 1) + 1;    //+1表示把JPEG_EOI也进行拷贝
                if (frame_buff)
                {
                    if (frame_write_index + write_len <= frame_buff_total_len)  //查看当前分配的jpeg帧内存释放足够存放
                    {
                        memcpy(&frame_buff[frame_write_index], &read_buff[soi_index], write_len);
                    }
                    else    //不足够存放，需要重新分配内存单元，进行写入
                    {
                        frame_buff = (uint8_t*)realloc(frame_buff, frame_write_index + write_len);
                        frame_buff_total_len = frame_write_index + write_len;
                        memcpy(&frame_buff[frame_write_index], &read_buff[soi_index], write_len);
                    }
                }
                else
                {
                    frame_buff = (uint8_t*)malloc(write_len);
                    memcpy(&frame_buff[frame_write_index], &read_buff[soi_index], write_len);
                }
                jpeg_frame_data_t frame_data;
                frame_data.frame = frame_buff;
                frame_data.len = frame_write_index + write_len;
                frame_cnt++;
                //ESP_LOGI(TAG,"jpg frame detect.NO.:%i,len:%d",frame_cnt,frame_data.len);
                //这里已经提取到了一帧jpeg图像，等到读取事件，然后把数据放入队列
                EventBits_t ev = xEventGroupWaitBits(mjpeg_event,START_GET_EV|STOP_GET_EV,pdTRUE,pdFALSE,portMAX_DELAY);
                if(ev & START_GET_EV)
                {
                    xQueueSend(mjpeg_data_queue,&frame_data,portMAX_DELAY);
                }
                if(ev & STOP_GET_EV)
                {
                    if(frame_buff)
                        free(frame_buff);
                    goto mjpeg_task_del;
                }
                
                frame_buff = NULL;      //frame_buff已经通过队列发送出去了，这里置NULL，下次重新查找到SOI和SOE后将重新malloc
                frame_write_index = 0;
                jpeg_started = 0;
            }
        }
        //上面的一个检索循环结束后，检测到了起始标志，但没有检测到结束标志，需要把数据拷贝到frame_buff里
        //待下次的检索循环检索到后，结束标志后，就可以追加后面的内容，确保不漏掉一帧
        if (jpeg_started)
        {
            size_t write_len = s_mjpeg_cfg.buff_size - soi_index;   //长度为当前从文件读回来的数据减去SOI下标
            if (frame_buff)
            {
                if (frame_write_index + write_len > frame_buff_total_len)
                {
                    frame_buff = (uint8_t*)realloc(frame_buff, frame_buff_total_len);
                }
                frame_buff_total_len = frame_write_index + write_len;
                memcpy(&frame_buff[frame_write_index], &read_buff[soi_index], write_len);
                frame_write_index += write_len;
            }
            else
            {
                if(write_len > frame_buff_total_len)
                    frame_buff_total_len = write_len;
                frame_buff = (uint8_t*)malloc(frame_buff_total_len);
                memcpy(&frame_buff[0], &read_buff[soi_index], write_len);
                frame_write_index = write_len;
            }
        }
    }
mjpeg_task_del:
    if(f)
        fclose(f);
    mjpeg_started = 0;
    vTaskDelete(NULL);
}

void jpeg_frame_get_one(jpeg_frame_data_t *data)
{
    jpeg_frame_data_t frame_data = {0,0};
    if(!mjpeg_started)
        return;
    xEventGroupSetBits(mjpeg_event,START_GET_EV);   //标记一个获取事件
    if(pdTRUE == xQueueReceive(mjpeg_data_queue,&frame_data,pdMS_TO_TICKS(1000)))   //等待获取一帧JPEG数据
    {
        memcpy(data,&frame_data,sizeof(jpeg_frame_data_t));
        return;
    }
    else
    {
        data->len = 0;  //这里简单判断，返回0就认为播放结束
    }
}

void jpeg_frame_stop()
{
     xEventGroupSetBits(mjpeg_event,STOP_GET_EV);
}