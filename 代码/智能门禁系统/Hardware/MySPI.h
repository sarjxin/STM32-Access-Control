#ifndef __MYSPI_H
#define __MYSPI_H

#include "stm32f10x.h"

/**
 * @warning:
 * 使用该库移植时需要注意在MySPI_Init函数里面的时钟是否开启(RCC_APB2Periph_GPIOA不好封装)
 */
#define SPI_MISO_PORT     GPIOB
#define SPI_MOSI_SCK_PORT GPIOA

#define SPI_MOSI_PIN      GPIO_Pin_8
#define SPI_MISO_PIN      GPIO_Pin_1
#define SPI_SCK_PIN       GPIO_Pin_11

#define W64Q25_SS_PORT    GPIOB
#define W64Q25_SS_PIN     GPIO_Pin_0

#define RC522_SS_PORT     GPIOA
#define RC522_SS_PIN      GPIO_Pin_12

void MySPI_Init(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void MySPI_Start(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void MySPI_Stop(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
uint8_t MySPI_SwapByte(uint8_t data);

#endif
