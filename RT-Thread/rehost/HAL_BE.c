#include "HAL_BE.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define FUZZER_BUF_SIZE (6*1024)
#define MAX_FRAME_COUNT     100
#define FRAME_SIZE          1600

int input_loaded = 0;
// char fuzzer_buf[FUZZER_BUF_SIZE] = {0};
char *fuzzer_buf = NULL;
int fuzzer_buf_size = 0;
int fuzzer_buf_cursor = 0;

char fuzz_testcase[MAX_FRAME_COUNT * FRAME_SIZE] = {0x0};
int real_tc_size = 0;
int tc_cursor = 0;
int frame_queue_last = 0;
int frame_queue_cursor = 0;
char frame_queues[MAX_FRAME_COUNT][FRAME_SIZE] = {{0x0}};
int frame_lens[MAX_FRAME_COUNT] = {0};
const char marker[] = {0xbe, 0xef, 0xfa, 0xce};

int HAL_BE_return_0(){
    return 0;
}

int HAL_BE_return_1(){
    return 1;
}

int HAL_BE_Out(int len){
    return len;
}

int HAL_BE_In(void* buf, int len){
    return read(0, buf, len);
    // if(fuzzer_buf_cursor + len > fuzzer_buf_size){
    //     // struct timeval tv1;
    //     // gettimeofday(&tv1,NULL);
    //     // printf("%lu us\n", (tv1.tv_sec*1000000 + tv1.tv_usec));
    //     exit(0);
    // }else{
    //     memcpy(buf, fuzzer_buf + fuzzer_buf_cursor, len);
    //     fuzzer_buf_cursor += len;
    //     return len;
    // }
}

void HAL_BE_IO_read(void *buf, int size){
    // Deal with copying over the (remaining) fuzzing bytes
    if(size && tc_cursor + size <= real_tc_size) {
        memcpy(buf, &fuzz_testcase[tc_cursor], size);
        tc_cursor += size;
    } else {
        exit(0);
    }
}

int endwith(char *ptr, int len){
    if(len > 4){
        if(memcmp(ptr + len - 4, marker, 4) == 0) // string equal
            return 1;
        else
            return 0;
    }
    else
        return 0;
}

int fuzz_remaining() {
    if(real_tc_size == 0){
        real_tc_size = read(0, fuzz_testcase, MAX_FRAME_COUNT * FRAME_SIZE);
        tc_cursor = 0;
    }
    return real_tc_size - tc_cursor;
}

void load_frames(){
    int local_offset = 0;
    frame_queue_last = 0;
    frame_queue_cursor = 0;
    if(fuzz_remaining() == 0){
        printf("test case is exhausted\n");
        exit(0); // test case is exhausted
    }
    while(fuzz_remaining() > 0){
        local_offset = 0;
        if(frame_queue_last >= MAX_FRAME_COUNT){
            printf("Frame queues exhausted\n");
            exit(-1); // no more space left, please adjust it to bigger
        }
        while(fuzz_remaining() > 0 && endwith(frame_queues[frame_queue_last], local_offset) == 0){
            HAL_BE_IO_read(&frame_queues[frame_queue_last][local_offset], 1); // same as get_fuzz
            local_offset++;  
        }
        if(endwith(frame_queues[frame_queue_last], local_offset) == 1){
            local_offset -= 4;
        }
        if(local_offset > 1514){
            printf("Too huge frame\n");
            exit(-1);
        }
        frame_lens[frame_queue_last] = local_offset;
        frame_queue_last++;
    }
}

int get_rx_frame(){
    if(frame_queue_last - frame_queue_cursor == 0)
        load_frames();
    if(frame_queue_last - frame_queue_cursor > 0){
            frame_queue_cursor++;
            return frame_queue_cursor - 1;
    }
    printf("No way\n");
    exit(-1); // frames are exhausted, but generally shouldn't reach here
}

int HAL_BE_ENET_ReadFrame(void* buf, int length){
    int idx = get_rx_frame();
    if(length <= frame_lens[idx]){
        memcpy(buf, frame_queues[idx], length);
        return length;
    }else{
        memcpy(buf, frame_queues[idx], frame_lens[idx]);
        return frame_lens[idx];
    }
}