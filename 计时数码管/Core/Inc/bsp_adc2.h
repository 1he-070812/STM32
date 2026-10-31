/* bsp_adc2.h —— ADC2_IN2（PA2）电压采样模块头文件 */
#ifndef __BSP_ADC2_H
#define __BSP_ADC2_H

#include "main.h"

/* ===== 换算参数 ===== */
#define ADC2_VREF_MV          3300u   /* 参考电压 3.3V，单位 mV */
#define ADC2_FULL_SCALE       4095u   /* 12 位 ADC 满量程 */

/* ===== 轮询超时（ms） ===== */
#define ADC2_POLL_TIMEOUT_MS  10u     /* 单次转换等待超时 */

/* 读取一次 ADC2_IN2 原始值（0~4095），失败返回 0 */
uint16_t ADC2_ReadRaw(void);

/* 连续采样 times 次并求算术平均，times 为 0 时按 1 处理 */
uint16_t ADC2_ReadAverage(uint8_t times);

/* 原始值换算为电压（mV），带限幅 */
uint32_t ADC2_RawToMv(uint16_t raw);

/* 采样一次并直接返回电压（mV） */
uint32_t ADC2_ReadVoltageMv(void);

#endif /* __BSP_ADC2_H */

