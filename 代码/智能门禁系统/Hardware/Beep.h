#ifndef __BEEP_H
#define __BEEP_H

#include "stm32f10x.h"
#include "Delay.h"

void Beep_Init(void);
void Beep_ON(void);
void Beep_OFF(void);
void Beep_Flash(void);  //叫一声，75ms

/* 按键提示音编译期开关（1:开启，0:关闭） */
#define BEEP_KEY_VOICE_ENABLE   1

void Beep_KeyPrompt(void);              // 按键提示音（内部调用Beep_Flash，并受开关控制）
void Beep_SetKeyVoice(uint8_t enable);  // 运行期动态开关按键提示音（1:开启，0:关闭）

#endif

