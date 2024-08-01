#ifndef _CIRCLE_BUFFER_H
#define _CIRCLE_BUFFER_H

#include<semaphore.h>

#pragma pack(1)
typedef struct {
	unsigned int *len;
    unsigned char *data;
} xdma_frame_t;
#pragma pack()

typedef struct {
	/* 空闲帧对应的数组索引号 */
    unsigned int free_index;
	/* 待处理的帧对应的数组索引号 */
    unsigned int proc_index;
	/* 该队列总共的数据帧个数 */
	unsigned int total_count;
	/* 用来维护空闲缓冲区 */
	sem_t *free_frame_sem;
	/* 用来维护被占用的缓冲区 */
	sem_t *valid_frame_sem;
	/* 用来存放xdma数据帧 */
    xdma_frame_t *xdma_frames;
	unsigned char *data_ptr;
} xdma_queue_t;

/* 申请一个环形队列 */
xdma_queue_t *alloc_xdma_queue(int count, int frame_size, char* shm_name, char* free_name, char* valid_name);

/* 释放一个环形队列 */
void free_xdma_queue(xdma_queue_t *xdma_queue);

/* 在环形缓冲区中获取一个空闲帧，如果没有则通过信号量进行等待 */
xdma_frame_t *wait_free_xdma_frame(xdma_queue_t *xdma_queue);

/* 在环形缓冲区中获取一个有数据的帧，如果没有则通过信号量进行等待 */
xdma_frame_t *wait_proc_xdma_frame(xdma_queue_t *xdma_queue);

/* 有效帧增加一帧 */
void xdma_used_frame_inc(xdma_queue_t *xdma_queue);

/* 空闲帧增加一帧 */
void xdma_free_frame_inc(xdma_queue_t *xdma_queue);

#endif