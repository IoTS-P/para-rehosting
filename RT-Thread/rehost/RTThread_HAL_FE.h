#include "HAL_BE.h"

void RTThread_HAL_FE_void();
int RTThread_HAL_FE_return_success();
int RTThread_HAL_FE_Out(int len);
int RTThread_HAL_FE_In(void* buf, int len);

//#define HalIrqInit(void)    RTThread_HAL_FE_void() 

#include "../rt-thread/components/net/lwip/lwip-2.1.2/src/include/lwip/arch.h"

ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int s, void *mem, size_t len, int flags);


