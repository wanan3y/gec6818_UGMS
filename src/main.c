#include "main.h"
#include "carmera.h"
#include "touch.h"
#include "mySQLite.h"
#include "ISO14443A.h"

// 确保全局变量在函数中可见
extern char g_alpr_plate[20];
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
#include <termios.h> // 添加 termios.h 用于串口操作
#include <sys/mman.h> // 添加 mmap 支持
#include <fcntl.h>    // 添加文件控制选项

// ALPR结果监控线程函数 - 按需运行版本
void* alpr_result_monitor(void* arg) {
    // 声明全局变量
    extern pid_t alpr_pid;
    extern volatile bool alpr_response_received;
    extern volatile bool g_is_rfid_triggered;
    extern char g_alpr_plate[20];
    extern pthread_mutex_t alpr_mutex;
    extern pthread_cond_t alpr_cond;
    extern volatile int program_running;
    
    const char* result_file = (const char*)arg;
    char buffer[20] = {0};
    bool active = false;
    
    while (program_running) {
        // 检查是否需要激活ALPR
        pthread_mutex_lock(&alpr_mutex);
        bool should_run = g_is_rfid_triggered;
        pthread_mutex_unlock(&alpr_mutex);
        
        if (should_run && !active) {
            printf("ALPR监控: 激活车牌检测\n");
            active = true;
            // 启动ALPR进程
            alpr_pid = fork();
            if (alpr_pid == 0) {
                execlp("./alpr", "./alpr", result_file, NULL);
                perror("execlp alpr failed");
                _exit(1);
            }
        } else if (!should_run && active) {
            printf("ALPR监控: 停止车牌检测\n");
            kill(alpr_pid, SIGTERM);
            waitpid(alpr_pid, NULL, 0);
            active = false;
        }
        
        if (active) {
            // 读取结果
            int fd = open(result_file, O_RDONLY);
            if (fd != -1) {
                ssize_t bytes = read(fd, buffer, sizeof(buffer)-1);
                close(fd);
                
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    printf("ALPR监控: 检测到车牌: %s\n", buffer);
                    
                    pthread_mutex_lock(&alpr_mutex);
                    strncpy(g_alpr_plate, buffer, sizeof(g_alpr_plate)-1);
                    g_alpr_plate[sizeof(g_alpr_plate)-1] = '\0';
                    
                    // 如果识别到车牌且RFID触发中，标记处理完成
                    if (strlen(g_alpr_plate) > 0 && g_is_rfid_triggered) {
                        alpr_response_received = true;
                        pthread_cond_signal(&alpr_cond);
                    }
                    pthread_mutex_unlock(&alpr_mutex);
                }
            }
        }
        
        usleep(200000); // 降低检测频率
    }
    
    // 清理
    if (active && alpr_pid > 0) {
        kill(alpr_pid, SIGTERM);
        waitpid(alpr_pid, NULL, 0);
    }
    
    return NULL;
}

// [FIX] 为数据库就绪状态添加外部声明
extern pthread_mutex_t db_ready_mutex;
extern pthread_cond_t db_ready_cond;
extern volatile bool db_is_ready;

// [全局变量定义] 用于RFID与摄像头联动验证
char g_rfid_plate[20] = {0}; // 存储由RFID刷卡触发的、期望操作的车牌号
char g_alpr_plate[20] = {0}; // 存储ALPR识别的车牌号
volatile bool g_is_rfid_triggered = false; // 标志位，用于指示当前操作是否由RFID触发
volatile bool g_suppress_bmp_output = false; // [新增] 默认不抑制BMP输出

// 密码解锁相关变量
#define PASSWORD "1234"
#define MAX_INPUT_LENGTH 10
char input_password[MAX_INPUT_LENGTH] = {0};
int password_index = 0;
bool is_locked = true; // 系统启动时默认锁定



volatile int alpr_error_detected = 0; // ALPR 错误检测标志位
volatile bool alpr_response_received = false; // ALPR响应标志位

// ================= 全局变量定义 =================
int lcd_fd;         // LCD 文件描述符
int *mmap_p;        // LCD 内存映射指针
int touch_fd;       // 触摸屏文件描述符
int tty_fd;         // RFID (串口) 文件描述符
bool cardOn;        // RFID 卡片状态
struct termios original_termios; // 保存原始串口设置
volatile int program_running; // 控制程序和线程循环的标志位
pthread_mutex_t photo_mutex; // 照片处理互斥锁
pthread_cond_t photo_cond;   // 照片处理条件变量
volatile bool photo_ready;    // 照片是否准备好的标志
pthread_t alpr_monitor_tid;   // ALPR结果监控线程ID

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
// void *fun_sqlite(void *arg); // [FIX] 删除，此声明已在 mySQLite.h 中


// 辅助函数：播放音频文件
// 显示数字按钮函数
void show_number_button(int number, int x, int y) {
    char path[64];
    snprintf(path, sizeof(path), "./fonts/%d.bmp", number);
    show_bmp_any(path, x, y);
}

// 显示密码界面
void show_password_interface() {
    // 显示背景图片
    show_bmp_any("ui/beijing.bmp", 0, 0);
    
    // 显示退出按钮
    show_bmp_any("ui/exit.bmp", 0, 400);  // 退出按钮在左下角
    
    // 数字键盘配置
    int start_x = 280;         // 稍微向左移动一点
    int start_y = 140;         // 稍微向上移动一点
    int button_spacing = 100;  // 进一步增大按钮间距
    int button_size = 50;      // 增大按钮尺寸    // 显示标题
    show_bmp_any("./fonts/title.bmp", 300, 20);  // 添加标题提示用户输入密码
    
    // 显示数字键盘
    for(int i = 1; i <= 9; i++) {
        int row = (i-1) / 3;
        int col = (i-1) % 3;
        int button_x = start_x + col * button_spacing;
        int button_y = start_y + row * button_spacing;
        
        // 显示数字
        show_number_button(i, button_x, button_y);
    }
}

// 处理密码输入
void handle_password_input(int x, int y) {
    int start_x = 280;         // 与显示函数对应
    int start_y = 140;
    int button_spacing = 100;  // 使用更大的间距
    int button_width = 80;     // 显著增大触摸响应区域
    int button_height = 80;    // 显著增大触摸响应区域
    
    printf("触摸坐标: x=%d, y=%d\n", x, y);
    
    // 检查是否点击了退出按钮 (0-80, 400-480)
    if (x >= 0 && x < 80 && y >= 400 && y < 480) {
        printf("[按钮] 退出程序被点击...\n");
        show_bmp_any("ui/exit2.bmp", 0, 400); // 显示按下状态
        sleep(1);
        show_bmp_any("ui/beijing.bmp", 0, 0); // 显示清屏背景
        sleep(1);
        program_running = 0; // 触发程序退出
        return;
    }
    
    // 检查点击的是哪个数字
    for(int i = 1; i <= 9; i++) {
        int row = (i-1) / 3;
        int col = (i-1) % 3;
        int button_x = start_x + col * button_spacing;
        int button_y = start_y + row * button_spacing;
        
        // 检查点击是否在按钮区域内
        int click_margin = 15;  // 增加点击判定边距
        if(x >= (button_x - click_margin) && x < (button_x + button_width + click_margin) &&
           y >= (button_y - click_margin) && y < (button_y + button_height + click_margin)) {
            printf("检测到按钮 %d 被点击（坐标：x=%d, y=%d，按钮位置：x=%d, y=%d）\n", 
                   i, x, y, button_x, button_y);
            // 如果密码长度未达到上限
            if(password_index < MAX_INPUT_LENGTH - 1) {
                input_password[password_index++] = '0' + i;
                input_password[password_index] = '\0';
                
                // 显示输入的星号
                for(int j = 0; j < password_index; j++) {
                    // 在屏幕上显示星号，可以使用特定的图片或直接绘制
                    show_bmp_any("./fonts/star.bmp", 300 + j * 20, 50);
                }
                
                // 检查密码是否正确
                if(password_index == strlen(PASSWORD)) {
                    if(strcmp(input_password, PASSWORD) == 0) {
                        printf("密码验证成功！\n");
                        is_locked = false;  // 解锁成功
                        // 清空输入
                        memset(input_password, 0, MAX_INPUT_LENGTH);
                        password_index = 0;
                        return; // 立即返回以进入主界面
                    } else {
                        printf("密码错误！\n");
                        // 显示错误提示
                        show_bmp_any("./fonts/error.bmp", 300, 200);
                        
                        // 创建子进程来处理延迟清除
                        pid_t pid = fork();
                        if (pid == 0) {
                            // 子进程
                            sleep(2);
                            // 清除错误提示和密码星号
                            show_bmp_any("ui/beijing.bmp", 0, 0);
                            show_bmp_any("./fonts/title.bmp", 300, 20);
                            
                            // 重新显示数字键盘
                            int start_x = 280;
                            int start_y = 140;
                            int button_spacing = 100;
                            for(int i = 1; i <= 9; i++) {
                                int row = (i-1) / 3;
                                int col = (i-1) % 3;
                                int button_x = start_x + col * button_spacing;
                                int button_y = start_y + row * button_spacing;
                                show_number_button(i, button_x, button_y);
                            }
                            _exit(0);
                        }
                        
                        // 清空密码输入
                        memset(input_password, 0, MAX_INPUT_LENGTH);
                        password_index = 0;
                    }
                }
            }
            break;
        }
    }
}

void play_audio(const char *audio_path)
{
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork for aplay failed");
        return;
    } else if (pid == 0) {
        // 子进程
        char command[256];
        snprintf(command, sizeof(command), "aplay -q %s", audio_path); // 不再使用 &，因为子进程会立即退出
        execlp("aplay", "aplay", "-q", (char *)audio_path, NULL);
        perror("execlp aplay failed"); // 如果execlp失败
        _exit(1); // 子进程退出
    } else {
        // 父进程，不等待子进程
        // printf("尝试播放音频: %s\n", audio_path);
        // usleep(100000); // 添加短暂延迟，避免设备忙碌
    }
}

// RFID 线程函数
void *rfid_thread(void *arg)
{
    init_tty(); // 初始化串口
    // 保存原始串口设置
    if (tty_fd > 0) {
        tcgetattr(tty_fd, &original_termios);
        printf("RFID: 原始串口设置已保存\n");
    }
    printf("RFID 线程已启动，等待授权卡片...\n");

    // [FIX] 等待数据库线程准备就绪
    pthread_mutex_lock(&db_ready_mutex);
    while (!db_is_ready) {
        pthread_cond_wait(&db_ready_cond, &db_ready_mutex);
    }
    pthread_mutex_unlock(&db_ready_mutex);

    if (db == NULL) {
        printf("RFID: 检测到数据库未就绪，线程退出。\n");
        return NULL;
    }

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

        // =================== [代码重构开始] ===================

        // 1. 开始新操作前，重置ALPR响应标志
        pthread_mutex_lock(&alpr_mutex);
        alpr_response_received = false;
        pthread_mutex_unlock(&alpr_mutex);

        // 2. 触发拍照
        Shot = ON;

        // 3. 等待摄像头线程完成拍照 (带超时)
        pthread_mutex_lock(&photo_mutex);
        photo_ready = false; // 重置标志位
        struct timespec ts_photo;
        clock_gettime(CLOCK_REALTIME, &ts_photo);
        ts_photo.tv_sec += 3; // 3秒超时

        int photo_wait_ret = 0;
        while (!photo_ready && photo_wait_ret == 0) {
            photo_wait_ret = pthread_cond_timedwait(&photo_cond, &photo_mutex, &ts_photo);
        }
        pthread_mutex_unlock(&photo_mutex);

        if (photo_wait_ret == ETIMEDOUT) {
            printf("RFID: 等待摄像头拍照超时！\n");
            Shot = OFF; // 确保重置Shot标志
            play_audio("audio/timeout.wav");
            continue; // 跳过本次流程
        }

        // 4. 拍照成功，设置RFID联动标志，准备与ALPR交互
        pthread_mutex_lock(&photo_mutex);
        strncpy(g_rfid_plate, plate_number, sizeof(g_rfid_plate) - 1);
        g_rfid_plate[sizeof(g_rfid_plate) - 1] = '\0';
        g_is_rfid_triggered = true; // ★ 标记操作由RFID触发
        pthread_mutex_unlock(&photo_mutex);
        
        // 5. 判断是入库还是出库，并发送相应信号
        int signal_to_send;
        const char* direction_str;
        if (is_car_in_db(plate_number)) {
            direction_str = "出库";
            signal_to_send = SIGUSR2; // 出库信号
        } else {
            direction_str = "入库";
            signal_to_send = SIGUSR1; // 入库信号
        }
        
        printf("RFID: 车辆 %s 触发自动%s...\n", plate_number, direction_str);
        printf("RFID: 照片已生成，发送信号给ALPR进程...\n");

        if (kill(alpr_pid, signal_to_send) == -1) {
            perror("RFID: 发送信号失败");
        } else {
            // 6. [统一逻辑] 无论入库出库，都等待ALPR处理完成 (带超时)
            pthread_mutex_lock(&alpr_mutex);
            struct timespec ts_alpr;
            clock_gettime(CLOCK_REALTIME, &ts_alpr);
            ts_alpr.tv_sec += 5; // 5秒超时

            int alpr_wait_ret = 0;
            while (!alpr_response_received && alpr_wait_ret == 0) {
                alpr_wait_ret = pthread_cond_timedwait(&alpr_cond, &alpr_mutex, &ts_alpr);
            }
            pthread_mutex_unlock(&alpr_mutex);

            if (alpr_wait_ret == ETIMEDOUT) {
                printf("RFID: 等待ALPR处理超时!\n");
                play_audio("audio/timeout.wav");
            } else {
                printf("RFID: 收到ALPR处理完成的响应。\n");
                
                // 获取ALPR识别结果
                pthread_mutex_lock(&alpr_mutex);
                char detected_plate[20];
                strncpy(detected_plate, g_alpr_plate, sizeof(detected_plate));
                pthread_mutex_unlock(&alpr_mutex);
                
                // 严格车卡一致性校验
                printf("[DEBUG] 车卡校验: RFID车牌:%s vs ALPR车牌:%s\n", 
                       g_rfid_plate, detected_plate);
                
                if (strlen(detected_plate) == 0) {
                    printf("RFID: 错误 - ALPR未识别到车牌!\n");
                    if (access("audio/plate_not_detected.wav", F_OK) == 0) {
                        play_audio("audio/plate_not_detected.wav");
                    } else {
                        printf("警告: 音频文件 audio/plate_not_detected.wav 不存在\n");
                    }
                    continue;
                }
                
                if (strcmp(g_rfid_plate, detected_plate) != 0) {
                    printf("RFID: 错误 - 车卡不一致! RFID车牌:%s != 识别车牌:%s\n", 
                           g_rfid_plate, detected_plate);
                    if (access("audio/card_mismatch.wav", F_OK) == 0) {
                        play_audio("audio/card_mismatch.wav");
                    } else {
                        printf("警告: 音频文件 audio/card_mismatch.wav 不存在\n");
                    }
                    continue;
                }
                
                printf("RFID: 成功 - 车卡一致验证通过(RFID:%s == ALPR:%s)\n",
                       g_rfid_plate, detected_plate);
                
                // 根据数据库状态执行入库/出库
                if (is_car_in_db(detected_plate)) {
                    printf("RFID: 执行出库操作\n");
                    // 出库逻辑...
                } else {
                    printf("RFID: 执行入库操作\n");
                    // 入库逻辑...
                }
            }
        }

        // 7. [增强修复] 更彻底的状态重置流程
        printf("[DEBUG] RFID: 开始完整状态重置流程...\n");
        
        // 阶段1: 清理软件状态
        pthread_mutex_lock(&photo_mutex);
        g_is_rfid_triggered = false;
        memset(g_rfid_plate, 0, sizeof(g_rfid_plate));
        pthread_mutex_unlock(&photo_mutex);
        printf("[DEBUG] RFID: 软件状态已重置\n");
        
        // 阶段2: 强制硬件复位
        printf("[DEBUG] RFID: 执行强制硬件复位...\n");
        close_tty();
        usleep(500000); // 500ms延迟确保完全关闭
        
        // 阶段3: 重新初始化硬件
        init_tty();
        if (tty_fd > 0) {
            // 阶段4: 彻底清理串口
            tcflush(tty_fd, TCIOFLUSH);
            tcsetattr(tty_fd, TCSANOW, &original_termios); // 恢复原始串口设置
            printf("[DEBUG] RFID: 硬件复位成功(fd=%d)\n", tty_fd);
            
            // 阶段5: 验证硬件状态
            cardOn = false; // 强制重置状态
            usleep(100000); // 100ms等待硬件稳定
            printf("[DEBUG] RFID: 硬件状态验证: cardOn=%d\n", cardOn);
        } else {
            printf("[ERROR] RFID: 硬件复位失败!\n");
        }
        
        // 阶段6: 同步ALPR状态
        pthread_mutex_lock(&alpr_mutex);
        alpr_response_received = false;
        alpr_error_detected = 0;
        memset(g_alpr_plate, 0, sizeof(g_alpr_plate)); // 清空识别车牌
        pthread_mutex_unlock(&alpr_mutex);
        printf("[DEBUG] RFID: ALPR状态已同步\n");
        
        printf("[DEBUG] RFID: 完整状态重置完成\n");
        printf("RFID: 系统已完全重置，准备接收下一次刷卡\n");

        // =================== [代码重构结束] ===================

        sleep(2); // 适当延长延时确保稳定性
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

    pthread_mutex_init(&alpr_mutex, NULL); // 初始化ALPR互斥锁
    pthread_cond_init(&alpr_cond, NULL);   // 初始化ALPR条件变量

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

    // 创建共享文件用于ALPR结果传递
    int fd = open("/tmp/alpr_result", O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("创建ALPR结果文件失败");
        return -1;
    }
    ftruncate(fd, 20); // 预留车牌号空间
    close(fd);

    alpr_pid = fork();
    if (alpr_pid == -1) {
        perror("fork alpr process failed");
        return -1;
    } else if (alpr_pid == 0) {
        // 子进程 - ALPR程序
        execlp("./alpr", "./alpr", "/tmp/alpr_result", NULL);
        perror("execlp alpr program failed");
        _exit(1);
    } else {
        printf("ALPR 进程已启动，PID: %d\n", alpr_pid);
        
        // 启动ALPR结果监控线程
        pthread_t alpr_monitor_tid;
        if (pthread_create(&alpr_monitor_tid, NULL, alpr_result_monitor, "/tmp/alpr_result") != 0) {
            perror("创建ALPR监控线程失败");
            return -1;
        }
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
        if (is_locked) {
            // 显示锁屏界面
            Camera_state = TURN;
            show_password_interface();
            printf("已进入密码锁屏界面\n");
            
            // 处理触摸事件
            while(is_locked && program_running) {
                int touch_state = get_slide_xy(&x, &y);
                printf("触摸状态: %d, 坐标: x=%d, y=%d\n", touch_state, x, y);
                
                // 不再检查touch_state，直接处理坐标
                if (x >= 0 && y >= 0) {  // 只要坐标有效就处理
                    handle_password_input(x, y);
                    // 添加调试信息
                    printf("处理触摸事件：x=%d, y=%d\n", x, y);
                    printf("当前输入的密码: %s (长度: %d)\n", input_password, password_index);
                    
                    if (!is_locked) {
                        printf("密码正确，解锁成功！\n");
                        // 密码正确，显示主界面
                        show_bmp_any("./ui/main.bmp", 0, 0);
                        Camera_state = ON;
                        last_activity_time = time(NULL);
                        break;
                    }
                }
                usleep(100000); // 增加延迟时间
            }
            
            if (!program_running) {
                continue;
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
                        show_bmp_any("ui/ruku2.bmp", 640, 0); // 显示按下状态
                        play_audio("audio/car_in_progress.wav");
                        Shot = ON;
                        // ... (省略重复的等待逻辑)
                        kill(alpr_pid, SIGUSR1);
                        sleep(2);
                        show_bmp_any("ui/car_input.bmp", 640, 0); // 恢复原始状态
                    }
                    else if (y > 160 && y < 320)
                    {
                        printf("[按钮] 车辆出库被点击...\n");
                        show_bmp_any("ui/chuku2.bmp", 640, 160); // 显示按下状态
                        play_audio("audio/car_out_progress.wav");
                        Shot = ON;
                        // ... (省略重复的等待逻辑)
                        kill(alpr_pid, SIGUSR2);
                        sleep(2);
                        show_bmp_any("ui/car_output.bmp", 640, 160); // 恢复原始状态
                    }
                    else if (y > 320 && y < 400)
                    {
                        printf("[按钮] 锁屏被点击...\n");
                        show_bmp_any("ui/suoping2.bmp", 640, 320); // 显示按下状态
                        play_audio("audio/lock.wav"); // 播放锁屏音效
                        sleep(1);
                        is_locked = true;  // 设置锁定状态
                        Camera_state = TURN; // 关闭视频显示
                        in_menu = 0;  // 退出主菜单循环
                        memset(input_password, 0, MAX_INPUT_LENGTH); // 清空密码缓冲
                        password_index = 0; // 重置密码索引
                    }
                    else if (y > 400 && y < 480)
                    {
                        printf("[按钮] 退出程序被点击...\n");
                        show_bmp_any("ui/exit2.bmp", 720, 400); // 显示按下状态
                        play_audio("audio/exit.wav"); // 播放退出音效
                        sleep(1);
                        printf("用户主动退出程序...\n");
                        program_running = 0; // 触发程序退出
                        break; // 退出循环
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

    // 清理ALPR监控线程
    if (alpr_monitor_tid != 0) {
        pthread_cancel(alpr_monitor_tid);
        pthread_join(alpr_monitor_tid, NULL);
    }

    // 删除ALPR结果文件
    if (remove("/tmp/alpr_result") == -1) {
        perror("删除ALPR结果文件失败");
    }

    close_Camera();
    close_touch();
    close_lcd();
    pthread_mutex_destroy(&photo_mutex);
    pthread_cond_destroy(&photo_cond);
    
    printf("所有资源已清理，程序退出。\n");
    return 0;
}