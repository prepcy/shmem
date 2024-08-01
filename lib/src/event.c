#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>


#include "event.h"
#include "linux_list.h"
#include "err.h"
#include "log.h"

/*
* @brief 获取当前的开机时间
* @return 	>0:当前的开机时间（单位秒）
		  	=0：读取失败
*/
static int32_t get_uptime()
{
    struct sysinfo info;

    if (sysinfo(&info))
        return 0;

    return info.uptime;
}

/*
* @brief 判断当前是否有定时器事件到期
* @param[in] loop 事件循环结构体
* @return 	=1:有定时器到期需执行
		  	=0：无定时器到期需执行
*/
static int32_t loop_any_timer_event_timeout(loop_t *loop)
{
	loop_timer_event_t *node;
	int32_t now = get_uptime();

	//如果定时器事件为空，则不可能有到期事件
	if (list_empty(&loop->timer_list))
		return 0;
	
	for_each_timer_event(node, loop, list) {
		//判断该定时器是否到期
		if (now >= node->dst_uptime)
			return 1;
	}
	return 0;
}

/*
* @brief 重置io事件描述符的定时器回调函数
* @param[in] loop 事件循环结构体
* @param[in] data 定时器回调函数的参数
* @return 	=0：成功
*/
static int loop_reset_io_event_timer_cb(loop_t *loop, void *data)
{
	io_event_t * event= (io_event_t *)data;
	struct epoll_event ev = {0};

	//调用注册的回调函数去初始化fd
	event->fd = event->fd_generater();

	//如果初始化失败，则定时5s之后继续尝试
	if (event->fd < 0) {
		loop_add_timer_event(loop, 5, loop_reset_io_event_timer_cb, event);
	} else {
		if (loop->epoll_fd >= 0) {
			ev.data.ptr = event;
			ev.events = EPOLLIN;
			epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, event->fd, &ev);
		}
	}
		
	return RET_OK;
}

static void loop_stop_io_event(loop_t *loop, io_event_t *event)
{
	if (!event || !loop)
		return;
	
	event->flags &= ~EVENT_NEED_STOP;
	
	if (event->fd >= 0) {
		close(event->fd);
		event->fd = -1;
	}
}


/*
* @brief 重置io事件描述符
* @param[in] loop 事件循环结构体
* @param[in] event io事件结构体
*/
static void loop_reset_io_event(loop_t *loop, io_event_t *event)
{
	if (!event || !loop)
		return;
	event->flags &= ~EVENT_NEED_RESET;
	
	if (event->fd >= 0) {
		close(event->fd);
		event->fd = -1;
	}

	//添加定时器，立刻执行reset fd的操作
	loop_add_timer_event(loop, 0, loop_reset_io_event_timer_cb, event);
}



void io_event_set_reset(io_event_t *event)
{
	if (event)
		event->flags |= EVENT_NEED_RESET;
}

void io_event_set_stop(io_event_t *event)
{
	if (event)
		event->flags |= EVENT_NEED_STOP;
}

/*
* @brief 处理io事件
* @param[in] loop 事件循环结构体
* @return 	RET_OK：成功
			RET_ERR：出错
			RET_TIMEOUT：超时
*/
static int32_t loop_handle_io_event(loop_t *loop)
{
    fd_set fds;
    int32_t max_fd = 0;
    struct timeval tv = {0};
    io_event_t *node, *next;
    int32_t ret;
    int32_t handler_ret;

    FD_ZERO(&fds);

	//将所有io事件添加到fd_set中
    for_each_io_event_safe(node, next, loop, list)
    {
		if (node->flags & EVENT_NEED_RESET)
			loop_reset_io_event(loop, node);
		if (node->flags & EVENT_NEED_STOP)
			loop_stop_io_event(loop, node);
		
		//如果fd<0表示该事件正在等待初始化
		if (node->fd >= 0) {
	        FD_SET(node->fd, &fds);
	        if (node->fd > max_fd)
	            max_fd = node->fd;
		}
    }
	//判断是否有定时器事件到期，如果有，则select函数不设置超时时间，立刻返回
	if (loop_any_timer_event_timeout(loop))
		tv.tv_sec = 0;
	else
		tv.tv_sec = 1;
	
    ret = select(max_fd + 1, &fds, NULL, NULL, &tv);

    if (ret < 0) 
        return RET_ERR;
	if (ret == 0)
		return RET_TIMEOUT;
		
    for_each_io_event_safe(node, next, loop, list) {
    	//如果有未初始化的fd，跳过后续处理
    	if (node->fd < 0)
			continue;
        if (FD_ISSET(node->fd, &fds)) {
            if (node->fd_ready_handler) {
				//调用数据读取回调函数
                handler_ret = node->fd_ready_handler(node->fd, node->data);
				//如果返回了RET_FD_RESET， 则尝试去reset描述符
                if (handler_ret == RET_FD_RESET)
					if (node->fd_generater) {
						loop_reset_io_event(loop, node);
					} else {
						loop_del_io_event(node);
					}
            }
        }
    }
	return RET_OK;
}
static int32_t loop_handle_io_event_epoll(loop_t *loop)
{
	int nfds, i;
	io_event_t *node;
	int timeout = -1;
	struct epoll_event events[MAX_EVENT] = {0};

	if (loop_any_timer_event_timeout(loop))
		timeout = 0;
	
	nfds = epoll_wait(loop->epoll_fd, events, MAX_EVENT, timeout);
	
	for(i = 0; i < nfds; i++) {
		if(events[i].events & EPOLLIN) {
			node = (io_event_t *)events[i].data.ptr;
			if (node->fd_ready_handler) {
				node->fd_ready_handler(node->fd, node->data);
			}
		}
	}
	
	return RET_OK;
}

/*
* @brief 处理定时器事件
* @param[in] loop 事件循环结构体
* @return 	RET_OK：成功
*/
static int loop_handle_timer_event(loop_t *loop)
{
	//获取当前的开机时间
	int32_t now = get_uptime();

	loop_timer_event_t *node, *next;

	for_each_timer_event_safe(node, next, loop, list) {
		//当前时间大于等于定时器的期望执行时间，则尝试执行其回调函数
		 if (now >= node->dst_uptime) {
            if (node->cb) {
                node->cb(loop, node->data);
            }
			//执行后删除该定时器
			loop_del_timer_event(node);
        }
	}
	return RET_OK;
}

/*
* @brief 处理事件
* @param[in] loop 事件循环结构体
*/
static void loop_run_once(loop_t *loop)
{
	loop_handle_timer_event(loop);
	if (loop->epoll_fd < 0)
		loop_handle_io_event(loop);
	else
		loop_handle_io_event_epoll(loop);
}

void free_io_event(io_event_t *event)
{
 	close(event->fd);
	free(event);	
}

/*
* @brief 停止io事件
* @param[in] event io事件结构体
*/
void loop_del_io_event(io_event_t *event)
{
	if (event) {
	    list_del(&event->list);
	    close(event->fd);
	    free(event);
	}
}

/*
* @brief 停止timer事件
* @param[in] event timer事件结构体
*/
void loop_del_timer_event(loop_timer_event_t *event)
{
	if (event) {
		list_del(&event->list);
		free(event);
	}
}

/*
* @brief 添加一个定时器事件
* @param[in] loop 事件循环结构体
* @param[in] timeout timer超时时间
* @param[in] _cb timer超时后执行的回调函数
* @param[in] data 传入回调函数的指针
* @return 	>0：返回新建的timer事件结构体指针
			NULL：出错
*/
loop_timer_event_t *loop_add_timer_event(loop_t *loop, int timeout, timer_event_cb _cb, void *data)
{
    loop_timer_event_t *event;

    if (!_cb) 
        return NULL;
    
	event = malloc(sizeof(loop_timer_event_t));
	if (!event)
		return NULL;
	
    event->cb = _cb;
    event->data = data;
	//目标执行时间设置为当前时间加上超时时间
    event->dst_uptime = get_uptime() + timeout;
	
	//添加结构体到链表中
	list_add_tail(&event->list, &loop->timer_list);
	
    return event;

}

/*
* @brief 初始化一个事件循环结构体
* @return 	>0：初始化的事件循环结构体指针
			NULL：出错
*/

loop_t *loop_new(int use_epoll)
{
	loop_t *loop = malloc(sizeof(loop_t));
	
	if (!loop)
			return NULL;
	loop->epoll_fd = -1;
	INIT_LIST_HEAD(&loop->io_list);
	INIT_LIST_HEAD(&loop->timer_list);

	if (use_epoll)
		loop->epoll_fd = epoll_create(MAX_EVENT);

	log_info("Using %s as loop event api.\n", use_epoll?"EPOLL":"SELECT");
	
	return loop;
}

/*
* @brief 释放一个事件循环结构体及其所有事件
* @param[in] loop 事件循环结构体
*/
void loop_free(loop_t *loop)
{
	io_event_t *ionode, *ionext;
	loop_timer_event_t *tnode, *tnext;

	//释放所有io事件
	for_each_io_event_safe(ionode, ionext, loop, list) {
		loop_del_io_event(ionode);
		
	}
	//释放所有timer事件
	for_each_timer_event_safe(tnode, tnext, loop, list) {
		loop_del_timer_event(tnode);
	}
	
	free(loop);
}

/*
* @brief 添加一个IO事件
* @param[in] loop 事件循环结构体
* @param[in] fd_ready_handler 套接字有数据可读时执行的回调函数
* @param[in] fd_generater 套接字初始化或者复位时执行的回调函数
* @param[in] module 该IO事件的标识
* @return 	>0：返回新建的IO事件结构体指针
			NULL：出错
*/
io_event_t *loop_add_io_event(loop_t *loop, fd_ready_handler_cb fd_ready_handler, 
		fd_generater_cb fd_generater, void *data)
{
	io_event_t *event;

	if (!loop || !fd_ready_handler || !fd_generater)
		return NULL;
	
    event = malloc(sizeof(io_event_t));
    if (!event)
        return NULL;

	//设置fd为-1，表示该fd未初始化
	event->fd = -1;
    event->fd_ready_handler = fd_ready_handler;
    event->fd_generater = fd_generater;
	event->data = data;
	event->flags = 0;
	
	//添加定时器去立刻执行fd_generater初始化函数；
	//因为该函数可能会失败，所以以定时器的形式，
	//如果失败则继续启动定时器
	loop_add_timer_event(loop, 0, loop_reset_io_event_timer_cb, event);

    list_add_tail(&event->list, &loop->io_list);
    return event;
}

/*
* @brief 通过已有的fd添加一个IO事件
* @param[in] loop 事件循环结构体
* @param[in] fd_ready_handler 套接字有数据可读时执行的回调函数
* @param[in] fd 套接字描述符
* @param[in] module 该IO事件的标识
* @return 	>0：返回新建的IO事件结构体指针
			NULL：出错
*/
io_event_t *loop_add_io_event_with_fd(loop_t *loop, fd_ready_handler_cb fd_ready_handler, 
		int fd, void *data)
{
	io_event_t *event;
	struct epoll_event ev = {0};

    event = malloc(sizeof(io_event_t));
    if (!event)
        return NULL;

	event->fd = fd;
    event->fd_ready_handler = fd_ready_handler;
    event->fd_generater = NULL;
	event->data = data;
	event->flags = 0;

	if (loop) {
    	list_add(&event->list, &loop->io_list);
		if (loop->epoll_fd >= 0) {
			ev.data.ptr = event;
			ev.events = EPOLLIN;
			epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, event->fd, &ev);
		}
	}
    return event;
}

/*
* @brief 通过已有的fd获取IO事件结构体
* @param[in] loop 事件循环结构体
* @param[in] fd 套接字描述符
* @return 	>0：获取的IO事件结构体指针
			NULL：出错
*/
io_event_t *loop_get_io_event_by_fd(loop_t *loop, int32_t fd)
{
	io_event_t *node;

	for_each_io_event(node, loop, list) {
		if (node->fd == fd)
			return node;
	}
	return NULL;
}

/*
* @brief 开始事件循环，每次循环会遍历所有的IO事件与timer事件，
*		 如果有准备好的事件立即执行，否则休眠1s后继续遍历
* @param[in] loop 事件循环结构体
*/
void loop_run(loop_t *loop)
{
	while (1)
		loop_run_once(loop);
}

