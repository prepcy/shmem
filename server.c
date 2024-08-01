#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include "shmem.h"

long long int total_len;
long long int total_count;

void shmem_init(info_t *info)
{
	info->xdma_queue = alloc_xdma_queue(QUEUE_COUNT, IP_SIZE, \
						SHM_NAME, SEM_FREE, SEM_VALID);
	if (!info->xdma_queue) {
		log_err("alloc_xdma_queue err.\n");
		exit(0);
	}
}

void timer_init(void);
int main()
{
	info_t info;
	timer_init();
	shmem_init(&info);
	xdma_frame_t *xdma_frame = NULL;
	xdma_queue_t *xdma_queue = info.xdma_queue;

	while(1) {
		/* 在队列中等待、获取一个空闲的帧 */
		xdma_frame = wait_proc_xdma_frame(xdma_queue);

		if (xdma_frame->data[0] != (total_count&0xFF)) {
			printf("recv data error, data : 0x%02x\n", xdma_frame->data[0]);
			printf("count: %d, data: %d\n", total_count, xdma_frame->data[0]);
			exit(0);
		}

		total_len += *xdma_frame->len;
		total_count ++;

		memset(xdma_frame->data, 0x0, *xdma_frame->len);
		*xdma_frame->len = 0;
		/* 通知处理线程有新的一帧准备好 */
		xdma_free_frame_inc(xdma_queue);
		xdma_frame = NULL;
    }
}

void timer_callback(int signum)
{
	static long long int send_t = 0;
	long long int send = total_len - send_t;

	printf("\n-----------------------------\n");
	printf("recv speed : %lld MB/s[%fGbps][%lld KB/s]\n",	\
		   send / (1024 * 1024), (float)send * 8 / (1024 * 1024 * 1024), send / 1024);
	printf("recv count : %lld\n", total_count);
	printf("-----------------------------\n");

	send_t = total_len;
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

    log_info("Timer started. Waiting for timer to expire...\n");
}

