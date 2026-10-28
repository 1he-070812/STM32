#pragma once

#include "stm32f10x.h"

/*
 * 4 位 0.36 英寸数码管驱动（FJ3461AH 引脚，共阴/共阳可配置）
 *
 * 引脚（正面观看，左下角为 1 脚，逆时针排列）：
 *   1=E   2=D   3=DP  4=C   5=G   6=DIG4
 *   7=B   8=DIG3 9=DIG2 10=F 11=A 12=DIG1
 *
 * 当前接线：
 *   段选 a~dp：PB0、PB1、PB5~PB10，经 220Ω 限流
 *   位选 DIG1~DIG4：PA3~PA6 直连公共脚，高电平选中
 *
 * 注意：实物若为共阴，公共脚直连 GPIO 时应使用低电平选中，
 *       即 SEG_DIG_USE_TRANSISTOR 改为 0、SEG_COMMON_ANODE 改为 0；
 *       若公共脚经 S8050 低边驱动，位选仍为高电平有效，
 *       即 SEG_DIG_USE_TRANSISTOR 保持 1、SEG_COMMON_ANODE 改为 0。
 *       当前现象符合共阳，故 SEG_COMMON_ANODE 默认为 1。
 *
 * 扫描方式：每 1ms 调用一次 Segment_Scan()，4ms 完成一帧。
 */

#define SEG_GPIO_PORT       GPIOB
#define SEG_GPIO_CLK        RCC_APB2Periph_GPIOB

#define SEG_PIN_A           GPIO_Pin_0
#define SEG_PIN_B           GPIO_Pin_1
#define SEG_PIN_C           GPIO_Pin_5
#define SEG_PIN_D           GPIO_Pin_6
#define SEG_PIN_E           GPIO_Pin_7
#define SEG_PIN_F           GPIO_Pin_8
#define SEG_PIN_G           GPIO_Pin_9
#define SEG_PIN_DP          GPIO_Pin_10

#define SEG_PIN_ALL         (SEG_PIN_A | SEG_PIN_B | SEG_PIN_C | SEG_PIN_D | \
                             SEG_PIN_E | SEG_PIN_F | SEG_PIN_G | SEG_PIN_DP)

#define DIG_GPIO_PORT       GPIOA
#define DIG_GPIO_CLK        RCC_APB2Periph_GPIOA

#define DIG_PIN_1           GPIO_Pin_3
#define DIG_PIN_2           GPIO_Pin_4
#define DIG_PIN_3           GPIO_Pin_5
#define DIG_PIN_4           GPIO_Pin_6

#define DIG_PIN_ALL         (DIG_PIN_1 | DIG_PIN_2 | DIG_PIN_3 | DIG_PIN_4)

/*
 * 位选有效电平：
 * 1 = 高电平选中（共阴管经 NPN 低边驱动，或共阳管公共脚直连 GPIO）
 * 0 = 低电平选中（共阴管公共脚直连 GPIO，不推荐，电流可能超 GPIO 限额）
 *
 * 现象：共阴管直连却用 1，会变成选中位灭、其余三位亮，显示重叠乱码。
 */
#define SEG_DIG_USE_TRANSISTOR  1

/*
 * 位选左右顺序：
 * 0 = 正常顺序（PA3=DIG1 最左 ... PA6=DIG4 最右）
 * 1 = 反序（适用于位选线左右接反的板子）
 *
 * 现象：设为 0 显示 4321 时改 1；设为 1 显示 4321 时改 0。
 */
#ifndef SEG_DIG_ORDER_REVERSED
#define SEG_DIG_ORDER_REVERSED  0
#endif

/*
 * 段码极性：
 * 0 = 共阴（段码高电平点亮）
 * 1 = 共阳（段码低电平点亮，驱动内部自动取反）
 *
 * 现象：按共阴代码显示时所有小数点常亮、数字乱，通常说明实际是共阳管。
 */
#ifndef SEG_COMMON_ANODE
#define SEG_COMMON_ANODE  1
#endif

/* Segment_ShowDigit() 的特殊字符编号 */
#define SEG_CHAR_DASH       16
#define SEG_CHAR_BLANK      17

/**
 * 初始化数码管段选、位选引脚，并清空显示
 */
void Segment_Init(void);

/**
 * 清空显示缓冲区并关闭所有位选
 */
void Segment_Clear(void);

/**
 * 更新某一位的显示内容（只写缓冲区，需由 Segment_Scan() 输出）
 * @param pos   位号 0~3，0 为最左边 DIG1
 * @param value 0~15 对应 0~9/A~F；16='-'；17=空格
 * @param dot   1=该位小数点亮，0=灭
 */
void Segment_ShowDigit(uint8_t pos, uint8_t value, uint8_t dot);

/**
 * 显示 4 位十进制数（只写缓冲区，需由 Segment_Scan() 输出）
 * @param number  0~9999，超过范围按 9999 处理
 * @param dot_pos 0=无小数点；1/2/3/4=个/十/百/千位带小数点
 */
void Segment_ShowNum(uint16_t number, uint8_t dot_pos);

/**
 * 显示最多 4 个字符（只写缓冲区，需由 Segment_Scan() 输出）
 * @param p_str 以 '\0' 结尾的字符串；支持 0~9、A~F、
 *              O/P/E/N/H/r/S/t/d/L/C/U、'-'、空格和 '.'
 *              （小数点加在前一位上）
 */
void Segment_ShowString(const char *p_str);

/**
 * 动态扫描一位，必须在 1ms 定时中断中调用
 * 每次调用刷新一位，4 次完成一帧，每位刷新率 250Hz
 */
void Segment_Scan(void);
