#ifndef USER_CONFIG_H
#define USER_CONFIG_H


// I2C
#define ESP32_SCL_NUM (GPIO_NUM_12)
#define ESP32_SDA_NUM (GPIO_NUM_11)

//encoder 

#define EXAMPLE_ENCODER_ECA_PIN    8
#define EXAMPLE_ENCODER_ECB_PIN    7


//bit
#define SET_BIT(reg,bit) (reg |= ((uint32_t)0x01<<bit))
#define CLEAR_BIT(reg,bit) (reg &= (~((uint32_t)0x01<<bit)))
#define READ_BIT(reg,bit) (((uint32_t)reg>>bit) & 0x01)
#define BIT_EVEN_ALL (0x00ffffff)

#endif