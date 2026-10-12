#include "ST7735.h"

typedef enum
{
    LCD_BITMAP_OPAQUE = 0,       /* 1 写前景色，0 写背景色 */
    LCD_BITMAP_TRANSPARENT = 1   /* 只画 1，0 保留原画面 */
} LCD_BitmapMode;

/*使用模式0: 空闲状态SCL为低电平——>所以CPOL=0；
            先准备好数据，第一个时钟沿来就开始读——>所以CPHA=0*/

static void ST7735_SetRES(uint8_t BitVal)
{
    GPIO_WriteBit(ST7735_SCL_MOSI_RES_DC_BLK_PORT,ST7735_RES_PIN,(BitAction) BitVal);
}

static void ST7735_SetDC(uint8_t BitVal)
{
    GPIO_WriteBit(ST7735_SCL_MOSI_RES_DC_BLK_PORT,ST7735_DC_PIN,(BitAction) BitVal);
}

static void ST7735_SetBLK(uint8_t BitVal)
{
    GPIO_WriteBit(ST7735_SCL_MOSI_RES_DC_BLK_PORT,ST7735_BLK_PIN,(BitAction) BitVal);
}

static void ST7735_Start(void)
{
    // SS引脚产生下降沿
    GPIO_WriteBit(ST7735_CS_PORT,ST7735_CS_PIN,Bit_RESET);
}

static void ST7735_Stop(void)
{
    /* 等待移位寄存器和总线彻底空闲 */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET);
    //  SS引脚产生上升沿
    GPIO_WriteBit(ST7735_CS_PORT,ST7735_CS_PIN,Bit_SET);

}

static void ST7735_SendByte(uint8_t data)
{
    // 等待TXE(输出寄存器空)标志位
    while( SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_TXE) == RESET);
    // 向输出寄存器放入数据
    SPI_I2S_SendData(SPI1,data);
}

static void ST7735_Bus_Init(void)
{
    // 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB 
                    | RCC_APB2Periph_AFIO | RCC_APB2Periph_SPI1, ENABLE);

    // PA15是JTDI、PB3是JTDO、PB4是NJTRST
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);//重定义功能，将JTAG失能，且保持SWD功能
    GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);
    
    // 设置GPIO引脚模式
    // SS是推挽输出模式
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = ST7735_CS_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ST7735_CS_PORT,&GPIO_InitStruct);

    // CLK、MOSI是复用推挽输出模式
    GPIO_InitStruct.GPIO_Pin = ST7735_SCL_PIN | ST7735_MOSI_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ST7735_SCL_MOSI_RES_DC_BLK_PORT, &GPIO_InitStruct);

    SPI_InitTypeDef SPI_InitStruct;
    // SPI_BaudRatePrescaler_256：实际932ms
    // SPI_BaudRatePrescaler_32：实际117ms
    // SPI_BaudRatePrescaler_8：实际29ms
    // SPI_BaudRatePrescaler_4 / SPI_BaudRatePrescaler_2：实际20ms
    SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8; //72MHz / 8 = 9MHz
    SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStruct.SPI_CRCPolynomial = 7;
    SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStruct.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
    SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;
    SPI_Init(SPI1,&SPI_InitStruct);

    // 初始化RES、DC、BLK的GPIO
    GPIO_InitTypeDef ST7735_GPIO_InitStructure;
    ST7735_GPIO_InitStructure.GPIO_Pin =  ST7735_RES_PIN | ST7735_DC_PIN | ST7735_BLK_PIN;
    ST7735_GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    ST7735_GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ST7735_SCL_MOSI_RES_DC_BLK_PORT, &ST7735_GPIO_InitStructure);

    // 初始化引脚状态：令 CS=1、SCL=0、RES=1
    ST7735_Stop();
    ST7735_SetRES(1);
    ST7735_SetDC(0);
    ST7735_SetBLK(0);

    // 启动SPI1外设
    SPI_Cmd(SPI1,ENABLE);
}

static void LCD_WriteCommand(uint8_t Command)
{
    // 通过DC告诉ST7735这是命令信息
    ST7735_SetDC(0);
    ST7735_Start();
    ST7735_SendByte(Command);
    ST7735_Stop();
}

static void LCD_WriteData8(uint8_t Data)
{
    // 通过DC告诉ST7735这是数据信息
    ST7735_SetDC(1);
    ST7735_Start();
    ST7735_SendByte(Data);
    ST7735_Stop();
}

/**
 * @brief 用于发送一个RGB565像素，是不是就是发两个八位的字节就可以了？
 */
static void LCD_WriteData16(uint16_t Data)
{
    // 先发高八位
    LCD_WriteData8(Data >> 8);
    // 再发低八位
    LCD_WriteData8(Data);
}

static void LCD_SetWindow(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
    LCD_WriteCommand(0x2A);
    LCD_WriteData16(x_start + 2);   // x_start + 2 的高、低字节
    LCD_WriteData16(x_end + 2);     // x_end   + 2 的高、低字节

    LCD_WriteCommand(0x2B);
    LCD_WriteData16(y_start + 3);   // y_start + 2 的高、低字节
    LCD_WriteData16(y_end + 3);     // y_end   + 2 的高、低字节

    LCD_WriteCommand(0x2C);         // 后续数据写入显存(什么意思？)
}

static void LCD_Reset(void)
{
    ST7735_SetRES(1);
    Delay_ms(1);

    ST7735_SetRES(0);
    Delay_ms(10);

    ST7735_SetRES(1);
    Delay_ms(120);
}

void LCD_Init(void)
{
    ST7735_Bus_Init();
    // 1.先软件复位
    LCD_Reset();    //Reset before lcd init,跟商家的复位方式不太一样，延时时间不一样
    // 2.sleep out
    LCD_WriteCommand(0x11);   //Sleep exit
    // 3.延时120ms
    Delay_ms(120);
    // 4.面板初始化参数
    /*直接抄例子*/
	//ST7735R Frame Rate
	LCD_WriteCommand(0xB1); 
	LCD_WriteData8(0x01); 
	LCD_WriteData8(0x2C); 
	LCD_WriteData8(0x2D); 

	LCD_WriteCommand(0xB2); 
	LCD_WriteData8(0x01); 
	LCD_WriteData8(0x2C); 
	LCD_WriteData8(0x2D); 

	LCD_WriteCommand(0xB3); 
	LCD_WriteData8(0x01); 
	LCD_WriteData8(0x2C); 
	LCD_WriteData8(0x2D); 
	LCD_WriteData8(0x01); 
	LCD_WriteData8(0x2C); 
	LCD_WriteData8(0x2D); 
	
	LCD_WriteCommand(0xB4); //Column inversion 
	LCD_WriteData8(0x07); 
	
	//ST7735R Power Sequence
	LCD_WriteCommand(0xC0); 
	LCD_WriteData8(0xA2); 
	LCD_WriteData8(0x02); 
	LCD_WriteData8(0x84); 
	LCD_WriteCommand(0xC1); 
	LCD_WriteData8(0xC5); 

	LCD_WriteCommand(0xC2); 
	LCD_WriteData8(0x0A); 
	LCD_WriteData8(0x00); 

	LCD_WriteCommand(0xC3); 
	LCD_WriteData8(0x8A); 
	LCD_WriteData8(0x2A); 
	LCD_WriteCommand(0xC4); 
	LCD_WriteData8(0x8A); 
	LCD_WriteData8(0xEE); 
	
	LCD_WriteCommand(0xC5); //VCOM 
	LCD_WriteData8(0x0E); 
    // 0x36（显示方向和 RGB/BGR）
    LCD_WriteCommand(0x36); //MX, MY, RGB mode 
	LCD_WriteData8(0xC8);

/*照抄*/
	//ST7735R Gamma Sequence
	LCD_WriteCommand(0xe0); 
	LCD_WriteData8(0x0f); 
	LCD_WriteData8(0x1a); 
	LCD_WriteData8(0x0f); 
	LCD_WriteData8(0x18); 
	LCD_WriteData8(0x2f); 
	LCD_WriteData8(0x28); 
	LCD_WriteData8(0x20); 
	LCD_WriteData8(0x22); 
	LCD_WriteData8(0x1f); 
	LCD_WriteData8(0x1b); 
	LCD_WriteData8(0x23); 
	LCD_WriteData8(0x37); 
	LCD_WriteData8(0x00); 	
	LCD_WriteData8(0x07); 
	LCD_WriteData8(0x02); 
	LCD_WriteData8(0x10); 

	LCD_WriteCommand(0xe1); 
	LCD_WriteData8(0x0f); 
	LCD_WriteData8(0x1b); 
	LCD_WriteData8(0x0f); 
	LCD_WriteData8(0x17); 
	LCD_WriteData8(0x33); 
	LCD_WriteData8(0x2c); 
	LCD_WriteData8(0x29); 
	LCD_WriteData8(0x2e); 
	LCD_WriteData8(0x30); 
	LCD_WriteData8(0x30); 
	LCD_WriteData8(0x39); 
	LCD_WriteData8(0x3f); 
	LCD_WriteData8(0x00); 
	LCD_WriteData8(0x07); 
	LCD_WriteData8(0x03); 
	LCD_WriteData8(0x10);  
	
	LCD_WriteCommand(0x2a);
	LCD_WriteData8(0x00);
	LCD_WriteData8(0x00);
	LCD_WriteData8(0x00);
	LCD_WriteData8(0x7f);

	LCD_WriteCommand(0x2b);
	LCD_WriteData8(0x00);
	LCD_WriteData8(0x00);
	LCD_WriteData8(0x00);
	LCD_WriteData8(0x9f);
	
	LCD_WriteCommand(0xF0); //Enable test command  
	LCD_WriteData8(0x01); 
	LCD_WriteCommand(0xF6); //Disable ram power save mode 
	LCD_WriteData8(0x00); 

    // 0x3A + 0x05（16 位颜色格式）
    LCD_WriteCommand(0x3A); //65k mode 
	LCD_WriteData8(0x05); 

    // 0x29（Display On）
    LCD_WriteCommand(0x29);//Display on	

    // BLK=1
    ST7735_SetBLK(1);
}

static void LCD_WriteColorRepeat(uint16_t color, uint32_t count)
{
    uint8_t high = color >> 8;
    uint8_t low  = color & 0xFF;

    ST7735_SetDC(1);
    ST7735_Start();

    while (count > 0)
    {
        ST7735_SendByte(high);
        /* 等待移位寄存器和总线彻底空闲 */
        //while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET);
        ST7735_SendByte(low);
        //while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET);
        count--;
    }

    ST7735_Stop();
}

void LCD_FillColor(uint16_t Color)
{
    // 先给一个像素范围(这里是fill，即全屏染色，所以是128×128=16384个像素点)
    LCD_SetWindow(0, 0, 127, 127);

    // 依次给颜色，这里不需要手动选择像素点缀色，控制器会对给的范围进行指针指向，给了颜色之后自动指针+1
    // for (uint16_t i = 0; i < 128UL * 128UL; i++)
    // {
    //     LCD_WriteData16(Color);
    // }
    LCD_WriteColorRepeat(Color, 128UL*128UL);

}

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_SetWindow(x, y, x, y);
    LCD_WriteData16(color);
}

void LCD_DrawLine(uint16_t x_start, uint16_t y_start, 
                uint16_t x_end, uint16_t y_end, uint16_t color)
{
    int x0 = x_start;
    int y0 = y_start;
    int x1 = x_end;
    int y1 = y_end;

    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);

    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    while (1)
    {
        LCD_DrawPoint(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    uint32_t color_size = width * height;

    LCD_SetWindow( x,  y,  x + width - 1,  y + height - 1);
    LCD_WriteColorRepeat(color, color_size);
}

static void LCD_WriteBytes(const uint8_t *data, uint32_t byte_count)
{
    ST7735_SetDC(1);
    ST7735_Start();
    for (uint32_t i = 0; i < byte_count; i++)
    {
        ST7735_SendByte(data[i]);
    }
    ST7735_Stop();    // 内部等待 BSY=0 后拉高 CS

}

void LCD_DrawImageRGB565(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    const uint8_t *image
)
{
    LCD_SetWindow(x,
                y,
                x + width - 1,
                y + height - 1);

    uint32_t byte_count = (uint32_t)width * (uint32_t)height * 2U;

    LCD_WriteBytes(image, byte_count);
}

void LCD_DrawBitmapall(uint16_t x,
                       uint16_t y,
                       uint16_t width,
                       uint16_t height,
                       const uint8_t *bitmap,
                       uint16_t foreground,
                       uint16_t background)
{
    uint16_t row;
    uint16_t column;
    uint16_t byte_index;
    uint16_t bit_mask;
    uint16_t color;
    uint16_t bytes_per_row = (width + 7U) / 8U; //根据width计算每一行要读取多少个字节

    if ((bitmap == 0) || (width == 0U) || (height == 0U) ||
    (x >= 128U) || (y >= 128U) ||
    (width > (128U - x)) || (height > (128U - y)))
    {
        return;
    }

    LCD_SetWindow(x, y, x + width - 1, y + height - 1);
    ST7735_SetDC(1);
    ST7735_Start();

    for (row = 0U; row < height; row++)
    {
        for (column = 0U; column < width; column++)
        {
            // 字节索引
            byte_index = row * bytes_per_row + column / 8U;
            // 选择位
            bit_mask   = 0x80U >> (column % 8U);

            if ((bitmap[byte_index] & bit_mask) != 0U)
            {
                color = foreground;
            }
            else
            {
                color = background;
            }
            ST7735_SendByte(color >> 8);
            ST7735_SendByte(color);
        }
    }
    ST7735_Stop();
}


void LCD_DrawChar8x16(uint16_t x,
                      uint16_t y,
                      char ch,
                      uint16_t foreground,
                      uint16_t background)
{
    uint8_t code = (uint8_t)ch;
    if(code < 0x20 || code > 0x7E)
    {
        return;
    }
    uint8_t index = code - 0x20;
    LCD_DrawBitmapall(x, y, 8, 16, font8x16[index], foreground, background);
}

void LCD_DrawString8x16(uint16_t x,
                        uint16_t y,
                        const char *text,
                        uint16_t foreground,
                        uint16_t background)
{
    uint16_t x_offset = x;
    while(*text != '\0')
    {
        LCD_DrawChar8x16(x_offset, y, *text, foreground, background);
        x_offset += 8;
        text++;
    }
}

/* 10 的 n 次方表：POW10[4] = 10000，POW10[3] = 1000 */
static const uint32_t POW10[10] = {
    1, 10, 100, 1000, 10000,
    100000, 1000000, 10000000, 100000000, 1000000000
};

void LCD_DrawUInt32(uint16_t x,
                    uint16_t y,
                    uint32_t value,
                    uint8_t min_digits,
                    uint16_t foreground,
                    uint16_t background)
{
    // 计算几位数
    uint8_t count = 0;
    uint32_t count_value = value; 
    while(count_value != 0)
    {
        count++;
        count_value /= 10;
    }
    if(min_digits < count)
    {
        min_digits = count;
    }
    for(uint8_t i = 0; i < min_digits; i++)
    {
        if(count + i < min_digits)
        {
            LCD_DrawChar8x16(x, y, '0', foreground, background);
            x += 8;
        }
        else
        {
            // 取出最高位
            uint8_t digit = value / POW10[min_digits-i-1];
            // digit + '0'是将数字转变为字符格式
            LCD_DrawChar8x16(x, y, digit + '0', foreground, background);
            x += 8;
            value %= POW10[min_digits-i-1]; // 去除刚刚那位
        }
    }
}

void LCD_DrawHex32(uint16_t x,
                   uint16_t y,
                   uint32_t value,
                   uint8_t min_digits,
                   uint16_t foreground,
                   uint16_t background)
{
    uint8_t buffer[8];
    uint8_t digits = 0;
    uint8_t hex_value;

    if (min_digits > 8U)
    {
        min_digits = 8U;
    }

    /* 从最低位开始转换 */
    do
    {
        hex_value = (uint8_t)(value & 0x0FU);

        if (hex_value < 10U)
        {
            buffer[digits] = (uint8_t)('0' + hex_value);
        }
        else
        {
            buffer[digits] = (uint8_t)('A' + hex_value - 10U);
        }

        digits++;
        value >>= 4U;

    } while ((value != 0U) && (digits < 8U));

    /* 不足min_digits时，在高位补0 */
    while (digits < min_digits)
    {
        buffer[digits] = '0';
        digits++;
    }

    /* buffer中的字符顺序是反的，所以倒序显示 */
    while (digits > 0U)
    {
        digits--;

        LCD_DrawChar8x16(x,
                          y,
                          buffer[digits],
                          foreground,
                          background);

        x += 8U;
    }
}


void LCD_DrawChinese16x16(uint16_t x,
                       uint16_t y,
                       uint8_t GBK1, uint8_t GBK2,
                       uint16_t foreground,
                       uint16_t background)
{
    uint16_t index = 0;
    const uint8_t *bitmap_start = 0;
    // 根据GBK1、GBK2找出字模
    while(index < GBK16_FONT_COUNT)
    {
        if( (GBK1 ==g_gbk16_font[index].gbk[0]) && (GBK2 ==g_gbk16_font[index].gbk[1]) )
        {
            bitmap_start = g_gbk16_font[index].bitmap;
            LCD_DrawBitmapall(x, y, 16, 16, bitmap_start, foreground, background);
            break;
        }
        index++;
    }


    if(index >= GBK16_FONT_COUNT)
    {
        LCD_DrawChar8x16(x,y,'?',BLACK, WHITE);

    }
    
}

/*打印中文字符串需要配合g_gbk16_font字库以及字符串数组
const uint8_t nihao_GBK[] =
{
    0xC4U, 0xE3U,  你 
    0xBAU, 0xC3U,  好 
    0x00U
};
*/
void LCD_DrawGBKString16x16(uint16_t x,
                            uint16_t y,
                            const uint8_t *text,
                            uint16_t foreground,
                            uint16_t background)
{
    uint16_t current_x = x;

    if (text == 0)
    {
        return;
    }

    /* 16像素高：y最大只能是112，占用112～127行 */
    if (y > 112U)
    {
        return;
    }

    while (text[0] != 0x00U)
    {
        /* 只剩一个字节，无法组成完整GBK汉字 */
        if (text[1] == 0x00U)
        {
            break;
        }

        /* 16像素宽：x最大只能是112，占用112～127列 */
        if (current_x > 112U)
        {
            break;
        }

        LCD_DrawChinese16x16(current_x,
                            y,
                            text[0],
                            text[1],
                            foreground,
                            background);

        current_x += 16U;
        text += 2;
    }
}




