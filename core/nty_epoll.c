/*
 *  Author : WangBoJing , email : 1989wangbojing@gmail.com
 * 
 *  Copyright Statement:
 *  --------------------
 *  This software is protected by Copyright and the information contained
 *  herein is confidential. The software may not be copied and the information
 *  contained herein may not be used or disclosed except with the written
 *  permission of Author. (C) 2017
 * 
 *

****       *****                                      *****
  ***        *                                       **    ***
  ***        *         *                            *       **
  * **       *         *                           **        **
  * **       *         *                          **          *
  *  **      *        **                          **          *
  *  **      *       ***                          **
  *   **     *    ***********    *****    *****  **                   ****
  *   **     *        **           **      **    **                 **    **
  *    **    *        **           **      *     **                 *      **
  *    **    *        **            *      *     **                **      **
  *     **   *        **            **     *     **                *        **
  *     **   *        **             *    *      **               **        **
  *      **  *        **             **   *      **               **        **
  *      **  *        **             **   *      **               **        **
  *       ** *        **              *  *       **               **        **
  *       ** *        **              ** *        **          *   **        **
  *        ***        **               * *        **          *   **        **
  *        ***        **     *         **          *         *     **      **
  *         **        **     *         **          **       *      **      **
  *         **         **   *          *            **     *        **    **
*****        *          ****           *              *****           ****
                                       *
                                      *
                                  *****
                                  ****



 *
 */

#include <sys/eventfd.h>

#include "nty_coroutine.h"


int nty_epoller_create(void) {
	return epoll_create(1024);
} 

int nty_epoller_wait(struct timespec t) {
	nty_schedule *sched = nty_coroutine_get_sched();
	return epoll_wait(sched->poller_fd, sched->eventlist, NTY_CO_MAX_EVENTS, t.tv_sec*1000.0 + t.tv_nsec/1000000.0);
}

int nty_epoller_ev_register_trigger(void) {
	nty_schedule *sched = nty_coroutine_get_sched();

	if (!sched->eventfd) {
            sched->eventfd = eventfd(0, EFD_NONBLOCK); //eventfd()是Linux 2.6提供的一种系统调用，它可以用来实现事件通知;参数:计数器值为0，非阻塞
            assert(sched->eventfd != -1);
	}

	struct epoll_event ev;
        ev.events = EPOLLIN;   //设置为可读事件
	ev.data.fd = sched->eventfd;
        int ret = epoll_ctl(sched->poller_fd, EPOLL_CTL_ADD, sched->eventfd, &ev);  //添加epoll的监听事件

	assert(ret != -1);
}


