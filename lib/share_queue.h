#ifndef SHARE_QUEUE_H
#define SHARE_QUEUE_H

#include <stdatomic.h>
#include <semaphore.h>

#define SHARE_MASTER	1
#define SHARE_SLAVE		0

typedef struct {
    unsigned char *data;
} share_mem_t;

typedef struct {
	/* 空闲帧对应的数组索引号 */
    atomic_int  * free_index;
	/* 待处理的帧对应的数组索引号 */
    atomic_int  * proc_index;
	/* 该队列总共的数据帧个数 */
	unsigned int total_count;
	/* 用来维护空闲缓冲区 */
	sem_t * free_frame_sem;
	/* 用来维护被占用的缓冲区 */
	sem_t * valid_frame_sem;
	/* 用来存放share数据帧 */
    share_mem_t *share_frames;
	unsigned char *data_ptr;
} share_queue_t;

void *create_shared_memory(char *file, int size);
void share_used_frame_inc(share_queue_t *share_queue);
void share_used_frame_wait_dec(share_queue_t *share_queue);
void share_free_frame_inc(share_queue_t *share_queue);
void share_free_frame_wait_dec(share_queue_t *share_queue);
void *wait_free_share_frame(share_queue_t *share_queue);
void *wait_proc_share_frame(share_queue_t *share_queue);
void *wait_proc_share_frame(share_queue_t *share_queue);
share_queue_t *alloc_share_queue(int flag, char* share_file, char* free_sem_key, char* valid_sem_key,
										int count, int frame_size, int struct_size);
void free_share_queue(share_queue_t *share_queue);

#endif

