#ifndef RTThread_FE
#define RTThread_FE

#include <stdlib.h>
#include <rtthread.h>
#include <RTOS_BE.h>
/*
// 信号处理相关函数
int signal_install(int sig, void (*func)(int));
int signal_mask(void);

// 任务创建相关函数
rt_uint8_t *rt_hw_stack_init(void *pEntry, void *pParam, rt_uint8_t *pStackAddr, void *pExit);

// 中断处理相关函数
rt_base_t rt_hw_interrupt_disable(void);
void rt_hw_interrupt_enable(rt_base_t level);

// 上下文切换函数
void rt_hw_context_switch(rt_uint32_t from, rt_uint32_t to);
void rt_hw_context_switch_interrupt(rt_uint32_t from, rt_uint32_t to);
void rt_hw_context_switch_to(rt_uint32_t to);
*/
// 开发板初始化函数
void rt_hw_board_init();

// 内存管理函数
void *rt_malloc(rt_size_t nbytes);
void rt_free(void *ptr);
void* rt_calloc(rt_size_t count, rt_size_t size);
void* rt_realloc(void *ptr, rt_size_t nbytes);
/**
                    rt_malloc(), rt_malloc_sethook()
                    rt_free(),   rt_free_sethook()
                    rt_calloc(), rt_realloc()
                    rt_memory_info()
                    rt_system_heap_init()*/

#endif