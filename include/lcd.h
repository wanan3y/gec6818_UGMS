#ifndef LCD_H
#define LCD_H

//LCD显示屏初始化
int lcd_init(void);
//LCD显示屏的关闭
void close_lcd(void);
//在任意位置显示任意大小的BMP图片：BMP图片名称、x坐标，y坐标
void show_bmp_any(char *bmp_name, int r_x, int r_y);
extern int lcd_fd;		//LCD文件描述符
extern int *mmap_p;		//LCD内存映射

#endif