#include "stm32f10x.h"
#include "Delay.h"
#include "Segment.h"

/**
 * 显示字符串并保持指定时间
 * @param p_str 要显示的字符串
 * @param ms    保持时间，单位 ms
 */
static void show_string_for(const char *p_str, uint16_t ms)
{
    Segment_ShowString(p_str);

    while (ms > 0)
    {
        Segment_Scan();
        Delay_ms(1);
        ms--;
    }
}

/**
 * 显示数字并保持指定时间
 * @param number  要显示的数字
 * @param dot_pos 小数点位置：0 无，1 个位，2 十位，3 百位，4 千位
 * @param ms      保持时间，单位 ms
 */
static void show_num_for(uint16_t number, uint8_t dot_pos, uint16_t ms)
{
    Segment_ShowNum(number, dot_pos);

    while (ms > 0)
    {
        Segment_Scan();
        Delay_ms(1);
        ms--;
    }
}

int main(void)
{
    uint16_t i;
    uint8_t j;

    Segment_Init();

    /* 1. 全亮：检查 8 个段和限流电阻是否都正常 */
    show_string_for("8888", 1000);

    /* 2. 全灭：检查消隐是否正常 */
    show_string_for("    ", 500);

    /* 3. 位序：从左到右应显示 1 2 3 4 */
    show_string_for("1234", 1500);

    /* 4. 字母显示：OPEN、SHOr、OFF */
    show_string_for("OPEN", 1500);
    show_string_for("SHOr", 1500);
    show_string_for("OFF ", 1500);

    /* 5. 小数点：显示 123.4 */
    show_num_for(1234, 2, 1500);

    /* 6. 0~9999 循环计数，每个数显示约 20ms */
    while (1)
    {
        for (i = 0; i <= 9999; i++)
        {
            Segment_ShowNum(i, 0);

            for (j = 0; j < 20; j++)
            {
                Segment_Scan();
                Delay_ms(1);
            }
        }
    }
}