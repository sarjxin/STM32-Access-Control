#include "audio.h"
#include "Delay.h"
#include "stm32f10x.h"

/* 协议：指令手册第1页；7字节固定字段 + N字节数据。 */
#define AUDIO_FRAME_HEAD 0x7E
#define AUDIO_FRAME_TAIL 0xEF
#define AUDIO_FRAME_BASE_SIZE 7
#define AUDIO_MAX_DATA_SIZE 3 // 当前功能最多3字节，不包含路径播放
#define AUDIO_FLASH_DISK 0x02

/*
 * 工程等待值，商家资料未给出上电就绪时间和最小命令间隔，需实物确认。
 * 9600/8N1发送1字节约需1.04ms，20次1ms轮询留有余量；中断可延长墙钟时间。
 * Delay_ms使用项目现有SysTick延时，因此这些接口不允许在中断中调用。
 */
#define AUDIO_POWER_ON_WAIT_MS 500
#define AUDIO_COMMAND_GAP_MS 20
#define AUDIO_TX_WAIT_MS 20

static uint8_t audio_initialized = 0;

/* 等待发送寄存器为空或最后一个字节发送完，避免串口故障导致死等。 */
static Audio_Status Audio_WaitFlag(uint16_t flag)
{
    uint16_t elapsed_ms = 0;

    while (USART_GetFlagStatus(USART3, flag) == RESET)
    {
        if (elapsed_ms >= AUDIO_TX_WAIT_MS)
        {
            return AUDIO_TX_TIMEOUT;
        }
        Delay_ms(1);
        elapsed_ms++;
    }
    return AUDIO_OK;
}

/* 只发送实际字节，不发送HEX文本，不添加额外校验或回车换行。 */
static Audio_Status Audio_UART_SendBuffer(const uint8_t *buffer, uint8_t length)
{
    uint8_t i;
    Audio_Status status;

    for (i = 0; i < length; i++)
    {
        status = Audio_WaitFlag(USART_FLAG_TXE);
        if (status != AUDIO_OK)
        {
            return status;
        }
        USART_SendData(USART3, buffer[i]);
    }

    // TC表示最后一个字节的停止位也已发送，TXE仅表示可以写下一个字节。
    return Audio_WaitFlag(USART_FLAG_TC);
}

/* 统一组装7/8/9/10字节命令。data_length=0时，data可以为空。 */
static Audio_Status Audio_SendPacket(uint8_t cmd, const uint8_t *data, uint8_t data_length)
{
    uint8_t packet[AUDIO_FRAME_BASE_SIZE + AUDIO_MAX_DATA_SIZE];
    uint8_t length;
    uint8_t checksum = 0;
    uint8_t i;
    Audio_Status status;

    if (data_length > AUDIO_MAX_DATA_SIZE || (data_length > 0 && data == 0))
    {
        return AUDIO_INVALID_PARAM;
    }
    if (audio_initialized == 0)
    {
        return AUDIO_NOT_INITIALIZED;
    }

    length = AUDIO_FRAME_BASE_SIZE + data_length;
    packet[0] = AUDIO_FRAME_HEAD;
    packet[1] = length;
    packet[2] = 0xFF; // 广播地址高字节
    packet[3] = 0xFF; // 广播地址低字节
    packet[4] = cmd;

    for (i = 0; i < data_length; i++)
    {
        packet[5 + i] = data[i];
    }

    // 累加到数据末尾，不包括校验字节和结束码；uint8_t转换保留低8位。
    for (i = 0; i < length - 2; i++)
    {
        checksum = (uint8_t)(checksum + packet[i]);
    }
    packet[length - 2] = checksum;
    packet[length - 1] = AUDIO_FRAME_TAIL;

    status = Audio_UART_SendBuffer(packet, length);
    if (status == AUDIO_OK)
    {
        Delay_ms(AUDIO_COMMAND_GAP_MS);
    }
    return status;
}

Audio_Status Audio_Init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;
    Audio_Status status;
    uint8_t disk = AUDIO_FLASH_DISK;

    audio_initialized = 0;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    // USART3默认TX在PB10。只发控制命令，不使用RX和接收中断。
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    USART_Cmd(USART3, DISABLE);
    USART_StructInit(&uart);
    uart.USART_BaudRate = 9600;
    uart.USART_WordLength = USART_WordLength_8b;
    uart.USART_StopBits = USART_StopBits_1;
    uart.USART_Parity = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode = USART_Mode_Tx;
    USART_Init(USART3, &uart);
    USART_Cmd(USART3, ENABLE);
    audio_initialized = 1;

    Delay_ms(AUDIO_POWER_ON_WAIT_MS);

    // 手册第4页：FLASH盘符为02；选曲播放使用当前盘符。
    status = Audio_SendPacket(AUDIO_CMD_SELECT_DISK, &disk, 1);
    if (status == AUDIO_OK)
    {
        status = Audio_SetVolume(AUDIO_DEFAULT_VOLUME);
    }
    if (status == AUDIO_OK)
    {
        // 手册第6页的单曲停止示例：模式06、循环次数字节00 00。
        status = Audio_SetPlayMode(AUDIO_PLAY_MODE_SINGLE_STOP, 0);
    }
    if (status != AUDIO_OK)
    {
        audio_initialized = 0;
    }
    return status;
}

Audio_Status Audio_SendCommand(uint8_t cmd)
{
    // 防止误用Audio_SendCommand(AUDIO_CMD_SELECT)发送缺少参数的命令。
    switch (cmd)
    {
    case AUDIO_CMD_PLAY:
    case AUDIO_CMD_PAUSE:
    case AUDIO_CMD_STOP:
    case AUDIO_CMD_PREV:
    case AUDIO_CMD_NEXT:
    case AUDIO_CMD_VOL_UP:
    case AUDIO_CMD_VOL_DOWN:
    case AUDIO_CMD_RESET:
        return Audio_SendPacket(cmd, 0, 0);
    default:
        return AUDIO_INVALID_PARAM;
    }
}

Audio_Status Audio_PlayTrack(uint16_t track)
{
    uint8_t data[2];

    if (track == 0)
    {
        return AUDIO_INVALID_PARAM;
    }
    // 手册第3页：编号1~65535，高字节在前，只选根目录中的五位数字文件名。
    data[0] = (uint8_t)(track >> 8);
    data[1] = (uint8_t)track;
    return Audio_SendPacket(AUDIO_CMD_SELECT, data, 2);
}

Audio_Status Audio_SetVolume(uint8_t volume)
{
    if (volume > 30)
    {
        return AUDIO_INVALID_PARAM;
    }
    // 手册第5页：音量0~30，1字节。
    return Audio_SendPacket(AUDIO_CMD_SET_VOLUME, &volume, 1);
}

Audio_Status Audio_SetPlayMode(uint8_t mode, uint16_t repeat_count)
{
    uint8_t data[3];

    if (mode < 1 || mode > 7)
    {
        return AUDIO_INVALID_PARAM;
    }
    // 手册第6页：模式1字节，循环次数2字节，高字节在前。
    data[0] = mode;
    data[1] = (uint8_t)(repeat_count >> 8);
    data[2] = (uint8_t)repeat_count;
    return Audio_SendPacket(AUDIO_CMD_SET_MODE, data, 3);
}
