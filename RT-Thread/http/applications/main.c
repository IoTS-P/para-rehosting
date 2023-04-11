/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-09-04     ChenYong     first implementation
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <wlan_mgnt.h>
#include <webclient.h>
#include <stdio.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include <RTThread_HAL_FE.h>

#define HTTP_GET_URL "http://www.rt-thread.com/service/rt-thread.txt"
#define HTTP_POST_URL "http://www.rt-thread.com/service/echo"

const char *post_data = "RT-Thread is an open source IoT operating system from China!";

extern int webclient_get_data(void);
extern int webclient_post_data(void);

int main() 
{
    rt_hw_interrupt_disable();
    rt_hw_board_init();
    rt_show_version();
    rt_system_timer_init();
    rt_system_scheduler_init();
    rt_application_init();
    rt_system_timer_thread_init();
    rt_thread_idle_init();
    rt_system_scheduler_start();
    return 0;
}

int main_SIM(void)
{
    /* HTTP GET 请求发送 */
    webclient_get_data();
    /* HTTP POST 请求发送 */
    //webclient_post_data();
}

/* HTTP client download data by GET request */
int webclient_get_data(void)
{
    unsigned char *buffer = RT_NULL;
    int length = 0;

    length = webclient_request(HTTP_GET_URL, RT_NULL, RT_NULL, &buffer);
    if (length < 0)
    {
        printf("webclient GET request response data error.\n");
        exit(0);
    }

    printf("webclient GET request response data :\n");
    printf("%s\n", buffer);

    web_free(buffer);
    exit(0);
}

/* HTTP client upload data to server by POST request */
int webclient_post_data(void)
{
    unsigned char *buffer = RT_NULL;
    int length = 0;

    length = webclient_request(HTTP_POST_URL, RT_NULL, post_data, &buffer);
    if (length < 0)
    {
        printf("webclient POST request response data error.\n");
        return -RT_ERROR;
    }

    printf("webclient POST request response data :\n");
    printf("%s\n", buffer);

    web_free(buffer);
    exit(0);
    return RT_EOK;
}
