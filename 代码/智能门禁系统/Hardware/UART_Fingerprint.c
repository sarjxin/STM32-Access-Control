#include "UART_Fingerprint.h"

volatile uint8_t UART2_RxBuffer[UART2_RX_BUFFER_SIZE];
volatile uint16_t UART2_RxWriteIndex = 0; // 写指针
volatile uint16_t UART2_RxReadIndex = 0;  // 读指针

// 用UART2接收指纹模块的数据
// RX(模块的RX)_PA2(stm32的TX)、TX(模块的TX)_PA3(stm32的RX)

void UART_Fingerprint_Init(void)
{
    // 开启时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 配置GPIO口
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; // GPIO_Pin_2
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3; // GPIO_Pin_3
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置USART参数
    USART_InitTypeDef USART2_InitStructure;
    USART2_InitStructure.USART_BaudRate = 57600;
    USART2_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 不太懂
    USART2_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART2_InitStructure.USART_Parity = USART_Parity_No;
    USART2_InitStructure.USART_StopBits = USART_StopBits_2;
    USART2_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART2, &USART2_InitStructure);

    // 设置NVIC中断组
    // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    // 设置NVIC中断
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 2;
    NVIC_Init(&NVIC_InitStruct);

    // 开启USART中断启动
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    // 使能硬件外设
    USART_Cmd(USART2, ENABLE);
}

void UART_Fingerprint_SendByte(uint8_t data)
{
    USART_SendData(USART2, data);
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET)
        ; // 如果还没发出去就等着
}

void UART_Fingerprint_SendBuffer(uint8_t *buffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        UART_Fingerprint_SendByte(buffer[i]);
    }
}

// 没有经过环形缓冲区的接收方式
// uint8_t UART_ReceiveByte(void)
// {
//     // 先清掉可能挂着的 ORE/FE/PE，避免它们干扰后续接收
//     // if(USART_GetFlagStatus(USART2, USART_FLAG_ORE) == SET)
//     // {
//     //     uint8_t dummy = USART_ReceiveData(USART2);   // 读 DR 清 ORE
//     //     (void)dummy;
//     // }

//     while (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == RESET)
//         ;

//     return (uint8_t)USART_ReceiveData(USART2);
// }

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
int8_t UART_Fingerprint_ReadByte(uint8_t *data)
{
    // 1. 如果读写指针重合，说明没有新数据，返回 0
    if (UART2_RxReadIndex == UART2_RxWriteIndex)
    {
        return 0;
    }

    // 2. 取出当前读指针指向格子的数据
    *data = UART2_RxBuffer[UART2_RxReadIndex]; // 取数据
    UART2_RxReadIndex++;                       // 读指针前进一格

    // 3. 环形核心：读到末尾了，同样绕回 0 号格子
    if (UART2_RxReadIndex >= UART2_RX_BUFFER_SIZE)
    {
        UART2_RxReadIndex = 0;
    }
    return 1; // 成功读到一个字节
}

/*死等模式*/
uint8_t UART_Fingerprint_WaitByte(void)
{
    uint8_t data;
    // 一直循环调用 ReadByte，直到中断把数据写入缓冲区（ReadByte 返回 1）才跳出
    while (UART_Fingerprint_ReadByte(&data) == 0)
    {
    }

    return data;
}

// ① 要不要绕回 0？
// ② 缓冲区满没满？
void USART2_IRQHandler(void) // USART来数据便进入中断进行接收，接收到缓冲区里面
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
    {
        // 进入了中断要做什么事情

        uint8_t data = 0;
        uint16_t next_write_index;

        /* 读取收到的字节 */
        data = USART_ReceiveData(USART2);

        /* 1. 算一下写指针走下一步的位置 */
        next_write_index = UART2_RxWriteIndex + 1;

        /* 2. 环形核心：如果走到数组末尾，绕回 0 号格子 */
        if (next_write_index >= UART2_RX_BUFFER_SIZE)
        {
            next_write_index = 0;
        }

        /* 3. 检查缓冲区满没满：
        如果写指针跨前一步正好撞上读指针，说明缓冲区装满了！不能再写了 */
        if (next_write_index != UART2_RxReadIndex)
        {
            UART2_RxBuffer[UART2_RxWriteIndex] = data; // 放入当前格子
            UART2_RxWriteIndex = next_write_index;     // 写指针前进一格
        }
        else
        {
            /* 满了，丢弃新数据，避免覆盖未读的旧数据 */
        }

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}
