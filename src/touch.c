#include "main.h"
#include <unistd.h> // 为了 close() 函数
#include <math.h> // For fabs

extern int touch_fd;        //外部声明
  
//初始化触摸屏
void init_touch(void)
{
    // （1）打开触摸屏的设备文件：open
    touch_fd = open("/dev/input/event0", O_RDWR);       //以可读可写的方式打开触摸屏的设备文件
    if (touch_fd < 0)
    {
        printf("open TOUCH error\n");
        return;
    }
    return;
}

//获取滑动或者触摸屏的坐标
int get_slide_xy(int *x, int *y)
{
    struct input_event tc;      //声明输入事件结构体的结构体变量tc
    int x1, x2, y1, y2;         //存放按下和松开时的坐标
    int tmp_x, tmp_y, val_x, val_y;     //存放临时的xy坐标，以及按下和松开的差值

    while (1)
    {
        //将数据读取到输入事件结构体中
        read(touch_fd, &tc, sizeof(tc));
        if (tc.type == EV_ABS && tc.code == ABS_X)      //表示是触摸屏的触摸事件中x轴事件
        {
            tmp_x = tc.value;      //获取x轴的坐标
        }else if (tc.type == EV_ABS && tc.code == ABS_Y)      //表示是触摸屏的触摸事件中Y轴事件
        {
            tmp_y = tc.value;      //获取y轴的坐标
        }

        if (tc.type == EV_KEY && tc.code == BTN_TOUCH)      //表示是按键按下或者松开的事件
        {
            printf("TOUCH = [%d]\n", tc.value);
            if (tc.value == 0)      //表示松开
            {
                x2 = tmp_x;
                y2 = tmp_y;         //获取松开时的坐标
                break;
            }else if (tc.value == 1)
            {
                x1 = tmp_x;
                y1 = tmp_y;         //获取按下时的坐标
            }            
        }   
        
    }

    //获取按下和松开的差值
    val_x = x2 - x1;
    val_y = y2 - y1;

    if ((val_x * val_x + val_y * val_y) > 50*50)    //判断是否为滑动
    {
        *x = *y = -1;       //重置xy的坐标
        if (fabs(val_x) > fabs(val_y))      //表示是左或者右滑
        {
            if (val_x > 0)      //右滑
            {
                return RIGHT;
            }else
            {
                return LEFT;   //左滑 
            }
            
        }else if (fabs(val_x) <= fabs(val_y))       //上滑或者下滑
        {
            if (val_y > 0)      //下滑
            {
                return DOWN;
            }else
            {
                return UP;      //上滑
            }
        }

    }else
    {
        // 蓝色边框板子使用的坐标获取
        // *x = x2 ;
        // *y = y2 ;
        //黑色边框板子使用的坐标获取，蓝色不需要（屏蔽）
        *x = x2 * 800 / 1024;
        *y = y2 * 480 / 600;
    }
    return -1;
}

// //获取触摸屏坐标的函数
// void *touch(void *arg)
// {
//     int x = -1, y = -1;

//     while (1)
//     {
//         //获取滑动或者触摸屏的坐标
//         get_slide_xy(&x, &y);
//         if (x < 0 || x > 800  || y < 0 || y > 480)       //表示坐标范围错误
//         {
// 			x = y = -1;		//表示数据获取失败，重置存放坐标的变量
//             continue;
//         }
//         printf("[TOUCH线程](%d,%d)\n",x,y);  

//         if (x > 640 && x < 800 && y > 0 && y < 240)		//入库的按键
// 		{
// 			printf("[车辆入库]\n");
// 			kill(tid, SIGUSR1);	    //发送入库车牌识别的信号

// 		}else if (x > 640 && x < 800 && y > 240 && y < 480)	//出库按钮
// 		{
// 			printf("[车辆出库]\n");
// 			kill(tid, SIGUSR2);	    //发送入库车牌识别的信号
// 		}

//     }

// }

//关闭触摸屏
void close_touch(void)
{
    close(touch_fd);
}