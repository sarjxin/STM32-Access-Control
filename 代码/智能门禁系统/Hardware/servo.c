#include "servo.h"
uint16_t TimerValue = 0;
/**
 * @brief 定时器初始化
 */
void SERVO_Init(void)
{
    // 1.开启时钟，这里是GPIO的PA0_CH1时钟和TIM2的时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    //RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);     //打开AFIO的时钟

    //GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM2,ENABLE);    //将TIM2_CH1从PA0重定义到PA15
    //GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE); //取消PB15调试端口的复用

    // 2.配置内部时钟作为时钟源，别用错函数了
    TIM_InternalClockConfig(TIM2);

    // 3.配置GPIO口
    GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;       //GPIO_Pin_15
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3.配置时基单元
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 200 - 1; 
            //一共数200个数，舵机20ms里面，0.5ms~25ms对应0~180°，根据换算，CCR的值(数数)可以是5~25          
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1; //PSC   //一个周期20ms(由Period和Prescaler共同决定)
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // 4.配置输出比较单元
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0;    //CCR。如果改变占空比的话，这个值必须是0，否则会有影响
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC1Init(TIM2, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);

    // 6.使能定时器(在使能之前，可以先清除一下中断标志位，
    //                      这是由于前面的配置时基单元函数末尾手动更新了一个更新事件，让预分频值有效)
    TIM_Cmd(TIM2, ENABLE);

}

/**
 * @brief 用于设置角度，一共20个档位，180/20=9°
 * @param 度数。uint16_t angle:用了uint16_t，不是很合理，毕竟180<256，但防止用者想要360°以内旋转出现隐藏bug
 * @return 0:成功，1:失败(angle>180，传入参数不合理)
 */
uint8_t SERVO_SetAngle(uint16_t angle)
{
    if(angle > 180)
    {
        // 该函数的传参不支持180以上的角度
        return 1;
    }

    // 一共是20档。5(0°)~25(180°)
    // 先将角度转换为档位，再将档位转换为CCR1的值
    TIM2->CCR1 = ( (angle + 4) / 9) + 5;   //通过angle + 4，形成了“四舍五入”的效果

    return 0;
}




