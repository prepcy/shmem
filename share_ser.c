#include "share_queue.h"
#include "share_utils.h"

info_t info;

int main(void)
{
	uint32_t len = 0;
	uint8_t data[1600] = {0xff};
	share_queue_t *queue = alloc_share_queue(SHARE_MASTER, SHARE_FILE, FREE_SEM, VALID_SEM,
							SHARE_COUNT, SHARE_SIZE);

	timer_init();

	// 接收端
	while(1) {
		len = recv_share_frame(queue, data, 1600);

		if ( (len == 1500) && (data[0] == 0x45) ) {
			info.recv_count++;
			info.recv_len += len;
		}
	}

	return 0;
}
