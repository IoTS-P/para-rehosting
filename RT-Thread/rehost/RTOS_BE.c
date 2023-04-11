#include "RTOS_BE.h"
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>

#define MAX_NUMBER_OF_TASKS     16         
#define SIG_SUSPEND             SIGUSR1
#define SIG_RESUME              SIGUSR2
#define SIG_TICK                SIGALRM
#define TIMER_TYPE              ITIMER_REAL // Based on our test, ITIMER_VIRTUAL is not stable for fuzzing, and we are trying to fix it. This will not affect the fuzzing process

typedef struct{
    pthread_t thread;
    int valid;
    void* task_id;
    int critical_nest;
    #if TASK_FUNC_PARAMS_NUM==1
        void *args1;
        void (*task_func)(void*);
    #elif TASK_FUNC_PARAMS_NUM==2
        void *args1;
        void *args2;
        void (*task_func)(void*, void*);
    #elif TASK_FUNC_PARAMS_NUM==3
        void *args1;
        void *args2;
        void *args3;
        void (*task_func)(void*, void*, void*);
    #elif TASK_FUNC_PARAMS_NUM==4
        void *args1;
        void *args2;
        void *args3;
        void *args4;
        void (*task_func)(void*, void*, void*, void*);
    #endif
}PMCU_Thread;

PMCU_Thread pmcu_threads[MAX_NUMBER_OF_TASKS];
pthread_mutex_t single_core_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t scheduler_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_t init_thread;
pthread_t ender_thread;
int ender_thread_idx;
int init_once = 1;
int recent_task_idx = 0;
int scheduler_flag = 0;
int tick_flag = 0;
int pend_flag = 0;
int svc_flag = 0;
int interrupt_flag=1;
int nesting_counter = 0;

void (*systick)(int) = NULL;
extern void PMCU_FE_Yield();
extern void FE_Thread_Exit();

int PMCU_SingleLock(){
    return pthread_mutex_lock(&single_core_mutex);
}
int PMCU_SingleUnlock(){
    return pthread_mutex_unlock(&single_core_mutex);
}
int PMCU_SingleTryLock(){
    return pthread_mutex_trylock( &single_core_mutex );
}
void PMCU_SetPending(){
    pend_flag = 1;
}
void PMCU_ClearPending(){
    pend_flag = 0;
}
int PMCU_GetPending(){
    return pend_flag;
}
void PMCU_SetSVC(){
    svc_flag = 1;
}
void PMCU_ClearSVC(){
    svc_flag = 0;
}
int PMCU_GetSVC(){
    return svc_flag;
}
void PMCU_DisableInterrupts(){
    interrupt_flag = 0;
}
void PMCU_EnableInterrupts(){
    interrupt_flag = 1;
}
int PMCU_IsInterruptEnabled(){
    return interrupt_flag;
}
void PMCU_EnterCritical(){
    PMCU_DisableInterrupts();
    nesting_counter++;
}
int PMCU_ExitCritical(){
    if (nesting_counter > 0){
        nesting_counter--;
    }
    if(nesting_counter == 0){
        if (PMCU_GetPending() == 1){
            PMCU_ClearPending();
            PMCU_FE_Yield();
        }
        PMCU_EnableInterrupts();
    }
    return nesting_counter;
}
void PMCU_EnterServicingTick(){
    tick_flag = 1;
}
void PMCU_ExitServicingTick(){
    tick_flag = 0;
}
int PMCU_IsServicingTick(){
    return tick_flag;
}
static void PMCU_SetNesting(int idx, int count){
    pmcu_threads[idx].critical_nest = count;
}
static int PMCU_GetNesting(int idx){
    return pmcu_threads[idx].critical_nest;
}
static void PMCU_ThreadExit(void *param){
    PMCU_Thread *state = (PMCU_Thread *)param;
    state->valid = 0;
    state->task_id = NULL;
    if(state->critical_nest > 0){
        state->critical_nest = 0;
        nesting_counter = 0;
        PMCU_EnableInterrupts();
    }
    FE_Thread_Exit();
}
static void PMCU_SuspendCur(pthread_t thread){
    pthread_mutex_lock(&scheduler_mutex);
    scheduler_flag = 0;
    pthread_mutex_unlock(&scheduler_mutex);
    pthread_kill(thread, SIG_SUSPEND);
    while (scheduler_flag == 0 && tick_flag != 1){
        sched_yield();
    }
}
static void PMCU_SuspendHandler(int signal){
    sigset_t sigset;
    sigemptyset( &sigset );
    sigaddset( &sigset, SIG_RESUME );
    scheduler_flag = 1;
    PMCU_SingleUnlock();
    sigwait(&sigset, &signal);
    if (nesting_counter == 0){
        PMCU_EnableInterrupts();
    }else{
        PMCU_DisableInterrupts();
    }
    PMCU_ClearSVC();
}
static void PMCU_ResumeNext( pthread_t thread )
{
    if ( !pthread_equal(pthread_self(), thread))
    {
        pthread_kill( thread, SIG_RESUME );
    }
}
static void PMCU_ResumeHandler(int sig)
{
    (void)sig;
    if (PMCU_SingleLock() == 0) {
        PMCU_SingleUnlock();
    }
}
int PMCU_Find_Idx(void* task_id){
    int i = -1;
    for(i = 0; i<MAX_NUMBER_OF_TASKS; i++){
        if(pmcu_threads[i].task_id == task_id)
            return i;
    }
    return -1;
}
void PMCU_Schedule(int cur_idx, int next_idx){
    PMCU_SetNesting(cur_idx, nesting_counter);
    nesting_counter = PMCU_GetNesting(next_idx);
    PMCU_ResumeNext(pmcu_threads[next_idx].thread);
    PMCU_SuspendCur(pmcu_threads[cur_idx].thread);
}
static void PMCU_Install_Handler( void ){
    int i;
    struct sigaction suspend_sig, resume_sig, tick_sig;
    memset(pmcu_threads, 0, sizeof(pmcu_threads));
    suspend_sig.sa_flags = 0;
    suspend_sig.sa_handler = PMCU_SuspendHandler;
    sigfillset( &suspend_sig.sa_mask );
    resume_sig.sa_flags = 0;
    resume_sig.sa_handler = PMCU_ResumeHandler;
    sigfillset( &resume_sig.sa_mask );
    tick_sig.sa_flags = 0;
    tick_sig.sa_handler = systick;
    sigfillset( &tick_sig.sa_mask );
    sigaction( SIG_SUSPEND, &suspend_sig, NULL );
    sigaction( SIG_RESUME, &resume_sig, NULL );
    sigaction( SIG_TICK, &tick_sig, NULL );
    init_thread = pthread_self();
}
static void *PMCU_ThreadBody( void * pvParams ){
    PMCU_Thread *pmcu_thread = (PMCU_Thread *)pvParams;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(PMCU_ThreadExit, pmcu_thread);
    PMCU_SingleLock();
    PMCU_SuspendCur(pthread_self());
    #if TASK_FUNC_PARAMS_NUM==1
        pmcu_thread->task_func(pmcu_thread->args1);
    #elif TASK_FUNC_PARAMS_NUM==2
        pmcu_thread->task_func(pmcu_thread->args1, pmcu_thread->args2);
    #elif TASK_FUNC_PARAMS_NUM==3
        pmcu_thread->task_func(pmcu_thread->args1, pmcu_thread->args2, pmcu_thread->args3);
    #elif TASK_FUNC_PARAMS_NUM==4
        pmcu_thread->task_func(pmcu_thread->args1, pmcu_thread->args2, pmcu_thread->args3, pmcu_thread->args4);
    #endif
    pthread_cleanup_pop(1);
    return NULL;
}
#if TASK_FUNC_PARAMS_NUM==1
void *PMCU_CreateNewThread(void* top_of_stack, void(*func)(void*), void* param1, void(*systick_func)(int)){
#elif TASK_FUNC_PARAMS_NUM==2
void *PMCU_CreateNewThread(void* top_of_stack, void(*func)(void*, void*), void* param1, void* param2, void(*systick_func)(int)){
#elif TASK_FUNC_PARAMS_NUM==3
void *PMCU_CreateNewThread(void* top_of_stack, void(*func)(void*, void*, void*), void* param1, void* param2, void* param3, void(*systick_func)(int)){
#elif TASK_FUNC_PARAMS_NUM==4
void *PMCU_CreateNewThread(void* top_of_stack, void(*func)(void*, void*, void*, void*), void* param1, void* param2, void* param3, void* param4, void(*systick_func)(int)){
#endif
    int i;
    pthread_attr_t xThreadAttributes;
    if(systick == NULL){
        systick = systick_func;
    }
    if(init_once){
        PMCU_Install_Handler();
        init_once = 0;
    }
    pthread_attr_init( &xThreadAttributes );
    pthread_attr_setdetachstate( &xThreadAttributes, PTHREAD_CREATE_DETACHED );
    for(i=0;i<MAX_NUMBER_OF_TASKS;i++){
        if(pmcu_threads[i].valid == 0){
            recent_task_idx = i;
            break;
        }
    }
    assert(i<MAX_NUMBER_OF_TASKS);
    pmcu_threads[i].task_func = func;
    #if TASK_FUNC_PARAMS_NUM==1
        pmcu_threads[i].args1 = param1;
    #elif TASK_FUNC_PARAMS_NUM==2
        pmcu_threads[i].args1 = param1;
        pmcu_threads[i].args2 = param2;
    #elif TASK_FUNC_PARAMS_NUM==3
        pmcu_threads[i].args1 = param1;
        pmcu_threads[i].args2 = param2;
        pmcu_threads[i].args3 = param3;
    #elif TASK_FUNC_PARAMS_NUM==4
        pmcu_threads[i].args1 = param1;
        pmcu_threads[i].args2 = param2;
        pmcu_threads[i].args3 = param3;
        pmcu_threads[i].args4 = param4;
    #endif
    
    PMCU_SingleLock();
    scheduler_flag = 0;
    pthread_create(&pmcu_threads[i].thread, &xThreadAttributes, PMCU_ThreadBody, &pmcu_threads[i]);
    pmcu_threads[i].valid = 1;
    PMCU_SingleUnlock();
    while(scheduler_flag == 0)
        sched_yield();
    return top_of_stack;
}

static void PMCU_StartTicker(int tick_rate){
    struct itimerval new_timer, old_timer;
    suseconds_t MicroSeconds = (suseconds_t)(tick_rate % 1000000);
    time_t Seconds = tick_rate / 1000000;
    getitimer( TIMER_TYPE, &new_timer);
    new_timer.it_interval.tv_sec = Seconds;
    new_timer.it_interval.tv_usec = MicroSeconds;
    new_timer.it_value.tv_sec = Seconds;
    new_timer.it_value.tv_usec = MicroSeconds;
    setitimer( TIMER_TYPE, &new_timer, &old_timer );  
}

void PMCU_StartScheduler(void *first_task_id, int tick_rate){
    int i;
    int sig;
    sigset_t sig_set;
    sigset_t sig_block;
    sigset_t sig_blocked;
    sigfillset(&sig_block);
    pthread_sigmask(SIG_SETMASK, &sig_block, &sig_blocked);
    PMCU_StartTicker(tick_rate);
    for(i=0;i<MAX_NUMBER_OF_TASKS;i++){
        if(pmcu_threads[i].task_id == first_task_id){
            break;
        }
    }
    PMCU_ResumeNext(pmcu_threads[i].thread);
    sigemptyset(&sig_set);
    sigaddset(&sig_set, SIG_RESUME);
    sigwait(&sig_set, &sig); 
    pthread_cancel(ender_thread);
    usleep(100);
    pthread_mutex_destroy( &scheduler_mutex );
    pthread_mutex_destroy( &single_core_mutex );
    usleep(100);
}

void PMCU_EndScheduler(){
    int i;
    struct sigaction sig_action;
    sig_action.sa_flags = 0;
    sig_action.sa_handler = SIG_IGN;
    sigfillset(&sig_action.sa_mask);
    sigaction(SIG_TICK, &sig_action, NULL);
    sigaction(SIG_RESUME, &sig_action, NULL);
    sigaction(SIG_SUSPEND, &sig_action, NULL);
    for(i=0;i<MAX_NUMBER_OF_TASKS;i++){
        if(pmcu_threads[i].valid){
            if (pthread_equal(pmcu_threads[i].thread, pthread_self())){
                ender_thread = pmcu_threads[i].thread;
                ender_thread_idx = i;
            }else{
                pthread_cancel(pmcu_threads[i].thread);
                usleep(100);
            }
        }
    }
    pthread_kill(init_thread, SIG_RESUME);
}

void PMCU_Add_Task_ID(void *task_id){
    int i;
    pmcu_threads[recent_task_idx].task_id = task_id;
}
