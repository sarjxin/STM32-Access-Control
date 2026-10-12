#include "MySPI.h"
#include "Beep.h"
#include "Delay.h"
#include "stm32f10x.h"

/*使用模式0*/

void MySPI_SetMOSI(uint8_t BitVal)
{
    GPIO_WriteBit(SPI_MOSI_SCK_PORT, SPI_MOSI_PIN, (BitAction)BitVal);
}

void MySPI_CLK(uint8_t BitVal)
{
    GPIO_WriteBit(SPI_MOSI_SCK_PORT, SPI_SCK_PIN, (BitAction)BitVal);
}

void MySPI_Start(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    // SS引脚产生下降沿
    GPIO_WriteBit(GPIOx, GPIO_Pin, Bit_RESET);
}

void MySPI_Stop(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    //  SS引脚产生上升沿
    GPIO_WriteBit(GPIOx, GPIO_Pin, Bit_SET);
}

uint8_t MySPI_SwapByte(uint8_t data)
{
    uint8_t in_value = 0x00;
    for (int i = 0; i < 8; i++)
    {
        // MOSI移出数据
        MySPI_SetMOSI(data & (0x80 >> i));

        // 拉高时钟
        MySPI_CLK(1);

        // MISO移入数据
        if (GPIO_ReadInputDataBit(SPI_MISO_PORT, SPI_MISO_PIN) == 1)
        {
            in_value |= (0x80 >> i);
        }

        // 拉低时钟
        MySPI_CLK(0);
    }
    return in_value;
}

void MySPI_Init(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    // 开启GPIO时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 设置GPIO引脚模式
    // CLK、MOSI、SS都是推挽输出模式
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = SPI_SCK_PIN | SPI_MOSI_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_MOSI_SCK_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = W64Q25_SS_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(W64Q25_SS_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = RC522_SS_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RC522_SS_PORT, &GPIO_InitStruct);

    // MISO是浮空输入模式
    GPIO_InitStruct.GPIO_Pin = SPI_MISO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_MISO_PORT, &GPIO_InitStruct);

    // 设置GPIO初始状态
    // CLK为低，SS为高
    MySPI_SetMOSI(0);
    MySPI_CLK(0);

    MySPI_Stop(GPIOx, GPIO_Pin);
}
