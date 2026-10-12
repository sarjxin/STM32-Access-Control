#include "Beep.h"

/*硬件电路：PC13高电平蜂鸣器响*/

/**
 * @brief 初始化蜂鸣器
 */
void Beep_Init(void)
{
    // 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);

    // 设置引脚模式
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13;
    GPIO_Init(GPIOC,&GPIO_InitStruct);

    // 设置引脚初始状态(蜂鸣器不叫)
    GPIO_ResetBits(GPIOC,GPIO_Pin_13);
}

void Beep_ON(void)
{
    GPIO_SetBits(GPIOC,GPIO_Pin_13);
}

void Beep_OFF(void)
{
    GPIO_ResetBits(GPIOC,GPIO_Pin_13);
}

void Beep_Flash(void)
{
    Beep_ON();
    Delay_ms(75);
    Beep_OFF();
}

#if BEEP_KEY_VOICE_ENABLE
/* 运行期按键音开关标志：默认1开启，后续可通过Beep_SetKeyVoice(0)关闭 */
static uint8_t s_beep_key_voice_enable = 1;
#endif

/**
 * @brief 按键提示音（内部调用Beep_Flash，并受开关控制）
 */
void Beep_KeyPrompt(void)
{
#if BEEP_KEY_VOICE_ENABLE
    if (s_beep_key_voice_enable)
    {
        Beep_Flash();
    }
#endif
}

/**
 * @brief 运行期设置按键提示音开关
 * @param enable 1:开启, 0:关闭
 */
void Beep_SetKeyVoice(uint8_t enable)
{
#if BEEP_KEY_VOICE_ENABLE
    s_beep_key_voice_enable = enable;
#endif
}



