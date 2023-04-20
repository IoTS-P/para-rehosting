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
int next_taskid = 10000;
extern pthread_t main_thread;
unsigned int times_of_schedule = 0;
int current_taskidx = 1;

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
int first_schedule = 1;
int critical_nest = 0;
PMCU_Thread pmcu_threads[THREAD_NUMBER];
// int next_valid_pmcu_thread = 0;
static pthread_mutex_t systick_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t single_core_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t schedule_mutex;
static pthread_cond_t schedule_cond = PTHREAD_COND_INITIALIZER;
int creat_not_done = 0;
void (*pmcu_systick_handler)(void);
void Schedule_process(void);

void print_selfthread(){
    printf("Thread %lu: ", pthread_self());
}

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
    print_selfthread();
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
    print_selfthread();
    printf("Systick_Signal_Handler(int sig = %d)\n", sig);
#endif
    int ret = 0;
    ret = pthread_mutex_trylock(&systick_mutex); // if other systick is handling, ignore.
    if(ret == 0){
    #ifdef DEBUG
        printf("\033[1;34mLock got, ready to execute systick_handler in thread %lu.\033[0m\n", pthread_self());
    #endif
        pmcu_systick_handler();
        pthread_mutex_unlock(&systick_mutex);
        // sleep(3);
    }else{
        printf("\033[1;34mFailed to get lock, cannot execute systick_handler in thread %lu.\033[0m\n", pthread_self());
    }
}

/*
 * Task or Thread
*/
void Suspend_Thread(pthread_t thread){
#ifdef DEBUG
    print_selfthread();
    printf("Suspend_Thread() %lu\n", pthread_self());
    printf("\033[1;32mSent a signal SIG_SUSPEND from %lu to %lu\033[0m\n", pthread_self(), thread);
#endif
    pthread_kill(thread, SIG_SUSPEND);
}
void Suspend_Signal_Handler(int sig){
    sigset_t signal;
    sigemptyset(&signal);
    sigaddset(&signal, SIG_RESUME);
#ifdef DEBUG
    print_selfthread();
    printf("Suspend_Signal_Handler() %lu\n", pthread_self());
#endif
    creat_not_done = 0;
    pthread_mutex_unlock(&single_core_mutex);
#ifdef DEBUG
    printf("\033[1;32mThread %lu waiting for signal SIG_RESUME...\033[0m\n", pthread_self());
#endif
    sigwait(&signal, &sig);         // the thread waits here
#ifdef DEBUG
    printf("\033[1;33mThread %lu has got a SIG_RESUME signal, continue to execute...\033[0m\n", pthread_self());
    print_selfthread();
    printf("Suspend_Signal_Handler() %lu exit\n", pthread_self());
#endif
}

void ResumeThread(pthread_t thread){
    // pthread_mutex_lock(&single_core_mutex);
    int lock_success = pthread_mutex_lock(&single_core_mutex);

#ifdef DEBUG
    print_selfthread();
    printf("ResumeThread() %lu\n", thread);
    printf("\033[1;32mSent a signal SIG_RESUME from %lu to %lu\033[0m\n", pthread_self(), thread);
#endif
    int rc = pthread_kill(thread, SIG_RESUME);
    if(rc != 0){
        printf("\033[1;34mFailed to send a signal from %lu to %lu!\033[0m\n", pthread_self(), thread);
    }
#ifdef DEBUG
    print_selfthread();
    printf("ResumeThread() %lu end\n", thread);
#endif
    pthread_mutex_unlock(&single_core_mutex);
}

void Resume_Signal_Handler(int sig){
#ifdef DEBUG
    print_selfthread();
    printf("Resume_Signal_Handler()\n");
#endif
}

void Setup_Signal_Handler(){
#ifdef DEBUG
    print_selfthread();
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
    print_selfthread();
    printf("Thread_Wrapper() %d\n", thread->taskid);
#endif
    pthread_mutex_lock(&single_core_mutex);

    // for pthread terminate
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#ifdef DEBUG
    printf("\033[1;32mThread %lu suspended itself.\033[0m\n", pthread_self());
#endif
    // Suspend_Thread(pthread_self());
    User_Task_Tick_Handler();
#ifdef DEBUG
    print_selfthread();
    printf("Thread_Wrapper() %d start to run\n", thread->taskid);
#endif
    thread->real_func(thread->real_params[0], thread->real_params[1], thread->real_params[2], thread->real_params[3]);
    // clean routine may need to implement here.
    int i = 0;
    for(;i < THREAD_NUMBER;i++){
        if(pthread_equal(pmcu_threads[i].Thread, pthread_self())){
        #ifdef DEBUG
            print_selfthread();
            printf("\033[1;34mclear pmcu_threads[%d], taskid %d\033[0m\n", i, pmcu_threads[i].taskid);
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
    int ret = 0, i = 1, j = 0;
    pthread_attr_t thread_attr;
#ifdef DEBUG
    print_selfthread();
    printf("PMCU_BE_Task_Create() %d\n", taskid);
#endif
    PMCU_BE_Disable_Irq();
    if(first_time){
        pmcu_systick_handler = tick_handler;
        Setup_Signal_Handler();
        first_time = 0;
    }

    for (i = 1; i < THREAD_NUMBER; i++ )
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
    print_selfthread();
    printf("PMCU_BE_Task_Create() ready create %d\n", taskid);
#endif
    // pthread_mutex_lock(&single_core_mutex);

    // creat_not_done = 1;
    pthread_create(&pmcu_threads[i].Thread, &thread_attr, Thread_Wrapper, &pmcu_threads[i]);
#ifdef DEBUG
    print_selfthread();
    printf("PMCU_BE_Task_Create() create %d wait\n", taskid);
#endif
    pthread_mutex_unlock(&single_core_mutex);

    while(creat_not_done){ // wait for created new task suspended before real function
        sleep(0);
        // sched_yield();
    }
    PMCU_BE_Enable_Irq();
#ifdef DEBUG
    print_selfthread();
    printf("PMCU_BE_Task_Create() create %d done\n", taskid);
#endif
}

int running_task_id = 0;

int find_task(pthread_t thread){
    for(int i=0; i<THREAD_NUMBER; i++){
        if(thread == pmcu_threads[i].Thread)
            return i;
    }
    return -1;
}

int find_next_task(int start){
    int ptr = start + 1;
    if(ptr == THREAD_NUMBER)
        ptr = 1;
    while(ptr != start){
        if(pmcu_threads[ptr].valid)
            return ptr;
        ptr++;
        if(ptr == THREAD_NUMBER){
            if(start == 0)
                return -1;
            else
                ptr = 1;
        }
    }
    return start;
}

void Start_Scheduler(unsigned int taskid, unsigned int interval){
    pthread_mutex_init(&schedule_mutex, NULL);
    running_task_id = taskid;
    pthread_mutex_lock(&schedule_mutex);    // locked first
    int i = 0;
    printf("\033[1;32mMain thread: %lu\n\033[0m", main_thread);
    pmcu_threads[0].taskid = 1234;  // magic number
    pmcu_threads[0].Thread = main_thread;
    pmcu_threads[0].valid = 1;
    pmcu_threads[0].index = 0;
#ifdef DEBUG
    print_selfthread();
    printf("Start_Scheduler()\n");
#endif
    Setup_Ticker(interval);
    Schedule_process();
    while(1);
    for(; i < THREAD_NUMBER; i++){
        if(pmcu_threads[i].taskid == taskid && pmcu_threads[i].valid == 1){
        #ifdef DEBUG
            print_selfthread();
            printf("Start_Scheduler() run %d end\n", taskid);
        #endif
            Setup_Ticker(interval);
            ResumeThread(pmcu_threads[i].Thread);
        #ifdef DEBUG
            print_selfthread();
            printf("Start_Scheduler() suspend itself\n");
        #endif
            // Suspend_Thread(pthread_self());
            break;
        }
    }
}

void Pthread_Schedule(unsigned int new_task_id, unsigned int run_task_id){
#ifdef DEBUG
    print_selfthread();
    printf("Pthread_Schedule(new_task=%d,run_task=%d)\n", new_task_id, run_task_id);
#endif
    int i = 0, j = 0;
    for(;i < THREAD_NUMBER;i++){
        if(new_task_id == pmcu_threads[i].taskid && pmcu_threads[i].valid == 1){
            break;
        }
    }
    assert(i<THREAD_NUMBER);
    ResumeThread(pmcu_threads[i].Thread);
    for(;j < THREAD_NUMBER;j++){
        if(run_task_id == pmcu_threads[j].taskid && pmcu_threads[j].valid == 1){
            break;
        }
    }
    assert(j<THREAD_NUMBER);
    Suspend_Thread(pmcu_threads[j].Thread);
}

void Reset_Handler(){}

void User_Task_Tick_Handler(){
//    if(pthread_self() == main_thread){
//        printf("\033[1;34mx\n\033[0m");
//        return;
//    }
    int idx = find_task(pthread_self());
    assert(current_taskidx != -1);
    int next_idx = find_next_task(idx);
    if(next_idx == -1){
        exit(0);
    }
    if(next_idx == idx)
        return;
    current_taskidx = next_idx;
    pthread_cond_signal(&schedule_cond);
    while(current_taskidx != idx){
        pthread_cond_wait(&schedule_cond, &schedule_mutex);
    }
    // pthread_mutex_lock(schedule_mutex);
    // pthread_mutex_unlock(&schedule_mutex);
}

void Tick_Handler_TaskSchedule(){
#ifdef DEBUG
    print_selfthread();
    printf("Tick_Handler_TaskSchedule, entered for #%d time.\n", times_of_schedule);
#endif
    times_of_schedule++;
    int current_thread = 0;
    int current_thread_id = 0;
    for(; current_thread < THREAD_NUMBER; current_thread++){
        if(pmcu_threads[current_thread].taskid == running_task_id){
            if(pmcu_threads[current_thread].valid != 1){
                print_selfthread();
                printf("Fatal error: corrupted running_task_id, running_task is actually invalid!\n");
                assert(0);
            }
            current_thread_id = pmcu_threads[current_thread].Thread;
            break;
        }
    }
    if(current_thread == THREAD_NUMBER){
        print_selfthread();
        printf("Fatal error: corrupted running_task_id, not found in thread pool!\n");
        assert(0);
    }
#ifdef DEBUG
    print_selfthread();
    printf("Tick_Handler_TaskSchedule: Running task found, id = %d, thread_id = %u\n", running_task_id, current_thread_id);
#endif
    int next_thread = (current_thread + 1) % THREAD_NUMBER;
    int next_thread_id = 0;
    while(pmcu_threads[next_thread].valid != 1 && next_thread != current_thread){
        next_thread++;
        next_thread %= THREAD_NUMBER;
    }
    next_thread_id = pmcu_threads[next_thread].Thread;
    if(current_thread != next_thread){
#ifdef DEBUG
        print_selfthread();
        printf("Tick_Handler_TaskSchedule: New task found, id = %d, thread_id = %u\n", pmcu_threads[next_thread].taskid, next_thread_id);
#endif
        running_task_id = pmcu_threads[next_thread].taskid;
        Pthread_Schedule(pmcu_threads[next_thread].taskid, pmcu_threads[current_thread].taskid);
    }else{
#ifdef DEBUG
        print_selfthread();
        printf("Tick_Handler_TaskSchedule: No other task found, continue executing previous task.\n");
#endif
        // Pthread_Schedule(next_thread, current_thread);
    }
}

void idle_task(){
    int loop = 0;
    while(1){
        printf(".");
        usleep(100);
        ;
    }
}

// as task #0
void Schedule_process(){
    while(1){
        int nextidx = find_next_task(current_taskidx);
    #ifdef DEBUG
        printf("\033[1;32mnext task idx = %d\n\033[0m", nextidx);
    #endif
        if(nextidx == -1){  // NO TASK TO SCHEDULE, EXIT DIRECTLY
        #ifdef DEBUG
            printf("Main Thread: No other task to schedule, exiting...\n");
        #endif
            exit(0);
        }
        if(first_schedule){
            #ifdef DEBUG
            printf("\033[1;34mReady for first schedule...\n\033[0m");
            #endif
            ResumeThread(pmcu_threads[nextidx].Thread);
            first_schedule = 0;
        }else{
            #ifdef DEBUG
            printf("\033[1;34mReady for next schedule...\n\033[0m");
            #endif
            Pthread_Schedule(pmcu_threads[current_taskidx].taskid, pmcu_threads[nextidx].taskid);
        }
        #ifdef DEBUG
        printf("\033[1;34mLocked and waiting for next tick...\n\033[0m");
        #endif
        pthread_mutex_lock(&schedule_mutex);
        current_taskidx = nextidx;
    }
}