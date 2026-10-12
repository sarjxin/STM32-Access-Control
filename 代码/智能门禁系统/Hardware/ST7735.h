#ifndef ST7735_H
#define ST7735_H

#include "stm32f10x.h"
#include "Delay.h"
#include "picture.h"

// 将常用的 0~255 的 R, G, B 值直接转为 16位 RGB565
#define COLOR_RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

/*引脚端口*/
#define ST7735_SCL_MOSI_RES_DC_BLK_PORT GPIOB
#define ST7735_CS_PORT                  GPIOA

/*引脚PIN*/
#define ST7735_SCL_PIN  GPIO_Pin_3
#define ST7735_MOSI_PIN GPIO_Pin_5
#define ST7735_RES_PIN  GPIO_Pin_4
#define ST7735_DC_PIN   GPIO_Pin_6 // 让LCD区分信息类型：0——命令  1——数据
#define ST7735_BLK_PIN  GPIO_Pin_7 // 背光：0——灭  1——亮

#define ST7735_CS_PIN   GPIO_Pin_15

/*常见颜色的RGB565(rrrrr gggggg bbbbb)定义,后面附带更多颜色*/
//  red  green  blue
#define RED    0xF800 // 11111 000000 00000
#define GREEN  0x07E0 // 00000 111111 00000
#define BLUE   0x001F // 00000 000000 11111
#define WHITE  0xFFFF // 11111 111111 11111
#define BLACK  0x0000 // 00000 000000 00000
#define YELLOW 0xFFE0 // 11111 111111 00000

typedef enum {
    LCD_8X8 = 0,
    LCD_16X16
} LCD_SIZE;

/*LCD初始化*/
void LCD_Init(void);

/*填充颜色*/
void LCD_FillColor(uint16_t Color);

/*画点*/
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);

/*画线*/
void LCD_DrawLine(uint16_t x_start, uint16_t y_start,
                  uint16_t x_end, uint16_t y_end, uint16_t color);

/*打印长方形*/
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

/*打印一张图片*/
void LCD_DrawImageRGB565(uint16_t x, uint16_t y, uint16_t width,
                         uint16_t height, const uint8_t *image);

/*打印任意字模，需要给宽度和高度*/
void LCD_DrawBitmapall(uint16_t x, uint16_t y,
                       uint16_t width, uint16_t height,
                       const uint8_t *bitmap,
                       uint16_t foreground, uint16_t background);

/*写字符*/
void LCD_DrawChar8x16(uint16_t x, uint16_t y,
                      char ch,
                      uint16_t foreground, uint16_t background);

/*写字符串*/
void LCD_DrawString8x16(uint16_t x, uint16_t y,
                        const char *text,
                        uint16_t foreground, uint16_t background);

/*写数字(十进制)*/
void LCD_DrawUInt32(uint16_t x, uint16_t y,
                    uint32_t value, uint8_t min_digits,
                    uint16_t foreground, uint16_t background);

/*十六进制数字*/
void LCD_DrawHex32(uint16_t x,
                   uint16_t y,
                   uint32_t value,
                   uint8_t min_digits,
                   uint16_t foreground,
                   uint16_t background);

/*显示单个中文字符，通过GBK对中文字库索引。
可以在picture.c里面添加字模。且在picture.h里面extern出来*/
void LCD_DrawChinese16x16(uint16_t x, uint16_t y,
                          uint8_t GBK1, uint8_t GBK2,
                          uint16_t foreground, uint16_t background);

/*显示中文字符串*/
void LCD_DrawGBKString16x16(uint16_t x, uint16_t y,
                            const uint8_t *text,
                            uint16_t foreground, uint16_t background);

/* 基础颜色 */
#define RGB565_BLACK   0x0000 // 黑色
#define RGB565_WHITE   0xFFFF // 白色
#define RGB565_RED     0xF800 // 红色
#define RGB565_GREEN   0x07E0 // 绿色
#define RGB565_BLUE    0x001F // 蓝色

#define RGB565_YELLOW  0xFFE0 // 黄色
#define RGB565_CYAN    0x07FF // 青色
#define RGB565_MAGENTA 0xF81F // 品红

/* 灰度 */
#define RGB565_GRAY       0x8410 // 灰色
#define RGB565_DARK_GRAY  0x4208 // 深灰
#define RGB565_LIGHT_GRAY 0xC618 // 浅灰
#define RGB565_SILVER     0xC618 // 银灰

/* 红色系 */
#define RGB565_DARK_RED   0x8000 // 深红
#define RGB565_MAROON     0x7800 // 栗色
#define RGB565_CRIMSON    0xD8A7 // 猩红
#define RGB565_SALMON     0xFC0E // 鲑红
#define RGB565_CORAL      0xFBEA // 珊瑚色
#define RGB565_PINK       0xF81F // 粉红/品红
#define RGB565_LIGHT_PINK 0xFDB8 // 浅粉

/* 橙黄色系 */
#define RGB565_ORANGE      0xFD20 // 橙色
#define RGB565_DARK_ORANGE 0xFC60 // 深橙
#define RGB565_GOLD        0xFEA0 // 金色
#define RGB565_KHAKI       0xF731 // 卡其色
#define RGB565_BEIGE       0xF7BB // 米色

/* 绿色系 */
#define RGB565_DARK_GREEN   0x0320 // 深绿
#define RGB565_LIME         0x07E0 // 亮绿
#define RGB565_LIME_GREEN   0x3666 // 酸橙绿
#define RGB565_OLIVE        0x8400 // 橄榄色
#define RGB565_FOREST_GREEN 0x2444 // 森林绿
#define RGB565_SEA_GREEN    0x2C4A // 海绿色
#define RGB565_MINT         0x9FF3 // 薄荷绿

/* 蓝色系 */
#define RGB565_DARK_BLUE  0x0010 // 深蓝
#define RGB565_NAVY       0x000F // 海军蓝
#define RGB565_SKY_BLUE   0x867D // 天蓝
#define RGB565_LIGHT_BLUE 0xAEDC // 浅蓝
#define RGB565_ROYAL_BLUE 0x435C // 皇家蓝
#define RGB565_STEEL_BLUE 0x4416 // 钢蓝
#define RGB565_TURQUOISE  0x471A // 绿松石

/* 紫色系 */
#define RGB565_PURPLE   0x8010 // 紫色
#define RGB565_VIOLET   0xEC1D // 紫罗兰
#define RGB565_INDIGO   0x4810 // 靛蓝
#define RGB565_ORCHID   0xDB9A // 兰花紫
#define RGB565_LAVENDER 0xE73F // 淡紫

/* 棕色系 */
#define RGB565_BROWN      0xA145 // 棕色
#define RGB565_DARK_BROWN 0x6182 // 深棕
#define RGB565_CHOCOLATE  0xD343 // 巧克力色
#define RGB565_TAN        0xD5B1 // 棕褐色

/* ======================== 高级 UI 设计专用配色 ======================== */
/* 1. 科技深蓝/青色系（极佳的标题栏背景与科技高亮） */
#define RGB565_NAVY_BLUE     0x1128 // 深邃藏蓝 RGB(16, 37, 66) - 沉稳内敛
#define RGB565_MIDNIGHT_BLUE 0x194A // 午夜深蓝 RGB(25, 42, 86) - 现代科技
#define RGB565_TECH_BLUE     0x03DF // 经典科技蓝 RGB(0, 122, 255) - 醒目亮蓝
#define RGB565_CYAN_BLUE     0x061F // 冰蓝青色 RGB(0, 195, 255) - 极具未来感
/* 2. 轻奢门锁金色/橙色系（极力推荐的主菜单标题色） */
#define RGB565_CHAMPAGNE_GOLD 0xD566 // 香槟金 RGB(212, 175, 55) - 尊贵轻奢
#define RGB565_AMBER_GOLD     0xFDE0 // 琥珀金 RGB(255, 191, 0) - 明亮醒目
#define RGB565_HERMES_ORANGE  0xFC00 // 爱马仕橙 RGB(255, 128, 0) - 现代活力
/* 3. 暗黑主题结构灰（底色、卡片、边框、分割线） */
#define RGB565_CARD_BG_DARK  0x18E3 // 炭黑卡片底 RGB(30, 30, 30) - 比纯黑更具质感
#define RGB565_DARK_SLATE    0x29A8 // 深石墨灰 RGB(47, 53, 66) - 高质感顶栏底色
#define RGB565_BORDER_GRAY   0xB5B6 // 边框浅灰 RGB(180, 180, 180) - 细分割线
#define RGB565_ELEGANT_WHITE 0xF7BE // 象牙柔白 RGB(245, 245, 245) - 柔和不刺眼
/* 4. 状态反馈色（温和且明确，取代刺眼的纯红纯绿） */
#define RGB565_CORAL_RED      0xEA69 // 珊瑚红 RGB(235, 77, 75) - 错误/关锁/警报
#define RGB565_EMERALD_GREEN  0x2E6E // 翡翠绿 RGB(46, 204, 113) - 成功/开锁
#define RGB565_WARNING_YELLOW 0xFE20 // 柔和警告黄 RGB(254, 202, 87) - 待机/提示

#endif
