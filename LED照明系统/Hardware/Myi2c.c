#include "stm32f10x.h"                  // Device header
#include "Delay.h"
void My_W_SCL(uint8_t BitVal)//写入数据，SCL置电平,时钟线
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_10,(BitAction)BitVal);
	Delay_us(10);
}

void My_W_SDA(uint8_t BitVal)//写入数据，SDA置电平，数据线
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_11,(BitAction)BitVal);
	Delay_us(10);
}


uint8_t My_R_SDA(void)//读取SDA写入的电平
{
	uint8_t BitVal;
	BitVal=GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11);
	Delay_us(10);
	return BitVal;
}
void Myi2c_Init(void)
{
	
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);		//开启GPIOB的时钟
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);			
	
	GPIO_SetBits(GPIOB,GPIO_Pin_10 | GPIO_Pin_11);
}

void Myi2c_Start(void)//起始条件
{
	My_W_SDA(1);
	My_W_SCL(1);
	My_W_SDA(0);
	My_W_SCL(0);


}

void Myi2c_Stop(void)
{
	My_W_SDA(0);
	My_W_SCL(1);
	My_W_SDA(1);
}

void Myi2c_Sendbyte(uint8_t byte)
{
    uint8_t i;
    for(i=0;i<8;i++)
    {
        My_W_SDA(!!(byte & (0x80 >> i)));   // 用 & 不是 &=，!! 保险起见
        My_W_SCL(1);
        My_W_SCL(0);
    }
}

uint8_t Myi2c_readbyte(void)//读取写入的数据
{
	uint8_t i,Byte=0X00;
	My_W_SDA(1);
	for(i=0;i<8;i++)
	{
		My_W_SCL(1);//SCL为高电平时读取
		if(My_R_SDA()==1)
		{
			Byte|=(0X80>>i);
		}
		My_W_SCL(0);
	}
	return Byte;	
}

void Myi2c_SendAck(uint8_t ACk)//发送应答
{
	My_W_SDA(ACk);
	My_W_SCL(1);
	My_W_SCL(0);
}

uint8_t Myi2c_readAck(void)
{
	uint8_t ACk;
	My_W_SDA(1);
	My_W_SCL(1);
	ACk=My_R_SDA();//0有应答，1无
	My_W_SCL(0);
	return ACk;
	
}
