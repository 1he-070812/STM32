#ifndef __SEGMENT_H__
#define __SEGMENT_H__

#include "stm32f10x.h"

/* ====================== 引脚配置（按你的接线修改） ====================== */
/* 段选：共阴数码管，高电平点亮，每段串 220~330Ω 限流电阻 */
#define SEG_PORT		GPIOA
#define SEG_RCC			RCC_APB2Periph_GPIOA
#define SEG_PIN_A		GPIO_Pin_0
#define SEG_PIN_B		GPIO_Pin_1
#define SEG_PIN_C		GPIO_Pin_2
#define SEG_PIN_D		GPIO_Pin_3
#define SEG_PIN_E		GPIO_Pin_4
#define SEG_PIN_F		GPIO_Pin_5
#define SEG_PIN_G		GPIO_Pin_6
#define SEG_PIN_DP		GPIO_Pin_7
#define SEG_ALL			(SEG_PIN_A | SEG_PIN_B | SEG_PIN_C | SEG_PIN_D | SEG_PIN_E | SEG_PIN_F | SEG_PIN_G | SEG_PIN_DP)

/* 位选（公共脚）：建议经 NPN 三极管（S8050）驱动，GPIO 高电平=选中该位；
   若把公共脚直连 GPIO（不推荐，电流会超），需把下面宏改为 0，逻辑变为低电平选中 */
#define SEG_DIG_USE_TRANSISTOR	1

#define DIG_PORT		GPIOB
#define DIG_RCC			RCC_APB2Periph_GPIOB
#define DIG_PIN_1		GPIO_Pin_12
#define DIG_PIN_2		GPIO_Pin_13
#define DIG_PIN_3		GPIO_Pin_14
#define DIG_PIN_4		GPIO_Pin_15
#define DIG_ALL			(DIG_PIN_1 | DIG_PIN_2 | DIG_PIN_3 | DIG_PIN_4)

/* ============================ 函数声明 ============================ */
void Segment_Init(void);									// 初始化引脚
void Segment_ShowDigit(uint8_t Bit, uint8_t Num, uint8_t Dot);	// 点亮某一位：Bit=0~3（0为最左边DIG1），Num=0~9/A-F/'-'/空格，Dot=1带小数点
void Segment_ShowNum(uint16_t Num, uint8_t DotBit);			// 显示4位数字：0~9999，DotBit=0无小数点，1~4分别是个/十/百/千位带点
void Segment_Clear(void);									// 全部熄灭

#endif
