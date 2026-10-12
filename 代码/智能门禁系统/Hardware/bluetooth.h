#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H

#include "stm32f10x.h"
#include "string.h"

#define UART1_RX_BUFFER_SIZE 128
#define bluetooth_data_size  128
#define CMD_TABLE_SIZE       (sizeof(CMD_TABLE) / sizeof(CMD_TABLE[0]))
typedef enum {       // 这里添加之后还需要在Bluetooth_Cmd_Map CMD_TABLE[]添加
    BT_CMD_NONE = 0, // 无有效指令 / 未知指令
    BT_CMD_CONNECT,  // 蓝牙连接成功
    UNLOCK,          // 开锁指令
    LOCK,            // 关锁指令
    STATUS,          // 查状态指令
    exitToMemu,      // 退出到主菜单指令
} Bluetooth_Cmd_Type;

extern volatile uint8_t bluetooth_data[bluetooth_data_size];
/*功能待写，蓝牙解锁*/
void JDY31_Init(uint32_t baudrate);
void UART_JDY31_SendByte(uint8_t data);
void UART_JDY31_SendBuffer(uint8_t *buffer, uint16_t length); // STM32发送数据给蓝牙，蓝牙会把数据发给手机
uint8_t UART_JDY31_WaitCmd(void);                             // STM32死等蓝牙接收到的数据（手机发来的串口数据）
Bluetooth_Cmd_Type JDY31_analyseCmd(void);
#endif
