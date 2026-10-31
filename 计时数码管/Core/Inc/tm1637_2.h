/**
  ******************************************************************************
  * @file    tm1637.h
  * @brief   TM1637 四位 LED 数码管模块驱动（GPIO 软件模拟 I2C 时序）
  * @note    目标芯片：STM32F103C8T6    库：HAL
  *          模块引脚：VCC  GND  DIO  CLK
  *          本驱动占用 PA4(CLK) / PA3(DIO)
  ******************************************************************************
  */

#ifndef __TM1637_2_H
#define __TM1637_2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ======================== 硬件连接（按需修改） ======================== */
#define TM1637_CLK_PORT         GPIOA
#define TM1637_CLK_PIN          GPIO_PIN_4      /* CLK  -> PA4 */
#define TM1637_DIO_PORT         GPIOA
#define TM1637_DIO_PIN          GPIO_PIN_3      /* DIO  -> PA3 */

/* ======================== TM1637 指令集 ======================== */
#define TM1637_CMD_WRITE_AUTO   0x40            /* 自动地址递增写显存 */
#define TM1637_CMD_WRITE_FIXED  0x44            /* 固定地址写显存 */
#define TM1637_CMD_ADDR_START   0xC0            /* 显存起始地址 0x00 */
#define TM1637_CMD_DISPLAY_ON   0x88            /* 开显示 + 亮度(0~7) */
#define TM1637_CMD_DISPLAY_OFF  0x80            /* 关显示 */

/* ======================== 内部用到的位定义 ======================== */
#define TM1637_DOT_MASK         0x80            /* 小数点是段码最高位 */
#define TM1637_MINUS_CODE       0x40            /* 负号 "-" 的段码 */

/* ======================== 对外接口 ======================== */

void TM1637_Init(void);

void TM1637_SetBrightness(uint8_t level);       /* level: 0 ~ 7，0 最暗 */

void TM1637_DisplayOn(void);
void TM1637_DisplayOff(void);

void TM1637_Clear(void);

void TM1637_DisplayRaw(const uint8_t seg[4]);

/**
  * @brief  以定点数形式显示
  * @param  value     要显示的数（纯整数，不含小数点）
  * @param  decDigits 小数位数 0~3：3 表示 value=1234 显示为 "1.234"
  * @param  blankZero 1 = 消隐前导零（显示 "  23"），0 = 补零（显示 "0023"）
  */
void TM1637_ShowNumber(uint32_t value, uint8_t decDigits, uint8_t blankZero);

/**
  * @brief  显示错误提示 "- --"（数值超量程时使用）
  */
void TM1637_ShowError(void);

/**
  * @brief  数字 0~9 转段码（供上层自己拼装特殊显示，如负号、单位符号）
  */
uint8_t TM1637_DigitToSeg(uint8_t digit);

#ifdef __cplusplus
}
#endif

#endif /* __TM1637_2_H */
