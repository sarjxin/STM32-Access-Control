#include "W64Q25.h"
#include "Beep.h"
#include "stm32f10x.h"

/*有个技巧，发送的时候是uint8_t的类型，高位会被舍弃。因此编程的时候，
把要发送的数据放到低八位就可以了*/
/*功能函数都是根据W64Q25的数据手册来写的，看他的特性*/

/*W64Q25的数据采样都发生在"时钟从空闲变为有效"的那个边沿，因此模式0/3都可以*/

void W64Q25_Init(void)
{
    MySPI_Init(W64Q25_SS_PORT, W64Q25_SS_PIN);
}

/**
 * @brief 等待忙状态过去
 * @return 1:超时
 */
uint8_t W64Q25_WaitBusy(void)
{

    uint32_t timeout = 0;

    // 开始信号
    MySPI_Start(W64Q25_SS_PORT, W64Q25_SS_PIN);
    // 读取状态寄存器1信号
    // 判断busy位，进入while循环
    MySPI_SwapByte(W25Q64_READ_STATUS_REGISTER_1);
    while ((MySPI_SwapByte(W25Q64_DUMMY_BYTE) & 0x01) == 1)
    {
        timeout++;
        if (timeout > 1000000)
        {
            return 1;
        }
    }
    MySPI_Stop(W64Q25_SS_PORT, W64Q25_SS_PIN);
    return 0;
}

/**
 * @brief 写使能函数(对于W64Q25，写之前需要先写使能一下，写完之后自动会写失能)
 */
void W64Q25_WriteEnable(void)
{
    MySPI_Start(W64Q25_SS_PORT, W64Q25_SS_PIN);
    MySPI_SwapByte(W25Q64_WRITE_ENABLE);
    MySPI_Stop(W64Q25_SS_PORT, W64Q25_SS_PIN);
}

void W64Q25_ReadID(uint8_t *Mid, uint16_t *DID)
{
    MySPI_Start(W64Q25_SS_PORT, W64Q25_SS_PIN);
    MySPI_SwapByte(0x9F); // 读指令，对于W64Q25不需要写指令
    *Mid = MySPI_SwapByte(0xFF);
    *DID = MySPI_SwapByte(0xFF);
    *DID <<= 8;
    *DID |= MySPI_SwapByte(0xFF);

    MySPI_Stop(W64Q25_SS_PORT, W64Q25_SS_PIN);
}

void W64Q25_SectorErase(uint32_t addr) // 也是一种“写”
{
    W64Q25_WriteEnable();

    MySPI_Start(W64Q25_SS_PORT, W64Q25_SS_PIN);
    MySPI_SwapByte(W25Q64_SECTOR_ERASE_4KB); // 写一页的指令
    MySPI_SwapByte(addr >> 16);
    MySPI_SwapByte(addr >> 8);
    MySPI_SwapByte(addr);
    MySPI_Stop(W64Q25_SS_PORT, W64Q25_SS_PIN);

    W64Q25_WaitBusy();
}

void W64Q25_PageProgram(uint32_t addr, const uint8_t *data, uint32_t size) // 写函数
{
    // W64Q25_SectorErase(addr);
    W64Q25_WriteEnable();

    MySPI_Start(W64Q25_SS_PORT, W64Q25_SS_PIN);
    MySPI_SwapByte(W25Q64_PAGE_PROGRAM); // 写一页的指令
    MySPI_SwapByte(addr >> 16);
    MySPI_SwapByte(addr >> 8);
    MySPI_SwapByte(addr);
    for (int i = 0; i < size; i++)
    {
        MySPI_SwapByte(data[i]);
    }
    MySPI_Stop(W64Q25_SS_PORT, W64Q25_SS_PIN);

    W64Q25_WaitBusy();
}

void W64Q25_ReadData(uint32_t addr, uint8_t *data, uint32_t size) // 读取函数
{
    // 开始信号
    MySPI_Start(W64Q25_SS_PORT, W64Q25_SS_PIN);
    // 给03h的读指令
    MySPI_SwapByte(W25Q64_READ_DATA);
    // 给24位地址
    MySPI_SwapByte(addr >> 16);
    MySPI_SwapByte(addr >> 8);
    MySPI_SwapByte(addr);
    // 开始读数据，存放到data中
    for (int i = 0; i < size; i++)
    {
        data[i] = MySPI_SwapByte(W25Q64_DUMMY_BYTE);
    }
    // 给停止信号
    MySPI_Stop(W64Q25_SS_PORT, W64Q25_SS_PIN);
}

/**
 * @brief 在扇区中任意写入字节,且不影响选定地区以外的数据
 * @param addr 写入的起始地址(W25Q64内部的地址)
 * @param data 要写入的数据地址(数组名)
 * @param size 要写入的数据的长度，单位为字节
 */
void W64Q25_SectorProgram(uint32_t addr, const uint8_t *data, uint32_t size)
{
    Beep_Flash();
    // 1.先把地址取一下，把整个扇区的数据读下来
    uint32_t sector_addr = addr & 0xFFFFF000;
    // 拿个装扇区的容器
    static uint8_t temp_buffer[4096] = {0};
    // 读取扇区数据装进去
    W64Q25_ReadData(sector_addr, temp_buffer, 4096);

    // 扇区擦除
    W64Q25_SectorErase(sector_addr);

    // 把写入扇区的起始位置提取出来
    uint16_t start_addr = addr & 0x00000FFF;

    if ((start_addr + size) > 4096)
    {
        size = 4096 - start_addr;
    }

    // 把要放入的数据写进指定的位置里
    for (uint16_t i = 0; i < size; i++)
    {
        // 把data的数据写进开始的位置
        temp_buffer[start_addr + i] = data[i];
    }

    // 然后将temp_buffer的数据写进扇区里面
    for (uint16_t i = 0; i < 16; i++)
    {
        W64Q25_PageProgram(sector_addr + i * 256, temp_buffer + i * 256, 256);
    }
}
