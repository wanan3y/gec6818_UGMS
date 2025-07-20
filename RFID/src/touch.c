#include "touch.h"

int TOUCH_fd = 0;       // 触摸屏的文件描述符
int tc_x = -1, tc_y = -1, tc_type = -1;     // 存放xy坐标，滑动类型
int state = 1;          // 图片滚动状态标志

// 触摸屏的初始化功能函数
void TOUCH_init(void)
{
    // 1）打开触摸屏：open
    TOUCH_fd = open("/dev/input/event0", O_RDWR);
    if(TOUCH_fd == -1)
    {
        perror("open TOUCH");
        return;
    }

    printf("触摸屏初始化成功\n");
}

// 关闭触摸屏
void TOUCH_close(void)
{
    // 关闭触摸屏
    close(TOUCH_fd);
}

// 获取触摸屏坐标
void get_xy(int *x, int *y)
{
    struct input_event tc;      // 输入事件的管理结构体变量

    while(1)
    {
        // 监听触摸屏的动作
        read(TOUCH_fd, &tc, sizeof(tc));

        if(tc.type == EV_ABS)   // 触摸屏事件
        {   
            if(tc.code == ABS_X)    // x轴事件
            {
                *x = tc.value;      // 获取x坐标值
            }else if(tc.code == ABS_Y)    // y轴事件
            {
                *y = tc.value;      // 获取y轴坐标
            }

        }
        // 按键中按下松开事件
        if(tc.type == EV_KEY && tc.code == BTN_TOUCH)
        {
            //printf("TOUCH = %d\n", tc.value);
            if(tc.value == 0)
            {
                break;
            }
        }
    }

    // 处理黑色边框的触摸屏坐标值: 蓝色边框注释该部分内容，黑色边框需要使用
    // *x = *x * 800 / 1024;
    // *y = *y * 480 / 600;
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
        read(TOUCH_fd, &tc, sizeof(tc));
        if (tc.type == EV_ABS && tc.code == ABS_X)      //表示是触摸屏的触摸事件中x轴事件
        {
            tmp_x = tc.value;      //获取x轴的坐标
        }else if (tc.type == EV_ABS && tc.code == ABS_Y)      //表示是触摸屏的触摸事件中Y轴事件
        {
            tmp_y = tc.value;      //获取y轴的坐标
        }

        if (tc.type == EV_KEY && tc.code == BTN_TOUCH)      //表示是按键按下或者松开的事件
        {
            // printf("TOUCH = [%d]\n", tc.value);
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
        if (fabs(val_x) > fabs(val_y))      //表示是左或者右滑:fabs: 求绝对值
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
        //蓝色边框开发板使用
        *x = x2;
        *y = y2;
        // //黑色边框的坐标值需要处理
        // *x = x2 * 800 / 1024;
        // *y = y2 * 480 / 600;

    }
    return -1;
}

// 获取触摸屏的坐标线程
void *touch_get(void *arg)
{
    while(1)
    {
        // 获取坐标，滑动类型
        tc_type = get_slide_xy(&tc_x, &tc_y);
        if (tc_type == -1)         // 点击
        {
            printf("(%d, %d)\n", tc_x, tc_y);
        } 
        if(tc_type == DOWN) // 下滑退出
        {
            printf("[退出线程]\n");
            break;
        }
        usleep(10000);  // 10ms  
    }
    pthread_exit(0);
}

