#include "stm32f10x.h"
#include "Delay.h"
#include "Segment.h"

/*
 * FJ3461AH：4位 0.36寸 共阴数码管（红光）
 * 引脚（正面观看逆时针）：A-11 B-7 C-4 D-2 E-1 F-10 G-5 DP-3
 * 公共脚：12=DIG1(最左) 9=DIG2 8=DIG3 6=DIG4(最右)
 * 段码表位定义：bit7=DP bit6=G bit5=F bit4=E bit3=D bit2=C bit1=B bit0=A
 */

/* 共阴段码表：0~9、A~F，以及 '-'、空格 */
static const uint8_t Seg_Code[18] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,		// 0 1 2 3 4 5 6 7
	0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,		// 8 9 A b C d E F
	0x40, 0x00											// '-' 空格
};

/**
  * @brief  向8根段选引脚输出一个段码（共阴，1亮0灭）
  * @param  Code 段码，bit7=DP ... bit0=A
  * @retval 无
  */
static void Segment_SetSeg(uint8_t Code)
{
	GPIO_WriteBit(SEG_PORT, SEG_PIN_A,  (BitAction)((Code >> 0) & 0x01));
	GPIO_WriteBit(SEG_PORT, SEG_PIN_B,  (BitAction)((Code >> 1) & 0x01));
	GPIO_WriteBit(SEG_PORT, SEG_PIN_C,  (BitAction)((Code >> 2) & 0x01));
	GPIO_WriteBit(SEG_PORT, SEG_PIN_D,  (BitAction)((Code >> 3) & 0x01));
	GPIO_WriteBit(SEG_PORT, SEG_PIN_E,  (BitAction)((Code >> 4) & 0x01));
	GPIO_WriteBit(SEG_PORT, SEG_PIN_F,  (BitAction)((Code >> 5) & 0x01));
	GPIO_WriteBit(SEG_PORT, SEG_PIN_G,  (BitAction)((Code >> 6) & 0x01));
	GPIO_WriteBit(SEG_PORT, SEG_PIN_DP, (BitAction)((Code >> 7) & 0x01));
}

/**
  * @brief  选中某一位（动态扫描用）
  * @param  Bit 0~3，0=最左边DIG1
  * @retval 无
  */
static void Segment_SetDig(uint8_t Bit)
{
	uint16_t Pin;
	if (Bit > 3) return;

#if SEG_DIG_USE_TRANSISTOR
	/* 经 NPN 三极管：高电平导通，公共脚被拉低 -> 该位选通 */
	GPIO_SetBits(DIG_PORT, (DIG_PIN_1 << Bit));
#else
	/* 直连：共阴公共脚低电平选通 */
	GPIO_ResetBits(DIG_PORT, (DIG_PIN_1 << Bit));
#endif
}

/**
  * @brief  初始化段选、位选引脚为推挽输出，并熄灭
  * @retval 无
  */
void Segment_Init(void)
{
	RCC_APB2PeriphClockCmd(SEG_RCC | DIG_RCC, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Pin = SEG_ALL;
	GPIO_Init(SEG_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = DIG_ALL;
	GPIO_Init(DIG_PORT, &GPIO_InitStructure);

	Segment_Clear();
}

/**
  * @brief  全部熄灭
  * @retval 无
  */
void Segment_Clear(void)
{
	GPIO_ResetBits(SEG_PORT, SEG_ALL);		// 段选全灭
	GPIO_ResetBits(DIG_PORT, DIG_ALL);		// 位选全关
}

/**
  * @brief  点亮某一位，显示一个字符（动态扫描中的一步，一次只亮一位）
  * @param  Bit  位号 0~3（0=最左边）
  * @param  Num  要显示的内容：0~15 对应 0-9/A-F，16='-'，17=空格
  * @param  Dot  1=该位小数点亮
  * @retval 无
  */
void Segment_ShowDigit(uint8_t Bit, uint8_t Num, uint8_t Dot)
{
	if (Bit > 3) return;
	if (Num > 17) Num = 0;					// 查表越界保护

	/* 1. 消影：先关全部位选，避免上一位的段码串到这一位（残影） */
	GPIO_ResetBits(DIG_PORT, DIG_ALL);

	/* 2. 送段码（按需加小数点） */
	Segment_SetSeg(Seg_Code[Num] | (Dot ? 0x80 : 0x00));

	/* 3. 选通目标位 */
	Segment_SetDig(Bit);
}

/**
  * @brief  显示4位数字（内部逐位刷新，每位亮1ms，一轮4ms，无闪烁）
  * @param  Num    0~9999
  * @param  DotBit 0=无小数点；1/2/3/4 = 个/十/百/千位带小数点
  * @retval 无
  * @note   本函数自带延时，主循环里直接循环调用即可，无需再逐位手动扫描
  */
void Segment_ShowNum(uint16_t Num, uint8_t DotBit)
{
	uint8_t i;
	uint8_t Digit[4];

	if (Num > 9999) Num = 9999;

	Digit[0] = Num / 1000;			// 千位 -> 最左边 DIG1
	Digit[1] = Num / 100 % 10;		// 百位
	Digit[2] = Num / 10 % 10;		// 十位
	Digit[3] = Num % 10;			// 个位

	for (i = 0; i < 4; i++)
	{
		Segment_ShowDigit(i, Digit[i], (i + DotBit == 4) ? 1 : 0);	// DotBit=1/2/3/4 -> 个/十/百/千位带点
		Delay_ms(1);				// 每位点亮 1ms
	}
}
