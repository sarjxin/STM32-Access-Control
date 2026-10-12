#ifndef __W64Q25_H
#define __W64Q25_H
#include "stm32f10x.h"
#include "MySPI.h"
#include "W64Q25_Reg.h"

#define PICTURE_ADMINISTRATOR_45X45 0X021000
#define PICTURE_BLUETOOTH_45X45     0X022000
#define PICTURE_PASSWORD_45X45      0X023000
#define PICTURE_CARD_45X45          0X024000
#define PICTURE_FINGERPRINT_45X45   0X025000

/* 卡片信息存储地址与魔数定义（占用 0x030000 扇区） */
#define CARD_STORAGE_ADDR           0X030000 // 卡片数据在 W25Q64 中的存储基地址
#define CARD_STORAGE_MAGIC          0xA55A   // 卡片数据初始化魔数（用于识别是否首次上电）

/* 单卡偏移地址（备用宏定义） */
#define CARD_UID1                   0X030000
#define CARD_UID2                   0X030004
#define CARD_UID3                   0X030008
#define CARD_UID4                   0X03000C
#define CARD_UID5                   0X030010
#define CARD_UID6                   0X030014

void W64Q25_Init(void);
void W64Q25_ReadID(uint8_t *Mid, uint16_t *DID);
void W64Q25_PageProgram(uint32_t addr, const uint8_t *data, uint32_t size);
void W64Q25_SectorProgram(uint32_t addr, const uint8_t *data, uint32_t size);
void W64Q25_SectorErase(uint32_t addr);
void W64Q25_ReadData(uint32_t addr, uint8_t *data, uint32_t size);

#endif
