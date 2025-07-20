#ifndef _TOUCH_H_
#define _TOUCH_H_        //用于防止头文件被重复包含并编译

// 头文件: 系统头文件、自定义的头文件
#include "main.h"
// 宏定义、全局变量的外部声明
extern int tc_x , tc_y , tc_type;     // 存放xy坐标，滑动类型
extern int state;          // 图片滚动状态标志

// 结构体、枚举、共用体（联合体）
//滑屏算法返回值枚举（上下左右）
enum slide{UP, DOWN, LEFT, RIGHT};

// 函数的声明
// 触摸屏的初始化功能函数
void TOUCH_init(void);
// 关闭触摸屏
void TOUCH_close(void);
// 获取触摸屏坐标
void get_xy(int *x, int *y);
//获取滑动或者触摸屏的坐标
int get_slide_xy(int *x, int *y);

// 获取触摸屏的坐标线程
void *touch_get(void *arg);
#endif