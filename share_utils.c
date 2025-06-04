#include "share_utils.h"


void show_hex(uint8_t *data, int32_t len)
{
    int32_t i;

    for (i = 0; i < len; i++) {
		if (!(i % 16))
			printf("0x%08x	", i);

        printf("%02x ", data[i]);
		if (i % 16 == 15)
			printf("\n");
    }
    printf("\n\n");
}

extern info_t info;
static void timer_callback(int signum __attribute__((unused)))
{
	static long long int send_t = 0;
	static long long int recv_t = 0;
	long long int send = info.send_len - send_t;
	long long int recv = info.recv_len - recv_t;

	printf("\n-----------------------------\n");
	printf("send speed : %lld MB/s[%fGbps][%lld KB/s]\n",	\
		   send / (1024 * 1024), (float)send * 8 / (1024 * 1024 * 1024), send / 1024);
	printf("recv speed : %lld MB/s[%fGbps][%lld KB/s]\n",	\
		   recv / (1024 * 1024), (float)recv * 8 / (1024 * 1024 * 1024), recv / 1024);
	printf("send count : %ld\n", info.send_count);
	printf("recv count : %ld\n", info.recv_count);
	printf("send len : %ld\n", info.send_len);
	printf("recv len : %ld\n", info.recv_len);
	printf("-----------------------------\n");

	send_t = info.send_len;
	recv_t = info.recv_len;
}

void timer_init(void)
{
	// 设置定时器处理函数
    signal(SIGALRM, timer_callback);

	// 创建定时器
    struct itimerval timer;
    timer.it_value.tv_sec = 1;		// 初始定时器时间（秒）
    timer.it_value.tv_usec = 0;		// 初始定时器时间（微秒）
    timer.it_interval.tv_sec = 1;	// 重复定时器时间（秒）
    timer.it_interval.tv_usec = 0;	// 重复定时器时间（微秒）

    // 启动定时器
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer");
        exit(1);
    }

    printf("Timer started. Waiting for timer to expire...\n");
}




