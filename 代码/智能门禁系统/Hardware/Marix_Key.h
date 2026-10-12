#ifndef __MARIX_KEY_H
#define __MARIX_KEY_H

#include "stm32f10x.h"
#include "Delay.h"
#include "Beep.h"

/*获取按键值的方式有两种：
    一种是阻塞式的等待按键按下(Get_Key)，
    另一种是配合Timer.c文件的TIM2外设扫描key_flag值判断是否按下*/

void Matrix_Key_Init(void);
uint8_t Matrix_Key_Scan(void);
//uint16_t Get_Key(void);



#endif
