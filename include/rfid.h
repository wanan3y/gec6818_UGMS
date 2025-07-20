#ifndef RFID_H
#define RFID_H

#include <stdbool.h>
#include <stdint.h>

// RFID 相关的函数声明
void init_tty(void);
void request_card(int fd);
int get_id(int fd);
int get_cardid(void);
void close_tty(void);

// 外部变量声明
extern bool cardOn;
extern int tty_fd;

#endif