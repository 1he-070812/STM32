

#include "tm1637.h"
extern TIM_HandleTypeDef htim4;
 
const unsigned char SEGMENT_CODE[]=  {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x77,0x7C,0x39,0x5E,0x79,0x71};
 
 
unsigned int delay_time=2;/* us。TM1637 要求时钟半周期 ≥2.5us，配合上面 DWT 延时用 2 最稳 */
 
/* 微秒级延时：改用内核 DWT 周期计数器
   原实现依赖 TIM4，而 TIM4 的 Prescaler = 0（计数频率 72MHz，1 tick 仅 13.9ns），
   导致 delay_us(1) 实际只延时 0.014us，时序完全不准。
   DWT 不受 CubeMX 配置影响，改主频后也能自动跟随。 */
void delay_us(uint16_t us)
{
    static uint8_t dwt_ready = 0U;
    uint32_t start;
    uint32_t ticks;

    if (dwt_ready == 0U)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        dwt_ready = 1U;
    }

    start = DWT->CYCCNT;
    ticks = (uint32_t)us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < ticks)
    {
        /* busy wait */
    }
}
 
 
 
void TM1637_START()                   //开始条件
{
  
        TM1637_CLK_H();
        delay_us(delay_time);
    
        TM1637_SDA_H();
        delay_us(delay_time);
    
        TM1637_SDA_L();
        delay_us(delay_time);
    
        TM1637_CLK_L();
        delay_us(delay_time);
            
}
 
 
void TM1637_STOP()                     //结束条件
{
        
        TM1637_CLK_L();
        delay_us(delay_time);
    
        TM1637_SDA_L();
        delay_us(delay_time);
    
        TM1637_CLK_H();
        delay_us(delay_time);
    
        TM1637_SDA_H();
        delay_us(delay_time);
    
}
 
 
void TM1637_WRITE_BYTE_DATA(unsigned char mydata)          //送数据    低位先传
{
 
 
        unsigned char  i;
 
        for(i=0;i<8;i++)
        {
            TM1637_CLK_L();
            delay_us(delay_time);        
            
            if(mydata&0x01)
            {
               TM1637_SDA_H();
            }
            else
            {
               TM1637_SDA_L();
            }
            
            delay_us(delay_time);
        
            TM1637_CLK_H();
            delay_us(delay_time);
        
            mydata>>=1;
            delay_us(delay_time);
        
        }
 
        TM1637_CLK_L();
        delay_us(delay_time);
 
 
 
}
 
 
 
 
void TM1637_CHECK_ack()
{
    
        // 检查应答位，
        
     
        TM1637_CLK_L();
    
        delay_us(delay_time);
 
        /* ---- 第 9 个时钟（ACK 时隙）----
           原代码在这里死等：while(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_1)==1)
           但本工程的 SDA 是 PA6，PB1 根本没配置、是悬空输入。
           悬空脚读数受干扰随机跳变，一旦读到高就无限循环，
           把主循环卡住几百毫秒甚至更久 —— 这就是倒计时跳数的原因。
           这里只走完时序，不检查应答（TM1637 不要求主机读 ACK）---- */
     
     
        TM1637_CLK_H();
        delay_us(delay_time);
     
        TM1637_CLK_L();
        delay_us(delay_time);
 
 
}
 
 
 
/*
     
     采用固定地址模式=============
 
     TM1637---支持6位8段数码管========================================
     
     如果有4个数码管，此程序被反复执行4次
     addr===     0~5,TM1637支持6个数码管
     my_data==== 第0个数码管的数据
 
 
*/
void TM1637_WRITE_DISPLAY_BYTE_FIX_ADDRESS(unsigned char addr, unsigned char my_data)
{
    
        unsigned char get_segment=0;
    
        
        //时序  (1) start
        TM1637_START(); 
 
        //（2）发送commmd1==0X44就是固定地址
        TM1637_WRITE_BYTE_DATA(0x40+addr);
        TM1637_CHECK_ack();
        //注意这个有STOP
        TM1637_STOP();
     
     
        TM1637_START();
        //(3)发送起始地址， 
        TM1637_WRITE_BYTE_DATA(0xC0+addr);
        //要等DIN在第9个脉冲被拉低
        TM1637_CHECK_ack();
 
     
         get_segment =  SEGMENT_CODE[my_data] ;
        //(4) 发送该地址的数据
        TM1637_WRITE_BYTE_DATA(get_segment|0x80);//|0x80 此处发送0x80是打开时间中间的2个点
        //要等DIN在第9个脉冲被拉低
        TM1637_CHECK_ack(); 
     
        TM1637_STOP();    
 
        //==============
        TM1637_START(); 
        TM1637_WRITE_BYTE_DATA(0X8C);              //开显示
        TM1637_STOP();
 
 
}
 