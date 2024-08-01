#ifndef SHMEM_H
#define SHMEM_H

#include "log.h"
#include "utils.h"
#include "xdma_queue.h"

#define IP_SIZE 1600
#define QUEUE_COUNT 1024

#define SEM_FREE "/ft_sem_free"
#define SEM_VALID "/ft_sem_valid"
#define SHM_NAME "/ft_shm_name"

typedef struct  {
	xdma_queue_t *xdma_queue;
	xdma_frame_t *send_frame;
} info_t;

#endif

