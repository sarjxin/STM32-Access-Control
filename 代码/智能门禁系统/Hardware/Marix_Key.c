#include "Marix_Key.h"

#define Delay_time 1

/**
 * @brief 矩阵键盘初始化
 */
void Matrix_Key_Init(void)
{
    // 1.开启GPIOA和GPIOB的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    // 2.设置GPIOA为推挽输出，设置GPIOB为下拉输入
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_5 | GPIO_Pin_4;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 3.初始状态是每行都是低电平
    GPIO_ResetBits(GPIOA, GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_5 | GPIO_Pin_4);
}

/**
 * @brief 矩阵键盘扫描
 */
uint8_t Matrix_Key_Scan(void)
{

    // 1.每次扫描前先把四行全部拉低，清除上一轮的残留电平状态
    GPIO_ResetBits(GPIOA, GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_5 | GPIO_Pin_4);

    // 2.定义行、列索引，及按键键值表(用const，他后面不可改变)
    const uint16_t rowPins[4] = {GPIO_Pin_7, GPIO_Pin_6, GPIO_Pin_5, GPIO_Pin_4};
    const uint16_t colPins[4] = {GPIO_Pin_12, GPIO_Pin_13, GPIO_Pin_14, GPIO_Pin_15};
    const uint8_t keyValue[4][4] = {{1, 2, 3, 10},    // 第一行：1 2 3 A
                                    {4, 5, 6, 11},    // 第二行：4 5 6 B
                                    {7, 8, 9, 12},    // 第三行：7 8 9 C
                                    {20, 0, 21, 13}}; // 第四行：* 0 # D

    for (uint8_t row = 1; row <= 4; row++)
    {
        // 1.设置第row的引脚为推挽输出1
        GPIO_SetBits(GPIOA, rowPins[row - 1]);

        // 2.让每列依次输入值(再用一个for循环)，并且如果扫描到了，就将第row的引脚推挽输出为0，且返回keyValue[row-1][col-1]
        for (uint8_t col = 1; col <= 4; col++)
        {
            if (GPIO_ReadInputDataBit(GPIOB, colPins[col - 1]) == 1)
            {
                // Beep_Flash();   //蜂鸣器叫1声反馈按键按下信号
                GPIO_ResetBits(GPIOA, rowPins[row - 1]);
                return keyValue[row - 1][col - 1];
            }
        }
        // 3.如果一直没有扫描到，那么设置第row的引脚为推挽输出0
        GPIO_ResetBits(GPIOA, rowPins[row - 1]);
    }
    // 如果啥也没有，就返回50
    return 50;
}

/**
 * @brief 矩阵键盘扫描，并返回按键值
 * @return uint8_t 按键值
 */
// uint16_t Get_Key(void)
// {
//     uint8_t key_num = 50;
//     while(key_num == 50)
//     {
//         key_num = Matrix_Key_Scan();
//         if(key_num != 50)
//         {
//             Delay_ms(20);
//             if(key_num != 50)
//             {
//                 while(Matrix_Key_Scan() != 50);
//                 return key_num;

//             }
//         }
//     }
// }
