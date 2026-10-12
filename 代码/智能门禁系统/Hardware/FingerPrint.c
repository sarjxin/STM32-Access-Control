#include "FingerPrint.h"
#include <stdint.h>

volatile uint16_t JM101_status = 0;
volatile uint16_t JM101_param2Status = 0;

/*指纹模块的数据包遵循高字节在前，低字节在后的传输方式*/
/*return 1代表成功，return 0代表失败*/
uint8_t Cmd_Packet[17] = {0};

/*自动注册指令格式
包头 | 芯片地址 | 包标识 | 包长度 | 指令码 | ID号 | 录入次数 | 参数 | 校验和
包头       2 bytes
芯片地址   4 bytes
包标识     1 byte
包长度     2 bytes
指令码     1 byte(0x31)
ID号       2 bytes
录入次数   1 byte
参数       2 bytes
校验和     2 bytes
*/
uint8_t register_cmd[] = {
    0xEF, 0x01,             // 包头
    0xFF, 0xFF, 0xFF, 0xFF, // 芯片地址
    0x01,                   //	包标识，01代表命令，02是数据包(且有后续包)，08是最后一个数据包(即结束包)
    0x00, 0x08,             // 包长度，0008代表8个字节(不包括包头)：包长度至校验和（指令、参数或数据）的总字节数，包含校验和，但不含包长度本身的字节数。
    0x31,                   // 指令码
    0x00, 0x01,             // ID号
    0x02,                   // 录入次数
    0x00, 0x00,             // 参数
    0x00, 0x3D              // 校验和(怎么算的)
};

/**
 * @brief 初始化stm32跟指纹模块连接的引脚/外设
 */
void JM101_Init(void)
{
    UART_Fingerprint_Init();

    // JM101在上电后需要100ms的缓冲时间
    Delay_ms(100);
}

/**
 * @brief 根据数据包，计算校验位的值：从包标识开始，一直加到校验和之前；结果超过 2 字节时，忽略进位
 */
static uint16_t JM101_CalCheckBitValue(uint8_t *data, uint16_t length)
{
    uint16_t sum = 0;
    uint16_t i;
    for (i = 0; i < length; i++)
    {
        sum += data[i];
    }
    return sum;
}

/**
 * @brief 自动注册指令
 */
void JM101_RegisterCommand(uint16_t id, uint8_t enroll_count, uint16_t param)
{
    Cmd_Packet[0] = 0xEF;
    Cmd_Packet[1] = 0x01;

    Cmd_Packet[2] = 0xFF;
    Cmd_Packet[3] = 0xFF;
    Cmd_Packet[4] = 0xFF;
    Cmd_Packet[5] = 0xFF;

    Cmd_Packet[6] = 0x01;

    Cmd_Packet[7] = 0x00;
    Cmd_Packet[8] = 0x08;

    Cmd_Packet[9] = 0x31;

    Cmd_Packet[10] = (id >> 8) & 0xFF;
    Cmd_Packet[11] = id & 0xFF;

    Cmd_Packet[12] = enroll_count;

    Cmd_Packet[13] = (param >> 8) & 0xFF;
    Cmd_Packet[14] = param & 0xFF;

    // 计算校验和并放到校验位
    uint16_t sum = JM101_CalCheckBitValue(&Cmd_Packet[6], 9);
    Cmd_Packet[15] = (sum >> 8) & 0xFF;
    Cmd_Packet[16] = sum & 0xFF;

    // 发送出去
    UART_Fingerprint_SendBuffer(Cmd_Packet, sizeof(Cmd_Packet));
}

/**
 * @brief 接收应答包
 * @param packet 接收到的数据包指针
 * @return 返回接收到的数据包的长度
 */
uint16_t JM101_ReceivePacket(uint8_t *packet)
{
    uint8_t rx_data = 0;
    uint16_t packet_length = 0;
    uint16_t packet_total_length = 0;

    // 检测包头EF和01
    while (1)
    {
        rx_data = UART_Fingerprint_WaitByte();
        if (rx_data == 0xEF)
        {
            rx_data = UART_Fingerprint_WaitByte();
            if (rx_data == 0x01)
            {
                packet[0] = 0xEF;
                packet[1] = 0x01;
                break;
            }
        }
    }

    // 接收地址4字节
    for (int i = 0; i < 4; i++)
    {
        packet[i + 2] = UART_Fingerprint_WaitByte();
    }

    // 接收包标识
    packet[6] = UART_Fingerprint_WaitByte();

    // 连续接收两个字节(包长度)，根据这个数据定义这样大小的for循环次数接收后面的数据
    for (int i = 0; i < 2; i++)
    {
        packet[i + 7] = UART_Fingerprint_WaitByte();
    }
    packet_length = ((uint16_t)packet[7] << 8) | packet[8];

    if (packet_length > 55) // 超越数组警告
    {
        return 0;
    }

    // 接收剩余的数据
    for (int i = 0; i < packet_length; i++)
    {
        packet[i + 9] = UART_Fingerprint_WaitByte();
    }

    /**********计算整个包的长度***********/
    packet_total_length = 9 + packet_length;

    return packet_total_length;
}

/**
 * @brief 通过校验和判断数据是否有效：算出拿到数据的校验和(包标识~返回参数)跟数据的校验和对比
 * @param Packet 数据包指针
 * @param packet_length 数据包里面的包长度数值
 */
uint8_t JM101_ReceivePactketCalCheckSum(uint8_t *Packet, uint16_t packet_total_length)
{
    // 定义拿到的数据包通过计算的得出的校验和、定义数据包里面的校验和
    uint16_t CalCheckSum = 0;
    uint16_t ReceiveCheckSun = (((uint16_t)Packet[packet_total_length - 1 - 1]) << 8) | Packet[packet_total_length - 1];
    // 注：Packet[packet_total_length  - 1]：索引到数据包的最后一个字节，即低位的校验和

    // 定义计算校验和的长度
    uint16_t CalCheckSum_length = packet_total_length - 6 - 2; // 6:包头+芯片地址  2：最后两位校验和

    CalCheckSum = JM101_CalCheckBitValue(&Packet[6], CalCheckSum_length);

    if (CalCheckSum == ReceiveCheckSun)
    {
        return 1;
    }
    else
    {
        return 0; // 在调用这个函数，可以if(JM101_ReceivePactketCalCheckSum())，0则代表校验和不对，判断结果为否
    }
}

/**
 * @brief “解码”数据包
 * @param packet 接收到的数据包指针
 */
void JM101_ReceivePacketRespondse(uint8_t *packet)
{
    /********先取出确认码和两个参数********/
    // 先定义装确认码以及装两个参数的变量
    uint8_t confirm_code = 0;
    uint8_t param1 = 0;
    uint8_t param2 = 0;

    // 在数据包里取出确认码和两个参数，根据应答包的正常格式：packet[9]确认码，两个参数是packet[10]、packet[11]
    confirm_code = packet[9];
    param1 = packet[10];
    param2 = packet[11];

    // 通过switch() case语句，根据不同的情况进行不同的处理。返回不同的参数
    switch (confirm_code)
    {
    case 0x00:
        switch (param1)
        {
        case 0x00:
            /*指纹合法性检测*/
            JM101_status = 0;
            break;

        case 0x01:
            /*获取图像中*/
            JM101_status = 1;
            break;

        case 0x02:
            /*生产特征中*/
            JM101_status = 2;
            break;

        case 0x03:
            /*判断手指离开*/
            JM101_status = 3;
            break;

        case 0x04:
            /*合并模块*/
            JM101_status = 4;
            break;

        case 0x05:
            /*注册检验*/
            JM101_status = 5;
            break;

        case 0x06:
            /*存储模块*/
            JM101_status = 6;
            break;

        default:
            /*其他未知参数的处理*/
            JM101_status = 0xFF;
            break;
        }

        JM101_param2Status = param2;

        break;

    case 0x07:
        /*生成特征失败*/
        JM101_status = 0x07;
        break;

    case 0x0A:
        /*合并模块失败*/
        JM101_status = 0x0A;
        break;

    case 0x0B:
        /*ID 号超出范围*/
        JM101_status = 0x0B;
        break;

    case 0x1f:
        /*指纹库已满*/
        JM101_status = 0x1f;
        break;

    case 0x22:
        /*指纹模板非空*/
        JM101_status = 0x22;
        break;

    case 0x25:
        /*录入次数设置错误*/
        JM101_status = 0x25;
        break;

    case 0x26:
        /*超时*/
        JM101_status = 0x26;
        break;

    case 0x27:
        /*指纹已存在*/
        JM101_status = 0x27;
        break;

    default:
        /*未知参数的处理*/
        JM101_status = 0xFF;
        break;
    }
}

/**
 * @brief 指纹注册函数
 * @param id 指纹ID
 * @param enroll_count 指纹录入次数
 * @param param 指纹录入参数
 */
uint8_t JM101_Register(uint16_t id, uint8_t enroll_count, uint16_t param)
{
    uint8_t packet[64] = {0};
    uint16_t packet_length;

    /* 开始注册 */
    // Serial_Printf("\r\n");
    // Serial_Printf("============================\r\n");
    // Serial_Printf("       指纹注册开始\r\n");
    // Serial_Printf("============================\r\n");

    // Serial_Printf("注册 ID：%d\r\n", id);
    // Serial_Printf("录入次数：%d\r\n", enroll_count);

    /* 发送注册命令 */
    JM101_RegisterCommand(id, enroll_count, param);
    // Serial_Printf("已发送注册命令，等待指纹模块响应...\r\n");
    while (1)
    {
        /* 接收一个完整数据包 */
        packet_length = JM101_ReceivePacket(packet);

        /*接收后校验一下*/
        if (JM101_ReceivePactketCalCheckSum(packet, packet_length) == 0)
        {
            // Serial_Printf("收到的数据包校验失败\r\n");
            continue;
        }

        /*解析一下*/
        JM101_ReceivePacketRespondse(packet);

        /*对解析的结果处理一下*/
        switch (JM101_status)
        {
        case 0x00:
            // Serial_Printf("注册命令合法，开始录入\r\n");
            // LCD_DrawGBKString16x16(0, 16, press_finger_GBK, YELLOW, BLACK); // 按下手指
            LCD_FillRect(0, 20, 128, 16, WHITE);
            LCD_DrawGBKString16x16(0, 20, press_finger_GBK, YELLOW, WHITE);
            break;

        case 0x01:
            if (JM101_param2Status == 1)
            {
                // Serial_Printf("已检测到按下手指...（第1次）\r\n");
                LCD_FillRect(0, 20, 128, 16, WHITE);                           // 清除原来的指纹状态
                LCD_DrawGBKString16x16(0, 20, press_again_GBK, YELLOW, WHITE); // 请松开后再按下
            }
            else if (JM101_param2Status == 2)
            {
                // Serial_Printf("已检测到再次按下手指（第2次）\r\n");
                // LCD_DrawGBKString16x16(36, 32, enrolling_fingerprint_GBK, YELLOW, BLACK);
            }
            else
            {
                //  Serial_Printf("请按下手指\r\n");
            }
            break;

        case 0x02:
            // Serial_Printf("已生产特征中,手指请离开\r\n", JM101_param2Status);
            break;
        case 0x03:
            // Serial_Printf("已检测到手指离开，请再按下\r\n");
            LCD_FillRect(0, 20, 16 * 8, 16, WHITE);
            LCD_DrawGBKString16x16(0, 20, please_press_again_GBK, YELLOW, WHITE);
            break;
        case 0x04:
            // Serial_Printf("正在合并模块\r\n");
            break;
        case 0x05:
            // Serial_Printf("正在注册检验\r\n");
            break;
        case 0x06:
            // Serial_Printf("正在保存指纹模板...\r\n");
            // Serial_Printf("****************************\r\n");
            // Serial_Printf("       指纹注册成功！\r\n");
            // Serial_Printf("       ID = %d\r\n", id);
            // Serial_Printf("****************************\r\n");
            LCD_FillRect(0, 20, 16 * 8, 16, WHITE);                          // 清除原来的指纹状态
            LCD_DrawGBKString16x16(0, 20, enroll_succeed_GBK, GREEN, WHITE); // 成功录入指纹
            Delay_ms(1000);
            return 1;

        case 0x07:
            // Serial_Printf("生成特征失败，请重新按压指纹\r\n");
            return 0;

        case 0x26:
            // Serial_Printf("超时\r\n");
            return 0;

        case 0x0A:
            // Serial_Printf("合成模板失败！请重试\r\n");
            return 0;

        default:
            // Serial_Printf("注册失败：未知错误，状态码 = 0x%02X\r\n", JM101_status);
            return 0;
        }
    }
}

/**
 * @brief 自动验证指纹的指令
 */
void JM101_VerifyCommand(uint16_t id, uint8_t Securitylevel, uint16_t param)
{
    Cmd_Packet[0] = 0xEF;
    Cmd_Packet[1] = 0x01; // 包头

    Cmd_Packet[2] = 0xFF;
    Cmd_Packet[3] = 0xFF;
    Cmd_Packet[4] = 0xFF;
    Cmd_Packet[5] = 0xFF; // 芯片地址

    Cmd_Packet[6] = 0x01; // 宝标识

    Cmd_Packet[7] = 0x00;
    Cmd_Packet[8] = 0x08; // 包长度

    Cmd_Packet[9] = 0x32; // 指令码

    Cmd_Packet[10] = Securitylevel; // 安全等级

    Cmd_Packet[11] = (id >> 8) & 0xFF;
    Cmd_Packet[12] = id & 0xFF;

    Cmd_Packet[13] = (param >> 8) & 0xFF;
    Cmd_Packet[14] = param & 0xFF;

    // 计算校验和并放到校验位
    uint16_t sum = JM101_CalCheckBitValue(&Cmd_Packet[6], 9);
    Cmd_Packet[15] = (sum >> 8) & 0xFF;
    Cmd_Packet[16] = sum & 0xFF;

    // 发送出去
    UART_Fingerprint_SendBuffer(Cmd_Packet, sizeof(Cmd_Packet));
}

/**
 * @brief 指纹验证函数
 * @param id指纹ID:大端模式。比如录入1号指纹，则是 0001H。ID号为0xFFFF，则进行1:N搜索。否进行 1:1 匹配
 * @param Securitylevel 安全等级
 * @param param 指纹验证参数
 */
uint8_t JM101_Verify(uint16_t id, uint8_t Securitylevel, uint16_t param)
{
    uint8_t packet[64] = {0};
    uint16_t packet_length;

    /* 开始验证 */
    // Serial_Printf("\r\n");
    // Serial_Printf("============================\r\n");
    // Serial_Printf("       指纹验证开始\r\n");
    // Serial_Printf("============================\r\n");

    // Serial_Printf("拇指号 ID：%d\r\n", id);
    // Serial_Printf("安全等级：%d\r\n", Securitylevel);

    /* 发送验证命令 */
    JM101_VerifyCommand(id, Securitylevel, param);
    // Serial_Printf("已发送验证命令，等待指纹模块响应...\r\n");
    while (1)
    {
        /* 接收一个完整数据包 */
        packet_length = JM101_ReceivePacket(packet);

        /*接收后校验一下*/
        if (JM101_ReceivePactketCalCheckSum(packet, packet_length) == 0)
        {
            // Serial_Printf("收到的数据包校验失败\r\n");
            continue;
        }

        /********先取出确认码和两个参数********/
        // 先定义装确认码以及装两个参数的变量
        uint8_t confirm_code = packet[9];
        uint8_t param = packet[10];
        uint16_t code = ((uint16_t)packet[13] << 8) | packet[14];
        uint16_t return_id = ((uint16_t)packet[11] << 8) | packet[12];

        /*解析一下*/
        switch (confirm_code) // 先取确认码
        {
        case 0x00:
            switch (param)
            {
            case 0x00:
                // Serial_Printf("请将验证的手指按下\r\n");
                break;
            case 0x01:
                // Serial_Printf("已获取图像，正在比对\r\n");
                break;
            case 0x05: // 验证成功/搜索到了
                if (id == 0xFFFF)
                {
                    // Serial_Printf("成功搜寻到匹配指纹，id号为%d，得分为%d\r\n", return_id, code);
                }
                else
                {
                    // Serial_Printf("指纹验证通过！得分为%d\r\n", code);
                }
                return 1;
            }
            break;
        case 0x09:
            if (id == 0xFFFF)
            {
                // Serial_Printf("未搜寻到匹配指纹\r\n");
            }
            else
            {
                // Serial_Printf("验证失败\r\n");
            }
            return 0;
        case 0x26:
            // Serial_Printf("超时\r\n");
            return 0;
        case 0x24:
            // Serial_Printf("指纹库为空，请先录入指纹\r\n");
            return 0;
        case 0x0b:
            // Serial_Printf("ID号无效\r\n");
            return 0;
        default:
            // Serial_Printf("读取失败：未知错误，状态码 = 0x%02X\r\n", confirm_code);
            return 0;
        }
    }
}

/**
 * @brief 删除的命令
 */
void JM101_DeleteCommand(uint16_t id, uint16_t number)
{
    Cmd_Packet[0] = 0xEF;
    Cmd_Packet[1] = 0x01; // 包头

    Cmd_Packet[2] = 0xFF;
    Cmd_Packet[3] = 0xFF;
    Cmd_Packet[4] = 0xFF;
    Cmd_Packet[5] = 0xFF; // 芯片地址

    Cmd_Packet[6] = 0x01; // 包标识

    Cmd_Packet[7] = 0x00;
    Cmd_Packet[8] = 0x07; // 包长度

    Cmd_Packet[9] = 0x0c; // 指令码

    Cmd_Packet[10] = (id >> 8) & 0xFF;
    Cmd_Packet[11] = id & 0xFF; // 页码

    Cmd_Packet[12] = (number >> 8) & 0xFF;
    Cmd_Packet[13] = number & 0xFF; // 删除个数

    // 计算校验和并放到校验位
    uint16_t sum = JM101_CalCheckBitValue(&Cmd_Packet[6], 8);
    Cmd_Packet[14] = (sum >> 8) & 0xFF;
    Cmd_Packet[15] = sum & 0xFF;

    // 发送出去
    UART_Fingerprint_SendBuffer(Cmd_Packet, 16);
}

/**
 * @brief 删除指纹函数
 * @param 要删除的id号
 * @param 删除个数
 */
uint8_t JM101_Delete(uint16_t id, uint16_t number)
{
    uint8_t packet[20] = {0};
    uint16_t packet_length;
    /* 开始删除 */
    // Serial_Printf("\r\n");
    // Serial_Printf("============================\r\n");
    // Serial_Printf("       删除指纹开始\r\n");
    // Serial_Printf("============================\r\n");

    // Serial_Printf("拇指起始号 ID：%d\r\n", id);
    // Serial_Printf("删除个数：%d\r\n", number);

    /* 发送验证命令 */
    JM101_DeleteCommand(id, number);
    // Serial_Printf("已发送删除命令，等待指纹模块响应...\r\n");

    while (1)
    {
        /* 接收一个完整数据包 */
        packet_length = JM101_ReceivePacket(packet);

        /*接收后校验一下*/
        if (JM101_ReceivePactketCalCheckSum(packet, packet_length) == 0)
        {
            // Serial_Printf("收到的数据包校验失败\r\n");
            continue;
        }

        /********取出确认码********/
        // 先定义装确认码
        uint8_t confirm_code = packet[9];

        /*解析一下*/
        switch (confirm_code) // 取确认码
        {
        case 0x00:
            //Serial_Printf("删除模板成功！\r\n");
            return 1;
        case 0x01:
            //Serial_Printf("收包有错！\r\n");
            return 0;
        case 0x10:
            //Serial_Printf("删除模块失败\r\n");
            return 0;
        default:
            //Serial_Printf("其他未知情况，请联系开发人员\r\n");
            return 0;
        }
    }
}

/**
 * @brief 清空指纹库命令
 */
void JM101_DeleteAllCommand(void)
{
    Cmd_Packet[0] = 0xEF;
    Cmd_Packet[1] = 0x01; // 包头

    Cmd_Packet[2] = 0xFF;
    Cmd_Packet[3] = 0xFF;
    Cmd_Packet[4] = 0xFF;
    Cmd_Packet[5] = 0xFF; // 芯片地址

    Cmd_Packet[6] = 0x01; // 包标识

    Cmd_Packet[7] = 0x00;
    Cmd_Packet[8] = 0x03; // 包长度

    Cmd_Packet[9] = 0x0d; // 指令码

    // 计算校验和并放到校验位
    uint16_t sum = JM101_CalCheckBitValue(&Cmd_Packet[6], 4);
    Cmd_Packet[10] = (sum >> 8) & 0xFF;
    Cmd_Packet[11] = sum & 0xFF;

    // 发送出去
    UART_Fingerprint_SendBuffer(Cmd_Packet, 12);
}

/**
 * @brief 清空指纹库
 */
uint8_t JM101_DeleteAll(void)
{
    uint8_t packet[20] = {0};
    uint16_t packet_length;
    /* 开始删除 */
    //Serial_Printf("\r\n");
    //Serial_Printf("============================\r\n");
    //Serial_Printf("      清空指纹库开始\r\n");
    //Serial_Printf("============================\r\n");

    /* 发送验证命令 */
    JM101_DeleteAllCommand();
    //Serial_Printf("已发送清空命令，等待指纹模块响应...\r\n");

    while (1)
    {
        /* 接收一个完整数据包 */
        packet_length = JM101_ReceivePacket(packet);

        /*接收后校验一下*/
        if (JM101_ReceivePactketCalCheckSum(packet, packet_length) == 0)
        {
            //Serial_Printf("收到的数据包校验失败\r\n");
            continue;
        }

        /********取出确认码********/
        // 先定义装确认码
        uint8_t confirm_code = packet[9];

        /*解析一下*/
        switch (confirm_code) // 取确认码
        {
        case 0x00:
            //Serial_Printf("清空指纹成功！\r\n");
            return 1;
        case 0x01:
            //Serial_Printf("收包有错！\r\n");
            return 0;
        case 0x11:
            //Serial_Printf("清空模板失败\r\n");
            return 0;
        default:
            //Serial_Printf("读取失败：未知错误，状态码 = 0x%02X\r\n", confirm_code);
            return 0;
        }
    }
}

/**
 * @brief 读取有效模板命令
 */
void JM101_ValidTempleteNumCommand(void)
{
    Cmd_Packet[0] = 0xEF;
    Cmd_Packet[1] = 0x01; // 包头

    Cmd_Packet[2] = 0xFF;
    Cmd_Packet[3] = 0xFF;
    Cmd_Packet[4] = 0xFF;
    Cmd_Packet[5] = 0xFF; // 芯片地址

    Cmd_Packet[6] = 0x01; // 包标识

    Cmd_Packet[7] = 0x00;
    Cmd_Packet[8] = 0x03; // 包长度

    Cmd_Packet[9] = 0x1d; // 指令码

    // 计算校验和并放到校验位
    uint16_t sum = JM101_CalCheckBitValue(&Cmd_Packet[6], 4);
    Cmd_Packet[10] = (sum >> 8) & 0xFF;
    Cmd_Packet[11] = sum & 0xFF;

    // 发送出去
    UART_Fingerprint_SendBuffer(Cmd_Packet, 12);
}

/**
 * @brief 读有效模板个数
 */
uint8_t JM101_ReadValidTempleteNum(uint16_t *num)
{
    uint8_t packet[20] = {0};
    uint16_t packet_length;
    /* 开始删除 */
    //Serial_Printf("\r\n");
    //Serial_Printf("============================\r\n");
    //Serial_Printf("      读取有效模板开始\r\n");
    //Serial_Printf("============================\r\n");

    /* 发送验证命令 */
    JM101_ValidTempleteNumCommand();
    //Serial_Printf("已发送读取有效模板命令，等待指纹模块响应...\r\n");

    while (1)
    {
        /* 接收一个完整数据包 */
        packet_length = JM101_ReceivePacket(packet);

        /*接收后校验一下*/
        if (JM101_ReceivePactketCalCheckSum(packet, packet_length) == 0)
        {
            //Serial_Printf("收到的数据包校验失败\r\n");
            continue;
        }

        /********取出确认码********/
        // 先定义装确认码
        uint8_t confirm_code = packet[9];

        /*解析一下*/
        switch (confirm_code) // 取确认码
        {
        case 0x00:
            *num = ((uint16_t)packet[10] << 8) | packet[11];
            //Serial_Printf("读取有效指纹成功！\r\n");
            return 1;
        case 0x01:
            //Serial_Printf("收包有错！\r\n");
            return 0;
        default:
            //Serial_Printf("读取失败：未知错误，状态码 = 0x%02X\r\n", confirm_code);
            return 0;
        }
    }
}
