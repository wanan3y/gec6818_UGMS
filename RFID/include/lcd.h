#ifndef _LCD_H_
#define _LCD_H_        //用于防止头文件被重复包含并编译

// 头文件: 系统头文件、自定义的头文件
#include "main.h"
// 宏定义、全局变量的外部声明

// 结构体、枚举、共用体（联合体）

// 函数的声明
// LCD的初始化功能函数
void LCD_init(void);
// 关闭LCD显示屏
void LCD_close(void);
// 显示图片: 传递图片名称
void show_bmp(char *bmpname);
// 显示图片: 参数：图片名称、x坐标、y坐标
void show_bmp_any(char *bmpname, int r_x, int r_y);
// 显示图片: 参数：图片名称、x坐标、y坐标
void display_bmp(char *bmpname, int r_x, int r_y) ;
// 显示图片: 参数：图片名称、x坐标、y坐标、缩放比例
void display_bmp_scale(char *bmpname, int r_x, int r_y, float scale);
void show_bmp_diffuse(char *bmpname);
void show_bmp_diffuse1(char *bmpname);
#endif