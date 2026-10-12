#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

void SERVO_Init(void);
uint8_t SERVO_SetAngle(uint16_t angle);


#endif
