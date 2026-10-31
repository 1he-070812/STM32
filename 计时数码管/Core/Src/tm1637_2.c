/**
  ******************************************************************************
  * @file    tm1637.c
  * @brief   TM1637 四位 LED 数码管模块驱动实现
  * @note    TM1637 的"类 I2C"时序特点：
  *          1) 数据 LSB 先发，共 8 位，第 9 个时钟是 ACK
  *          2) 起始：CLK 为高时 DIO 由高变低
  *          3) 停止：CLK 为高时 DIO 由低变高
  *          4) 数据在 CLK 低电平期间改变，CLK 上升沿被采样
  *          5) 总线速率上限约 250kHz，本驱动用 5us 半周期（≈100kHz）足够稳
  ******************************************************************************
  */

#include "tm1637_2.h"

/* ============================ 段码表（共阴，0~9） ============================ */
static const uint8_t SEG_CODE[10] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F  /* 9 */
};

#define SEG_BLANK   0x00

static uint8_t s_brightness = 3;    /* 默认亮度 3，够用且不刺眼 */

/* ============================ 底层引脚操作 ============================ */

#define CLK_HIGH()  HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_SET)
#define CLK_LOW()   HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_RESET)
#define DIO_HIGH()  HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_SET)
#define DIO_LOW()   HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_RESET)
#define DIO_READ()  HAL_GPIO_ReadPin(TM1637_DIO_PORT, TM1637_DIO_PIN)

/**
  * @brief  DIO 方向切换：发数据时输出，读 ACK 时输入（避免与模块拉低打架）
  */
static void DIO_ConfigOutput(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = TM1637_DIO_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TM1637_DIO_PORT, &gpio);
}

static void DIO_ConfigInput(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = TM1637_DIO_PIN;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TM1637_DIO_PORT, &gpio);
}

/**
  * @brief  微秒级延时（基于 DWT 周期计数器，比空循环精确）
  * @note   必须在 TM1637_Init() 中先使能 DWT
  */
static void TM1637_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks)
    {
        /* busy wait */
    }
}

/* ============================ 时序层 ============================ */

static void TM1637_Start(void)
{
    CLK_HIGH();
    DIO_HIGH();
    TM1637_DelayUs(5);
    DIO_LOW();          /* CLK 高电平期间 DIO 下降 = 起始信号 */
    TM1637_DelayUs(5);
    CLK_LOW();
    TM1637_DelayUs(5);
}

static void TM1637_Stop(void)
{
    CLK_LOW();
    DIO_LOW();
    TM1637_DelayUs(5);
    CLK_HIGH();
    TM1637_DelayUs(5);
    DIO_HIGH();         /* CLK 高电平期间 DIO 上升 = 停止信号 */
    TM1637_DelayUs(5);
}

/**
  * @brief  写一个字节（LSB first），并读取 ACK 位
  * @retval 0 = 模块应答，1 = 无应答
  */
static uint8_t TM1637_WriteByte(uint8_t data)
{
    uint8_t ack;

    DIO_ConfigOutput();

    for (uint8_t i = 0; i < 8U; i++)
    {
        CLK_LOW();
        TM1637_DelayUs(3);

        if (data & 0x01U)
        {
            DIO_HIGH();
        }
        else
        {
            DIO_LOW();
        }
        TM1637_DelayUs(3);

        CLK_HIGH();
        TM1637_DelayUs(6);      /* CLK 上升沿，模块采样 */

        data >>= 1;
    }

    /* 第 9 个时钟：读 ACK */
    CLK_LOW();
    DIO_ConfigInput();          /* 释放总线，让模块拉低 */
    TM1637_DelayUs(3);
    CLK_HIGH();
    TM1637_DelayUs(3);
    ack = (DIO_READ() == GPIO_PIN_SET) ? 1U : 0U;
    CLK_LOW();
    DIO_ConfigOutput();
    TM1637_DelayUs(3);

    return ack;
}

static void TM1637_WriteCommand(uint8_t cmd)
{
    TM1637_Start();
    (void)TM1637_WriteByte(cmd);
    TM1637_Stop();
}

/* ============================ 对外接口 ============================ */

void TM1637_Init(void)
{
    /* 1) 使能 DWT 周期计数器，供延时使用 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    /* 2) 引脚初始化：空闲电平必须为高 */
    CLK_HIGH();
    DIO_HIGH();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = TM1637_CLK_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TM1637_CLK_PORT, &gpio);

    DIO_ConfigOutput();

    /* 3) 清屏并开显示 */
    TM1637_Clear();
}

void TM1637_SetBrightness(uint8_t level)
{
    if (level > 7U)
    {
        level = 7U;
    }
    s_brightness = level;
    TM1637_WriteCommand(TM1637_CMD_DISPLAY_ON | s_brightness);
}

void TM1637_DisplayOn(void)
{
    TM1637_WriteCommand(TM1637_CMD_DISPLAY_ON | s_brightness);
}

void TM1637_DisplayOff(void)
{
    TM1637_WriteCommand(TM1637_CMD_DISPLAY_OFF);
}

void TM1637_Clear(void)
{
    uint8_t seg[4] = {SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK};
    TM1637_DisplayRaw(seg);
}

/**
  * @brief  直接写 4 个字节的段码（seg[0] 最左，seg[1..3] 依次向右）
  */
void TM1637_DisplayRaw(const uint8_t seg[4])
{
    /* 第一步：设置为自动地址递增模式 */
    TM1637_WriteCommand(TM1637_CMD_WRITE_AUTO);

    /* 第二步：从 0xC0 开始连续写 4 字节显存 */
    TM1637_Start();
    (void)TM1637_WriteByte(TM1637_CMD_ADDR_START);
    for (uint8_t i = 0; i < 4U; i++)
    {
        (void)TM1637_WriteByte(seg[i]);
    }
    TM1637_Stop();

    /* 第三步：开显示（亮度设置） */
    TM1637_WriteCommand(TM1637_CMD_DISPLAY_ON | s_brightness);
}

void TM1637_ShowNumber(uint32_t value, uint8_t decDigits, uint8_t blankZero)
{
    uint8_t digits[4];
    uint8_t seg[4];
    uint32_t v = value;
    int8_t i;

    if (decDigits > 3U)
    {
        decDigits = 3U;
    }

    /* 拆位：digits[3] 是个位，digits[0] 是最高位 */
    for (i = 3; i >= 0; i--)
    {
        digits[i] = (uint8_t)(v % 10U);
        v /= 10U;
    }

    for (i = 0; i < 4; i++)
    {
        seg[i] = SEG_CODE[digits[i]];
    }

    /* 前导零消隐：至少保留 (decDigits + 1) 位，保证 "0.123" 里那个 0 不会消失 */
    if (blankZero)
    {
        uint8_t keepFrom = (uint8_t)(3U - decDigits);
        for (i = 0; i < (int8_t)keepFrom; i++)
        {
            if (digits[i] == 0U)
            {
                seg[i] = SEG_BLANK;
            }
            else
            {
                break;      /* 遇到第一个非零位就停止消隐 */
            }
        }
    }

    /* 加小数点 */
    if (decDigits > 0U)
    {
        seg[3U - decDigits] |= TM1637_DOT_MASK;
    }

    TM1637_DisplayRaw(seg);
}

void TM1637_ShowError(void)
{
    uint8_t seg[4] = {SEG_BLANK, TM1637_MINUS_CODE, TM1637_MINUS_CODE, TM1637_MINUS_CODE};
    TM1637_DisplayRaw(seg);
}

uint8_t TM1637_DigitToSeg(uint8_t digit)
{
    return (digit > 9U) ? SEG_BLANK : SEG_CODE[digit];
}
