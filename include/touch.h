#ifndef TOUCH_H
#define TOUCH_H


//滑屏算法返回值枚举（上下左右）
enum slide{UP, DOWN, LEFT, RIGHT};

//初始化触摸屏
void init_touch(void);
//获取滑动或者触摸屏的坐标
int get_slide_xy(int *x, int *y);

//关闭触摸屏
void close_touch(void);

#endif