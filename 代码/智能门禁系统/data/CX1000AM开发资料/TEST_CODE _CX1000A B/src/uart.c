/******************** (C) COPYRIGHT  ***************************
 * 文件名  ：uart.C
 * 描述    ：   
 * 库版本  ： 
 * 作者    ：
 * 博客    ：
 *修改时间 ：

*****************************************************************/
#include "uart.h"

void UART_Config(void)
{
  UART1->CR1 = (0<<4)|(0<<3)|(0<<2)|(0<<1)|(0<<0);//1位起始位8位数据位，CR3决定停止位，不使用奇偶校验位，不使能奇偶校验功能
  UART1->CR2 = (0<<7)|(0<<6)|(1<<5)|(0<<4)|(1<<3)|(1<<2);//使能发送、接收 接收中断使能 禁止发送中断
  UART1->CR3 = (0<<6)|(0<<5)|(0<<4)|(0<<3); //设置1位停止位 不使能SCLK
  
  //设置波特率    
  //16M/9600=1667=683H
  UART1->BRR2 = 0x03; 
  UART1->BRR1 = 0x68;
  
  
  UART1->CR1 &= ~(1<<5);//使能uart
}


/*************发送一个字节函数*****************/
void UART1_SendByte(unsigned char data)
{
    while (!(UART1->SR & 0x80)); //数据转移完成
    UART1->DR=data;
    while(!(UART1->SR & 0x40));  //发送完成
}


/***************发送字符串***********************/
void UART1_SendBuf(unsigned char *send_buf, unsigned int send_buf_len)
{
    unsigned int i = 0;
    
    for(i=0;i<send_buf_len;i++)
    {
        UART1_SendByte(send_buf[i]);
    }
}


/*************校验和获取*****************/
static unsigned char Get_Sum(unsigned char* buf, unsigned int len)
{
    unsigned int i = 0;
    unsigned long sum = 0;

    for(i=0;i<len;i++)
    {
        sum += buf[i];
    }								 
    return((unsigned char)sum);
}

/****************发送串口协议指令*********************/
/* 输入参数说明：dev_id 目标设备地址
			 cmd 指令码
			 data_buf 数据，若无写入0
			 data_buf_len 数据长度，若无写入0
	返回参数：发送指令长度	
*/
void uart_send_cmd(unsigned int dev_id, unsigned char cmd, unsigned char *data_buf, unsigned int data_buf_len)
{
    unsigned int i=0, n=0;
    unsigned char send_buf[20];
    unsigned int send_buf_len = 7 + data_buf_len;

    //指令组包
    send_buf[i++] = UART_PROTOCOL_HEAD;
    send_buf[i++] = send_buf_len;
    send_buf[i++] = (unsigned char)(dev_id>>8);
    send_buf[i++] = (unsigned char)(dev_id);
    send_buf[i++] = cmd;
    //数据
    if(data_buf_len)
    {
        for(n=0;n<data_buf_len;n++)
        {
            send_buf[i++] = data_buf[n];
        }
    }
    //发送和校验、帧尾
    send_buf[i++] = Get_Sum(send_buf,send_buf_len-2);
    send_buf[i++] = UART_PROTOCOL_TAIL;

    //串口发送指令
    UART1_SendBuf(send_buf, send_buf_len);
}




