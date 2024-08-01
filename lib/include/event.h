#ifndef __IO__H__
#define __IO__H__

#include <stdint.h>
#include "linux_list.h"

typedef struct loop_s loop_t;

#define MAX_EVENT 32

//定时器回调函数原型
typedef int (*timer_event_cb)(loop_t *loop, void *data);

//定时器事件结构体
typedef struct {
    timer_event_cb cb;		//定时器回调函数指针
    int32_t dst_uptime;		//超时时间，值为开机时间的绝对值
    void *data;				//传入定时器回调函数的参数
	struct list_head list;	//链表结构
} loop_timer_event_t;

typedef struct io_event_s io_event_t;

//套接字数据读取回调函数原型
typedef int32_t (*fd_ready_handler_cb)(int32_t fd, void *data);

//套接字初始化或者复位回调函数原型
typedef int32_t (*fd_generater_cb)(void);

enum io_event_flag {
	EVENT_NEED_RESET = 1 << 0,
	EVENT_NEED_STOP = 2 << 0,
};
//io事件结构体
typedef struct io_event_s {
    int32_t fd;									//io对应的描述符
    void *data;							//io所属的模块
    struct list_head list;						//链表结构
    fd_ready_handler_cb fd_ready_handler;		//数据读取函数指针
    fd_generater_cb fd_generater;				//套接字复位函数指针
    int flags;
} io_event_t;


//事件循环结构体
typedef struct loop_s {
	struct list_head io_list;		//io时间链表头
	struct list_head timer_list;	//定时器事件链表头
	int epoll_fd;
}loop_t;

//使用该宏来定义一个事件循环结构体
#define DECLARE_LOOP(name) \
		static loop_t name = { \
			LIST_HEAD_INIT(name.io_list), \
			LIST_HEAD_INIT(name.timer_list), \
		}


//遍历loop中每一个io事件
#define for_each_io_event(node, loop, mem) \
		list_for_each_entry(node, (&(loop)->io_list), mem)
		
//遍历loop中每一个timer事件		
#define for_each_timer_event(node, loop, mem) \
		list_for_each_entry(node, (&(loop)->timer_list), mem)
		
//遍历loop中每一个io事件并可能删除某个事件
#define for_each_io_event_safe(node, next, loop, mem) \
    	list_for_each_entry_safe(node, next, (&(loop)->io_list), mem)
    	
//遍历loop中每一个io事件并可能删除某个事件
#define for_each_timer_event_safe(node, next, loop, mem) \
    	list_for_each_entry_safe(node, next, (&(loop)->timer_list), mem)


loop_t *loop_new(int use_epoll);

void loop_free(loop_t *loop);
void free_io_event(io_event_t *event);

io_event_t *loop_add_io_event(loop_t *loop, fd_ready_handler_cb fd_ready_handler, 
		fd_generater_cb fd_generater, void *data);

io_event_t *loop_add_io_event_with_fd(loop_t *loop, fd_ready_handler_cb fd_ready_handler, 
		int32_t fd, void *data);

void loop_del_io_event(io_event_t *event);
io_event_t *loop_get_io_event_by_fd(loop_t *loop, int32_t fd);

loop_timer_event_t *loop_add_timer_event(loop_t *loop, int timeout, timer_event_cb _cb, void *data);

void loop_del_timer_event(loop_timer_event_t *event);


void loop_run(loop_t *loop);
void io_event_set_reset(io_event_t *event);
void io_event_set_stop(io_event_t *event);



#endif //__IO__H__
