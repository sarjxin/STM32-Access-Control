/******************** (C) COPYRIGHT  ***************************
 * 文件名  ：uart.H
 * 描述    ：   
 * 库版本  ： 
 * 作者    ：
 * 博客    ：
 *修改时间 ：

*****************************************************************/
#ifndef __UART_H_
#define __UART_H_
#include "stm8s.h"


#define  UART_PROTOCOL_HEAD     0x7E	//协议帧头
#define  UART_PROTOCOL_TAIL     0xEF	//协议帧尾

#define  BOARDCAST_ADDR   0xFFFF  //广播地址



#define  QUERY_PLAY_STATUS_CMD           0x01  //查询播放状态
#define  PLAY_CMD                        0x02  //播放
#define  PAUSE_CMD						 0x03  //暂停
#define  STOP_CMD						 0x04  //停止

#define  PREV_FILE_PLAY_CMD				 0x05  //上一曲
#define  NEXT_FILE_PLAY_CMD				 0x06  //下一曲

#define  APPOINTED_FILE_PLAY_CMD		 0x07  //选择曲目播放
#define  APPOINTED_DISK_PATH_PLAY_CMD	 0x08  //指定盘符指定路径播放
//数据{0x02,0x2F,0x63,0x68,0x61,0x62,0x6F,0x2F,0x30,0x30,0x30,0x31,0x31,0x2E,0x2A}--播放FLASH中路径/chabo/00011.*文件播放(文件名为00011的曲目播放)

#define  QUERY_ONLINE_DISK_CMD			 0x09  //查询在线盘符
#define  QUERY_PLAY_DISK_CMD			 0x0A  //查询当前播放盘符
#define  SWITCH_APPOINTED_DISK_CMD		 0x0B  //切换到指定盘符--数据{0x02}切换到FLASH

#define  QUERY_TOTAL_FILE_NUM_CMD		 0x0C  //查询总曲目
#define  QUERY_CUR_FILE_NUM_CMD			 0x0D  //查询当前曲目

#define  PREV_FOLDER_PLAY_CMD			 0x0E  //上一文件夹目录
#define  NEXT_FOLDER_PLAY_CMD			 0x0F  //下一文件夹目录

#define  SET_VOLUME_CMD					 0x13  //设置音量--数据{0x14}设置音量为20级
#define  VOLUME_UP_CMD					 0x14  //音量+
#define  VOLUME_DOWN_CMD				 0x15  //音量-

#define  INSERT_APPOINTED_PATH_PLAY_CMD	 0x17  //指定路径插播
//数据{0x02,0x2F,0x63,0x68,0x61,0x62,0x6F,0x2F,0x30,0x30,0x30,0x31,0x31,0x2E,0x2A}--插播FLASH中路径/chabo/00011.*文件播放(文件名为00011的曲目播放)
#define  INSERT_END_PLAY_CMD			 0x10  //结束插播

#define  SET_PLAY_MODE_CMD				 0x18  //设置播放模式及循环次数
//数据{0x06,0x00,0x00}--播放模式为单曲停止，循环次数为0
#define  SET_EQ_MODE_CMD				 0x1A  //设置EQ模式
//数据{0x02}--设置EQ模式为POP

#define  COMB_FILE_PALY_CMD				 0x1B  //组合播放
//数据{0x30,0x31,0x30,0x32}--文件名为“01”、“02”歌曲组合播放

#define  SELECT_FILE_NONE_PLAY_CMD		 0x1F  //选曲不播放
//数据{0x00,0x05}--选择根目录下文件名为00005的曲目，不播放



void UART_Config(void);
void uart_send_cmd(unsigned int dev_id, unsigned char cmd, unsigned char *data_buf, unsigned int data_buf_len);

#endif