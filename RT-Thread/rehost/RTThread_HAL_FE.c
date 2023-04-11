#include "RTThread_HAL_FE.h"

void RTThread_HAL_FE_void(){}

int RTThread_HAL_FE_return_success(){
    return HAL_BE_return_0();
}

int RTThread_HAL_FE_Out(int len){
    return HAL_BE_Out(len);
}

int RTThread_HAL_FE_In(void* buf, int len){
    int temp_len = HAL_BE_In(buf, len);
    if(temp_len <= 0)
        return -1;
    return temp_len;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return RTThread_HAL_FE_Out(len);
}
ssize_t recv(int s, void *mem, size_t len, int flags) {
    return RTThread_HAL_FE_In(mem, len);
}