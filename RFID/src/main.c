#include "main.h"
#include "lcd.h"    
#include "touch.h"      // 包含头文件，是一个展开其中内容的过程   
#include "RFID.h"      // 包含头文件，是一个展开其中内容的过程   


/* 触摸显示屏、线程、RFID结合的示例程序 */
int main(int argc, char const *argv[])
{
    // LCD的初始化功能函数
    LCD_init();
    // 触摸屏的初始化功
    TOUCH_init();
    // 串口初始化
    TTY_init();
   
    // 显示图片: 参数：图片名称、x坐标、y坐标
    display_bmp_scale("1.bmp", 0, 0, 1);
    // 创建触摸屏坐标获取的线程
    pthread_t touch_tid;
    pthread_create(&touch_tid, NULL, touch_get,NULL);
    int cardid = 0;
    int authorized_card_id = 0xaff963ea;
    while(1)
    {
        printf("[等待刷卡...]");
        cardid = get_cardid();

        if (cardid == authorized_card_id)
        {
            printf("卡号验证成功\n");
            display_bmp_scale("welcome.bmp", 0, 0, 1);
            sleep(2); // 停留2秒
            show_bmp_diffuse("door_open.bmp");
        }
        else
        {
            printf("卡号验证失败\n");
            display_bmp_scale("alarm.bmp", 0, 0, 1);
        }
        
        sleep(3); // 停留3秒
        display_bmp_scale("1.bmp", 0, 0, 1); // 返回首页
    }

    pthread_join(touch_tid, NULL);
    // 关闭LCD显示屏
    LCD_close();
    // 关闭触摸屏
    TOUCH_close();
    // 关闭串口
    TTY_close();
    return 0;
}
