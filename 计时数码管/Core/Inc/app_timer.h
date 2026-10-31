/**
  ******************************************************************************
  * @file    app_timer.h
  * @brief   三按键控制 + 60 秒倒计时，显示在 PA5/PA6 那块四位数码管上
  * @note    目标芯片：STM32F103C8T6    库：HAL
  ******************************************************************************
  */

#ifndef __APP_TIMER_H
#define __APP_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ============================ 硬件连接 ============================
 * 三个按键全部：一端接引脚，另一端接 GND，代码用内部上拉。
 * 松开读到高电平，按下读到低电平。
 * ================================================================= */
#define KEY_PORT                GPIOB       /* PB11 主键：长按开关屏 / 短按开始暂停 */
#define KEY_PIN                 GPIO_PIN_15

#define KEY_UP_PORT             GPIOA       /* PB13 加时间 */
#define KEY_UP_PIN              GPIO_PIN_8

#define KEY_DOWN_PORT           GPIOA       /* PA9  减时间 */
#define KEY_DOWN_PIN            GPIO_PIN_9

/* ============================ 可调参数 ============================ */
#define KEY_DEBOUNCE_MS         20U         /* 按键消抖时间（ms） */
#define KEY_LONG_MS             1000U       /* 按住超过这个时长算长按（ms） */
#define TIMER_TOTAL_MS          60000U      /* 倒计时总时长：60 秒 */
#define TIMER_STEP_MS           100U        /* 计时步进：0.1 秒 */
#define ADJ_STEP_MS             30000U      /* 加/减键每次调整 30 秒 */
#define TIMER_MAX_MS            5999000U    /* 上限 99 分 59 秒（两位数分钟） */

/* 倒计时正在跑的时候，加/减键是否有效：
 *   0 = 无效（默认，按需求：跑动中按键不响应，只有待机/暂停时能调）
 *   1 = 有效（想做成"随时加时"就改成 1） */
#define ADJ_ENABLE_WHILE_RUNNING 0U

/* 按键行为：
 *   PB11 长按 1 秒 ：数码管 灭 <-> 亮 切换（开机时倒计时复位为 60 秒）
 *   PB11 短按      ：亮着时才有效
 *                     待机   -> 开始倒计时
 *                     计时中 -> 暂停（保留剩余时间）
 *                     暂停中 -> 继续
 *                     归零后 -> 重新从 01:00 开始
 *   PB13 短按      ：时间 +30 秒
 *   PA9  短按      ：时间 -30 秒
 *                   （两个都只在数码管亮着、且倒计时没在跑的时候有效）
 *   灭屏时短按     ：忽略
 */

/* ============================ 对外接口 ============================ */
void AppTimer_Init(void);
void AppTimer_Task(void);       /* 放在 while(1) 里周期调用，非阻塞 */

#ifdef __cplusplus
}
#endif

#endif /* __APP_TIMER_H */
