#include "main.h"

#define DEV_PATH   "/dev/ttySAC1"      	//串口设备定义

extern bool cardOn;					//card打开或者关闭标志位
extern int tty_fd;        				//串口的文件描述符

//初始化串口
void init_tty(void)
{
	//1、打开开发板串口
	tty_fd = open(DEV_PATH, O_RDWR | O_NOCTTY);
	if(tty_fd == -1)
	{
		printf("open %s error: %s\n", DEV_PATH, strerror(errno));
		return;
	}
	printf("DEBUG: Opened TTY: %s with fd %d\n", DEV_PATH, tty_fd);
	//声明设置串口的结构体
	struct termios config;
	bzero(&config, sizeof(config));

	// 设置为原始模式 (raw mode)
	cfmakeraw(&config);

	//设置波特率
	cfsetispeed(&config, B9600);
	cfsetospeed(&config, B9600);

	// CLOCAL: 忽略调制解调器状态线, CREAD: 开启接收
	config.c_cflag |= CLOCAL | CREAD;

	// 一位停止位
	config.c_cflag &= ~CSTOPB;

    // [关键修改] 设置读取超时
    // VTIME: 设置等待时间，单位为0.1秒。设置为5表示0.5秒。
    // VMIN: 设置最小接收字符数。VTIME > 0, VMIN = 0时, read会等待VTIME时间或直到有数据到来。
	config.c_cc[VTIME] = 5; // 等待 0.5 秒
	config.c_cc[VMIN] = 0;  // 即使没有数据也返回

	// 清空输入/输出缓冲区
	tcflush (tty_fd, TCIFLUSH);

	//激活串口设置
	if(tcsetattr(tty_fd, TCSANOW, &config) != 0)
	{
		perror("设置串口失败");
		exit(0);
	}
}

//不断发送A指令（请求RFID卡），一旦探测到卡片就退出
void request_card(int fd)
{
	init_REQUEST();
	char recvinfo[128];
	while(1)
	{
		// 向串口发送指令
		tcflush(fd, TCIFLUSH);
		int bytes_written = write(fd, PiccRequest_IDLE, PiccRequest_IDLE[0]);
		printf("DEBUG: request_card: Wrote %d bytes\n", bytes_written);

		usleep(50*1000);

		bzero(recvinfo, 128);
		int bytes_read = read(fd, recvinfo, 128);
		printf("DEBUG: request_card: Read %d bytes\n", bytes_read);
		if(bytes_read == -1) {
			perror("DEBUG: request_card: Read error");
			continue;
		}

		//应答帧状态部分为0 则请求成功
		if(recvinfo[2] == 0x00)
		{
			cardOn = true;
			printf("DEBUG: Card detected!\n");
			break;
		}
		cardOn = false;
	}
}

//获取RFID卡号
int get_id(int fd)
{
	// 刷新串口缓冲区
	tcflush (fd, TCIFLUSH);

	// 初始化获取ID指令并发送给读卡器
	init_ANTICOLL();

	//发送防碰撞（一级）指令
	int bytes_written = write(fd, PiccAnticoll1, PiccAnticoll1[0]);
	printf("DEBUG: get_id: Wrote %d bytes\n", bytes_written);

	usleep(50*1000);

	// 获取读卡器的返回值
	char info[256];
	bzero(info, 256);
	int bytes_read = read(fd, info, 128);
	printf("DEBUG: get_id: Read %d bytes\n", bytes_read);
	if(bytes_read == -1) {
		perror("DEBUG: get_id: Read error");
		return -1;
	}

	// 应答帧状态部分为0 则成功
	uint32_t id = 0;
		if(info[2] == 0x00)
	{
		printf("DEBUG: get_id: Response status OK\n");
		// 确保复制的长度不超过id的大小
		size_t copy_len = (info[3] < sizeof(id)) ? info[3] : sizeof(id);
		memcpy(&id, &info[4], copy_len);
		if(id == 0)
		{
			printf("DEBUG: get_id: ID is 0\n");
			return -1;
		}
	}else
	{
		printf("DEBUG: get_id: Response status NOT OK (0x%x)\n", info[2]);
		return -1;
	}
	return id;
}

// 用户获取rfid卡号
int get_cardid(void)
{
    int cardid = 0;
    // printf("waiting card......\n"); // 注释掉此行，避免刷屏

    //1、检测附近是否有卡片
    request_card(tty_fd);


    //2、获取卡号
    cardid = get_id(tty_fd);

    // 忽略非法卡号
    if(cardid == 0 || cardid == 0xFFFFFFFF)
        return -1;

    printf("RFID卡号: %x\n", cardid);
    return cardid;       // 将获取到的卡号返回
}

// 关闭串口
void close_tty(void)
{
    close(tty_fd);
}