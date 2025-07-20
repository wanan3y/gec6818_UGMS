#include "main.h"
#include "carmera.h"
#include "touch.h"
#include "mySQLite.h"
#include "ISO14443A.h"
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <stdio.h>
#include <string.h> // 确保包含了 string.h 用于 memset 和 strcmp
#include <errno.h> // 添加 errno.h 用于错误检查

// [全局变量定义] 用于RFID与摄像头联动验证
char g_rfid_plate[20] = {0}; // 存储由RFID刷卡触发的、期望操作的车牌号
volatile bool g_is_rfid_triggered = false; // 标志位，用于指示当前操作是否由RFID触发
volatile bool g_suppress_bmp_output = false; // [新增] 默认不抑制BMP输出



volatile int alpr_error_detected = 0; // ALPR 错误检测标志位
volatile bool alpr_response_received = false; // ALPR响应标志位

// ================= 全局变量定义 =================
int lcd_fd;         // LCD 文件描述符
int *mmap_p;        // LCD 内存映射指针
int touch_fd;       // 触摸屏文件描述符
int tty_fd;         // RFID (串口) 文件描述符
bool cardOn;         // RFID 卡片状态
volatile int program_running; // 控制程序和线程循环的标志位
pthread_mutex_t photo_mutex; // 照片处理互斥锁
pthread_cond_t photo_cond;   // 照片处理条件变量
volatile bool photo_ready;    // 照片是否准备好的标志

pid_t alpr_pid;     // ALPR 进程的 PID

time_t last_activity_time; // 最后一次用户活动时间

// 定义RFID卡号和车牌号的映射结构体
typedef struct {
    const char *rfid_id;
    const char *plate_number;
} RfidPlateMap;

// 创建一个包含所有有效卡片信息的全局数组
RfidPlateMap rfid_plate_maps[] = {
    {"aff963ea", "贵A61000"},
    {"350d4af3", "京Q58A77"}
};
// 计算数组中的条目数量
int num_rfid_maps = sizeof(rfid_plate_maps) / sizeof(rfid_plate_maps[0]);


// ==============================================

// 自动锁屏超时时间 (秒)
#define AUTO_LOCK_TIMEOUT_SECONDS 60

// 过场动画配置
#define SPLASH_FRAME_COUNT 62
#define SPLASH_FRAME_DELAY_US 50000 // 50毫秒


// 外部全局变量，在 camera.c 中定义
extern int Camera_state;
extern int Shot;

// 外部函数声明 (通常在头文件中，但为确保完整性在此列出)
// void lcd_init(void);  // 已在lcd.h中声明
// void show_bmp_any(const char *pathname, int x, int y);  // 已在lcd.h中声明
void close_lcd(void);
void init_touch(void);
void close_touch(void);
int get_slide_xy(int *x, int *y);
void init_camera(void);
void close_Camera(void);
void *Camera(void *arg);
void init_tty(void);
void close_tty(void);
int get_cardid(void);
void *fun_sqlite(void *arg);


// 辅助函数：播放音频文件
void play_audio(const char *audio_path)
{
    char command[256];
    snprintf(command, sizeof(command), "aplay -q %s &", audio_path); // 使用 -q 静音模式并在后台播放
    printf("尝试播放音频: %s\n", audio_path);
    system(command);
    usleep(100000); // 添加短暂延迟，避免设备忙碌
}

// RFID 线程函数
void *rfid_thread(void *arg)
{
    init_tty(); // 初始化串口
    printf("RFID 线程已启动，等待授权卡片...\n");

    while(program_running) // 使用全局标志位控制循环
    {
        int card_id_int = get_cardid(); // 获取整数形式的卡号

        if (card_id_int == -1) { // 未读到卡
            usleep(500000); // 等待0.5秒
            continue;
        }

        char card_id_str[20];
        snprintf(card_id_str, sizeof(card_id_str), "%x", card_id_int);
        printf("RFID: 检测到卡片ID: %s\n", card_id_str);

        const char* plate_number = NULL;
        for (int i = 0; i < num_rfid_maps; i++) {
            if (strcmp(card_id_str, rfid_plate_maps[i].rfid_id) == 0) {
                plate_number = rfid_plate_maps[i].plate_number;
                break;
            }
        }

        if (plate_number == NULL) {
            printf("RFID: 卡号 %s 未授权，操作已忽略。\n", card_id_str);
            play_audio("audio/unauthorized_card.wav");
            sleep(2);
            continue;
        }

        printf("RFID: 授权卡 %s -> 车牌 %s\n", card_id_str, plate_number);

        Shot = ON;
        pthread_mutex_lock(&photo_mutex);
        photo_ready = false;
        pthread_mutex_unlock(&photo_mutex);

        pthread_mutex_lock(&photo_mutex);
        while (!photo_ready) {
            pthread_cond_wait(&photo_cond, &photo_mutex);
        }
        pthread_mutex_unlock(&photo_mutex);

        pthread_mutex_lock(&photo_mutex);
        strncpy(g_rfid_plate, plate_number, sizeof(g_rfid_plate) - 1);
        g_rfid_plate[sizeof(g_rfid_plate) - 1] = '\0';
        g_is_rfid_triggered = true;
        pthread_mutex_unlock(&photo_mutex);
        
        usleep(500000); // 等待半秒，确保摄像头数据稳定
        printf("RFID: 发送统一识别信号给ALPR进程...\n");
        if (kill(alpr_pid, SIGUSR1) == -1) {
            perror("RFID: 发送信号失败");
        } else {
            // 重置标志位，准备等待ALPR响应
            alpr_response_received = false;
            // 等待ALPR处理完成（最长等待5秒）
            int timeout = 0;
            while (!alpr_response_received && timeout < 50) {
                usleep(100000); // 等待100ms
                timeout++;
            }
            if (!alpr_response_received) {
                printf("RFID: ALPR处理超时\n");
                play_audio("audio/timeout.wav");
            }
        }
    }
    close_tty();
    return NULL;
}



// 密码验证函数
bool verify_password(const char *input)
{
    return strcmp(input, "2580") == 0;
}

// 显示密码函数 - 暂时不显示任何内容
void display_password(int input_count, const char *password_input)
{
    // 暂时不显示密码输入区域，改为上滑解锁
    // 可以在这里添加提示文字或其他界面元素
}

// 主函数
int main(int argc, char const *argv[])
{
    // 修改数据库删除路径
    // if (remove("parking.db") == 0) {
    // 修改数据库删除路径
    // if (remove("/mnt/hgfs/qrsgx/18/Car1/parking.db") == 0) {
    printf("[系统初始化]: 尝试删除数据库文件 /mnt/udisk/Carsystem/parking.db\n");
    
    // 删除数据库文件及其相关文件（WAL、SHM等）
    const char *db_files[] = {
        "/mnt/udisk/Carsystem/parking.db",
        "/mnt/udisk/Carsystem/parking.db-wal",
        "/mnt/udisk/Carsystem/parking.db-shm",
        "/mnt/udisk/Carsystem/parking.db-journal"
    };
    
    for (int i = 0; i < 4; i++) {
        if (access(db_files[i], F_OK) == 0) {
            printf("[系统初始化]: 删除文件: %s\n", db_files[i]);
            if (remove(db_files[i]) == 0) {
                printf("[系统初始化]: 文件 %s 删除成功\n", db_files[i]);
            } else {
                perror("[系统初始化警告]: 删除文件失败");
                printf("[系统初始化]: 错误代码: %d\n", errno);
            }
        }
    }

    program_running = 1;

    char password_input[5] = {0};
    int input_count = 0;

    pthread_mutex_init(&photo_mutex, NULL);
    pthread_cond_init(&photo_cond, NULL);

    lcd_init();

    printf("播放过场动画...\n");
    g_suppress_bmp_output = true;
    char splash_path[256];
    for (int i = 1; i <= SPLASH_FRAME_COUNT; i++) {
        snprintf(splash_path, sizeof(splash_path), "splash/splash_%02d.bmp", i);
        show_bmp_any(splash_path, 0, 0);
        usleep(SPLASH_FRAME_DELAY_US);
    }
    g_suppress_bmp_output = false;

    init_touch();
    init_camera();

    alpr_pid = fork();
    if (alpr_pid == -1) {
        perror("fork alpr process failed");
        return -1;
    } else if (alpr_pid == 0) {
        execlp("./alpr", "./alpr", NULL);
        perror("execlp alpr program failed");
        _exit(1);
    } else {
        printf("ALPR 进程已启动，PID: %d\n", alpr_pid);
    }

    pthread_t camera_tid = 0, sqlite_tid = 0, rfid_tid = 0;
    if (pthread_create(&camera_tid, NULL, Camera, NULL) != 0) {
        perror("创建摄像头线程失败");
        goto cleanup;
    }
    if (pthread_create(&sqlite_tid, NULL, fun_sqlite, NULL) != 0) {
        perror("创建SQLite线程失败");
        goto cleanup;
    }
    if (pthread_create(&rfid_tid, NULL, rfid_thread, NULL) != 0) {
        perror("创建RFID线程失败");
        goto cleanup;
    }
    printf("所有后台线程已启动。\n");

    int x, y;
    last_activity_time = time(NULL);

    while(program_running)
    {
        // 显示锁屏界面
        Camera_state = TURN;
        show_bmp_any("ui/lock_screen.bmp", 0, 0);
        printf("已进入锁屏界面。上滑解锁，下滑退出...\n");
        
        // 初始化密码输入
        input_count = 0;
        memset(password_input, 0, sizeof(password_input));
        display_password(input_count, password_input);

        // 锁屏循环 - 改为上滑解锁
        int locked = 1;
        while(locked && program_running)
        {
            fd_set rdfs;
            FD_ZERO(&rdfs);
            FD_SET(touch_fd, &rdfs);
            struct timeval timeout = {1, 0};

            int ret = select(touch_fd + 1, &rdfs, NULL, NULL, &timeout);
            if (ret > 0 && FD_ISSET(touch_fd, &rdfs)) {
                int action = get_slide_xy(&x, &y);
                if (action == DOWN) // 下滑退出
                {
                    printf("收到退出指令...\n");
                    program_running = 0;
                    locked = 0;
                }
                else if (action == UP) // 上滑解锁
                {
                    printf("检测到上滑解锁手势，进入主界面...\n");
                    locked = 0;
                    last_activity_time = time(NULL);
                }
                // 保留点击解锁作为备用方案
                else if (x > 300 && y > 400 && x < 500 && y < 480) // 临时解锁区域
                {
                    printf("临时解锁区域被点击，进入主界面...\n");
                    locked = 0;
                    last_activity_time = time(NULL);
                }
            }
        }

        if (!program_running) {
            continue;
        }

        // 显示主功能界面
        Camera_state = RUN;
        show_bmp_any("ui/car_input.bmp", 640, 0);
        show_bmp_any("ui/car_output.bmp", 640, 160);
        show_bmp_any("ui/button_panel.bmp", 640, 320);
        printf("已进入主功能界面。视频监控已开启。\n");

        int in_menu = 1;
        while(in_menu && program_running)
        {
            if (time(NULL) - last_activity_time > AUTO_LOCK_TIMEOUT_SECONDS)
            {
                printf("自动锁屏超时，返回锁屏界面...\n");
                in_menu = 0;
                continue;
            }
            
            fd_set rdfs;
            FD_ZERO(&rdfs);
            FD_SET(touch_fd, &rdfs);
            struct timeval timeout = {0, 100000};

            int ret = select(touch_fd + 1, &rdfs, NULL, NULL, &timeout);
            if (ret > 0 && FD_ISSET(touch_fd, &rdfs))
            {
                get_slide_xy(&x, &y);
                last_activity_time = time(NULL);
                
                if (x > 640 && x < 800)
                {
                    if (y > 0 && y < 160)
                    {
                        printf("[按钮] 车辆入库被点击...\n");
                        play_audio("audio/car_in_progress.wav");
                        Shot = ON;
                        // ... (省略重复的等待逻辑)
                        kill(alpr_pid, SIGUSR1);
                    }
                    else if (y > 160 && y < 320)
                    {
                        printf("[按钮] 车辆出库被点击...\n");
                        play_audio("audio/car_out_progress.wav");
                        Shot = ON;
                        // ... (省略重复的等待逻辑)
                        kill(alpr_pid, SIGUSR2);
                    }
                    else if (y > 320 && y < 480)
                    {
                        printf("[按钮] 锁屏被点击...\n");
                        in_menu = 0;
                    }
                }
            }
        }
    }

cleanup:
    printf("正在清理资源...\n");
    program_running = 0;
    Camera_state = OUT;

    if (alpr_pid > 0) {
        kill(alpr_pid, SIGTERM);
        waitpid(alpr_pid, NULL, 0);
    }

    if (camera_tid != 0) pthread_cancel(camera_tid);
    if (sqlite_tid != 0) pthread_cancel(sqlite_tid);
    if (rfid_tid != 0) pthread_cancel(rfid_tid);
    
    if (camera_tid != 0) pthread_join(camera_tid, NULL);
    if (sqlite_tid != 0) pthread_join(sqlite_tid, NULL);
    if (rfid_tid != 0) pthread_join(rfid_tid, NULL);

    close_Camera();
    close_touch();
    close_lcd();
    pthread_mutex_destroy(&photo_mutex);
    pthread_cond_destroy(&photo_cond);
    
    printf("所有资源已清理，程序退出。\n");
    return 0;
}