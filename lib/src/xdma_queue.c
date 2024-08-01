#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "xdma_queue.h"

xdma_queue_t *alloc_xdma_queue(int count, int frame_size, char* shm_name, char* free_name, char* valid_name)
{
	int i;
	xdma_queue_t *xdma_queue = NULL;

	if (count <= 0)
		return NULL;

	xdma_queue = malloc(sizeof(xdma_queue_t));
	if (!xdma_queue)
		return NULL;

	memset(xdma_queue, 0, sizeof(xdma_queue_t));

	/* 给帧管理结构申请空间 */
	xdma_queue->xdma_frames = malloc(sizeof(xdma_frame_t) * count);
	if (!xdma_queue->xdma_frames) {
		free(xdma_queue);
		return NULL;
	}

	frame_size += sizeof(xdma_frame_t *);

	memset(xdma_queue->xdma_frames, 0, sizeof(xdma_frame_t) * count);

    // 创建或打开共享内存对象
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
		perror("shm_open");
		free(xdma_queue->xdma_frames);
		free(xdma_queue);
		return NULL;
	}

    // 设置共享内存对象的大小
    if (ftruncate(shm_fd, frame_size * count) == -1) {
		perror("ftruncate");
		close(shm_fd);
		free(xdma_queue->xdma_frames);
		free(xdma_queue);
		return NULL;
    }

    // 将共享内存对象映射到进程的地址空间
    xdma_queue->data_ptr = mmap(NULL, frame_size * count, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (xdma_queue->data_ptr == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
		free(xdma_queue->xdma_frames);
		free(xdma_queue);
		return NULL;
    }

	xdma_queue->total_count = count;
	memset(xdma_queue->data_ptr, 0, sizeof(char) * frame_size * count);

	/* 将申请的DMA内存分帧长赋值给帧管理结构体 */
	for (i = 0; i < count; i++) {
		xdma_queue->xdma_frames[i].len = (int*)(xdma_queue->data_ptr + frame_size * i);
		xdma_queue->xdma_frames[i].data = xdma_queue->data_ptr + frame_size * i + sizeof(int);
	}

	/* 初始化有效帧对应信号量为0，表示已接收到帧数为0 */
	xdma_queue->valid_frame_sem = sem_open(valid_name, O_CREAT, 0666, 0);
	if (xdma_queue->valid_frame_sem == NULL) {
		perror("sem_open");
		exit(0);
	}

	/* 初始化空闲帧对应信号量为帧总数 */
	xdma_queue->free_frame_sem = sem_open(free_name, O_CREAT, 0666, count);
	if (xdma_queue->free_frame_sem == NULL) {
		perror("sem_open");
		exit(0);
	}

	return xdma_queue;
}

void free_xdma_queue(xdma_queue_t *xdma_queue)
{
	if (xdma_queue) {
		free(xdma_queue->data_ptr);
		free(xdma_queue->xdma_frames);
		sem_destroy(xdma_queue->valid_frame_sem);
		sem_destroy(xdma_queue->free_frame_sem);
		free(xdma_queue);
	}
}

void xdma_used_frame_inc(xdma_queue_t *xdma_queue)
{
	sem_post(xdma_queue->valid_frame_sem);
}

void xdma_used_frame_wait_dec(xdma_queue_t *xdma_queue)
{
	sem_wait(xdma_queue->valid_frame_sem);
}

void xdma_free_frame_inc(xdma_queue_t *xdma_queue)
{
	sem_post(xdma_queue->free_frame_sem);
}

void xdma_free_frame_wait_dec(xdma_queue_t *xdma_queue)
{
	sem_wait(xdma_queue->free_frame_sem);
}

xdma_frame_t *wait_free_xdma_frame(xdma_queue_t *xdma_queue)
{
	unsigned int get_index;
	/* 等待、递减信号量 */
	xdma_free_frame_wait_dec(xdma_queue);

	get_index = xdma_queue->free_index;
	xdma_queue->free_index++;

	/* 处理队列环回 */
	xdma_queue->free_index %= xdma_queue->total_count;

	return &xdma_queue->xdma_frames[get_index];
}

xdma_frame_t *wait_proc_xdma_frame(xdma_queue_t *xdma_queue)
{
	unsigned int get_index;

	/* 等待、递减信号量 */
	xdma_used_frame_wait_dec(xdma_queue);

	get_index = xdma_queue->proc_index;
	xdma_queue->proc_index++;

	/* 处理队列环回 */
	xdma_queue->proc_index %= xdma_queue->total_count;

	return &xdma_queue->xdma_frames[get_index];
}

