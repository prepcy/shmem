#include "share_queue.h"
#include "share_utils.h"

info_t info;

void main(void)
{
	char tmp[1600] = {0xff};
	share_frame_t *share_frame;
	share_mem_t *share_mem;
	uint8_t *data;
	share_queue_t *queue = alloc_share_queue(SHARE_MASTER, SHARE_FILE, FREE_SEM, VALID_SEM,
							SHARE_COUNT, SHARE_SIZE, sizeof(share_frame_t));

	timer_init();

	// 接收端
	while(1) {
		share_mem = wait_proc_share_frame(queue);

		//__printf("get sem \n");
		share_frame = (share_frame_t *)share_mem->data;
		data = (uint8_t *)share_frame+sizeof(share_frame_t);

		//show_hex(share_mem->data, 64);

		//if ( (share_frame->flag == i) && (data[0] == 0x45) ) {
		if ( (data[share_frame->len-1] == 0x54) && (data[0] == 0x45) ) {
			memcpy(tmp, data, share_frame->len);
			info.recv_count++;
			info.recv_len += share_frame->len;
			memset(share_mem->data, 0x0, share_frame->len);
		}

		share_free_frame_inc(queue);
		//__printf("send sem \n");
	}


}
