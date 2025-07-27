#include "include/Pipeline.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

using namespace std;
int in_fd;		//入库管道文件描述符
int out_fd;		//出库管道文件描述符

// [新增] 控制BMP图片尺寸输出的全局标志定义
volatile bool g_suppress_bmp_output = false;

// 全局模型对象
pr::PipelinePR* prc = nullptr;

//入库执行函数
void in_car(int sig)
{
    cout<<"【收到入库识别信号】"<<endl;
    if (!prc) {
        cout<<"【加载车牌识别数据模型...】"<<endl;
        prc = new pr::PipelinePR("model/cascade.xml",
            "model/HorizonalFinemapping.prototxt",
            "model/HorizonalFinemapping.caffemodel",
            "model/Segmentation.prototxt",
            "model/Segmentation.caffemodel",
            "model/CharacterRecognization.prototxt",
            "model/CharacterRecognization.caffemodel",
            "model/SegmenationFree-Inception.prototxt",
            "model/SegmenationFree-Inception.caffemodel");
        cout<<"【加载车牌识别数据模型完成】"<<endl;
    }
    string pn;
    cout<<"【in_car】: 尝试读取图片 car.jpg..."<<endl;
    cv::Mat image = cv::imread("car.jpg");
    if (image.empty()) {
        cout << "【in_car错误】: 无法读取图片 car.jpg!" << endl;
        write(in_fd, "err", 3);
        return;
    }
    cout<<"【in_car】: 图片 car.jpg 读取成功。"<<endl;
    std::vector<pr::PlateInfo> res;
    try{
        cout<<"【in_car】: 尝试识别车牌..."<<endl;
        res = prc->RunPiplineAsImage(image,pr::SEGMENTATION_FREE_METHOD);
        cout<<"【in_car】: 车牌识别完成。"<<endl;
    }
    catch(...){
        cout << "【in_car错误】: 未检测到车牌或识别异常！" << endl;
        write(in_fd, "err", 3);
        return ;
    }
    for(auto st:res)
    {
        pn = st.getPlateName();
        if(pn.length() == 9)
        {
            cout << "【in_car】: 检测到车牌: " << pn.data();
            cout << "，确信率: "   << st.confidence << endl;
            if(st.confidence > 0.8)
            {
                cout<<"【in_car】: 确信度高，写入管道。"<<endl;
                write(in_fd, pn.data(), 9);
                cout << "【入库车牌发送完成】" << endl;
                return ;
            }
        }
    }
    cout<<"【in_car】: 未找到高确信度车牌，写入err。"<<endl;
    write(in_fd, "err", 3);
}
//出库执行函数
void out_car(int sig)
{
    cout<<"【收到出库识别信号】"<<endl;
    if (!prc) {
        cout<<"【加载车牌识别数据模型...】"<<endl;
        prc = new pr::PipelinePR("model/cascade.xml",
            "model/HorizonalFinemapping.prototxt",
            "model/HorizonalFinemapping.caffemodel",
            "model/Segmentation.prototxt",
            "model/Segmentation.caffemodel",
            "model/CharacterRecognization.prototxt",
            "model/CharacterRecognization.caffemodel",
            "model/SegmenationFree-Inception.prototxt",
            "model/SegmenationFree-Inception.caffemodel");
        cout<<"【加载车牌识别数据模型完成】"<<endl;
    }
    string pn;
    cout<<"【out_car】: 尝试读取图片 car.jpg..."<<endl;
    cv::Mat image = cv::imread("car.jpg");
    if (image.empty()) {
        cout << "【out_car错误】: 无法读取图片 car.jpg!" << endl;
        write(out_fd, "err", 3);
        return;
    }
    cout<<"【out_car】: 图片 car.jpg 读取成功。"<<endl;
    std::vector<pr::PlateInfo> res;
    try{
        cout<<"【out_car】: 尝试识别车牌..."<<endl;
        res = prc->RunPiplineAsImage(image,pr::SEGMENTATION_FREE_METHOD);
        cout<<"【out_car】: 车牌识别完成。"<<endl;
    }
    catch(...){
        cout << "【out_car错误】: 未检测到车牌或识别异常！" << endl;
        write(out_fd, "err", 3);
        return ;
    }
    for(auto st:res)
    {
        pn = st.getPlateName();
        if(pn.length() == 9)
        {
            cout << "【out_car】: 检测到车牌: " << pn.data();
            cout << "，确信率: "   << st.confidence << endl;
            if(st.confidence > 0.8)
            {
                cout<<"【out_car】: 确信度高，写入管道。"<<endl;
                write(out_fd, pn.data(), 9);
                cout << "【出库车牌发送完成】" << endl;
                return ;
            }
        }
    }
    cout<<"【out_car】: 未找到高确信度车牌，写入err。"<<endl;
    write(out_fd, "err", 3);
}

//车牌识别主功能函数
int main(int argc,char **argv)
{
    printf("[车牌识别程序ALPR启动成功】......\n");
    //注册信号
    signal(SIGUSR1, in_car);
    signal(SIGUSR2, out_car);
    signal(SIGRTMIN, in_car); // RFID触发的也走入库识别流程
    //打开入库管道文件
    in_fd = open("/tmp/LPR2SQLitIn", O_RDWR | O_NONBLOCK);
    if(in_fd == -1)
    {
        printf("open /tmp/LPR2SQLitIn err!\n");
    }
    //打开出库管道文件
    out_fd = open("/tmp/LPR2SQLitOut", O_RDWR | O_NONBLOCK);
    if(out_fd == -1)
    {
        printf("open /tmp/LPR2SQLitOut err!\n");
    }
    while(1) {
        sleep(1);
    }
    return 0;
}
