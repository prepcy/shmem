#ifndef SHARE_UTILS_H
#define SHARE_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>

#define SHARE_COUNT 1024
#define SHARE_SIZE (32 * 1024)
#define SHARE_FILE	"/share_mem"
#define FREE_SEM	"/share_sem_free"
#define VALID_SEM	"/share_sem_valid"

typedef struct {
	long send_count;
	long recv_count;
	long send_len;
	long recv_len;
} info_t;

void timer_init(void);
void show_hex(uint8_t *data, int32_t len);
#define __printf(fmt, ...) \
    printf("[%s][%s:%d]  " fmt, __FILE__, __func__, __LINE__, ##__VA_ARGS__);

#endif

