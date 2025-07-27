#ifndef MAIN_H
#define MAIN_H

//存放所有头文件

//系统头文件
#include <time.h>
#include <sys/wait.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <setjmp.h>
#include <assert.h>
#include <termios.h> 
#include <sys/select.h>
#include <netdb.h>

//自定义或者引用头文件
#include "ISO14443A.h"
#include "jpeglib.h"
#include "jconfig.h"
#include "mySQLite.h"
#include "touch.h"
#include "rfid.h"
#include "lcd.h"
#include "carmera.h"

// 新增信号定义
#define SIG_IN_CAR  SIGUSR1
#define SIG_OUT_CAR SIGUSR2

extern volatile int alpr_error_detected; // ALPR 错误检测标志位

volatile int program_running; // 程序运行标志位
pthread_mutex_t photo_mutex;
pthread_cond_t photo_cond;
volatile bool photo_ready;

pthread_mutex_t alpr_mutex; // ALPR响应互斥锁
pthread_cond_t alpr_cond;   // ALPR响应条件变量

// [新增] RFID与摄像头联动验证的全局变量 (extern声明)
extern char g_rfid_plate[20];
extern volatile bool g_is_rfid_triggered;
extern volatile bool g_suppress_bmp_output; // [新增] 控制BMP图片尺寸输出的全局标志

// 需补充信号处理原型
void handle_alpr_in(int sig);
void handle_alpr_out(int sig);

#endif
