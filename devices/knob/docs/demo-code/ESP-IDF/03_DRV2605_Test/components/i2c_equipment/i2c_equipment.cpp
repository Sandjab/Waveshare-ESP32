#include <stdio.h>
#include "i2c_equipment.h"
#include "user_config.h"
#include "SensorDRV2605.hpp"
#include "i2c_bsp.h"


static const char *TAG = "i2c_equipment";

SensorDRV2605 drv;

bool i2c_dev_Callback(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len, bool writeReg, bool isWrite)
{
  uint8_t ret;
  int areg = reg;
  i2c_master_dev_handle_t i2c_dev_handle = NULL;
  if(addr == EXAMPLE_DRV2605_ADDR)
  i2c_dev_handle = drv2605_dev_handle;
  if(isWrite) // 写寄存器
  {
    if(writeReg)
    {
      ret = i2c_write_buff(i2c_dev_handle,areg,buf,len);
    }
    else
    {
      ret = i2c_write_buff(i2c_dev_handle,-1,buf,len);
    }
  }
  else
  {
    if(writeReg)
    {
      ret = i2c_read_buff(i2c_dev_handle,areg,buf,len);
    }
    else
    {
      ret = i2c_read_buff(i2c_dev_handle,-1,buf,len);
    }
  }
  return (ret == ESP_OK) ? true : false;
}

void i2c_drv2605_setup(void)
{
  if(!drv.begin(i2c_dev_Callback))
  {
    ESP_LOGE(TAG,"drv2605 init failure");
  }
  drv.selectLibrary(5);

  // I2C trigger by sending 'run' command
  // default, internal trigger when sending RUN command
  drv.setMode(SensorDRV2605::MODE_INTTRIG); //使用i2c触发,不需要TRIG
}


void i2c_drv2605_loop_task(void *arg)
{
  uint8_t effect = 1;
  for(;;)
  {
    #if 0
    drv.setWaveform(0, 1);       // 设置槽位 0 播放内部波形1
    drv.setWaveform(1, 0);       // 设置槽位 1 0:表示停止播放
    drv.run();                   // 播放开启
    vTaskDelay(pdMS_TO_TICKS(3000));
    #else
    printf("Effect # %d\n",effect);
    if (effect == 1) {
        printf("11.2 Waveform Library Effects List\n");
    }

    if (effect == 1) {
      printf("1 - Strong Click - 100%%\n");
    }
    if (effect == 2) {
      printf("2 - Strong Click - 60%%\n");
    }
    if (effect == 3) {
      printf("3 - Strong Click - 30%%\n");
    }
    if (effect == 4) {
      printf("4 - Sharp Click - 100%%\n");
    }
    if (effect == 5) {
      printf("5 - Sharp Click - 60%%\n");
    }
    if (effect == 6) {
      printf("6 - Sharp Click - 30%%\n");
    }
    if (effect == 7) {
      printf("7 - Soft Bump - 100%%\n");
    }
    if (effect == 8) {
      printf("8 - Soft Bump - 60%%\n");
    }
    if (effect == 9) {
      printf("9 - Soft Bump - 30%%\n");
    }
    if (effect == 10) {
      printf("10 - Double Click - 100%%\n");
    }
    if (effect == 11) {
      printf("11 - Double Click - 60%%\n");
    }
    if (effect == 12) {
      printf("12 - Triple Click - 100%%\n");
    }
    if (effect == 13) {
      printf("13 - Soft Fuzz - 60%%\n");
    }
    // set the effect to play
    drv.setWaveform(0, effect);  // play effect
    drv.setWaveform(1, 0);       // end waveform

    // play the effect!
    drv.run();

    // wait a bit
    vTaskDelay(pdMS_TO_TICKS(500));

    effect++;
    if (effect > 13) effect = 1;
    #endif
  }
}