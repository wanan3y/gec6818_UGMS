#include "carmera.h"




struct timeval timeout;		//设置超时时间

camera_t* camera;			//摄像头结构体指针变量
unsigned char* rgb; 		//开辟存放RGB颜色数据内存：640*480*3

int Shot =  OFF;			//拍照的标准：默认没有按下拍照
int Camera_state = OUT;		//摄像头状态，默认退出状态



void quit(const char * msg){fprintf(stderr, "[%s] %d: %s\n", msg, errno, strerror(errno));exit(EXIT_FAILURE);}

int xioctl(int fd, int request, void* arg)
{
  for (int i = 0; i < 100; i++) {
    int r = ioctl(fd, request, arg);
    if (r != -1 || errno != EINTR) return r;
  }
  return -1;
}

//摄像头的具体格式列举
void camera_check(camera_t* camera)
{
	struct v4l2_fmtdesc fmt;
	int ret;

	memset(&fmt, 0, sizeof(fmt));

    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	while ((ret = ioctl(camera->fd, VIDIOC_ENUM_FMT, &fmt)) == 0) //查看摄像头所支持的格式
	{
		fmt.index++;
		printf("{ pixelformat = '%c%c%c%c', description = '%s' }\n",
		fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,
		(fmt.pixelformat >> 16) & 0xFF, (fmt.pixelformat >> 24) & 0xFF,fmt.description);

	}

}

//打开摄像头
camera_t* camera_open(const char * device, uint32_t width, uint32_t height)
{
	int fd = open(device, O_RDWR | O_NONBLOCK, 0);
	if (fd == -1) quit("open");
	
	camera_t* camera = malloc(sizeof (camera_t));
	camera->fd = fd;
	camera->width = width;  //设置宽度
	camera->height = height;//设置高度
	camera->buffer_count = 0;
	camera->buffers = NULL;
	camera->head.length = 0;
	camera->head.start = NULL;
	
	camera_check(camera);
	return camera;
}

//摄像头的初始化
void camera_init(camera_t* camera) 
{
    struct v4l2_capability cap;
    if (xioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == -1) quit("VIDIOC_QUERYCAP");
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) quit("no capture");
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) quit("no streaming");

    struct v4l2_cropcap cropcap;
    memset(&cropcap, 0, sizeof cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera->fd, VIDIOC_CROPCAP, &cropcap) == 0) 
    {
		struct v4l2_crop crop;
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;
		if (xioctl(camera->fd, VIDIOC_S_CROP, &crop) == -1) 
		{
		// cropping not supported
		}
    }

    struct v4l2_format format;
    memset(&format, 0, sizeof format);
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = camera->width;   //图片采集宽640
    format.fmt.pix.height = camera->height;//图片采集高：480
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  //采集格式 YUYV
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(camera->fd, VIDIOC_S_FMT, &format) == -1) quit("VIDIOC_S_FMT");

    //开辟内存
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1) quit("VIDIOC_REQBUFS");
    camera->buffer_count = req.count;
    camera->buffers = calloc(req.count, sizeof (buffer_t));

    //映射开辟的空间
    size_t buf_max = 0;
    for (size_t i = 0; i < camera->buffer_count; i++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == -1)
          quit("VIDIOC_QUERYBUF");
        if (buf.length > buf_max) buf_max = buf.length;
        camera->buffers[i].length = buf.length;
        camera->buffers[i].start = \
		mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,camera->fd, buf.m.offset);
        if (camera->buffers[i].start == MAP_FAILED) quit("mmap");
  	}
  	camera->head.start = malloc(buf_max);
}

void camera_start(camera_t* camera)
{
	for (size_t i = 0; i < camera->buffer_count; i++) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) quit("VIDIOC_QBUF");
	}

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMON, &type) == -1)
		quit("VIDIOC_STREAMON");
}

void camera_stop(camera_t* camera)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMOFF, &type) == -1)
		quit("VIDIOC_STREAMOFF");
}

void camera_finish(camera_t* camera)
{
	for (size_t i = 0; i < camera->buffer_count; i++) {
		munmap(camera->buffers[i].start, camera->buffers[i].length);
	}
	free(camera->buffers);
	camera->buffer_count = 0;
	camera->buffers = NULL;
	free(camera->head.start);
	camera->head.length = 0;
	camera->head.start = NULL;
}

void camera_close(camera_t* camera)
{
	if (close(camera->fd) == -1) quit("close");
	free(camera);
}

int camera_capture(camera_t* camera)
{
	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if (xioctl(camera->fd, VIDIOC_DQBUF, &buf) == -1) return FALSE;
	//将数据存放好
	memcpy(camera->head.start, camera->buffers[buf.index].start, buf.bytesused);
	camera->head.length = buf.bytesused;
	
	//将缓冲区加入队列
	if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) return FALSE;
	return TRUE;
}

//取数据放在缓冲区
int camera_frame(camera_t* camera, struct timeval timeout) 
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(camera->fd, &fds);
	int r = select(camera->fd + 1, &fds, 0, 0, &timeout);
	if (r == -1) quit("select");
	if (r == 0) return FALSE;
	return camera_capture(camera);
}

//将RGB转为jpg
void jpeg(FILE* dest, uint8_t* rgb, uint32_t width, uint32_t height, int quality)
{
  JSAMPARRAY image;
  image = calloc(height, sizeof (JSAMPROW));
  for (size_t i = 0; i < height; i++) {
    image[i] = calloc(width * 3, sizeof (JSAMPLE));
    for (size_t j = 0; j < width; j++) {
      image[i][j * 3 + 0] = rgb[(i * width + j) * 3 + 0];
      image[i][j * 3 + 1] = rgb[(i * width + j) * 3 + 1];
      image[i][j * 3 + 2] = rgb[(i * width + j) * 3 + 2];
    }
  }

  struct jpeg_compress_struct compress;
  struct jpeg_error_mgr error;
  compress.err = jpeg_std_error(&error);
  jpeg_create_compress(&compress);
  jpeg_stdio_dest(&compress, dest);

  compress.image_width = width;
  compress.image_height = height;
  compress.input_components = 3;
  compress.in_color_space = JCS_RGB;
  jpeg_set_defaults(&compress);
  jpeg_set_quality(&compress, quality, TRUE);
  jpeg_start_compress(&compress, TRUE);
  jpeg_write_scanlines(&compress, image, height);
  jpeg_finish_compress(&compress);
  jpeg_destroy_compress(&compress);

  for (size_t i = 0; i < height; i++) {
    free(image[i]);
  }
  free(image);
}

int minmax(int min, int v, int max)
{
	return (v < min) ? min : (max < v) ? max : v;
}



uint32_t sign3 = 0;

int yuyv2rgb(int y, int u, int v)
{
     unsigned int pixel24 = 0;
     unsigned char *pixel = (unsigned char *)&pixel24;
     int r, g, b;
     static int  ruv, guv, buv;

     if(sign3)
     {
         sign3 = 0;
         ruv = 1159*(v-128);
         guv = 380*(u-128) + 813*(v-128);
         buv = 2018*(u-128);
     }

     r = (1164*(y-16) + ruv) / 1000;
     g = (1164*(y-16) - guv) / 1000;
     b = (1164*(y-16) + buv) / 1000;

     if(r > 255) r = 255;
     if(g > 255) g = 255;
     if(b > 255) b = 255;
     if(r < 0) r = 0;
     if(g < 0) g = 0;
     if(b < 0) b = 0;

     pixel[0] = r;
     pixel[1] = g;
     pixel[2] = b;

     return pixel24;
}

int yuyv2rgb0(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
     unsigned int in, out;
     int y0, u, y1, v;
     unsigned int pixel24;
     unsigned char *pixel = (unsigned char *)&pixel24;
     unsigned int size = width*height*2;

     for(in = 0, out = 0; in < size; in += 4, out += 6)
     {
		y0 = yuv[in+0];
		u  = yuv[in+1];
		y1 = yuv[in+2];
		v  = yuv[in+3];

		sign3 = 1;
		pixel24 = yuyv2rgb(y0, u, v);
		rgb[out+0] = pixel[0];    
		rgb[out+1] = pixel[1];
		rgb[out+2] = pixel[2];

		pixel24 = yuyv2rgb(y1, u, v);
		rgb[out+3] = pixel[0];
		rgb[out+4] = pixel[1];
		rgb[out+5] = pixel[2];

     }
	return 0;
}


//初始化摄像头
void init_camera(void)
{
	//打开摄像头
	camera = camera_open("/dev/video7", 640, 480);
	camera_init(camera);
	camera_start(camera);

	//设置超时时间
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	//跳过前五帧画面（启动摄像头）
	for (int i = 0; i < 5; i++) {
		camera_frame(camera, timeout);
	}

	//开辟内存：640*480*3
 	rgb = malloc(camera->width*camera->height*3); //RGB 640*480*3

	return;	  
}


//关闭摄像头、释放对应资源
void close_Camera(void)
{
	free(rgb); 
	// rgb = NULL; 
	camera_stop(camera);
	camera_finish(camera);
	camera_close(camera);
}


//摄像头获取数据的线程执行函数
void *Camera(void *arg)
{
	Camera_state = TURN;		//打开摄像头并不显示
    while (Camera_state)	
    {		
		
		// 无论 Camera_state 是 RUN 还是 TURN，都尝试获取一帧数据
        // 这样即使在锁屏状态下，RFID 触发拍照也能工作
        camera_frame(camera, timeout);
        yuyv2rgb0(camera->head.start, rgb, camera->width, camera->height);
        
        if (Shot)	//获取一帧画面保存为jpg图片
        {
            FILE* out = fopen("car.jpg","w");
            //将RGB转为jpg
            jpeg(out, rgb,camera->width, camera->height, 100);
            fclose(out);
            Shot = OFF;

            // 通知主线程照片已准备好
            pthread_mutex_lock(&photo_mutex);
            photo_ready = true;
            pthread_cond_signal(&photo_cond);
            pthread_mutex_unlock(&photo_mutex);
        }

		if(Camera_state == RUN)
        {
            // 只有在 RUN 状态才显示画面
            for(int y=0; y<camera->height; y++)  //480
            {
                for(int x=0; x<camera->width; x++)  //0~640
                {
                    *(mmap_p+ y*800 + x)= rgb[3*y*camera->width +3*x ]<<16 | rgb[3*y*camera->width +3*x +1]<<8 | rgb[3*y*camera->width +3*x +2]; 
                }
                
            }
        }

        // [最终修复] 在每次循环后强制休眠，将CPU时间让给UI主线程
        usleep(30000); // 休眠30毫秒，确保UI响应
		
    }
	pthread_exit("0");		//退出线程
}