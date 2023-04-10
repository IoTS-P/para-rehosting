#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include "RTOS_BE.h"

#define DEBUG

#define SIG_SUSPEND             SIGUSR1
#define SIG_RESUME              SIGUSR2
#define SIG_TICK                SIGALRM
#define TIMER_TYPE              ITIMER_REAL 
// You may need to adjust this part for different OS
#define THREAD_NUMBER     16
#define MAX_NUMBER_OF_PARAMS    4
#define THREAD_FUNC             (*real_func)(unsigned int, unsigned int, unsigned int, unsigned int)
// End

const int __heap_start = 0x9000000;
const int __heap_size = 0x40000000;
unsigned int times_of_schedule = 0;

typedef struct ThreadState
{
    pthread_t       Thread;
    int             valid;    /* Treated as a boolean */
    unsigned int    taskid;
    int             critical_nesting;
    void            THREAD_FUNC;
    unsigned int    real_params[MAX_NUMBER_OF_PARAMS];
    int             param_number;
    int             index;
} PMCU_Thread;

int first_time = 1;
int critical_nest = 0;
PMCU_Thread pmcu_threads[THREAD_NUMBER];
// int next_valid_pmcu_thread = 0;
static pthread_mutex_t systick_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t single_core_mutex = PTHREAD_MUTEX_INITIALIZER;
int creat_not_done = 0;
void (*pmcu_systick_handler)(void);

// CPU fall into fault status
int Fault_BE(){
    // #ifdef DEBUG
    //     printf("Fault_BE()\n");
    // #endif
    abort();
    return 0;
}
/*
 * Interrupt
*/ 
int irq_disabled = 0;
unsigned int PMCU_BE_Disable_Irq(){
    irq_disabled = 1;
    // #ifdef DEBUG
    //     printf("PMCU_BE_Disable_Irq()\n");
    // #endif
    return irq_disabled;
}

unsigned int PMCU_BE_Enable_Irq(){
    irq_disabled = 0;
    // #ifdef DEBUG
    //     printf("PMCU_BE_Enable_Irq()\n");
    // #endif
    return irq_disabled;
}

unsigned int PMCU_BE_Enter_Critical(){
    critical_nest += 1;
    if(critical_nest > 0){
        irq_disabled = 1;
    }
    return irq_disabled;
}

unsigned int PMCU_BE_Leave_Critical(){
    critical_nest -= 1;
    if(critical_nest == 0){
        irq_disabled = 0;
    }
    if(critical_nest < 0){
        abort();
    }
    return irq_disabled;
}

/*
 * Systick
*/ 
void Setup_Ticker(unsigned int time_interval){
    #ifdef DEBUG
    printf("Setup_Ticker()\n");
    #endif
    struct itimerval src_timer, tar_timer;
    suseconds_t microSeconds = (suseconds_t)(time_interval % 1000000);
    time_t seconds = time_interval / 1000000;
    getitimer(TIMER_TYPE, &src_timer);
    src_timer.it_interval.tv_sec = seconds;
    src_timer.it_interval.tv_usec = microSeconds;
    src_timer.it_value.tv_sec = seconds;
    src_timer.it_value.tv_usec = microSeconds;
    setitimer(TIMER_TYPE, &src_timer, &tar_timer);
}

void Systick_Signal_Handler(int sig){
#ifdef DEBUG
    printf("Systick_Signal_Handler(int sig = %d)\n", sig);
#endif
    int ret = 0;
    ret = pthread_mutex_trylock(&systick_mutex); // if other systick is handling, ignore.
    if(ret == 0){
        pmcu_systick_handler();
        pthread_mutex_unlock(&systick_mutex);
    }
}

/*
 * Task or Thread
*/
void Suspend_Thread(pthread_t thread){
#ifdef DEBUG
    printf("Suspend_Thread() %lu\n", pthread_self());
#endif
    pthread_kill(thread, SIG_SUSPEND);
}
void Suspend_Signal_Handler(int sig){
    sigset_t signal;
    sigemptyset(&signal);
    sigaddset(&signal, SIG_RESUME);
#ifdef DEBUG
    printf("Suspend_Signal_Handler() %lu\n", pthread_self());
#endif
    creat_not_done = 0;
    pthread_mutex_unlock(&single_core_mutex);
    sigwait(&signal, &sig);
#ifdef DEBUG
    printf("Suspend_Signal_Handler() %lu exit\n", pthread_self());
#endif
}

void ResumeThread(pthread_t thread){
    pthread_mutex_lock(&single_core_mutex);
#ifdef DEBUG
    printf("ResumeThread() %lu\n", thread);
#endif
    pthread_kill(thread, SIG_RESUME);
#ifdef DEBUG
    printf("ResumeThread() %lu end\n", thread);
#endif
    pthread_mutex_unlock(&single_core_mutex);
}

void Resume_Signal_Handler(int sig){
#ifdef DEBUG
    printf("Resume_Signal_Handler()\n");
#endif
}

void Setup_Signal_Handler(){
#ifdef DEBUG
    printf("Setup_Signal_Handler()\n");
#endif
    struct sigaction suspend, resume, sigtick;
    suspend.sa_flags = 0;
    suspend.sa_handler = Suspend_Signal_Handler;
    sigfillset(&suspend.sa_mask);
    resume.sa_flags = 0;
    resume.sa_handler = Resume_Signal_Handler;
    sigfillset(&resume.sa_mask);
    sigtick.sa_flags = 0;
    sigtick.sa_handler = Systick_Signal_Handler;
    sigfillset(&sigtick.sa_mask);
    assert(sigaction(SIG_SUSPEND, &suspend, NULL ) == 0);
    assert(sigaction(SIG_RESUME, &resume, NULL ) == 0);
    assert(sigaction(SIG_TICK, &sigtick, NULL ) == 0);
}

void *Thread_Wrapper( void * params ){
    PMCU_Thread* thread = (PMCU_Thread*)params;
#ifdef DEBUG
    printf("Thread_Wrapper() %d\n", thread->taskid);
#endif
    pthread_mutex_lock(&single_core_mutex);
    // for pthread terminate
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    Suspend_Thread(pthread_self());
#ifdef DEBUG
    printf("Thread_Wrapper() %d start to run\n", thread->taskid);
#endif
    thread->real_func(thread->real_params[0], thread->real_params[1], thread->real_params[2], thread->real_params[3]);
    // clean routine may need to implement here.
    int i = 0;
    for(;i < THREAD_NUMBER;i++){
        if(pthread_equal(pmcu_threads[i].Thread, pthread_self())){
        #ifdef DEBUG
            printf("clear pmcu_threads[%d], taskid %d\n", i, pmcu_threads[i].taskid);
        #endif
            pmcu_threads[i].Thread = 0;
            pmcu_threads[i].taskid = -1;
            pmcu_threads[i].valid = 0;
            break;
        }
    }
    pthread_exit(0);
    return NULL;
}

void PMCU_BE_Task_Create(void* func, unsigned int params[], int param_count, unsigned int taskid, void (*tick_handler)(void)){
    int ret = 0, i = 0, j = 0;
    pthread_attr_t thread_attr;
#ifdef DEBUG
    printf("PMCU_BE_Task_Create() %d\n", taskid);
#endif
    PMCU_BE_Disable_Irq();
    if(first_time){
        pmcu_systick_handler = tick_handler;
        Setup_Signal_Handler();
        first_time = 0;
    }

    for (i = 0; i < THREAD_NUMBER; i++ )
    {
        if (pmcu_threads[i].valid == 0)
        {
            pmcu_threads[i].valid = 1;
            pmcu_threads[i].critical_nesting = 0;
            pmcu_threads[i].taskid = taskid;
            break;
        }
    }
    // the pthread pool is exhausted
    assert(i<THREAD_NUMBER);
    
    pmcu_threads[i].real_func = func;
    if(params == NULL){
        pmcu_threads[i].param_number = 0;
    }else{
        for(j = 0; j < param_count; j++){
            pmcu_threads[i].real_params[j] = params[j];
        }
        pmcu_threads[i].param_number = param_count;
    }

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
#ifdef DEBUG
    printf("PMCU_BE_Task_Create() ready create %d\n", taskid);
#endif
    pthread_mutex_lock(&single_core_mutex);
    creat_not_done = 1;
    pthread_create(&pmcu_threads[i].Thread, &thread_attr, Thread_Wrapper, &pmcu_threads[i]);
#ifdef DEBUG
    printf("PMCU_BE_Task_Create() create %d wait\n", taskid);
#endif
    pthread_mutex_unlock(&single_core_mutex);
    while(creat_not_done){ // wait for created new task suspended before real function
        sleep(0);
        // sched_yield();
    }
    PMCU_BE_Enable_Irq();
#ifdef DEBUG
    printf("PMCU_BE_Task_Create() create %d done\n", taskid);
#endif
}

int running_task_id = 0;

void Start_Scheduler(unsigned int taskid, unsigned int interval){
    running_task_id = taskid;
    int i = 0;
#ifdef DEBUG
    printf("Start_Scheduler()\n");
#endif
    for(; i < THREAD_NUMBER; i++){
        if(pmcu_threads[i].taskid == taskid && pmcu_threads[i].valid == 1){
        #ifdef DEBUG
            printf("Start_Scheduler() run %d end\n", taskid);
        #endif
            Setup_Ticker(interval);
            ResumeThread(pmcu_threads[i].Thread);
        #ifdef DEBUG
            printf("Start_Scheduler() suspend itself\n");
        #endif
            Suspend_Thread(pthread_self());
            break;
        }
    }
}

void Pthread_Schedule(unsigned int new_task_id, unsigned int run_task_id){
#ifdef DEBUG
    printf("Pthread_Schedule(new_task=%d,run_task=%d)\n", new_task_id, run_task_id);
#endif
    int i = 0, j = 0;
    for(;j < THREAD_NUMBER;j++){
        if(run_task_id == pmcu_threads[j].taskid && pmcu_threads[j].valid == 1){
            break;
        }
    }
    assert(j<THREAD_NUMBER);
    Suspend_Thread(pmcu_threads[j].Thread);
    for(;i < THREAD_NUMBER;i++){
        if(new_task_id == pmcu_threads[i].taskid && pmcu_threads[i].valid == 1){
            break;
        }
    }
    assert(i<THREAD_NUMBER);
    ResumeThread(pmcu_threads[i].Thread);
}

void Reset_Handler(){}

void Tick_Handler_TaskSchedule(){
#ifdef DEBUG
    printf("Tick_Handler_TaskSchedule, entered for #%d time.\n", times_of_schedule);
#endif
    times_of_schedule++;
    int current_thread = 0;
    for(; current_thread < THREAD_NUMBER; current_thread++){
        if(pmcu_threads[current_thread].taskid == running_task_id){
            if(pmcu_threads[current_thread].valid != 1){
                printf("Fatal error: corrupted running_task_id, running_task is actually invalid!\n");
                assert(0);
            }
            break;
        }
    }
    if(current_thread == THREAD_NUMBER){
        printf("Fatal error: corrupted running_task_id, not found in thread pool!\n");
        assert(0);
    }
#ifdef DEBUG
    printf("Tick_Handler_TaskSchedule: Running task found, id = %d\n", running_task_id);
#endif
    int next_thread = (current_thread + 1) % THREAD_NUMBER;
    while(pmcu_threads[next_thread].valid != 1 && next_thread != current_thread){
        next_thread++;
        next_thread %= THREAD_NUMBER;
    }
    if(current_thread != next_thread){
#ifdef DEBUG
        printf("Tick_Handler_TaskSchedule: New task found, id = %d.\n", pmcu_threads[next_thread].taskid);
#endif
        Pthread_Schedule(next_thread, current_thread);
    }else{
#ifdef DEBUG
        printf("Tick_Handler_TaskSchedule: No other task found, continue executing previous task.\n");
#endif
        Pthread_Schedule(next_thread, current_thread);
    }
}