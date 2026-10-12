#ifndef __UART_FINGERPRINT_H
#define __UART_FINGERPRINT_H

#include "stm32f10x.h"

#define UART2_RX_BUFFER_SIZE 128

// extern volatile uint8_t UART2_RxBuffer[UART2_RX_BUFFER_SIZE];
// extern volatile uint16_t UART2_RxWriteIndex;   // 写指针
// extern volatile uint16_t UART2_RxReadIndex;    //读指针

void UART_Fingerprint_Init(void);
void UART_Fingerprint_SendByte(uint8_t data);
void UART_Fingerprint_SendBuffer(uint8_t *buffer, uint16_t length);
// uint8_t UART_ReceiveByte(void);

int8_t UART_Fingerprint_ReadByte(uint8_t *data);
uint8_t UART_Fingerprint_WaitByte(void);

#endif
