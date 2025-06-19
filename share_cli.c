#include "share_queue.h"
#include "share_utils.h"

#define SIZE 1500

info_t info;

int main(void)
{
	share_queue_t *queue = alloc_share_queue(SHARE_SLAVE, SHARE_FILE, FREE_SEM, VALID_SEM,
							SHARE_COUNT, SHARE_SIZE);
	timer_init();

	uint8_t data[SIZE] = {
	0x45, 0x00, 0x04, 0x1c, 0xe8, 0xe0 ,0x40, 0x00,
	0x40, 0x11, 0x00, 0x00, 0x10, 0x01, 0x03, 0x69,
	0x10, 0x01, 0x03, 0x64
	};

	// 发送端
	while(1) {
		if (info.send_count >= 10000000)
			while(1) sleep(1);

		uint32_t len = send_share_frame(queue, data, SIZE);
		info.send_count++;
		info.send_len += len;
	}

	return 0;
}

