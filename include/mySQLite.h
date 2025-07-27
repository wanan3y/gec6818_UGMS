#ifndef __MYSQLITE_H
#define __MYSQLITE_H

#include <sqlite3.h>
#include <stdbool.h>

// [FIX] 声明全局数据库句柄和状态检查函数
extern sqlite3 *db;
int is_car_in_db(const char *plate_num);
void *fun_sqlite(void *arg); // [FIX] 确保函数原型正确


#endif