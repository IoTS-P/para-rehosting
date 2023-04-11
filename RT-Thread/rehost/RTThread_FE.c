#include <RTThread_FE.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
/*
// 模拟 RT-Thread 的时钟中断处理程序
void TickSignalHandler(int sig) {
    (void)sig;
    void* current = NULL, *next = NULL;
    int cur_idx = -1, next_idx = -1;
    if(PMCU_IsInterruptEnabled() == 1 && PMCU_IsServicingTick() != 1) {
        if(PMCU_SingleTryLock() == 0) {
            // 模拟调度 RT-Thread 线程
            PMCU_EnterServicingTick();
            rt_tick_increase();
            current = rt_thread_self()->entry;
            cur_idx = PMCU_Find_Idx(current);
            rt_schedule();
            next = rt_thread_self()->entry;
            next_idx = PMCU_Find_Idx(next);
            // 若发生了线程切换，则进行 PMCU 调度
            if (cur_idx != next_idx) {
                PMCU_Schedule(cur_idx, next_idx);
            } else {
                PMCU_SingleUnlock();
            }
            PMCU_ExitServicingTick();
        } else {
            PMCU_SetPending();
        }
    } else {
        PMCU_SetPending();
    }
}
*/
// 模拟线程从临界区域启动
void PMCU_FE_Yield() {
    void *cur = NULL, *next = NULL; 
    int cur_idx = -1, next_idx = -1;
    PMCU_SingleLock();
    // 模拟调度 RT-Thread 线程
    cur = rt_thread_self()->entry;
    cur_idx = PMCU_Find_Idx(cur);
    rt_schedule();
    next = rt_thread_self()->entry;
    next_idx = PMCU_Find_Idx(next);
    // 若发生了线程切换，则进行 PMCU 调度
    if (cur_idx != next_idx) {
        PMCU_Schedule(cur_idx, next_idx);
    } else {
        PMCU_SingleUnlock();
    }
}

// 执行线程结束的函数: 什么都不做
void FE_Thread_Exit() {}
/*
// 设置信号处理函数
int signal_install(int sig, void (*func)(int)) {
    struct sigaction act;
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(sig, &act, 0);
}

// 设置 signal mask
int signal_mask(void) {
    sigset_t  sigmask, oldmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask);
}

// 由于 Rt-thread 的任务包含任务函数和结束处理函数，先对这两个函数进行包装
static void *rt_thread_warpper(void *pEntry, void *pParam, void *pExit) {
    // 首先执行正常的任务函数
    ((void (*)(void *))pEntry)(pParam);
    // 任务函数结束后执行退出函数
    ((void (*)())pExit)();
}

// 创建模拟线程和任务的栈空间
rt_uint8_t *rt_hw_stack_init(void *pEntry, void *pParam, rt_uint8_t *pStackAddr, void *pExit) {
    // 创建 PMCU 线程
    void **entry = (void **)(pStackAddr - sizeof(void *));
    *entry = pEntry;
    // RT-Thread 通过 sp 切换上下文，在 sp 处保存一个 entry 备份
    PMCU_CreateNewThread(pStackAddr, rt_thread_warpper, pEntry, pParam, pExit, TickSignalHandler);
    return entry;
}

// 中断相关函数
rt_base_t rt_hw_interrupt_disable(void) {
    PMCU_DisableInterrupts();
}
void rt_hw_interrupt_enable(rt_base_t level) {
    if (level)
        PMCU_EnableInterrupts();
    else
        PMCU_DisableInterrupts();
}

// 上下文切换函数
void rt_hw_context_switch(rt_uint32_t from, rt_uint32_t to) {
    void *cur = NULL, *next = NULL; 
    int cur_idx = -1, next_idx = -1;
    cur = *((void **)(from));
    cur_idx = PMCU_Find_Idx(cur);
    next = *((void **)(to));
    next_idx = PMCU_Find_Idx(next);
    PMCU_Schedule(cur_idx, next_idx);
    return;
}
void rt_hw_context_switch_interrupt(rt_uint32_t from, rt_uint32_t to) {
    rt_hw_context_switch(from, to);
}
void rt_hw_context_switch_to(rt_uint32_t to) {
    void *cur = NULL, *next = NULL; 
    int cur_idx = -1, next_idx = -1;
    cur = rt_thread_self()->entry;
    cur_idx = PMCU_Find_Idx(cur);
    if (cur_idx == -1) {
        PMCU_StartScheduler(0, RT_TICK_PER_SECOND);
        return;
    }
    next = *((void **)(to));
    next_idx = PMCU_Find_Idx(next);
    PMCU_Schedule(cur_idx, next_idx);
    return;
}
*/
// 开发板初始化函数
void rt_hw_board_init() {
    return;
}

// 内存管理函数:直接替换为 libc
void *rt_malloc(rt_size_t nbytes) {
    return malloc(nbytes);
}

void rt_free(void *ptr) {
    free(ptr);
}

void* rt_calloc(rt_size_t count, rt_size_t size) {
    return malloc(size * count);
}

void* rt_realloc(void *ptr, rt_size_t nbytes) {
    return realloc(ptr, nbytes);
}