#ifndef __FINGERPRINT_H
#define __FINGERPRINT_H

#include "stm32f10x.h"
#include "UART_Fingerprint.h"
#include "Delay.h"
#include "ST7735.h"
#include "picture.h"

extern volatile uint16_t JM101_status;
extern volatile uint16_t JM101_param2Status;
extern uint8_t register_cmd[];

/*初始化函数(只执行一次就好)——>发命令(做不同的事)——>接收数据包(通用)——>
——>验证数据是否有效(通用)——>解码数据包(除了注册环节，另外两个环节放到了执行函数里)*/

void JM101_Init(void); // 初始化stm32跟指纹模块连接的引脚/外设

/******************************实现过程的函数********************************/
// 注册命令(stm32对指纹模块发送命令)
void JM101_RegisterCommand(uint16_t id, uint8_t enroll_count, uint16_t param);

/*通用*/
uint8_t JM101_ReceivePactketCalCheckSum(uint8_t *Packet, uint16_t packet_total_length); // 接收完整的数据包
uint16_t JM101_ReceivePacket(uint8_t *packet);                                          // 检验数据是否有效：通过计算数据包的校验位和数据包的校验位比对得出

/*当前仅对注册回应包解码(2026.9.15注)*/
void JM101_ReceivePacketRespondse(uint8_t *packet); // “解码”数据包

/*通用*/
// 验证命令(stm32对指纹模块发送命令)
void JM101_VerifyCommand(uint16_t id, uint8_t Securitylevel, uint16_t param);
/**********************************************************************************/

/************直接调用的功能函数(！！！！！注意：调用前先初始化"JM101_Init();")******************************/
/*包装的函数*/
// 注册函数
uint8_t JM101_Register(uint16_t id, uint8_t enroll_count, uint16_t param);
// 验证函数
uint8_t JM101_Verify(uint16_t id, uint8_t Securitylevel, uint16_t param);
// 删除指纹函数(分为全部删除和指定删除)
uint8_t JM101_Delete(uint16_t id, uint16_t number);
uint8_t JM101_DeleteAll(void);
// 读取有效指纹模板数量
uint8_t JM101_ReadValidTempleteNum(uint16_t *num);

#endif
