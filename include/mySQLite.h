#ifndef __MYSQLITE_H
#define __MYSQLITE_H

#include "sqlite3.h"
#include "main.h"

// 数据库主线程函数
void *fun_sqlite(void *arg);

// 检查车辆是否在数据库中的辅助函数
int is_car_in_db(const char *plate_num);

#endif