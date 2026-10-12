#include "Timer.h"
#include "Beep.h"
#include "stm32f10x.h"

/*
 * 给主程序使用的“按键邮箱”：
 *
 * key_flag == 0：目前没有新的按键事件
 * key_flag == 1：有一个已消抖的新按键，键值保存在 keyPress_value
 */
volatile uint8_t keyPress_value = KEY_NONE;
volatile uint8_t key_flag = 0;

/* 以下变量只给 TIM1 中断内部使用，不在 main.c 里修改 */
static uint8_t raw_last = KEY_NONE;   // 上一次“原始扫描”结果
static uint8_t stable_key = KEY_NONE; // 已确认稳定的按键状态
static uint8_t same_count = 0;        // 连续相同的扫描次数

/**
 * @brief 定时器初始化
 */
void TIMER_Init(void)
{
    // 1.开启时钟，这里是RCC内部时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    // 2.配置内部时钟
    TIM_InternalClockConfig(TIM1);

    // 3.配置时基单元
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 720 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 1000 - 1; // 一个“水滴是”1/72us。要定时10ms
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    TIM_ClearFlag(TIM1, TIM_FLAG_Update); // 清除中断标志位

    // 4.配置定时器的中断输出控制
    TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);

    // 5.选择NVIC的抢占/响应优先级组、打通TIME2到NVIC的中断信号
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&NVIC_InitStructure);

    // 6.使能定时器(在使能之前，可以先清除一下中断标志位，
    //                      这是由于前面的配置时基单元函数末尾手动更新了一个更新事件，让预分频值有效)
    TIM_Cmd(TIM1, ENABLE);

    // 7.写好中断函数，先判断中断源是不是指定的TIMER，然后编写功能代码，最后清除中断标志位
}

/*空闲稳定为 50
→ 按下 2，连续两次扫描到 2
→ stable_key 从 50 变成 2：上报一次按键 2
→ 一直按着 2：stable_key 仍是 2，不再上报
→ 松手，连续两次扫描到 50
→ stable_key 从 2 变成 50：不报告事件
→ 下一次再按键，才能再次上报*/
void TIM1_UP_IRQHandler(void)
{
    uint8_t raw_key; // 本次扫描到的、尚未确认是否稳定的键值

    /* 先确认本次真的是 TIM1 更新中断 */
    if (TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
    {
        /* 必须先清中断标志，否则会不断进入中断 */
        TIM_ClearITPendingBit(TIM1, TIM_IT_Update);

        /*
         * 扫描矩阵键盘：
         * 有按键时返回 0~21 中的一个键值；
         * 没有按键时返回 KEY_NONE，也就是 50。
         */
        raw_key = Matrix_Key_Scan();

        /*
         * 第一步：判断本次扫描值，和上一次原始扫描值是否相同。
         *
         * 相同：说明按键状态可能稳定，连续计数 +1。
         * 不同：说明状态刚变化，例如“没按 → 按下 2”，
         *       或者“按下 2 → 松开”，重新开始计数。
         */
        if (raw_key == raw_last)
        {
            if (same_count < DEBOUNCE_SAMPLES)
            {
                same_count++;
            }
        }
        else
        {
            raw_last = raw_key;
            same_count = 1;
        }

        /*
         * 第二步：满足两个条件，才正式确认状态改变：
         *
         * 1. 连续两次扫描结果相同，已经完成消抖；
         * 2. 本次稳定状态和上一次确认的稳定状态不同。
         *
         * 举例：
         * 50 → 2：确认“按下数字 2”
         * 2  → 50：确认“数字 2 已松开”
         */
        if ((same_count >= DEBOUNCE_SAMPLES) &&
            (raw_key != stable_key)) // raw_key != stable_key条件可以保证不会重复上报
        {
            /*
             * 更新已确认的稳定状态。
             * 即使是松手，也要更新成 KEY_NONE，
             * 否则下一次再按同一个键不会被识别为新按键。
             */
            stable_key = raw_key;

            /*
             * 只有“按下某个键”才通知主程序。
             *
             * 松手时 raw_key == KEY_NONE：
             * 这里只更新 stable_key，不产生按键事件。
             *
             * 所以同一个键一直按着时，只会报告一次；
             * 必须松开后重新按，才会再次报告。
             */
            if (raw_key != KEY_NONE) // 不是50才选择上报，否则不进入这里
            {
                /*
                 * 若主程序还没取走旧按键，先不覆盖它。
                 * 这是一个只能保存一个按键的简单邮箱。
                 */
                if (key_flag == 0)
                {
                    keyPress_value = raw_key; // 保存本次按下的键值
                    key_flag = 1;             // 通知主程序：“有新按键”
                    Beep_KeyPrompt();         // 按键按下提示音
                }
            }
        }
    }
}

/*
内部判决组（负责“看清按键到底按没按、稳不稳定”）
raw_key：“本次抓拍值”。这一瞬间硬件引脚读出来的原始键值（50 表示没按，0~21 表示对应按键）。

raw_last：“上次抓拍值”。用于比对两次抓拍是否一致。

same_count：“稳定计数器”。记录连续抓到相同结果的次数。只有连续 2 次（20ms）都一样，才算通过“消抖”。

stable_key：“当前被系统正式认可的稳定状态”。记录上一次已经确认的状态（是“空闲没按”，还是“当前正按着某个键”）。
*/

/*
对外信箱组（负责“把结果通知 main 主函数”）
keyPress_value：“信件内容”。最终被确认按下的有效按键值。

key_flag：“邮箱红旗”。0 表示没新按键；1 表示有新按键未取走。
*/
