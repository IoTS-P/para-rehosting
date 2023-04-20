//
// Created by ubuntu on 23-4-10.
//
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "RTOS_BE.h"
extern int next_taskid;
void sub_thread();
void sub_thread2();
void test_main(int seed){
    printf("test_main: next_taskid = %d\n", next_taskid);
    PMCU_BE_Task_Create(sub_thread, NULL, 0, ++next_taskid, NULL);
    PMCU_BE_Task_Create(sub_thread2, NULL, 0, ++next_taskid, NULL);
    for(int i=0; i<1000; i++){
        printf("\033[1;31mthread 1: %d\n\033[0m", i);
    }
}

void sub_thread(){
    for(int i=0; i<1000; i++){
        printf("\033[1;31mthread 2: %d\n\033[0m", i);
    }
}

void sub_thread2(){
    for(int i=0; i<1000; i++){
        printf("\033[1;31mthread 3: %d\n\033[0m", i);
    }
}