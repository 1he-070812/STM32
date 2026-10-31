 
#ifndef __TM1637_H
#define __TM1637_H
 
#include "main.h"
 
#define TM1637_CLK_H()	HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_SET)
#define TM1637_CLK_L()	HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_RESET)
#define TM1637_SDA_H()	HAL_GPIO_WritePin(GPIOA,GPIO_PIN_6,GPIO_PIN_SET)
#define TM1637_SDA_L()	HAL_GPIO_WritePin(GPIOA,GPIO_PIN_6,GPIO_PIN_RESET)
 
 
 
void TM1637_START(void);
void TM1637_STOP(void);                 //结束条件
 
void TM1637_WRITE_BYTE_DATA(unsigned char mydata);
void TM1637_CHECK_ack(void);
 
void TM1637_WRITE_DISPLAY_BYTE_FIX_ADDRESS(unsigned char addr, unsigned char my_data);
 
#endif
 