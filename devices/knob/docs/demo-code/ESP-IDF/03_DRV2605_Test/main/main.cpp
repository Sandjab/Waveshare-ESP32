#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "i2c_bsp.h"
#include "i2c_equipment.h"
extern "C" void app_main(void)
{
  i2c_master_Init();
  i2c_drv2605_setup();
  xTaskCreate(i2c_drv2605_loop_task, "i2c_drv2605_loop_task", 3000, NULL, 2, NULL);
}
