#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <stdint.h>

/* CX1000A/B 指令手册第2页；带参数的指令通过对应功能函数调用。 */
#define AUDIO_CMD_PLAY              0x02 // 从头播放当前曲目
#define AUDIO_CMD_PAUSE             0x03 // 暂停
#define AUDIO_CMD_STOP              0x04 // 停止
#define AUDIO_CMD_PREV              0x05 // 上一曲
#define AUDIO_CMD_NEXT              0x06 // 下一曲
#define AUDIO_CMD_SELECT            0x07 // 选曲播放，需要2字节曲目编号
#define AUDIO_CMD_SELECT_DISK       0x0B // 切换盘符，需要1字节盘符
#define AUDIO_CMD_SET_VOLUME        0x13 // 设置音量，需要1字节音量
#define AUDIO_CMD_VOL_UP            0x14 // 音量+
#define AUDIO_CMD_VOL_DOWN          0x15 // 音量-
#define AUDIO_CMD_SET_MODE          0x18 // 播放模式及循环次数，需要3字节数据
#define AUDIO_CMD_RESET             0x34 // 重启模块，之后需等待并重新设置参数

#define AUDIO_DEFAULT_VOLUME        10
#define AUDIO_PLAY_MODE_SINGLE_STOP 0x06

// 以下配合Audio_PlayTrack函数使用
#define audio_aministrator        11 // 管理员界面
#define audio_password            12 // 密码解锁
#define audio_fingerprint         13 // 指纹解锁
#define audio_card                14 // 刷卡解锁
#define audio_bluetooth           15 // 蓝牙解锁
#define audio_open_door           16 // 已开锁
#define audio_close_door          17 // 已关锁
#define audio_password_pass       18 // 密码正确
#define audio_password_error      19 // 密码错误，请重试
#define audio_fingerprint_pass    20 // 指纹验证通过
#define audio_fingerprint_fail    21 // 未识别有效指纹
#define audio_card_pass           22 // 刷卡通过
#define audio_card_fail           23 // 未识别有效卡片
#define audio_bluetooth_connected 24 // 蓝牙已连接

#define audio_TOWTIGER            10 // 两只老虎（调试作用）

/* 以下返回值由本驱动定义，不是模块返回的应答。 */
typedef enum {
    AUDIO_OK = 0,        // 本机串口已发送完成，不代表模块已开始播放
    AUDIO_INVALID_PARAM, // 参数或指令不适用于本函数
    AUDIO_NOT_INITIALIZED,
    AUDIO_TX_TIMEOUT
} Audio_Status;

/*
 * 阻塞调用，仅在主循环使用，不在中断中调用。
 * USART3 TX=PB10 -> 模块UART_RX；两者共地。只发送，不占用PB11。
 * 不配置USART3重映射；使用前应确保其它代码未启用USART3重映射。
 * 初始化9600/8N1，并选择FLASH、设置默认音量、设置单曲播放完停止。
 * 含500ms上电等待，属于可按实物调整的工程值，不是资料规定时序。
 */
Audio_Status Audio_Init(void);

/* 仅支持上面的播放、暂停、停止、上下曲、音量加减、重启等无参数命令。 */
Audio_Status Audio_SendCommand(uint8_t cmd);

/* 根目录五位数字文件名，如编号5对应00005.mp3/00005.wav；范围1~65535。 */
Audio_Status Audio_PlayTrack(uint16_t track);

/* 音量范围0~30。 */
Audio_Status Audio_SetVolume(uint8_t volume);

/* 模式1~7，含义见手册第6页；循环次数0代表无限次，仅对循环模式有意义。 */
Audio_Status Audio_SetPlayMode(uint8_t mode, uint16_t repeat_count);

#endif
