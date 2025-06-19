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

#define XDMA_DMA_MEM_ALIGN 4096

/**
 * 计算大于等于(n/100)的最小2次幂
 * @param n 输入参数
 * @return 结果
 */
static unsigned int ceil_power_of_two(int n) {
    // 处理特殊值：当n<=0时，2^0=1是最小的2次幂
    if (n <= 0) {
        return 1;
    }

    // 计算n/100并向上取整（注意n是整数，除法向下取整，需要修正）
    unsigned int val = (n + 99) / 100;

    // 如果val是0（即n<100），结果为1
    if (val == 0) {
        return 1;
    }

    // 特殊情况：当val本身就是2次幂时
    if ((val & (val - 1)) == 0) {
        return val;
    }

    // 使用二进制位操作找到大于等于val的最小2次幂
    val--;
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val |= val >> 16;
    val++;

    return val;
}

/**
 * 将大小对齐到DMA内存对齐大小
 * @param size 大小
 * @return 对齐后的大小
 */
static int size_align_to_dma(int size)
{
	int div, mod;

	div = size / XDMA_DMA_MEM_ALIGN;
	mod = size % XDMA_DMA_MEM_ALIGN;

	/* when size equal 4096 return 4096, when size equal 4097 return 8192*/
	if (mod)
		return div * XDMA_DMA_MEM_ALIGN + XDMA_DMA_MEM_ALIGN;
	else
		return div * XDMA_DMA_MEM_ALIGN;
}

/**
 * 创建共享内存
 * @param file 共享文件
 * @param size 共享内存大小
 * @return 共享内存地址
 */
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

/**
 * 分配共享内存队列
 * @param flag 标志
 * @param share_file 共享文件
 * @param free_sem_key 空闲信号量
 * @param valid_sem_key 有效信号量
 * @param count 帧数
 * @param frame_size 帧大小
 * @return 共享内存队列
 */
share_queue_t *alloc_share_queue(int flag, char* share_file, char* free_sem_key, char* valid_sem_key,
										int count, int frame_size)
{
	int i, offset = 0;
	share_queue_t *share_queue = NULL;

	/* 计算table */
	int max_table_num = ceil_power_of_two(count);
	int table_size = max_table_num * sizeof(uint32_t);
	/* 计算table的索引 */
	int table_index_size = sizeof(uint32_t);


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

	/* 计算总长度，并4k对齐 */
	int total_len = (frame_size + table_size + table_index_size) * count + 2*sizeof(atomic_int);
	total_len = size_align_to_dma(total_len);

	/* 创建共享内存，最开始是2个原子变量，后面就是 count x frame_len 数据缓冲区 */
	char *data_tmp = create_shared_memory(share_file, total_len);
	/* 必须先将4k对齐的地址分支给 32k*1024 的buff */
	share_queue->data_ptr = (unsigned char*)(data_tmp);
	share_queue->total_count = count;

	/* 添加偏移量，存放原子变量 */
	offset += frame_size * count;
	share_queue->free_index = (atomic_int*)(data_tmp + offset);

	/* 添加偏移量，存放原子变量 */
	offset += sizeof(atomic_int);
	share_queue->proc_index = (atomic_int*)(data_tmp + offset);

	/* 添加偏移量，存放table的buff地址 */
	offset += sizeof(atomic_int);
	unsigned char *table_index = (unsigned char*)(data_tmp + offset);

	/* 存放table的buff地址 */
	offset += table_index_size * count;
	unsigned char *table = (unsigned char*)(data_tmp + offset);

	/* 主线程需要初始化原子变量 */
	if (flag == SHARE_MASTER) {
		memset(data_tmp, 0, sizeof(char) * total_len);
		atomic_init(share_queue->free_index, 0);
		atomic_init(share_queue->proc_index, 0);
	}

	/* 将申请的DMA内存分帧长赋值给帧管理结构体 */
	for (i = 0; i < count; i++) {
		/* 数据缓冲区，每帧32k字节 */
		share_queue->share_frames[i].data = share_queue->data_ptr + frame_size * i;
		/* 帧管理结构体，每帧512*4字节，用于存放table */
		share_queue->share_frames[i].table = (uint32_t*)(table + table_size * i);
		/* 帧管理结构体，每帧4字节，用于存放table的索引 */
		share_queue->share_frames[i].table_index = (uint32_t*)(table_index + table_index_size * i);
		// printf("share_queue->share_frames[%d].table_index %p. %d\n", i, share_queue->share_frames[i].table_index, *share_queue->share_frames[i].table_index);
	}


	/* 信号量和共享内存一样，可以被其他进程访问，就不用添加到共享内存里 */
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

	share_queue->max_table_num = max_table_num;
	share_queue->buff_size = frame_size;

	return share_queue;
}

/**
 * 释放共享内存队列
 * @param share_queue 共享内存队列
 */
void free_share_queue(share_queue_t *share_queue)
{
	if (share_queue) {
		free(share_queue->share_frames);
		sem_destroy(share_queue->valid_frame_sem);
		sem_destroy(share_queue->free_frame_sem);
		free(share_queue);
	}
}

/**
 * 增加已使用共享内存的缓冲区
 * @param share_queue 共享内存队列
 */
void share_used_frame_inc(share_queue_t *share_queue)
{
	sem_post(share_queue->valid_frame_sem);
}

/**
 * 等待已使用共享内存的缓冲区
 * @param share_queue 共享内存队列
 */
void share_used_frame_wait_dec(share_queue_t *share_queue)
{
	sem_wait(share_queue->valid_frame_sem);
}

/**
 * 增加空闲共享内存的缓冲区
 * @param share_queue 共享内存队列
 */
void share_free_frame_inc(share_queue_t *share_queue)
{
	sem_post(share_queue->free_frame_sem);
}

/**
 * 等待空闲共享内存的缓冲区
 * @param share_queue 共享内存队列
 * @return 缓冲区地址
 */
void share_free_frame_wait_dec(share_queue_t *share_queue)
{
	sem_wait(share_queue->free_frame_sem);
}

/**
 * 等待空闲共享内存的缓冲区
 * @param share_queue 共享内存队列
 * @return 缓冲区地址
 */
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

/**
 * 等待接收共享内存的缓冲区
 * @param share_queue 共享内存队列
 * @return 缓冲区地址
 */
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

/**
 * 发送共享内存的缓冲区
 * @param share_queue 共享内存队列
 * @param data 数据地址
 * @param len 数据长度
 * @return 数据长度
 */
uint32_t send_share_frame(share_queue_t *share_queue, uint8_t *data, uint32_t len)
{
	static share_mem_t *share_mem = NULL;
	static uint32_t total_len = 0;


	if (!share_mem) {
		share_mem = wait_free_share_frame(share_queue);
		*share_mem->table_index = 0;
	}


	/* 存放table */
	share_mem->table[*share_mem->table_index] = len;

	/* 存放table的索引 */
	(*share_mem->table_index)++;

	/* 存放数据 */
	memcpy(share_mem->data + total_len, data, len);

	total_len += len;

	/* 如果数据长度大于32k，则需要切换帧 */
	if (total_len + 1600 >= share_queue->buff_size) {
		share_used_frame_inc(share_queue);
		total_len = 0;
		share_mem = NULL;
	}

	return len;
}

/**
 * 接收共享内存的缓冲区
 * @param share_queue 共享内存队列
 * @param data 数据地址
 * @param len 数据长度
 * @return 数据长度
 */
uint32_t recv_share_frame(share_queue_t *share_queue, uint8_t *data, uint32_t len __attribute__((unused)))
{
	static share_mem_t *share_mem = NULL;
	static uint32_t index = 0;

	if (!share_mem)
		share_mem = wait_proc_share_frame(share_queue);

	/* 获取table的长度 */
	uint32_t table_len = share_mem->table[index++];

	/* 获取数据 */
	memcpy(data, share_mem->data + table_len, table_len);

	/* 如果table的索引大于table的个数，则需要切换帧 */
	if (index >= *share_mem->table_index) {
		share_free_frame_inc(share_queue);
		*share_mem->table_index = 0;
		memset(share_mem->data, 0, share_queue->buff_size);
		share_mem = NULL;
		index = 0;
	}

	return table_len;
}







/**
 * 获取发送共享内存的缓冲区
 * @param share_queue 共享内存队列
 * @return 缓冲区地址
 */
static uint32_t total_len = 0;
static share_mem_t *share_mem = NULL;
uint8_t * get_send_share_buff(share_queue_t *share_queue)
{
	if (!share_mem) {
		share_mem = wait_free_share_frame(share_queue);
		*share_mem->table_index = 0;
	}

	return share_mem->data + total_len;
}

/**
 * 发送共享内存的缓冲区
 * @param share_queue 共享内存队列
 * @param len 数据长度
 * @return 数据长度
 */
uint32_t send_share_frame_with_ptr(share_queue_t *share_queue, uint32_t len)
{
	/* 存放table */
	share_mem->table[*share_mem->table_index] = len;

	/* 存放table的索引 */
	(*share_mem->table_index)++;

	total_len += len;

	/* 如果数据长度大于32k，则需要切换帧 */
	if (total_len + 1600 >= share_queue->buff_size) {
		share_used_frame_inc(share_queue);
		total_len = 0;
		share_mem = NULL;
	}

	return len;
}

/**
 * 接收共享内存的缓冲区
 * @param share_queue 共享内存队列
 * @param data 数据地址
 * @param len 数据长度
 * @return 数据长度
 */
uint32_t recv_share_frame_with_ptr(share_queue_t *share_queue, uint8_t **data, uint32_t len __attribute__((unused)))
{
	static share_mem_t *share_mem = NULL;
	static uint32_t index = 0;

	if (!share_mem)
		share_mem = wait_proc_share_frame(share_queue);

	/* 获取table的长度 */
	uint32_t table_len = share_mem->table[index++];

	/* 获取数据 */
	*data = (uint8_t*)(&share_mem->data[table_len]);

	/* 如果table的索引大于table的个数，则需要切换帧 */
	if (index >= *share_mem->table_index) {
		share_free_frame_inc(share_queue);
		*share_mem->table_index = 0;
		share_mem = NULL;
		index = 0;
	}

	return table_len;
}
