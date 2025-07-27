#ifndef CARMERA_H
#define CARMERA_H

#include "main.h"

#define FALSE 0
#define TRUE 1

#define OFF 0		//开
#define ON 1		//关

#define RUN 1		//开启并显示
#define OUT 0		//退出
#define TURN 2		//开启不显示

extern int Shot;			//拍照的标准：默认没有按下拍照
extern int Camera_state;		//摄像头状态，默认退出状态

typedef struct
{
	uint8_t* start;
	size_t length;
} buffer_t;

typedef struct 
{
	int fd;
	uint32_t width;
	uint32_t height;
	size_t buffer_count;
	buffer_t* buffers;
	buffer_t head;
} camera_t;

//初始化摄像头
void init_camera(void);
//关闭摄像头、释放对应资源
void close_Camera(void);

//摄像头获取数据的线程执行函数
void *Camera(void *arg);
#endif