#ifndef _MJPEG_FRAME_H
#define _MJPEG_FRAME_H

#include <stdio.h>
#include <stdint.h>

typedef struct
{
    uint8_t *frame;
    size_t len;

}jpeg_frame_data_t;

typedef struct 
{
    size_t buff_size;
}jpeg_frame_cfg_t;

void jpeg_frame_config(jpeg_frame_cfg_t *cfg);

void jpeg_frame_start(const char *filename);

void jpeg_frame_stop(void);

void jpeg_frame_get_one(jpeg_frame_data_t *data);

#endif

