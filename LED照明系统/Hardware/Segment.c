#include "Segment.h"

#include <stddef.h>

/* 共阴段码表：bit0=A ... bit6=G，bit7=DP */
static const uint8_t s_seg_code[18] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
    0x40, 0x00
};

/* 段码位到 GPIO 引脚的映射，顺序与段码 bit0~bit7 一致 */
static const uint16_t s_seg_pins[8] = {
    SEG_PIN_A, SEG_PIN_B, SEG_PIN_C, SEG_PIN_D,
    SEG_PIN_E, SEG_PIN_F, SEG_PIN_G, SEG_PIN_DP
};

/* 位选引脚，下标 0 对应最左边 DIG1 */
#if SEG_DIG_ORDER_REVERSED
static const uint16_t s_dig_pins[4] = {
    DIG_PIN_4, DIG_PIN_3, DIG_PIN_2, DIG_PIN_1
};
#else
static const uint16_t s_dig_pins[4] = {
    DIG_PIN_1, DIG_PIN_2, DIG_PIN_3, DIG_PIN_4
};
#endif

/* 显示缓冲区，每个字节保存一位的段码；主循环写，扫描中断读 */
static volatile uint8_t s_display[4];

/* 当前扫描位 */
static uint8_t s_scan_pos;

/**
 * 将段码输出到段选引脚
 * @param code 段码，bit0=A ... bit6=G，bit7=DP
 */
static void segment_write_segments(uint8_t code)
{
    uint16_t set_mask = 0;
    uint16_t reset_mask = 0;
    uint8_t i;

#if SEG_COMMON_ANODE
    /* 共阳管段码低电平点亮，存入缓冲的是共阴逻辑码，输出前整体取反 */
    code = (uint8_t)~code;
#endif

    for (i = 0; i < 8; i++)
    {
        if ((code & (uint8_t)(1u << i)) != 0)
        {
            set_mask |= s_seg_pins[i];
        }
        else
        {
            reset_mask |= s_seg_pins[i];
        }
    }

    /* BSRR 低 16 位置 1、高 16 位清 0，一次写入避免中间状态 */
    SEG_GPIO_PORT->BSRR = (uint32_t)set_mask | ((uint32_t)reset_mask << 16);
}

/**
 * 关闭所有位选，用于换位前消隐
 */
static void segment_all_digits_off(void)
{
#if SEG_DIG_USE_TRANSISTOR
    DIG_GPIO_PORT->BRR = DIG_PIN_ALL;
#else
    DIG_GPIO_PORT->BSRR = DIG_PIN_ALL;
#endif
}

/**
 * 选通指定的一位
 * @param pos 0~3，0 为最左边 DIG1
 */
static void segment_select_digit(uint8_t pos)
{
#if SEG_DIG_USE_TRANSISTOR
    DIG_GPIO_PORT->BSRR = s_dig_pins[pos];
#else
    DIG_GPIO_PORT->BRR = s_dig_pins[pos];
#endif
}

/**
 * 将 ASCII 字符转换为段码
 * @param ch 待显示字符
 * @return 段码；无法显示时返回空白
 */
static uint8_t segment_char_to_code(char ch)
{
    switch (ch)
    {
        case '0': return 0x3F;
        case '1': return 0x06;
        case '2': return 0x5B;
        case '3': return 0x4F;
        case '4': return 0x66;
        case '5': return 0x6D;
        case '6': return 0x7D;
        case '7': return 0x07;
        case '8': return 0x7F;
        case '9': return 0x6F;
        case 'A': case 'a': return 0x77;
        case 'B': case 'b': return 0x7C;
        case 'C': case 'c': return 0x39;
        case 'D': case 'd': return 0x5E;
        case 'E': case 'e': return 0x79;
        case 'F': case 'f': return 0x71;
        case 'G': case 'g': return 0x7D;
        case 'H': case 'h': return 0x76;
        case 'L': case 'l': return 0x38;
        case 'N': case 'n': return 0x54;
        case 'O': case 'o': return 0x3F;
        case 'P': case 'p': return 0x73;
        case 'R': case 'r': return 0x50;
        case 'S': case 's': return 0x6D;
        case 'T': case 't': return 0x78;
        case 'U': case 'u': return 0x3E;
        case '-': return 0x40;
        case ' ': return 0x00;
        default:  return 0x00;
    }
}

/**
 * 初始化段选、位选引脚，并熄灭显示
 */
void Segment_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(SEG_GPIO_CLK | DIG_GPIO_CLK, ENABLE);

    /* 初始化前先输出熄灭电平，避免上电瞬间亮一下 */
    segment_write_segments(0x00);
#if SEG_DIG_USE_TRANSISTOR
    DIG_GPIO_PORT->BRR = DIG_PIN_ALL;
#else
    DIG_GPIO_PORT->BSRR = DIG_PIN_ALL;
#endif

    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;

    gpio_init.GPIO_Pin = SEG_PIN_ALL;
    GPIO_Init(SEG_GPIO_PORT, &gpio_init);

    gpio_init.GPIO_Pin = DIG_PIN_ALL;
    GPIO_Init(DIG_GPIO_PORT, &gpio_init);

    Segment_Clear();
}

/**
 * 清空显示缓冲区并关闭所有位选
 */
void Segment_Clear(void)
{
    uint8_t i;

    for (i = 0; i < 4; i++)
    {
        s_display[i] = s_seg_code[SEG_CHAR_BLANK];
    }

    segment_write_segments(0x00);
    segment_all_digits_off();
}

/**
 * 更新某一位的显示内容（只写缓冲区，由 Segment_Scan() 输出）
 * @param pos   位号 0~3，0 为最左边 DIG1
 * @param value 0~15 对应 0~9/A~F；16='-'；17=空格
 * @param dot   1=该位小数点亮，0=灭
 */
void Segment_ShowDigit(uint8_t pos, uint8_t value, uint8_t dot)
{
    uint8_t code;

    if (pos > 3)
    {
        return;
    }

    if (value > SEG_CHAR_BLANK)
    {
        value = SEG_CHAR_BLANK;
    }

    code = s_seg_code[value];
    if (dot != 0)
    {
        code |= 0x80;
    }

    s_display[pos] = code;
}

/**
 * 显示 4 位十进制数（只写缓冲区，由 Segment_Scan() 输出）
 * @param number  0~9999，超过范围按 9999 处理
 * @param dot_pos 0=无小数点；1/2/3/4=个/十/百/千位带小数点
 */
void Segment_ShowNum(uint16_t number, uint8_t dot_pos)
{
    uint8_t digit[4];
    uint8_t i;

    if (number > 9999)
    {
        number = 9999;
    }

    digit[0] = (uint8_t)(number / 1000);
    digit[1] = (uint8_t)(number / 100 % 10);
    digit[2] = (uint8_t)(number / 10 % 10);
    digit[3] = (uint8_t)(number % 10);

    for (i = 0; i < 4; i++)
    {
        Segment_ShowDigit(i, digit[i], (dot_pos == (uint8_t)(4 - i)) ? 1 : 0);
    }
}

/**
 * 显示最多 4 个字符（只写缓冲区，由 Segment_Scan() 输出）
 * @param p_str 以 '\0' 结尾的字符串；支持 0~9、A~F、O/P/E/N/H/r/S/t/d/L/C/U、
 *              '-'、空格和 '.'（小数点加在前一位上）
 */
void Segment_ShowString(const char *p_str)
{
    uint8_t pos;
    uint8_t code;
    char ch;

    for (pos = 0; pos < 4; pos++)
    {
        s_display[pos] = s_seg_code[SEG_CHAR_BLANK];
    }

    if (p_str == NULL)
    {
        return;
    }

    pos = 0;
    while ((*p_str != '\0') && (pos < 4))
    {
        ch = *p_str;
        p_str++;

        if (ch == '.')
        {
            if (pos > 0)
            {
                s_display[pos - 1] |= 0x80;
            }
            continue;
        }

        code = segment_char_to_code(ch);
        s_display[pos] = code;
        pos++;
    }
}

/**
 * 动态扫描一位，必须在 1ms 定时中断（如 SysTick）中调用
 * 每次调用刷新一位，4 次完成一帧，每位刷新率 250Hz
 */
void Segment_Scan(void)
{
    uint8_t code;

    /* 先关全部位选，避免上一位的段码串到新位 */
    segment_all_digits_off();

    code = s_display[s_scan_pos];
    segment_write_segments(code);
    segment_select_digit(s_scan_pos);

    s_scan_pos++;
    if (s_scan_pos >= 4)
    {
        s_scan_pos = 0;
    }
}
