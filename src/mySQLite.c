#include "mySQLite.h"
#include <math.h> // For ceil
#include <time.h> // For localtime, strftime

extern void play_audio(const char *audio_path); // 声明 play_audio 函数
extern volatile int alpr_error_detected; // 声明 ALPR 错误检测标志位
extern volatile bool alpr_response_received; // 声明 ALPR 响应标志位
extern int Camera_state; // 声明 Camera_state 变量

bool first = true;
sqlite3 *db = NULL; // 数据库的操作句柄
char *err;          // 报错信息
int fifoIn;
int fifoOut;

// 每当你使用SELECT得到N条记录时，就会自动调用N次以下函数
// 参数：
// arg: 用户自定义参数
// len: 列总数
// col_val: 每一列的值
// col_name: 每一列的名称（标题)
int showDB(void *arg, int len, char **col_val, char **col_name)
{
    printf("[DEBUG]: showDB被调用，len=%d\n", len);
    
    // 显示标题(只显示一次)
    if(first)
    {
        printf("\n");
        for(int i=0; i<len; i++)
        {
            printf("%s\t\t", col_name[i]);
        }
        printf("\n==============");
        printf("==============\n");
        first = false;
    }

    // 显示内容(一行一行输出)
    for(int i=0; i<len; i++)
    {
        printf("%s\t", col_val[i]);
    }
    printf("\n");

    // 返回0: 继续针对下一条记录调用本函数
    // 返回非0: 停止调用本函数
    return 0;
}

int checEmpty(void *arg, int len, char **col_val, char **col_name)
{
	printf("[DEBUG]: checEmpty被调用，len=%d, col_val[0]=%s\n", len, col_val[0]);
	if(arg != NULL && col_val[0] != NULL)
		(*(int *)arg) = atoi(col_val[0]); // 将字符串转换为整数并赋值
	return 0;
}

int getTime(void *arg, int len, char **col_val, char **col_name)
{
	if(arg != NULL)
		*(time_t *)arg = atol(col_val[0]);
	return 0;
}

// 辅助函数：计算停车费用 (已修改为每秒1元)
float calculate_fee(long parking_duration_seconds)
{
    // 计费标准：每秒1.0元
    const float CHARGE_PER_SECOND = 1.0f;

    if (parking_duration_seconds < 0) {
        return 0.0f; // 避免负数时长
    }

    float total_fee = parking_duration_seconds * CHARGE_PER_SECOND;

    return total_fee;
}

// 辅助函数：检查车牌是否在数据库中
int is_car_in_db(const char *plate_num)
{
    char SQL[100];
    int count = 0;
    snprintf(SQL, 100, "SELECT COUNT(*) FROM info WHERE 车牌='%s';", plate_num);
    printf("[DEBUG]: 执行SQL查询: %s\n", SQL);
    int result = sqlite3_exec(db, SQL, checEmpty, &count, &err);
    if (result != SQLITE_OK) {
        printf("[DEBUG]: SQL查询失败: %s\n", err);
        sqlite3_free(err);
        err = NULL;
        return 0;
    }
    printf("DEBUG: is_car_in_db(%s) returned %d (count=%d)\n", plate_num, count > 0, count);
    return count > 0;
}

// 数据库核心处理线程 - 已重构
// 使用 select I/O 多路复用模型统一处理入库和出库请求
void *fun_sqlite(void *arg)
{
    printf("[SQLite 线程启动成功]....\n");

    // 1. 确保管道文件存在
    if(access("/tmp/LPR2SQLitIn", F_OK) != 0) {
        mkfifo("/tmp/LPR2SQLitIn", 0666);
    }
    if(access("/tmp/LPR2SQLitOut", F_OK) != 0) {
        mkfifo("/tmp/LPR2SQLitOut", 0666);
    }

    // 2. 以非阻塞模式打开管道文件，以配合 select
    fifoIn = open("/tmp/LPR2SQLitIn", O_RDWR | O_NONBLOCK);
    fifoOut = open("/tmp/LPR2SQLitOut", O_RDWR | O_NONBLOCK);
    if (fifoIn == -1 || fifoOut == -1) {
        perror("打开管道文件失败");
        pthread_exit(NULL);
    }

    // 3. 打开或创建数据库
    // 修改数据库打开路径
    // int ret = sqlite3_open_v2("/mnt/hgfs/qrsgx/18/Car1/parking.db", &db, ...);
    printf("[SQLite]: 尝试打开/创建数据库: /mnt/udisk/Carsystem/parking.db\n");
    int ret = sqlite3_open_v2("/mnt/udisk/Carsystem/parking.db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if(ret != SQLITE_OK) {
        printf("创建/打开数据库失败: %s\n", sqlite3_errmsg(db));
        printf("[SQLite]: 错误代码: %d\n", ret);
        pthread_exit(NULL);
    } else {
        printf("[SQLite]: 数据库打开/创建成功\n");
    }

    // 4. 创建车辆信息表 (如果不存在)
    // 4. 创建车辆信息表 (如果不存在), 注意：时间字段必须为INTEGER类型
    int create_result = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS info (车牌 TEXT PRIMARY KEY, 时间 INTEGER);", NULL, NULL, &err);
    if (create_result != SQLITE_OK) {
        printf("[DEBUG]: 创建表失败: %s\n", err);
        sqlite3_free(err);
        err = NULL;
    } else {
        printf("[DEBUG]: 表创建成功\n");
    }

    // 测试查询，确保数据库正常工作
    printf("[DEBUG]: 测试数据库查询...\n");
    int count = 0;
    int test_result = sqlite3_exec(db, "SELECT COUNT(*) FROM info;", checEmpty, &count, &err);
    if (test_result != SQLITE_OK) {
        printf("[DEBUG]: 测试查询失败: %s\n", err);
        sqlite3_free(err);
        err = NULL;
    } else {
        printf("[DEBUG]: 测试查询成功，当前数据库中有 %d 条记录\n", count);
    }

    printf("[SQLite 准备就绪]，等待车牌信息...\n");

    // 5. 主循环：使用 select 监听两个管道
    fd_set read_fds;
    char carplate[20];
    char SQL[200];

    while(program_running)
    {
        FD_ZERO(&read_fds);
        FD_SET(fifoIn, &read_fds);
        FD_SET(fifoOut, &read_fds);

        int max_fd = (fifoIn > fifoOut) ? fifoIn : fifoOut;

        // select 会在此阻塞，直到任一管道有数据可读
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            printf("select 错误\n");
            break;
        }

        // ======== 处理入库请求 ========
        if (FD_ISSET(fifoIn, &read_fds))
        {
            bzero(carplate, 20);
            read(fifoIn, carplate, 20);

            if (strlen(carplate) == 0) continue; // 忽略空消息

            printf("--- 收到入库请求: %s ---\n", carplate);

            if (strcmp(carplate, "err") == 0) {
                printf("ALPR 识别失败 (入库)!\n");
                play_audio("audio/recognition_failed.wav");
                alpr_error_detected = 1;
                continue;
            }

            // 只要车牌已在库，提示并return，不再自动出库
            if (is_car_in_db(carplate)) {
                printf("车辆 %s 已在库，入库操作忽略。\n", carplate);
                printf("[DEBUG]: 当前数据库中的所有车辆:\n");
                first = true;
                int query_result = sqlite3_exec(db, "SELECT * FROM info;", showDB, NULL, &err);
                if (query_result != SQLITE_OK) {
                    printf("[DEBUG]: 查询数据库失败: %s\n", err);
                    sqlite3_free(err);
                    err = NULL;
                } else {
                    printf("[DEBUG]: 数据库查询成功，显示完成\n");
                }
                play_audio("audio/car_already_in.wav"); // 可选
                alpr_response_received = true; // 即使车辆已在库，也要通知RFID线程
                continue;
            }
            // 车辆不在库，执行入库流程
            printf("车辆 %s 不在库，执行入库流程...\n", carplate);
            snprintf(SQL, sizeof(SQL), "INSERT INTO info (车牌, 时间) VALUES ('%s', %ld);", carplate, time(NULL));
            printf("[DEBUG]: 执行入库SQL: %s\n", SQL);
            int insert_result = sqlite3_exec(db, SQL, NULL, NULL, &err);
            if (insert_result != SQLITE_OK) {
                printf("数据库插入失败: %s\n", err);
                printf("[DEBUG]: 插入错误代码: %d\n", insert_result);
                sqlite3_free(err); err = NULL;
            } else {
                printf("车辆 %s 入库成功！\n", carplate);
                play_audio("audio/car_in_success.wav");
            }

            // 无论出入库，都显示一下当前车库情况
            first = true; // 允许下次打印表格头
            sqlite3_exec(db, "SELECT * FROM info;", showDB, NULL, &err);
            alpr_response_received = true; // 设置响应标志
        }

        // ======== 处理手动出库请求(此逻辑保留，以防万一) ========
        if (FD_ISSET(fifoOut, &read_fds))
        {
            bzero(carplate, 20);
            read(fifoOut, carplate, 20);

            if (strlen(carplate) == 0) continue; // 忽略空消息

            printf("--- 收到出库请求: %s ---\n", carplate);

            // [逻辑增强] 检查是否由RFID触发，并进行验证
            pthread_mutex_lock(&photo_mutex);
            if (g_is_rfid_triggered) {
                if (strcmp(carplate, g_rfid_plate) != 0) {
                    printf("验证失败: 摄像头识别车牌[%s]与刷卡车牌[%s]不匹配!\n", carplate, g_rfid_plate);
                    play_audio("audio/mismatch.wav"); // 播放不匹配提示音
                    g_is_rfid_triggered = false; // 重置标志位
                    pthread_mutex_unlock(&photo_mutex);
                    continue; // 中断本次操作
                }
                printf("RFID联动验证成功: %s\n", carplate);
                g_is_rfid_triggered = false; // 重置标志位
            }
            pthread_mutex_unlock(&photo_mutex);

            if (strcmp(carplate, "err") == 0) {
                printf("ALPR 识别失败 (出库)!\n");
                play_audio("audio/recognition_failed.wav");
                alpr_error_detected = 1;
                continue;
            }

            if (!is_car_in_db(carplate)) {
                printf("出库失败: 车辆 %s 不在库中。\n", carplate);
                play_audio("audio/car_not_found.wav");
            } else {
                // 1. 获取入库时间
                // time_t entry_time = 1420082293; // 2015-01-01 03:18:13 (删除此行硬编码)
                time_t entry_time; // 添加变量声明
                snprintf(SQL, sizeof(SQL), "SELECT 时间 FROM info WHERE 车牌='%s';", carplate);
                sqlite3_exec(db, SQL, getTime, &entry_time, &err);

                // 2. 计算费用并打印账单
                time_t exit_time = time(NULL);
                long parking_duration = exit_time - entry_time;
                float fee = calculate_fee(parking_duration);

                char entry_time_str[30], exit_time_str[30];
                strftime(entry_time_str, sizeof(entry_time_str), "%Y-%m-%d %H:%M:%S", localtime(&entry_time));
                strftime(exit_time_str, sizeof(exit_time_str), "%Y-%m-%d %H:%M:%S", localtime(&exit_time));

                printf("\n==================== 停车账单 ====================\n");
                printf("  车牌号: %s\n", carplate);
                printf("  入库时间: %s\n", entry_time_str);
                printf("  出库时间: %s\n", exit_time_str);
                printf("  停车时长: %ld 秒\n", parking_duration);
                printf("  停车费用: %.2f 元\n", fee);
                printf("==================================================\n\n");

                // 3. 从数据库删除记录
                snprintf(SQL, sizeof(SQL), "DELETE FROM info WHERE 车牌='%s';", carplate);
                sqlite3_exec(db, SQL, NULL, NULL, &err);
                printf("车辆 %s 出库成功！\n", carplate);
                play_audio("audio/car_out_success.wav");

                // 4. 显示剩余车辆
                first = true; // 允许下次打印表格头
                sqlite3_exec(db, "SELECT * FROM info;", showDB, NULL, &err);
            }
        }
    }

    // 6. 清理资源
    printf("[SQLite 线程正在关闭]...\n");
    close(fifoIn);
    close(fifoOut);
    sqlite3_close(db);
    pthread_exit(NULL);
}