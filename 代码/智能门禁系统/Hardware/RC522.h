#ifndef RC522_H
#define RC522_H

#include "stm32f10x.h"
#include "RC522_Define.h"
#include "MySPI.h"
#include "Delay.h"

typedef enum {
    RC522_OK = 0,
    RC522_TIMEOUT,
    RC522_COLLISION,
    RC522_DEVICE_ERROR,
    RC522_FRAME_ERROR,
    RC522_BUFFER_ERROR,
    RC522_BCC_ERROR
} RC522_Status;

uint8_t RC522_Init(void);

void RC522_ReadRegister(uint8_t addr, uint8_t len, uint8_t *receiveBuffer);
void RC522_WriteRegister(uint8_t addr, uint8_t len, uint8_t *WriteBuffer);
uint8_t RC522_ReadReg(uint8_t addr);
void RC522_WriteReg(uint8_t addr, uint8_t val);

uint8_t RC522_Reset(void);
void RC522_AntennaOn(void);
void RC522_AntennaOff(void);

RC522_Status RC522_ReadUid4(uint8_t uid[4], uint8_t atqa[2]);

#endif
