#ifndef _RFID_H_
#define _RFID_H_        //用于防止头文件被重复包含并编译

// 头文件: 系统头文件、自定义的头文件
#include "main.h"
// 宏定义、全局变量的外部声明

// 结构体、枚举、共用体（联合体）

// 函数的声明
//初始化串口
void TTY_init(void);
//不断发送A指令（请求RFID卡），一旦探测到卡片就退出
void request_card(int fd);
//获取RFID卡号
int get_id(int fd);
// 关闭串口
void TTY_close(void);
// 用户获取rfid卡号
int get_cardid(void);
#endif