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

#include "nty_coroutine.h"

pthread_key_t global_sched_key;
static pthread_once_t sched_key_once = PTHREAD_ONCE_INIT;


int _switch(nty_cpu_ctx *new_ctx, nty_cpu_ctx *cur_ctx);

#ifdef __i386__
__asm__ (
"    .text                                  \n"
"    .p2align 2,,3                          \n"
".globl _switch                             \n"
"_switch:                                   \n"
"__switch:                                  \n"
"movl 8(%esp), %edx      # fs->%edx         \n"
"movl %esp, 0(%edx)      # save esp         \n"
"movl %ebp, 4(%edx)      # save ebp         \n"
"movl (%esp), %eax       # save eip         \n"
"movl %eax, 8(%edx)                         \n"
"movl %ebx, 12(%edx)     # save ebx,esi,edi \n"
"movl %esi, 16(%edx)                        \n"
"movl %edi, 20(%edx)                        \n"
"movl 4(%esp), %edx      # ts->%edx         \n"
"movl 20(%edx), %edi     # restore ebx,esi,edi      \n"
"movl 16(%edx), %esi                                \n"
"movl 12(%edx), %ebx                                \n"
"movl 0(%edx), %esp      # restore esp              \n"
"movl 4(%edx), %ebp      # restore ebp              \n"
"movl 8(%edx), %eax      # restore eip              \n"
"movl %eax, (%esp)                                  \n"
"ret                                                \n"
);
#elif defined(__x86_64__)
//%rdi 保存第一个参数的值，即 new_ctx 的值，%rsi 保存第二 个参数的值，即保存 cur_ctx 的值。
__asm__ (
"    .text                                  \n"
"       .p2align 4,,15                                   \n"
".globl _switch                                          \n"
".globl __switch                                         \n"
"_switch:                                                \n"
"__switch:                                               \n"
"       movq %rsp, 0(%rsi)      # save stack_pointer     \n"
"       movq %rbp, 8(%rsi)      # save frame_pointer     \n"
"       movq (%rsp), %rax       # save insn_pointer      \n"
"       movq %rax, 16(%rsi)                              \n"
"       movq %rbx, 24(%rsi)     # save rbx,r12-r15       \n"
"       movq %r12, 32(%rsi)                              \n"
"       movq %r13, 40(%rsi)                              \n"
"       movq %r14, 48(%rsi)                              \n"
"       movq %r15, 56(%rsi)                              \n"
"       movq 56(%rdi), %r15                              \n"
"       movq 48(%rdi), %r14                              \n"
"       movq 40(%rdi), %r13     # restore rbx,r12-r15    \n"
"       movq 32(%rdi), %r12                              \n"
"       movq 24(%rdi), %rbx                              \n"
"       movq 8(%rdi), %rbp      # restore frame_pointer  \n"
"       movq 0(%rdi), %rsp      # restore stack_pointer  \n"
"       movq 16(%rdi), %rax     # restore insn_pointer   \n"
"       movq %rax, (%rsp)                                \n"
"       ret                                              \n"
);
#endif


static void _exec(void *lt) {
#if defined(__lvm__) && defined(__x86_64__)
	__asm__("movq 16(%%rbp), %[lt]" : [lt] "=r" (lt));
#endif

	nty_coroutine *co = (nty_coroutine*)lt;
	co->func(co->arg);      //在这里调用co需要的处理的真正函数
	co->status |= (BIT(NTY_COROUTINE_STATUS_EXITED) | BIT(NTY_COROUTINE_STATUS_FDEOF) | BIT(NTY_COROUTINE_STATUS_DETACH));
#if 1
	nty_coroutine_yield(co);//处理完以后在切回到主流程
#else
	co->ops = 0;
	_switch(&co->sched->ctx, &co->ctx);
#endif
}

extern int nty_schedule_create(int stack_size);



void nty_coroutine_free(nty_coroutine *co) {
	if (co == NULL) return ;
	co->sched->spawned_coroutines --;
#if 1
	if (co->stack) {
		free(co->stack);
		co->stack = NULL;
	}
#endif
	if (co) {
		free(co);
	}

}

static void nty_coroutine_init(nty_coroutine *co) {

	void **stack = (void **)(co->stack + co->stack_size);

	stack[-3] = NULL;
	stack[-2] = (void *)co;

	co->ctx.esp = (void*)stack - (4 * sizeof(void*));
	co->ctx.ebp = (void*)stack - (3 * sizeof(void*));
	co->ctx.eip = (void*)_exec;
	co->status = BIT(NTY_COROUTINE_STATUS_READY);
	
}

void nty_coroutine_yield(nty_coroutine *co) {
	co->ops = 0;
	_switch(&co->sched->ctx, &co->ctx);
}

static inline void nty_coroutine_madvise(nty_coroutine *co) {

	size_t current_stack = (co->stack + co->stack_size) - co->ctx.esp;
	assert(current_stack <= co->stack_size);

	if (current_stack < co->last_stack_size &&
		co->last_stack_size > co->sched->page_size) {
		size_t tmp = current_stack + (-current_stack & (co->sched->page_size - 1));
		assert(madvise(co->stack, co->stack_size-tmp, MADV_DONTNEED) == 0);
	}
	co->last_stack_size = current_stack;
}

int nty_coroutine_resume(nty_coroutine *co) {
	
	if (co->status & BIT(NTY_COROUTINE_STATUS_NEW)) {
		nty_coroutine_init(co); //初始化co主要是设置co中调用堆栈，以及switch是调用eip 保存的函数exec
	}

	nty_schedule *sched = nty_coroutine_get_sched();
	sched->curr_thread = co;
    _switch(&co->ctx, &co->sched->ctx);  //进行上下文切换（寄存器切换），切换一直执行co->eip保存的函数exce(),在exce中执行co->func(co->arg)
	sched->curr_thread = NULL;

	nty_coroutine_madvise(co);
#if 1
	if (co->status & BIT(NTY_COROUTINE_STATUS_EXITED)) {
		if (co->status & BIT(NTY_COROUTINE_STATUS_DETACH)) {
			printf("nty_coroutine_resume --> \n");
			nty_coroutine_free(co);
		}
		return -1;
	} 
#endif
	return 0;
}


void nty_coroutine_renice(nty_coroutine *co) {
	co->ops ++;
#if 1
	if (co->ops < 5) return ;
#endif
	printf("nty_coroutine_renice\n");
	TAILQ_INSERT_TAIL(&nty_coroutine_get_sched()->ready, co, ready_next);
	printf("nty_coroutine_renice 111\n");
	nty_coroutine_yield(co);
}


void nty_coroutine_sleep(uint64_t msecs) {
	nty_coroutine *co = nty_coroutine_get_sched()->curr_thread;

	if (msecs == 0) {
		TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);
		nty_coroutine_yield(co);
	} else {
		nty_schedule_sched_sleepdown(co, msecs);
	}
}

void nty_coroutine_detach(void) {
	nty_coroutine *co = nty_coroutine_get_sched()->curr_thread;
	co->status |= BIT(NTY_COROUTINE_STATUS_DETACH);
}

static void nty_coroutine_sched_key_destructor(void *data) {
	free(data);
}

static void nty_coroutine_sched_key_creator(void) {
        //pthread_key_create():第一个参数就是声明的pthread_key_t变量，第二个参数是一个清理函数，用来在线程释放该线程存储的时候被调用。
	assert(pthread_key_create(&global_sched_key, nty_coroutine_sched_key_destructor) == 0);
        //pthread_setspecific():当线程中需要存储特殊值的时候调用该函数，该函数有两个参数，第一个为前面声明的pthread_key_t变量，第二个为void*变量，用来存储任何类型的值。
	assert(pthread_setspecific(global_sched_key, NULL) == 0);
	
	return ;
}


// coroutine --> 
// create 
//
int nty_coroutine_create(nty_coroutine **new_co, proc_coroutine func, void *arg) {
    //pthread_once()本函数使用初值为PTHREAD_ONCE_INIT的sched_key_once变量保证nty_coroutine_sched_key_creator()函数在本进程执行序列中仅执行一次
	assert(pthread_once(&sched_key_once, nty_coroutine_sched_key_creator) == 0);
    nty_schedule *sched = nty_coroutine_get_sched();  //获取调度器

    if (sched == NULL) {  //如果调度器不存在，则创建调度器
		nty_schedule_create(0);
		
		sched = nty_coroutine_get_sched();
		if (sched == NULL) {
			printf("Failed to create scheduler\n");
			return -1;
		}
	}

    //创建协程
	nty_coroutine *co = calloc(1, sizeof(nty_coroutine));
	if (co == NULL) {
		printf("Failed to allocate memory for new coroutine\n");
		return -2;
	}
	//int posix_memalign (void **memptr,size_t alignment,size_t size);
	//调用posix_memalign( )成功时会返回size字节的动态内存，并且这块内存的地址是alignment的倍数。参数alignment必须是2的幂，还是void指针的大小的倍数。返回的内存块的地址放在了memptr里面，内存开辟成功函数返回值是0.
	//开辟的内存 通过 free(memptr)进行释放
	int ret = posix_memalign(&co->stack, getpagesize(), sched->stack_size); //getpagesize()返回内存分页大小
	if (ret) {
		printf("Failed to allocate stack for new coroutine\n");
		free(co);
		return -3;
	}

	co->sched = sched;
	co->stack_size = sched->stack_size;
	co->status = BIT(NTY_COROUTINE_STATUS_NEW); //
	co->id = sched->spawned_coroutines ++;
	co->func = func;
#if CANCEL_FD_WAIT_UINT64
	co->fd = -1;
	co->events = 0;
#else
	co->fd_wait = -1;
#endif
	co->arg = arg;
    co->birth = nty_coroutine_usec_now(); //记录时间
	*new_co = co;

	TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);

	return 0;
}




