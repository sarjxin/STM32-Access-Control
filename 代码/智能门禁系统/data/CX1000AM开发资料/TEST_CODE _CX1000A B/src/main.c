/******************** (C) COPYRIGHT  ***************************
 * 文件名  ：led试验
 * 描述    ：  
 * 库版本  ： 
 * 作者    ：
 * 博客    ：
 *修改时间 ：
//--------------------------------------------------------------
  硬件连接
      ____________________
     |                    | 
     |  LED1---------PC6  |
     |____________________|

*****************************************************************/
/* Includes ------------------------------------------------- --*/
#include "stm8s.h"
#include "main.h"
#include "clk.h"
#include "led.h"
#include "delay.h"
#include "uart.h"


extern unsigned char recv_buf[20];
extern unsigned int recv_num;

void TIM2_Init(void)
{
  TIM2->PSCR =0x04; // TIM2的时钟频率=fCK_PSC/2^4，即16分频，得到1us时基
  TIM2->ARRH =0x00;
  TIM2->ARRL =0x63; // TIM2_ARR初值设定为0x0063=99，计数100    ----100us
  TIM2->EGR=0x01;
  TIM2->CR1|=0x80;
  TIM2->IER|=0x01;//enable tim int  
//  TIM2->CR1|=0x01;//enable TIM
}

void IO_init(void)
{
  /*
  GPIOD->DDR |= (1<<1) | (1<<2) | (1<<3) | (1<<4);    //PD4 output  PD1-PD3
  GPIOD->CR1 |= (1<<1) | (1<<2) | (1<<3) | (1<<4);
  //GPIOD->CR2 =~(1<<1) | ~(1<<2) | ~(1<<3) | ~(1<<4);
  GPIOD->ODR |= (1<<1) | (1<<2) | (1<<3) | (1<<4);
  
  GPIOC->DDR |= (1<<3) | (1<<4) | (1<<5) | (1<<6) | (1<<7);    //PC3-PC7 output
  GPIOC->CR1 |= (1<<3) | (1<<4) | (1<<5) | (1<<6) | (1<<7);
  //GPIOC->CR2 =~(1<<3) | ~(1<<4) | ~(1<<5) | ~(1<<6) | ~(1<<7);
  GPIOC->ODR |= (1<<3) | (1<<4) | (1<<5) | (1<<6) | (1<<7);
  */
  GPIOD->DDR &= ~(1<<6);    //PD6 UART1_RX
  GPIOD->CR1 &= ~(1<<6);
  GPIOD->CR2 &= ~(1<<6);
  
  GPIOD->DDR |= (1<<5);     //PD5 UART1_TX
  GPIOD->CR1 &= ~(1<<5);
  GPIOD->CR2 &= ~(1<<5);
  
}


void main(void)
{
  unsigned char data_buf[10] = {0};//可根据协议中数据字节长度定义缓存大小
  unsigned char play_mode, volume;
  unsigned int play_num, music;
  
  sim();
  CLK_Config();//CLK HSI 16MHz
  LED_Init();
  UART_Config();
  rim();
  CFG->GCR|= CFG_GCR_SWD;//Disable swim function
  IO_init();
  TIM2_Init();
  TIM2Delay_n100us(50000); //5s
  LED_OFF();
  
 //设置播放模式及循环次数
  play_mode = 6; //播放模式为单曲停止
  play_num = 0; //循环次数
  //数据buf填充
  data_buf[0] = play_mode;
  data_buf[1] = (unsigned char)(play_num>>8);
  data_buf[2] = (unsigned char)(play_num);
  uart_send_cmd(BOARDCAST_ADDR, SET_PLAY_MODE_CMD, data_buf, 3);//指令发送
  
  //设置音量
  volume = 23;//音量值
  //数据buf填充
  data_buf[0] = volume;
  uart_send_cmd(BOARDCAST_ADDR, SET_VOLUME_CMD, data_buf, 1);//指令发送
          
  while(1)  
  {
    //查询播放状态
    uart_send_cmd(BOARDCAST_ADDR, QUERY_PLAY_STATUS_CMD, 0, 0);
    TIM2Delay_n100us(5000);//500ms
    if((recv_buf[4] == 0x01) && (recv_buf[5] != 0x01))//查询到设备未播放
    {
        recv_buf[4] = 0;
        //uart_send_cmd(BOARDCAST_ADDR, PLAY_CMD, 0, 0);//播放
        //uart_send_cmd(BOARDCAST_ADDR, NEXT_FILE_PLAY_CMD, 0, 0);//下一曲
        //选曲播放--只能选择根目录下曲目播放
        music = 6;//第6首音乐
        //数据buf填充
        data_buf[0] = (unsigned char)(music>>8);
        data_buf[1] = (unsigned char)(music);
        uart_send_cmd(BOARDCAST_ADDR, APPOINTED_FILE_PLAY_CMD, data_buf, 2);//指令发送
        //指令发送结束LED灯闪烁一下
        LED_ON();
        TIM2Delay_n100us(1000);
        LED_OFF();
        TIM2Delay_n100us(1000);
    }
    TIM2Delay_n100us(30000);//3s
  } 

}


