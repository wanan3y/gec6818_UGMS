#include "lcd.h"

// 全局变量
int LCD_fd = 0;     // 显示屏的文件描述符
int *mmap_LCD;      // LCD内存映射变量


// 函数的定义
// LCD的初始化功能函数
void LCD_init(void)
{
    // 1）打开LCD显示屏：open
    LCD_fd = open("/dev/fb0", O_RDWR);
    if(LCD_fd == -1)
    {
        perror("open LCD");
        return;
    }
    // 对LCD显示屏进行内存映射
    mmap_LCD = mmap(NULL, 800*480*4, PROT_READ | PROT_WRITE, MAP_SHARED, LCD_fd, 0);
    if(mmap_LCD == NULL)
    {
        perror("mmap LCD");
        return;
    }

    printf("LCD显示屏初始化成功\n");

}

// 关闭LCD显示屏
void LCD_close(void)
{
    // 解除内存映射
    munmap(mmap_LCD, 800*480*4);

    // 关闭LCD显示屏close
    close(LCD_fd);
}

// 显示图片: 传递图片名称
void show_bmp(char *bmpname)
{
    int BMP_fd = open(bmpname, O_RDWR);
    if(BMP_fd == -1)
    {
        perror("open BMP");
        return ;
    }
    printf("图片: %s 打开成功\n", bmpname);

    // 2）读取BMP图片的数据
    //读取54字节的信息头
    char head[54];
    //读取数据头信息
    read(BMP_fd, head, sizeof(head));
    //获取图片的宽度和高度
    int width = head[18] |  head[19] << 8 |  head[20] << 16 |  head[21] << 24;
    int height =  head[22] |  head[23] << 8 |  head[24] << 16 |  head[25] << 24;
    printf("BMP: %d*%d\n", width, height);

    // 读取BGR颜色数据
    char bgr[800*480*3];    // b\g\r\b\g\r....
    read(BMP_fd, bgr, sizeof(bgr));

    // 拼接获取到颜色数据
    int color[800*480] = {0};   // 存放每一个像素点的数据
    for (int i = 0; i < 800*480*3; i+=3)   // 循环赋值每一个像素点的数值
    {
        color[i/3] = bgr[i+2] << 16 | bgr[i+1] << 8 | bgr[i];        // RGB颜色数据
    }
    
    // 将颜色数据进行上下翻转
    int BMP_buf[800*480];       //存放翻转过后的颜色数据
    for (int y = 0; y < 480; y++)
    {
        for (int x = 0; x < 800; x++)
        {   
            //: y*800+x 和 (479-y)*800+x 进行交换
            BMP_buf[y*800+x] = color[(479-y)*800+x];

        }
        
    }
    // 3）将颜色数据写入到LCD显示屏：write
    write(LCD_fd, BMP_buf, sizeof(BMP_buf));
    write(LCD_fd, BMP_buf, sizeof(BMP_buf));    // 写入两次，解决显示瑕疵问题


    // 进行LCD光标偏移，让显示屏可再次去显示图片
    lseek(LCD_fd, 0, SEEK_SET);     // 基于文件开头偏移0
  
    close(BMP_fd);

}

// 显示图片: 参数：图片名称、x坐标、y坐标
void show_bmp_any(char *bmpname, int r_x, int r_y)
{
    int BMP_fd = open(bmpname, O_RDWR);
    if(BMP_fd == -1)
    {
        perror("open BMP");
        return ;
    }
    printf("图片: %s 打开成功\n", bmpname);

    // 2）读取BMP图片的数据
    //读取54字节的信息头
    char head[54];
    //读取数据头信息
    read(BMP_fd, head, sizeof(head));
    //获取图片的宽度和高度
    int width = head[18] |  head[19] << 8 |  head[20] << 16 |  head[21] << 24;
    int height =  head[22] |  head[23] << 8 |  head[24] << 16 |  head[25] << 24;
    // 错误处理
    if (r_x + width > 800 || r_y + height > 480)        // 越界
    {
        printf("图片显示越界\n");
        return;
    }
    if(width % 4 != 0)  // 图片格式
    {
        printf("图片宽度需要为4的倍数\n");
        return;
    }
    printf("BMP: %d*%d\n", width, height);

    // 读取BGR颜色数据
    char bgr[width*height*3];    // b\g\r\b\g\r....
    read(BMP_fd, bgr, sizeof(bgr));

    // 拼接获取到颜色数据
    int color[width*height];   // 存放每一个像素点的数据
    for (int i = 0; i < width*height*3; i+=3)   // 循环赋值每一个像素点的数值
    {
        color[i/3] = bgr[i+2] << 16 | bgr[i+1] << 8 | bgr[i];        // RGB颜色数据
    }
    
    // 将颜色数据进行上下翻转，并赋值到虚拟内存中
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {   
            //: y*800+x 和 (479-y)*800+x 进行交换
            // 直接将数据写入到虚拟内存: LCD显示屏也有对显示
            mmap_LCD[(y + r_y)*800+x+r_x] = color[(height-1-y)*width+x];
        }   
    }
    close(BMP_fd);
}

// 显示图片: 参数：图片名称、x坐标、y坐标
void display_bmp(char *bmpname, int r_x, int r_y) {
    int BMP_fd = open(bmpname, O_RDWR);
    if (BMP_fd == -1) {
        perror("open BMP");
        return;
    }
    printf("图片: %s 打开成功\n", bmpname);

    // 读取54字节的信息头
    char head[54];
    if (read(BMP_fd, head, sizeof(head)) != sizeof(head)) {
        perror("read BMP header");
        close(BMP_fd);
        return;
    }

    // 获取图片的宽度和高度
    int width = head[18] | head[19] << 8 | head[20] << 16 | head[21] << 24;
    int height = head[22] | head[23] << 8 | head[24] << 16 | head[25] << 24;

    // 错误处理
    if (r_x + width > 800 || r_y + height > 480) {
        printf("图片显示越界\n");
        close(BMP_fd);
        return;
    }
    if (width % 4 != 0) {
        printf("图片宽度需要为4的倍数\n");
        close(BMP_fd);
        return;
    }
    printf("BMP: %d*%d\n", width, height);

    // 直接在读取 BGR 数据时进行颜色拼接和写入操作
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            char bgr[3];
            off_t offset = 54 + ((height - 1 - y) * width + x) * 3;
            if (lseek(BMP_fd, offset, SEEK_SET) == -1) {
                perror("lseek BMP");
                close(BMP_fd);
                return;
            }
            if (read(BMP_fd, bgr, sizeof(bgr)) != sizeof(bgr)) {
                perror("read BGR data");
                close(BMP_fd);
                return;
            }
            int color = bgr[2] << 16 | bgr[1] << 8 | bgr[0];
            mmap_LCD[(y + r_y) * 800 + x + r_x] = color;
        }
    }

    close(BMP_fd);
}

// 显示图片: 参数：图片名称、x坐标、y坐标、缩放比例：
void display_bmp_scale(char *bmpname, int r_x, int r_y, float scale) {
    int BMP_fd = open(bmpname, O_RDWR);
    if (BMP_fd == -1) {
        perror("open BMP");
        return;
    }
    printf("图片: %s 打开成功\n", bmpname);

    // 读取54字节的信息头
    char head[54];
    if (read(BMP_fd, head, sizeof(head)) != sizeof(head)) {
        perror("read BMP header");
        close(BMP_fd);
        return;
    }

    // 获取图片的宽度和高度
    int width = head[18] | head[19] << 8 | head[20] << 16 | head[21] << 24;
    int height = head[22] | head[23] << 8 | head[24] << 16 | head[25] << 24;

    // 计算缩放后的宽度和高度
    int new_width = (int)(width * scale);
    int new_height = (int)(height * scale);

    // 计算基于中心点缩放后的左上角偏移量
    int offset_x = (new_width - width) / 2;
    int offset_y = (new_height - height) / 2;

    // 调整显示位置，确保基于中心点缩放
    r_x -= offset_x;
    r_y -= offset_y;

    // 检查是否超出屏幕边界，如果超出则调整缩放比例
    if (r_x + new_width > 800 || r_y + new_height > 480) {
        float scale_w = (float)(800 - r_x) / new_width;
        float scale_h = (float)(480 - r_y) / new_height;
        scale = (scale_w < scale_h) ? scale_w : scale_h;
        new_width = (int)(width * scale);
        new_height = (int)(height * scale);
        printf("图片放大将超出屏幕，调整缩放比例为: %.2f\n", scale);
        
        // 重新计算偏移量和位置
        offset_x = (new_width - width) / 2;
        offset_y = (new_height - height) / 2;
        r_x = 0 - offset_x;
        r_y = 0 - offset_y;
    }

    // 错误处理
    if (width % 4 != 0) {
        printf("图片宽度需要为4的倍数\n");
        close(BMP_fd);
        return;
    }
    printf("BMP原始尺寸: %d*%d\n", width, height);
    printf("BMP缩放后尺寸: %d*%d\n", new_width, new_height);
    printf("显示位置: (%d, %d)\n", r_x, r_y);

    // 对屏幕进行填黑处理
    int black = 0x000000;  // 黑色
    for (int y = 0; y < 480; y++) {
        for (int x = 0; x < 800; x++) {
            // 检查是否在图片区域内
            if (x >= r_x && x < r_x + new_width && y >= r_y && y < r_y + new_height) {
                continue;  // 跳过图片区域
            }
            mmap_LCD[y * 800 + x] = black;
        }
    }

    // 读取BGR颜色数据
    char bgr[width * height * 3];
    if (read(BMP_fd, bgr, sizeof(bgr)) != sizeof(bgr)) {
        perror("read BGR data");
        close(BMP_fd);
        return;
    }

    // 对图片进行缩放处理（修正上下颠倒问题）
    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            // 计算原始图片中对应的坐标（修正y坐标的计算方式）
            int src_x = (int)(x / scale);
            int src_y = height - 1 - (int)(y / scale);  // 修正：从上到下映射

            // 计算原始图片中对应像素的索引
            int src_index = (src_y * width + src_x) * 3;

            // 获取原始图片中对应像素的BGR值
            char b = bgr[src_index];
            char g = bgr[src_index + 1];
            char r = bgr[src_index + 2];

            // 拼接RGB颜色数据
            int color = r << 16 | g << 8 | b;

            // 将缩放后的像素写入虚拟内存
            mmap_LCD[(y + r_y) * 800 + x + r_x] = color;
        }
    }

    close(BMP_fd);
}

// 从中间向四周扩散显示图片（固定800×480分辨率，无缩放）
void show_bmp_diffuse(char *bmpname) {
    int BMP_fd = open(bmpname, O_RDWR);
    if (BMP_fd == -1) {
        perror("open BMP");
        return;
    }
    printf("图片: %s 打开成功\n", bmpname);

    // 读取54字节的信息头
    char head[54];
    if (read(BMP_fd, head, sizeof(head)) != sizeof(head)) {
        perror("read BMP header");
        close(BMP_fd);
        return;
    }

    // 获取图片的宽度和高度
    int width = head[18] | head[19] << 8 | head[20] << 16 | head[21] << 24;
    int height = head[22] | head[23] << 8 | head[24] << 16 | head[25] << 24;

    // 错误处理：确保图片尺寸不超过屏幕
    if (width > 800 || height > 480) {
        printf("图片尺寸超过800×480，无法显示\n");
        close(BMP_fd);
        return;
    }
    if (width % 4 != 0) {
        printf("图片宽度需要为4的倍数\n");
        close(BMP_fd);
        return;
    }
    printf("BMP尺寸: %d*%d\n", width, height);

    // 计算图片显示位置（居中显示）
    int r_x = (800 - width) / 2;
    int r_y = (480 - height) / 2;
    printf("显示位置: (%d, %d)\n", r_x, r_y);

    // 对屏幕进行填黑处理
    int black = 0x000000;
    for (int y = 0; y < 480; y++) {
        for (int x = 0; x < 800; x++) {
            mmap_LCD[y * 800 + x] = black;
        }
    }

    // 读取BGR颜色数据
    char bgr[width * height * 3];
    if (read(BMP_fd, bgr, sizeof(bgr)) != sizeof(bgr)) {
        perror("read BGR data");
        close(BMP_fd);
        return;
    }

    // 计算图片中心点
    int center_x = r_x + width / 2;
    int center_y = r_y + height / 2;
    
    // 计算最大扩散半径
    int max_radius = (int)sqrt(pow(width/2, 2) + pow(height/2, 2));
    
    // 从中心点开始扩散显示
    for (int radius = 0; radius <= max_radius; radius += 40) {
        // 遍历整个图片区域
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // 计算当前点到中心点的距离
                int dx = x - width/2;
                int dy = y - height/2;
                int distance = (int)sqrt(dx*dx + dy*dy);
                
                // 如果距离小于当前半径，则绘制该像素
                if (distance <= radius) {
                    // 计算原始图片中对应的坐标（修正上下颠倒）
                    int src_y = height - 1 - y;
                    
                    // 计算原始图片中对应像素的索引
                    int src_index = (src_y * width + x) * 3;
                    
                    // 获取原始图片中对应像素的BGR值
                    char b = bgr[src_index];
                    char g = bgr[src_index + 1];
                    char r = bgr[src_index + 2];
                    
                    // 拼接RGB颜色数据
                    int color = r << 16 | g << 8 | b;
                    
                    // 将像素写入虚拟内存
                    mmap_LCD[(y + r_y) * 800 + x + r_x] = color;
                }
            }
        }
        
        // 延时控制扩散速度
        //usleep(1);  // 1毫秒
    }

    close(BMP_fd);
}
