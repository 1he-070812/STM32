#ifndef __Myi2c_H__
#define __Myi2c_H__
void Myi2c_Init(void);

void Myi2c_Start(void);
void Myi2c_Stop(void);
void Myi2c_Sendbyte(uint8_t byte);
uint8_t Myi2c_readbyte(void);
void Myi2c_SendAck(uint8_t ACk);
uint8_t Myi2c_readAck(void);
#endif
