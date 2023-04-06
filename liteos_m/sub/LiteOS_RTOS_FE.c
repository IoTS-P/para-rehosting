#include "RTOS_BE.h"
#include "LiteOS_RTOS_FE.h"
#include "los_task.h"
// #include "los_task_pri.h"

extern LosTask g_losTask;
extern void OsTickHandler(void);
extern volatile unsigned long g_tickCount[1];
unsigned int g_tickPerSecond = LOSCFG_BASE_CORE_TICK_PER_SECOND;

// #define ArchTaskSchedule OsTaskSchedule

void (*tick_handler)(void);
int tick_handler_not_set = 1;

const int    OS_FPU_CPACR               = 0xE000ED88;
const int    OS_FPU_CPACR_ENABLE        = 0x00F00000;
const int    OS_NVIC_INT_CTRL           = 0xE000ED04;
const int    OS_NVIC_SYSPRI2            = 0xE000ED20;
const int    OS_NVIC_PENDSV_PRI         = 0xF0F00000;
const int    OS_NVIC_PENDSVSET          = 0x10000000;
const int    OS_CONTROL_FPCA_ENABLE     = 0x4;
const int    OS_CONTROL_KERNEL_MODE     = 0x2;

void HalStartToRun(void){
//    __asm__(
//            "mov ecx, OS_NVIC_SYSPRI2;"
//            "mov edx, OS_NVIC_PENDSV_PRI;"
//            "mov [ecx], edx;"
//            "mov eax, OS_CONTROL_KERNEL_MODE;"
//            "mov cr0, eax;"
//            "mov ecx, DWORD PTR g_losTask;"
//            "mov edx, [ecx + 4];"
//            "mov esi, [edx];"
//            "sub esi, 100;"
//            "push eax;"
//            "push ebx;"
//            "push ecx;"
//            "push edx;"
//            "push esi;"
//            "push edi;"
//            "push ebp;"
//            "push esi;"
//            "fldz;"
//            "fstp st(0);"
//            "lea esp, [esi+72];"
//            "pop ebp;"
//            "sti;"
//            "jmp dword ptr [esi+24];"
//            );
}

int Hard_Fault_FE(){
    return Fault_BE();
}
/*
* Interrupt
*/
unsigned int ArchIntLock(){
    return PMCU_BE_Disable_Irq();
}

unsigned int ArchIntUnlock(){
    return PMCU_BE_Enable_Irq();
}

void ArchIntRestore(unsigned int save){
    PMCU_BE_Enable_Irq();
}

/*
 * Clock
*/
void HalClockInit(void){}
void HalClockStart(void){}

//uint64_t HalClockGetCycles(void){
//    return g_tickCount[0];
//}

void HalDelayUs(uint32_t usecs){}

/*
* Task
*/
//void PMCU_FE_Task_Create(void* func, unsigned int params[], int param_count, unsigned int taskid){
//    if(tick_handler_not_set){
//        g_tickCount[0] = 1000;
//        tick_handler = OsTickHandler;
//        tick_handler_not_set = 0;
//    }else{
//        tick_handler = NULL;
//    }
//    PMCU_BE_Task_Create(func, params, param_count, taskid, tick_handler);
//}

void ArchTaskSchedule(){
    Pthread_Schedule(g_losTask.runTask->taskID, g_losTask.newTask->taskID);
}
//void OsStartToRun(void *task){
//    g_runTask = task;
//    g_oldTask = task;
//    *(uint16_t*)(task+4) = 0x0010U; //OS_TASK_STATUS_RUNNING;
//    unsigned int taskid = *(unsigned int*)(task+20);
//    Start_Scheduler(taskid, 1000000 / g_tickPerSecond);
//}

void ArchExcInit(void){}


// patch for c99
size_t strnlen(const char *s, size_t n)
{
	const char *p = memchr(s, 0, n);
	return p ? p-s : n;
}

void error(){
    puts("ERROR");
}

void HalExcNMI(){error();}
void HalExcHardFault(){error();}
void HalExcMemFault(){error();}
void HalExcBusFault(){error();}
void HalExcUsageFault(){error();}
void HalExcSvcCall(){error();}
void HalPendSV(){error();}