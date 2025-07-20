#ifndef _ISO14443A_H
#define _ISO14443A_H

// ISO14443A命令：请求(IDLE)
static uint8_t PiccRequest_IDLE[7] = {0x07, // 帧长度
			           0x02, // 命令类型: ISO14443A
				   'A',  // 命令字'A'
				   0x01, // Length:1
				   0x26, // Info:IDLE
				   0x00, // 校验和
				   0x03};// ETX
// ISO14443A命令：请求(ALL)
static uint8_t PiccRequest_ALL[7] = {0x07, // 帧长度
			          0x02, // 命令类型: ISO14443A
				  'A',  // 命令字'A'
				  0x01, // Length:1
				  0x52, // Info:ALL
				  0x00, // 校验和
				  0x03};// ETX

// ISO14443A命令：防碰撞(一级)
static uint8_t PiccAnticoll1[8] = {0x08, // 帧长度
			        0x02, // 命令类型: ISO14443A
				'B',  // 命令字'B'
				0x02, // Length:2
				0x93, // 防碰撞等级
				0x00, // Info:获取卡号的所有位
				0x00, // 校验和
				0x03};// ETX
// ISO14443A命令：防碰撞(二级)
static uint8_t PiccAnticoll2[8] = {0x08, // 帧长度
			        0x02, // 命令类型: ISO14443A
				'B',  // 命令字'B'
				0x02, // Length:2
				0x95, // 防碰撞等级
				0x00, // Info:获取卡号的所有位
				0x00, // 校验和
				0x03};// ETX
// ISO14443A命令：防碰撞(三级)
static uint8_t PiccAnticoll3[8] = {0x08, // 帧长度
			        0x02, // 命令类型: ISO14443A
				'B',  // 命令字'B'
				0x02, // Length:2
				0x97, // 防碰撞等级
				0x00, // Info:获取卡号的所有位
				0x00, // 校验和
				0x03};// ETX

// ISO14443A命令：选择
static uint8_t SELECT[11] = {0x0B, // 帧长度
			  0x02, // 命令类型: ISO14443A
			  'C',  // 命令字'C'
			  0x05, // Length:5
			  0x00, // 防碰撞等级
			  0x00,0x00,0x00,0x00, // 卡序列号（4字节）
			  0x00, // 校验和
			  0x03};// ETX

// ISO14443A命令：直接密码证实
static uint8_t PiccAuthKey[18] = {0x12, // 帧长度
			       0x02, // 命令类型: ISO14443A
			       'F',  // 命令字'F'
			       0x12, // Length:12
			       0x60, // 密钥A/B
			       0x00,0x00,0x00,0x00, // 卡序列号（4字节）
			       0x00,0x00,0x00,0x00,0x00,0x00, // 密钥（6字节）
			       0x00, // 卡块号
			       0x00, // 校验和
			       0x03};// ETX

// ISO14443A命令：读
static uint8_t PiccRead[7] = {0x07, // 帧长度
			   0x02, // 命令类型: ISO14443A
			   'G',  // 命令字'G'
			   0x01, // Length:1
			   0x00, // 卡块号
			   0x00, // 校验和
			   0x03};// ETX

// ISO14443A命令：写
static uint8_t PiccWrite[23] = {0x17, // 帧长度
			     0x02, // 命令类型: ISO14443A
			     'H',  // 命令字'H'
			     0x17, // Length:17
			     0x00, // 卡块号
			     0x00,0x00,0x00,0x00, // 数据
			     0x00,0x00,0x00,0x00, // 数据
			     0x00,0x00,0x00,0x00, // 数据
			     0x00,0x00,0x00,0x00, // 数据
			     0x00, // 校验和
			     0x03};// ETX

static void BCC(uint8_t frame[])
{
	// pos为校验和所在下标
	// 校验和本身 与 结束标记不参与校验
	int pos = frame[0]-2;

	uint8_t bcc = 0;
	for(int i=0; i<pos; i++)
	{
		bcc ^= frame[i];
	}
	
	frame[pos] = ~bcc;
}

// 'A'
static void init_REQUEST()
{
	BCC(PiccRequest_IDLE);
	BCC(PiccRequest_ALL);
}

// 'B'
static void init_ANTICOLL()
{
	BCC(PiccAnticoll1);
	BCC(PiccAnticoll2);
	BCC(PiccAnticoll3);
}

// 'C'
static void init_SELECT(uint8_t coll, uint32_t serial_num)
{
	SELECT[4] = coll;

	memcpy(&SELECT[5], &serial_num, 4);
	BCC(SELECT);
}

// 'F'
static void init_PiccAuthKey(uint32_t serial_num)
{
	memcpy(&PiccAuthKey[5], &serial_num, 4);
	BCC(SELECT);
}

// 'G'
static void init_PiccRead(uint8_t blockth)
{
	PiccRead[4] = blockth;
	BCC(PiccRead);
}

// 'H'
static void init_PiccWrite(uint8_t blockth)
{
	PiccRead[4] = blockth;
	BCC(PiccRead);
}

#endif
