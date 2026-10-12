#ifndef __TIMER_H_
#define __TIMER_H_

#include "stm32f10x.h"
#include "Marix_Key.h"

/* 没有按键时 Matrix_Key_Scan() 返回 50 */
#define KEY_NONE            50

/*
 * TIM1 每 10ms 扫描一次。
 * 连续扫描到 2 次相同的值，才认为按键状态稳定。
 */
#define DEBOUNCE_SAMPLES    2


extern volatile uint8_t keyPress_value; 
extern volatile uint8_t key_flag;

void TIMER_Init(void);


#endif
