#ifndef I2C_EQUIPMENT_H
#define I2C_EQUIPMENT_H


#ifdef __cplusplus
extern "C" {
#endif



void i2c_drv2605_setup(void);
void i2c_drv2605_loop_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif 
