#include "bluetooth.h"

typedef struct
{
    const char *cmd_str;         // 目标字符串
    Bluetooth_Cmd_Type cmd_type; // 对应的事件枚举
    uint8_t is_exact_match;      // 1: 全字匹配(strcmp), 0: 包含匹配(strstr)
} Bluetooth_Cmd_Map;

static const Bluetooth_Cmd_Map CMD_TABLE[] = {
    {"CONNECTED", BT_CMD_CONNECT, 0}, // 包含 CONNECT 就算
    {"UNLOCK", UNLOCK, 0},
    {"LOCK", LOCK, 0},
    {"STATUS", STATUS, 0},         // 必须完全等于 UNLOCK
    {"exitToMemu", exitToMemu, 0}, // 退出到主菜单指令
};

volatile uint8_t UART1_RxBuffer[UART1_RX_BUFFER_SIZE];
volatile uint8_t bluetooth_data[bluetooth_data_size];
volatile uint16_t bluetooth_data_index = 0;
volatile uint16_t UART1_RxWriteIndex = 0; // 写指针
volatile uint16_t UART1_RxReadIndex = 0;  // 读指针

// volatile Bluetooth_Cmd_Type bluetooth_cmd_type = BT_CMD_NONE; // 什么情况下需要使用volatile？这里需要使用吗

// RX(模块的RX)_PA2(stm32的TX)、TX(模块的TX)_PA3(stm32的RX)
void JDY31_Init(uint32_t baudrate)
{
    // 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 配置GPIO口
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置USART参数
    USART_InitTypeDef USART1_InitStructure;
    USART1_InitStructure.USART_BaudRate = baudrate;
    USART1_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 不太懂
    USART1_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART1_InitStructure.USART_Parity = USART_Parity_No;
    USART1_InitStructure.USART_StopBits = USART_StopBits_1;
    USART1_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART1, &USART1_InitStructure);

    // 设置NVIC中断组
    // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    // 设置NVIC中断
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 2;
    NVIC_Init(&NVIC_InitStruct);

    // 开启USART中断启动
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    // 使能硬件外设
    USART_Cmd(USART1, ENABLE);
}

void UART_JDY31_SendByte(uint8_t data)
{
    USART_SendData(USART1, data);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        ; // 如果还没发出去就等着
}

void UART_JDY31_SendBuffer(uint8_t *buffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        UART_JDY31_SendByte(buffer[i]);
    }
}

/**
 * @brief 判断环形缓冲区有没有新数据，如果有，则从环形缓冲区里等待数据
 * @param *data：存放数据的指针
 * @return 0:没有新数据
 *         1:有新数据
 */
// ① read == write？    是 → 没数据
// ② 不相等    → 取 read 指针的数据
// ③ read++
// ④ 到头了就回 0
int8_t UART_JDY31_ReadByte(uint8_t *data)
{
    if (UART1_RxReadIndex == UART1_RxWriteIndex)
    {
        return 0;
    }

    *data = UART1_RxBuffer[UART1_RxReadIndex];
    UART1_RxReadIndex++;

    // 不能让读指针超过缓冲区的总容量
    if (UART1_RxReadIndex >= UART1_RX_BUFFER_SIZE)
    {
        UART1_RxReadIndex = 0;
    }
    return 1;
}

/*死等有没有数据*/
uint8_t UART_JDY31_WaitByte(void)
{
    uint8_t data;

    while (UART_JDY31_ReadByte(&data) == 0)
    {
    }

    return data;
}

uint8_t UART_JDY31_WaitCmd(void)
{
    memset((void *)bluetooth_data, 0, sizeof(bluetooth_data));
    bluetooth_data_index = 0;
    uint8_t data;
    uint8_t result = 0;

    while (1)
    {
        if (bluetooth_data_index >= bluetooth_data_size - 1)
        {
            result = 0;
            break;
        }
        data = UART_JDY31_WaitByte();
        bluetooth_data[bluetooth_data_index] = data;
        if (bluetooth_data_index > 0 && bluetooth_data[bluetooth_data_index] == '\n' && bluetooth_data[bluetooth_data_index - 1] == '\r')
        {
            result = 1;
            bluetooth_data[bluetooth_data_index - 1] = '\0';
            break; // 接收完了就跳出循环
        }
        bluetooth_data_index++;
    }

    return result;
}

Bluetooth_Cmd_Type JDY31_analyseCmd(void)
{
    if (UART_JDY31_WaitCmd() == 0)
        return BT_CMD_NONE;
    for (uint8_t i = 0; i < CMD_TABLE_SIZE; i++)
    {
        if (CMD_TABLE[i].is_exact_match)
        {
            if (strcmp((char *)bluetooth_data, CMD_TABLE[i].cmd_str) == 0)
                return CMD_TABLE[i].cmd_type;
        }
        else
        {
            if (strstr((char *)bluetooth_data, CMD_TABLE[i].cmd_str) != NULL)
                return CMD_TABLE[i].cmd_type;
        }
    }
    return BT_CMD_NONE;
}

// ① 要不要绕回 0？
// ② 缓冲区满没满？
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
    {
        // 进入了中断要做什么事情

        uint8_t data = 0;
        uint16_t next_write_index;

        /* 读取收到的字节 */
        data = USART_ReceiveData(USART1);

        /* 计算写入下一个位置 */
        next_write_index = UART1_RxWriteIndex + 1;

        /* 如果到数组末尾，则回到0 */
        if (next_write_index >= UART1_RX_BUFFER_SIZE)
        {
            next_write_index = 0;
        }

        /* 判断缓冲区是否已满 */
        // 如果没有满，把读出来的值放进缓冲区，并且调整UART1_RxWriteIndex值，为下一轮做准备
        if (next_write_index != UART1_RxReadIndex)
        {
            UART1_RxBuffer[UART1_RxWriteIndex] = data;
            UART1_RxWriteIndex = next_write_index;
        }
        else
        {
            /* 缓冲区满了，这个字节暂时丢弃 */
        }

        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}
