#include "RFID.h"

bool cardOn = false;					//card打开或者关闭标志位
#define DEV_PATH   "/dev/ttySAC1"      //设备定义: 串口2
int tty_fd = 0;							// 串口的文件描述符

//初始化串口
void TTY_init(void)
{    
	//1、打开开发板串口
	tty_fd = open(DEV_PATH, O_RDWR | O_NOCTTY);
	if(tty_fd == -1)
	{
		printf("open %s error\n", DEV_PATH);
		return;
	}
	//声明设置串口的结构体
	struct termios config;
	bzero(&config, sizeof(config));

	// 设置无奇偶校验
	// 设置数据位为8位
	// 设置为非规范模式（对比与控制终端）
	cfmakeraw(&config);

	//设置波特率
	cfsetispeed(&config, B9600);
	cfsetospeed(&config, B9600);

	// CLOCAL和CREAD分别用于本地连接和接受使能
	// 首先要通过位掩码的方式激活这两个选项。    
	config.c_cflag |= CLOCAL | CREAD;

	// 一位停止位
	config.c_cflag &= ~CSTOPB;

	// 可设置接收字符和等待时间，无特殊要求可以将其设置为0
	config.c_cc[VTIME] = 0;
	config.c_cc[VMIN] = 1;

	// 用于清空输入/输出缓冲区
	tcflush (tty_fd, TCIFLUSH);
	tcflush (tty_fd, TCOFLUSH);

	//完成配置后，可以使用以下函数激活串口设置
	if(tcsetattr(tty_fd, TCSANOW, &config) != 0)
	{
		perror("设置串口失败");
		exit(0);
	}

	// 将串口设置为非阻塞状态，避免第一次运行卡住的情况
	long state = fcntl(tty_fd, F_GETFL);
	state |= O_NONBLOCK;
	fcntl(tty_fd, F_SETFL, state);
	return;
}

//发送A指令（请求RFID卡），添加超时机制和最大尝试次数，避免无限循环
void request_card(int fd)
{
	init_REQUEST();
	char recvinfo[128];
	
	// 添加超时机制和最大尝试次数
	const int MAX_ATTEMPTS = 50;  // 最大尝试次数
	const int TIMEOUT_MS = 5000;  // 总超时时间(毫秒)
	int attempts = 0;
	struct timeval start_time, current_time;
	long elapsed_ms = 0;
	
	gettimeofday(&start_time, NULL);
	
	while(attempts < MAX_ATTEMPTS && elapsed_ms < TIMEOUT_MS)
	{
		// 向串口发送指令
		tcflush(fd, TCIFLUSH);

		//发送请求指令
		write(fd, PiccRequest_IDLE, PiccRequest_IDLE[0]);

		usleep(50*1000);  // 50ms延时

		bzero(recvinfo, 128);
		if(read(fd, recvinfo, 128) == -1)
		{
			attempts++;
			
			// 计算已经过去的时间
			gettimeofday(&current_time, NULL);
			elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + 
						 (current_time.tv_usec - start_time.tv_usec) / 1000;
			
			continue;
		}

		//应答帧状态部分为0 则请求成功
		if(recvinfo[2] == 0x00)	
		{
			cardOn = true;
			return;  // 成功检测到卡片，直接返回
		}
		
		attempts++;
		cardOn = false;
		
		// 计算已经过去的时间
		gettimeofday(&current_time, NULL);
		elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + 
					 (current_time.tv_usec - start_time.tv_usec) / 1000;
	}
	
	// 如果达到最大尝试次数或超时，设置cardOn为false并返回
	cardOn = false;
	printf("RFID读卡超时或达到最大尝试次数\n");
}

//获取RFID卡号
int get_id(int fd)
{
	// 刷新串口缓冲区
	tcflush (fd, TCIFLUSH);

	// 初始化获取ID指令并发送给读卡器
	init_ANTICOLL();

	//发送防碰撞（一级）指令
	write(fd, PiccAnticoll1, PiccAnticoll1[0]);

	usleep(50*1000);

	// 获取读卡器的返回值
	char info[256];
	bzero(info, 256);
	read(fd, info, 128);

	// 应答帧状态部分为0 则成功
	uint32_t id = 0;
	if(info[2] == 0x00) 
	{
		memcpy(&id, &info[4], info[3]);
		if(id == 0)		
		{
			return -1;
		}
	}else
	{
		return -1;
	}
	return id;
}

// 关闭串口
void TTY_close(void)
{
	close(tty_fd);
}

// 用户获取rfid卡号，添加错误处理和恢复机制
int get_cardid(void)
{
	int cardid = 0;
	static int error_count = 0;
	const int MAX_ERRORS = 5;  // 最大连续错误次数
	
	printf("waiting card......\n");

	//1、检测附近是否有卡片
	request_card(tty_fd);
	
	// 如果request_card超时或达到最大尝试次数，cardOn会被设置为false
	if (!cardOn) {
		error_count++;
		if (error_count >= MAX_ERRORS) {
			printf("RFID读卡连续失败%d次，尝试重置设备...\n", error_count);
			// 尝试重置设备
			close(tty_fd);
			usleep(100*1000);  // 等待100ms
			TTY_init();  // 重新初始化串口
			error_count = 0;  // 重置错误计数
		}
		return -1;
	}

	//2、获取卡号
	cardid = get_id(tty_fd);

	// 忽略非法卡号
	if(cardid == 0 || cardid == 0xFFFFFFFF) {
		error_count++;
		if (error_count >= MAX_ERRORS) {
			printf("RFID读卡连续失败%d次，尝试重置设备...\n", error_count);
			// 尝试重置设备
			close(tty_fd);
			usleep(100*1000);  // 等待100ms
			TTY_init();  // 重新初始化串口
			error_count = 0;  // 重置错误计数
		}
		return -1;
	}

	// 成功读取卡号，重置错误计数
	error_count = 0;
	
	printf("RFID卡号: %x\n", cardid);
	return cardid;		// 将获取到的卡号返回
}
/*获取RFID卡号主函数
int main(int argc, char **argv)
{
	//1、初始化串口
	TTY_init();

	int cardid;		//存放获取到的RFID卡号

	while(1)
	{	
		printf("waiting card......\n");
		
		//2、检测附近是否有卡片
		request_card(tty_fd);

		//3、获取卡号
		cardid = get_id(tty_fd);

		// 忽略非法卡号
		if(cardid == 0 || cardid == 0xFFFFFFFF)
			continue;

		printf("RFID卡号: %x\n", cardid);
		if (cardid == 0xa36e1cd0)
		{
			printf("卡号验证成功\n");
		}else
		{
			printf("卡号验证失败\n");
		}
		
		sleep(1);	//延时1秒
	}

	close(tty_fd);
	return 0;
}
*/