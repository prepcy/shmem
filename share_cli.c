#include "share_queue.h"
#include "share_utils.h"

#define SIZE 1000

info_t info;

void main(void)
{
	char tmp[1600] = {0xff};
	share_frame_t *share_frame;
	share_mem_t *share_mem;
	uint8_t *data;
	share_queue_t *queue = alloc_share_queue(SHARE_SLAVE, SHARE_FILE, FREE_SEM, VALID_SEM,
							SHARE_COUNT, SHARE_SIZE, sizeof(share_frame_t));

	timer_init();

	// 发送端
	while(1) {
		if (info.send_count >= 10000000)
			while(1);
			//sleep(100);
		share_mem = wait_free_share_frame(queue);

		//__printf("get sem \n");
		share_frame = (share_frame_t *)share_mem->data;
		data = (uint8_t *)share_frame+sizeof(share_frame_t);

		//share_frame->flag = i;
		share_frame->len = SIZE;
		memset(data, 0xff, SIZE);
		data[0] = 0x45;
		data[SIZE-1] = 0x54;

		info.send_count++;
		info.send_len += share_frame->len;

		share_used_frame_inc(queue);
		//__printf("send sem \n");
	}

}

