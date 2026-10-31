/* bsp_adc2.c —— ADC2_IN2（PA2）轮询电压采样模块 */
#include "bsp_adc2.h"

/* CubeMX 生成的 ADC2 句柄，在 adc.c 中定义 */
extern ADC_HandleTypeDef hadc2;

/**
 * @brief  启动 ADC2 单次软件触发转换，轮询等待，读取 12 位右对齐原始值
 * @param  无
 * @retval ADC 原始值（0~4095）；转换超时或失败返回 0
 */
uint16_t ADC2_ReadRaw(void)
{
    uint16_t raw = 0u;  /* ADC 原始值缓存 */

    /* 启动 ADC2 转换 */
    if (HAL_ADC_Start(&hadc2) != HAL_OK)
    {
        return 0u;  /* 启动失败直接返回 0 */
    }

    /* 轮询等待转换完成，超时 ADC2_POLL_TIMEOUT_MS 毫秒 */
    if (HAL_ADC_PollForConversion(&hadc2, ADC2_POLL_TIMEOUT_MS) == HAL_OK)
    {
        /* 读取 12 位右对齐转换结果 */
        raw = (uint16_t)HAL_ADC_GetValue(&hadc2);
    }

    /* 停止 ADC2，保持单次转换模式 */
    HAL_ADC_Stop(&hadc2);

    return raw;
}

/**
 * @brief  连续采样 times 次并求算术平均，抑制随机噪声
 * @param  times：采样次数，0 按 1 处理
 * @retval 平均后的 ADC 原始值（0~4095）
 */
uint16_t ADC2_ReadAverage(uint8_t times)
{
    uint32_t sum = 0u;  /* 累加和 */
    uint8_t  i;         /* 循环索引 */

    /* 参数保护：次数为 0 时按 1 次处理 */
    if (times == 0u)
    {
        times = 1u;
    }

    /* 连续采样并累加 */
    for (i = 0u; i < times; i++)
    {
        sum += (uint32_t)ADC2_ReadRaw();
    }

    /* 求算术平均 */
    return (uint16_t)(sum / (uint32_t)times);
}

/**
 * @brief  ADC 原始值换算为电压，单位 mV
 * @param  raw：ADC 原始值（0~4095），超出范围按满量程处理
 * @retval 电压值（0~3300 mV）
 */
uint32_t ADC2_RawToMv(uint16_t raw)
{
    /* 限幅保护：原始值不得超过 12 位满量程 */
    if (raw > ADC2_FULL_SCALE)
    {
        raw = ADC2_FULL_SCALE;
    }

    /* 电压(mV) = 原始值 × 参考电压 / 满量程 */
    return ((uint32_t)raw * ADC2_VREF_MV) / ADC2_FULL_SCALE;
}

/**
 * @brief  采样一次并直接返回电压，单位 mV
 * @param  无
 * @retval 电压值（0~3300 mV）
 */
uint32_t ADC2_ReadVoltageMv(void)
{
    return ADC2_RawToMv(ADC2_ReadRaw());
}
