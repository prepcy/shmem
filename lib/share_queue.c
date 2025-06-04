#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>

#include "share_queue.h"

// 创建共享内存
void *create_shared_memory(char *file, int size)
{
	int shm_fd = shm_open(file, O_CREAT | O_RDWR, 0666);
	if (shm_fd == -1) {
		perror("shm_open");
		exit(1);
	}

	if (ftruncate(shm_fd, size) == -1) {
		perror("ftruncate");
		exit(1);
	}

	void *shm_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (shm_ptr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	return shm_ptr;
}

share_queue_t *alloc_share_queue(int flag, char* share_file, char* free_sem_key, char* valid_sem_key,
										int count, int frame_size, int struct_size)
{
	int i, offset = 0;
	share_queue_t *share_queue = NULL;
	int frame_len =  frame_size + struct_size;

	if (count <= 0)
		return NULL;

	/* 给队列申请空间 */
	share_queue = malloc(sizeof(share_queue_t));
	if (!share_queue)
		return NULL;
	memset(share_queue, 0, sizeof(share_queue_t));


	/* 给帧管理结构申请空间 */
	share_queue->share_frames = malloc(sizeof(share_mem_t) * count);
	if (!share_queue->share_frames) {
		free(share_queue);
		return NULL;
	}
	memset(share_queue->share_frames, 0, sizeof(share_mem_t) * count);


	/* 创建共享内存，最开始是2个原子变量，后面就是 count x frame_len 数据缓冲区 */
	char *data_tmp = create_shared_memory(share_file, frame_len * count + 2*sizeof(atomic_int));
	share_queue->free_index = (atomic_int*)(data_tmp + offset);
	offset += sizeof(atomic_int);
	share_queue->proc_index = (atomic_int*)(data_tmp + offset);
	offset += sizeof(atomic_int);
	share_queue->data_ptr = (unsigned char*)(data_tmp + offset);
	share_queue->total_count = count;

	if (flag == SHARE_MASTER) {
		memset(data_tmp, 0, sizeof(char) * (frame_len * count + offset));
		atomic_init(share_queue->free_index, 0);
		atomic_init(share_queue->proc_index, 0);
	}


	/* 将申请的DMA内存分帧长赋值给帧管理结构体 */
	for (i = 0; i < count; i++) {
		share_queue->share_frames[i].data = share_queue->data_ptr + frame_len * i;
	}

	/* 初始化有效帧对应信号量为0，表示已接收到帧数为0 */
	share_queue->valid_frame_sem = sem_open(valid_sem_key, O_CREAT, 0666, 0);
	if (share_queue->valid_frame_sem == NULL) {
		perror("sem_open");
		exit(0);
	}

	/* 初始化空闲帧对应信号量为帧总数 */
	share_queue->free_frame_sem = sem_open(free_sem_key, O_CREAT, 0666, count);
	if (share_queue->free_frame_sem == NULL) {
		perror("sem_open");
		exit(0);
	}

	return share_queue;
}

void free_share_queue(share_queue_t *share_queue)
{
	if (share_queue) {
		free(share_queue->share_frames);
		sem_destroy(share_queue->valid_frame_sem);
		sem_destroy(share_queue->free_frame_sem);
		free(share_queue);
	}
}

void share_used_frame_inc(share_queue_t *share_queue)
{
	sem_post(share_queue->valid_frame_sem);
}

void share_used_frame_wait_dec(share_queue_t *share_queue)
{
	sem_wait(share_queue->valid_frame_sem);
}

void share_free_frame_inc(share_queue_t *share_queue)
{
	sem_post(share_queue->free_frame_sem);
}

void share_free_frame_wait_dec(share_queue_t *share_queue)
{
	sem_wait(share_queue->free_frame_sem);
}

void *wait_free_share_frame(share_queue_t *share_queue)
{
	/* 等待、递减信号量 */
	share_free_frame_wait_dec(share_queue);

	unsigned int get_index = atomic_load(share_queue->free_index);
	atomic_fetch_add(share_queue->free_index, 1);

	/* 处理队列环回 */
	if ((size_t)atomic_load(share_queue->free_index) >= share_queue->total_count)
		atomic_store(share_queue->free_index, 0);

	return &share_queue->share_frames[get_index];
}

void *wait_proc_share_frame(share_queue_t *share_queue)
{
	/* 等待、递减信号量 */
	share_used_frame_wait_dec(share_queue);

	unsigned int get_index = atomic_load(share_queue->proc_index);
	atomic_fetch_add(share_queue->proc_index, 1);

	/* 处理队列环回 */
	if ((size_t)atomic_load(share_queue->proc_index) >= share_queue->total_count)
		atomic_store(share_queue->proc_index, 0);

	return &share_queue->share_frames[get_index];
}



