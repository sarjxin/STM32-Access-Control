#include "RC522.h"

/*定义读数据的地址字节(高位先行)：1(表示从RC522里读，写则为0) 110111(读的地址) 0(规定)*/
/**
 * @brief 进阶版的读函数
 * @param addr 寄存器地址
 * @param len 读取的长度
 * @param receiveBuffer 读取的数据存储区
 */
void RC522_ReadRegister(uint8_t addr, uint8_t len, uint8_t *receiveBuffer)
{
    uint8_t addr_Byte = 0;
    addr_Byte = ((0x01 << 7) | (addr << 1)) & 0xFE;
    MySPI_Start(RC522_SS_PORT,RC522_SS_PIN);
    MySPI_SwapByte(addr_Byte);
    for(int i = 0; i < len; i++)
    {
        receiveBuffer[i] = MySPI_SwapByte(0xFF);
    }
    MySPI_Stop(RC522_SS_PORT,RC522_SS_PIN);
}

/*写数据的地址字节(高位先行)：0(表示从RC522里读，读则为1) 011001(写的地址) 0(规定)*/
/**
 * @brief 进阶版写函数
 * @param addr 寄存器地址
 * @param len 写的长度
 * @param WriteBuffer 写的数据存储区
 */
void RC522_WriteRegister(uint8_t addr, uint8_t len, uint8_t *WriteBuffer)
{
    uint8_t receive_Byte = 0;
    uint8_t addr_Byte = 0;
    addr_Byte = ((addr << 1) & 0xFE) & 0x7F;
    MySPI_Start(RC522_SS_PORT,RC522_SS_PIN);
    MySPI_SwapByte(addr_Byte);
    for(int i = 0; i < len; i++)
    {
        receive_Byte = MySPI_SwapByte(WriteBuffer[i]);
    }
    MySPI_Stop(RC522_SS_PORT,RC522_SS_PIN);
}

/**
 * @brief 基础的写函数，往指定地址写一个字节
 */
void RC522_WriteReg(uint8_t addr, uint8_t val)
{
    RC522_WriteRegister(addr, 1, &val);
}

/**
 * @brief 基础的读函数，往指定地址读一个字节
 */
uint8_t RC522_ReadReg(uint8_t addr)
{
    uint8_t val;
    RC522_ReadRegister(addr, 1, &val);
    return val;
}

/**
 * @brief 对 RC522 某个寄存器的指定位 置 1（保持其它位不变）
 * @param addr 寄存器地址
 * @param mask 掩码 (要置 1 的位)
 */
void RC522_SetBitMask(uint8_t addr, uint8_t mask)
{
    uint8_t value;
    value = RC522_ReadReg(addr);
    value |= mask;
    RC522_WriteReg(addr, value); // 按位或：指定位置 1
}

/**
 * @brief 对 RC522 某个寄存器的指定位 清 0（保持其它位不变）
 * @param reg 寄存器地址
 * @param mask 掩码 (要清 0 的位)
 */
void RC522_ClearBitMask(uint8_t reg, uint8_t mask)
{
    uint8_t value;
    value = RC522_ReadReg(reg);
    value &= (~mask);
    RC522_WriteReg(reg, value); // 按位与反码：指定位置 0
}


/**
 * @brief 对 RC522 执行软件复位，并等待复位完成
 *
 * 向 CommandReg 写入 SoftReset 命令，然后轮询 CommandReg 的命令字段。
 * 若命令字段不再等于 PCD_RESETPHASE，说明软件复位已结束。
 *
 * @retval 1 RC522 软件复位完成
 * @retval 0 等待超过 50 ms，复位失败或 RC522 通信异常
 *
 * @note 使用有上限的轮询，避免模块未连接时程序永久卡死。
 */
uint8_t RC522_Reset(void)
{
    RC522_WriteReg(CommandReg, PCD_RESETPHASE); //对RC522命令寄存器写复位命令
    // while(1)    // 等待复位完成 ==不能用这个方式，如果没有芯片，那么将在初始化一直在while==
    // {
    //     Reset_finish = RC522_ReadReg(CommandReg);
    //     if( (Reset_finish & 0x0F) != 0x0F) break;
    // }
    for(uint8_t retry = 0; retry < 50; retry++) //等待复位完成，这个有复位超时上限
    {
        if( (RC522_ReadReg(CommandReg) & 0x0F) != PCD_RESETPHASE)
        {
            return 1; //表示成功
        }
        Delay_ms(1);
    }
    return 0; //超时，初始化失败
}

/**
 * @brief 打开 RC522 的 RF 天线驱动
 *
 * 将 TxControlReg 的 TX1RFEn 和 TX2RFEn 位置 1，
 * 使 TX1、TX2 输出用于驱动天线的 13.56 MHz RF 信号。
 */
void RC522_AntennaOn(void)
{
    RC522_SetBitMask(TxControlReg, RC522_TX1TX2_RF_ENABLE);

    // RC522_SetBitMask(TxControlReg, Tx1RFEn_config | Tx2RFEn_config);

}

/**
 * @brief 关闭 RC522 的 RF 天线驱动
 *
 * 清除 TxControlReg 的 TX1RFEn 和 TX2RFEn 位，
 * 停止 TX1、TX2 的 RF 输出。
 */
void RC522_AntennaOff(void)
{
    RC522_ClearBitMask(TxControlReg, RC522_TX1TX2_RF_ENABLE); //这里采用了清零的函数
}

/**
 * @brief 初始化 STM32 与 RC522，并配置 Type A 读卡基础参数
 *
 * 初始化过程：
 * 1. 初始化软件 SPI 和 RC522 的 NSS 片选引脚；
 * 2. 对 RC522 执行软件复位；
 * 3. 配置发送和接收速率为 106 kBd；
 * 4. 关闭当前 REQA/防冲突流程不需要的 CRC；
 * 5. 设置 Type A 所需的 100% ASK 调制；
 * 6. 配置 RC522 内部定时器，超时约为 7.75 ms；
 * 7. 打开 RF 天线驱动。
 *
 * @retval 1 初始化成功
 * @retval 0 RC522 软件复位超时，初始化失败
 */
uint8_t RC522_Init(void)
{
    // 1.接入SPI底层通信
    MySPI_Init(RC522_SS_PORT,RC522_SS_PIN);

    //2. 对RC522内部进行软件复位
    if (RC522_Reset() == 0)
    {
        return 0;
    }

    /*3. 106 kBd，且 REQA / 防冲突阶段不使用 CRC */
    // 对RC522的收发天线进行设置，选择106kBd
    RC522_ClearBitMask(TxModeReg, RC522_TXRX_SPEED_MASK | RC522_TXRX_CRC_ENABLE);
    RC522_ClearBitMask(RxModeReg, RC522_TXRX_SPEED_MASK | RC522_TXRX_CRC_ENABLE);

    /*4. ISO/IEC 14443A 的读卡器方向使用 100% ASK */
    // 100%ASK：是否接受来自ModGsPReg register的影响
    RC522_SetBitMask(TxASKReg, RC522_FORCE_100_ASK);

    /*5. 自动启动计时器：TPrescaler=0xD3E，TReload=30，约 7.75 ms。发完后自动启动计时器，且写入预分频高位*/
    /*设置超时时间：定时7.5ms
    f_time = 13.56MHz / (TPrescaler + 1) = 13.56MHz / (0x0D3E + 1) = 约4KHz
    time_out = (1 / 4KHz) × TReload = 0.00025s/次 × (30 + 1)次 = 0.00775s = 7.75ms*/
    // 先设置RC522内部定时器的预分频，这边是产生4KHz频率的时钟
    RC522_SetBitMask(TModeReg, 0x8D);
    RC522_SetBitMask(TPrescalerReg, 0x3E);
    // 再设置定时器重载寄存器里面的数，这边是数到寄存器为30时向外发出信号
    RC522_SetBitMask(TReloadRegH, 0x00);
    RC522_SetBitMask(TReloadRegL, 0x1E);  //【！这里有些搞不懂，1E的话时30，寄存器30，代入计算不是要+1吗】

    // RF场开启(打开天线)
    RC522_AntennaOn();

    // 自动超时设置，发送信号后自动开始计时
    // RC522_SetBitMask(TModeReg, 0x80);【！之前AI给的教程里面好像提了这个东西，为什么这次没有】
    return 1;
}


/**
 * @brief 执行一次 RC522 “发送后接收”事务
 *
 * 本函数是 REQA 和防冲突共用的底层收发函数：
 * 1. 清除旧命令、中断标志和 FIFO 数据；
 * 2. 将 tx 数据写入 FIFO；
 * 3. 配置最后一个发送字节的有效位数；
 * 4. 启动 Transceive；
 * 5. 等待接收完成、错误或超时；
 * 6. 将接收到的 FIFO 数据复制到 rx 缓冲区。
 *
 * @param tx            待发送数据缓冲区
 * @param tx_len        待发送字节数
 * @param tx_last_bits  最后一个发送字节的有效位数；
 *                      0 表示完整 8 位，REQA 使用 7
 * @param rx            接收数据缓冲区
 * @param rx_capacity   rx 缓冲区最大容量，单位为字节
 * @param rx_len        输出参数，返回实际接收的字节数
 * @param rx_last_bits  输出参数，返回接收末字节的有效位数；
 *                      0 表示末字节完整有效
 *
 * @retval RC522_OK            发送成功且收到一帧无错误数据
 * @retval RC522_TIMEOUT       TimerIRq 到达或软件轮询超时
 * @retval RC522_COLLISION     检测到位冲突
 * @retval RC522_DEVICE_ERROR  RC522 报告通信、奇偶校验或协议错误
 * @retval RC522_BUFFER_ERROR  FIFO 数据长度超过 rx 缓冲区容量
 *
 * @note 该函数仅供 RC522.c 内部使用，因此使用 static。
 */
static RC522_Status RC522_Transceive(const uint8_t *tx, uint8_t tx_len,
                                     uint8_t tx_last_bits,
                                     uint8_t *rx, uint8_t rx_capacity,
                                     uint8_t *rx_len, uint8_t *rx_last_bits)
{
    uint8_t irq;
    uint8_t error;
    uint8_t fifo_len;
    uint16_t poll_limit;
    uint8_t i;

    *rx_len = 0;
    *rx_last_bits = 0;

    RC522_WriteReg(CommandReg, PCD_IDLE);
    RC522_WriteReg(ComIrqReg, RC522_IRQ_CLEAR_ALL);
    RC522_WriteReg(FIFOLevelReg, RC522_FIFO_FLUSH);

    for (i = 0; i < tx_len; i++)
    {
        RC522_WriteReg(FIFODataReg, tx[i]);
    }

    RC522_WriteReg(BitFramingReg, tx_last_bits & 0x07);
    RC522_WriteReg(CommandReg, PCD_TRANSCEIVE);
    RC522_SetBitMask(BitFramingReg, 0x80);

    /* StartSend 后，MFRC522 先发送 FIFO，再自动切换为接收。 */
    for (poll_limit = 0; poll_limit < 2000; poll_limit++)
    {
        irq = RC522_ReadReg(ComIrqReg);
        if (irq & (RC522_IRQ_RX | RC522_IRQ_ERR | RC522_IRQ_TIMER))
        {
            break;
        }
    }

    /* ErrorReg 描述“上一个命令”的错误。
       先保存它，再写 Idle，防止新命令清掉诊断证据。
       MFRC522 10.3.1.8 规定 Transceive 必须由写入任意命令清除；
       Idle 是本驱动选择的明确收尾命令。 */
    error = RC522_ReadReg(ErrorReg);
    RC522_WriteReg(CommandReg, PCD_IDLE);

    if (poll_limit == 2000)
    {
        return RC522_TIMEOUT;
    }
    if (irq & RC522_IRQ_TIMER)
    {
        return RC522_TIMEOUT;
    }
    if (error & RC522_ERR_COLLISION)
    {
        return RC522_COLLISION;
    }
    if ((irq & RC522_IRQ_ERR) ||
        (error & (RC522_ERR_BUFFER_OVFL | RC522_ERR_PARITY | RC522_ERR_PROTOCOL)))
    {
        return RC522_DEVICE_ERROR;
    }
    if ((irq & RC522_IRQ_RX) == 0)
    {
        return RC522_DEVICE_ERROR;
    }

    /* 执行到这里不是“突然假定 RxIRq”。前面已经逐项排除了：
       1. 软件轮询耗尽；2. TimerIRq；3. CollErr；4. ErrIRq/相关错误；
       5. 没有 RxIRq。
       因此此时确定 RxIRq=1，并且没有已处理的致命错误，可以读取接收结果。 */

    /* Table 42：FIFOLevelReg[6:0] 是 FIFO 当前保存的字节数。
       它决定后面需要从 FIFODataReg 读取多少次。 */
    fifo_len = RC522_ReadReg(FIFOLevelReg);

    /* Table 46：ControlReg[2:0] 是 RxLastBits。
       & 0x07 只保留低三位，去掉 TStopNow/TStartNow/保留位。
       结果为 0 表示接收末字节的 8 位全部有效；1..7 表示只有对应数量的位有效。 */
    *rx_last_bits = RC522_ReadReg(ControlReg) & 0x07;

    /* FIFO 有 fifo_len 字节，但调用者数组只能容纳 rx_capacity 字节。
       若前者更大，继续复制会越过 rx[] 边界，破坏其他内存。
       FIFOLevel 的来源是 Table 42；rx_capacity 是驱动为 C 内存安全加入的参数。 */
    if (fifo_len > rx_capacity)
    {
        return RC522_BUFFER_ERROR;
    }

    /* 此时 RF 接收其实已经结束。这里不是“继续接收卡的数据”，
       而是 MCU 通过 SPI，把 MFRC522 已经存进 FIFO 的结果逐字节取出。
       Table 40/42：每读一次 FIFODataReg，FIFO 读指针前进、FIFOLevel 减一。 */
    for (i = 0; i < fifo_len; i++)
    {
        rx[i] = RC522_ReadReg(FIFODataReg);
    }

    /* 函数返回值已经用于返回状态码，所以通过输出指针告诉调用者实际字节数。 */
    *rx_len = fifo_len;

    return RC522_OK;
}


/**
 * @brief 发送 Type A 的 REQA 请求，并读取 ATQA
 *
 * 向场内 Type A 卡发送 7 位 REQA 命令 0x26。
 * 若卡响应，则读取两个完整字节的 ATQA。
 *
 * @param atqa 输出缓冲区，长度必须至少为 2 字节
 *
 * @retval RC522_OK           成功收到 2 字节完整 ATQA
 * @retval RC522_TIMEOUT      场内无响应卡，或通信超时
 * @retval RC522_FRAME_ERROR  收到的数据不是 2 个完整字节
 * @retval 其他值             底层 RC522_Transceive() 返回的错误
 *
 * @note 本函数只在 RC522_ReadUid4() 内部使用，因此使用 static。
 */
static RC522_Status RC522_RequestA(uint8_t atqa[2])
{
    uint8_t request = 0x26;
    uint8_t rx_len;
    uint8_t rx_last_bits;
    RC522_Status status;

    status = RC522_Transceive(&request, 1, 7,
                               atqa, 2, &rx_len, &rx_last_bits);
    if (status != RC522_OK)
    {
        return status;
    }

    if ((rx_len != 2) || (rx_last_bits != 0))
    {
        return RC522_FRAME_ERROR;
    }

    return RC522_OK;
}


/**
 * @brief 执行 Type A 第 1 级防冲突，并读取 4 字节 UID
 *
 * 向卡发送 {0x93, 0x20}：
 * 0x93 表示第 1 串联级；
 * 0x20 表示读卡器尚未给出任何 UID 位。
 *
 * 卡应返回 UID0、UID1、UID2、UID3 和 BCC，共 5 个完整字节。
 * 本函数会验证 BCC 是否等于四个 UID 字节的异或值。
 *
 * @param uid 输出缓冲区，长度必须至少为 4 字节
 *
 * @retval RC522_OK           成功获得并校验 4 字节 UID
 * @retval RC522_COLLISION    检测到多卡位冲突
 * @retval RC522_FRAME_ERROR  响应不是 5 个完整字节
 * @retval RC522_BCC_ERROR    UID 的 BCC 校验失败
 * @retval 其他值             底层 RC522_Transceive() 返回的错误
 *
 * @note 本函数只在 RC522_ReadUid4() 内部使用，因此使用 static。
 */
static RC522_Status RC522_AnticollCL1(uint8_t uid[4])
{
    uint8_t command[2] = {0x93, 0x20};
    uint8_t response[5];
    uint8_t rx_len;
    uint8_t rx_last_bits;
    uint8_t bcc;
    RC522_Status status;

    status = RC522_Transceive(command, 2, 0,
                               response, 5, &rx_len, &rx_last_bits);
    if (status != RC522_OK)
    {
        return status;
    }

    if ((rx_len != 5) || (rx_last_bits != 0))
    {
        return RC522_FRAME_ERROR;
    }

    bcc = response[0] ^ response[1] ^ response[2] ^ response[3];
    if (bcc != response[4])
    {
        return RC522_BCC_ERROR;
    }

    uid[0] = response[0];
    uid[1] = response[1];
    uid[2] = response[2];
    uid[3] = response[3];

    return RC522_OK;
}

/**
 * @brief 读取一张 Type A 卡的 4 字节 UID
 *
 * 本函数依次执行：
 * 1. 发送 REQA，获得 ATQA；
 * 2. 发送第 1 级防冲突命令 0x93 0x20；
 * 3. 接收 UID 和 BCC；
 * 4. 验证 BCC；
 * 5. 将 4 字节 UID 写入 uid 数组。
 *
 * @param uid  输出缓冲区，长度必须至少为 4 字节；
 *              仅当返回 RC522_OK 时内容有效
 * @param atqa 输出缓冲区，长度必须至少为 2 字节；
 *              保存卡对 REQA 的响应
 *
 * @retval RC522_OK           成功读取并校验 4 字节 UID
 * @retval RC522_TIMEOUT      无卡响应或通信超时
 * @retval RC522_COLLISION    多张卡同时响应，发生位冲突
 * @retval RC522_FRAME_ERROR  ATQA 或防冲突响应长度不正确
 * @retval RC522_BCC_ERROR    UID 校验失败
 * @retval 其他值             RC522 底层通信错误
 *
 * @note 当前版本只支持 4 字节 UID，不支持 7 字节或 10 字节 UID。
 */
RC522_Status RC522_ReadUid4(uint8_t uid[4], uint8_t atqa[2])
{
    RC522_Status status;

    status = RC522_RequestA(atqa);
    if (status != RC522_OK)
    {
        return status;
    }

    return RC522_AnticollCL1(uid);
}



