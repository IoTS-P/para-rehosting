/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-09-01     ZeroFree     first implementation
 */

#include <rtthread.h>
#include <rtdevice.h>
// #include <msh.h>

// #include "drv_wlan.h"
// #include "wifi_config.h"

#include <stdio.h>
#include <stdlib.h>

// #include <dfs_fs.h>
// #include <dfs_posix.h>

#include <webnet.h>
#include <wn_module.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include "../../rehost/RTThread_FE.h"
#include "../../rehost/RTThread_HAL_FE.h"

static const char *sd_upload = "~/Documents/ParaRehosting/webnet";
static const char *upload_dir = "upload";
static int file_size = 0;

void webnet_demo(void);

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
    int result = RT_EOK;
    webnet_demo();
    return 0;
}

static void cgi_calc_handler(struct webnet_session *session)
{
    int a, b;
    const char *mimetype;
    struct webnet_request *request;
    static const char *header = "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; "
                                "charset=gb2312\" /><title> calc </title></head>";

    static const char *body = "<body><form method=\"post\" action=\"/cgi-bin/calc\"><input type=\"text\" name=\"a\" value=\"%d\"> "
                              "+ <input type=\"text\" name=\"b\" value=\"%d\"> = %d<br><input type=\"submit\" value=\"\xBC\xC6\xCB\xE3\"></form>"
                              "<br><a href=\"/index.html\">Go back to root</a></body></html>\r\n";

    RT_ASSERT(session != RT_NULL);
    request = session->request;
    RT_ASSERT(request != RT_NULL);

    /* 获得资源的媒体类型 */
    mimetype = mime_get_type(".html");

    a = 1;
    b = 1;
    /* 设置 HTTP 头部 */
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, "Ok", -1);

    webnet_session_write(session, (const rt_uint8_t *)header, rt_strlen(header));
    if (request->query_counter)
    {
        const char *a_value, *b_value;
        a_value = webnet_request_get_query(request, "a");
        b_value = webnet_request_get_query(request, "b");

        a = atoi(a_value);
        b = atoi(b_value);
    }

    webnet_session_printf(session, body, a, b, a + b);
}

static void cgi_hello_handler(struct webnet_session *session)
{
    const char *mimetype;
    static const char *status = "<html><head><title> hello </title>"
                                "</head><body><font size=\"+2\">hello world</font><br><br>"
                                "<a href=\"javascript:history.go(-1);\">Go back to root</a></body></html>\r\n";
    RT_ASSERT(session != RT_NULL);

    /* 获得资源的媒体类型 */
    mimetype = mime_get_type(".html");

    /* 设置 HTTP 头部 */
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, "Ok", strlen(status));

    webnet_session_write(session, (const rt_uint8_t *)status, rt_strlen(status));
}

static const char *get_file_name(struct webnet_session *session)
{
    const char *path = RT_NULL, *path_last = RT_NULL;

    path_last = webnet_upload_get_filename(session);
    if (path_last == RT_NULL)
    {
        printf("[ERROR] file name err!!\n");
        return RT_NULL;
    }

    path = strrchr(path_last, '\\');
    if (path != RT_NULL)
    {
        path++;
        path_last = path;
    }

    path = strrchr(path_last, '/');
    if (path != RT_NULL)
    {
        path++;
        path_last = path;
    }

    return path_last;
}

static int upload_open(struct webnet_session *session)
{
    int fd;
    const char *file_name = RT_NULL;

    file_name = get_file_name(session);
    LOG_I("Upload FileName: %s", file_name);
    LOG_I("Content-Type   : %s", webnet_upload_get_content_type(session));

    if (webnet_upload_get_filename(session) != RT_NULL)
    {
        int path_size;
        char *file_path;

        path_size = strlen(sd_upload) + strlen(upload_dir)
                    + strlen(file_name);

        path_size += 4;
        file_path = (char *)rt_malloc(path_size);

        if (file_path == RT_NULL)
        {
            fd = -1;
            goto _exit;
        }

        sprintf(file_path, "%s/%s/%s", sd_upload, upload_dir, file_name);

        printf("[Info] save to: %s\r", file_path);

        fd = open(file_path, O_WRONLY | O_CREAT, 0);
        if (fd < 0)
        {
            webnet_session_close(session);
            rt_free(file_path);

            fd = -1;
            goto _exit;
        }
    }

    file_size = 0;

_exit:
    return (int)fd;
}

static int upload_close(struct webnet_session *session)
{
    int fd;

    fd = (int)webnet_upload_get_userdata(session);
    if (fd < 0) return 0;

    close(fd);
    printf("[Info] Upload FileSize: %d\n", file_size);
    return 0;
}

static int upload_write(struct webnet_session *session, const void *data, rt_size_t length)
{
    int fd;

    fd = (int)webnet_upload_get_userdata(session);
    if (fd < 0) return 0;

    printf("[Info] write: length %d\n", length);

    write(fd, data, length);
    file_size += length;

    return length;
}

static int upload_done(struct webnet_session *session)
{
    const char *mimetype;
    static const char *status = "<html><head><title>Upload OK </title>"
                                "</head><body>Upload OK, file length = %d "
                                "<br/><br/><a href=\"javascript:history.go(-1);\">"
                                "Go back to root</a></body></html>\r\n";

    /* 获得资源的媒体类型 */
    mimetype = mime_get_type(".html");

    /* 设置 HTTP 头部 */
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, "Ok", rt_strlen(status));
    webnet_session_printf(session, status, file_size);

    return 0;
}

static const struct webnet_module_upload_entry upload_entry_upload =
{
    "/upload",
    upload_open,
    upload_close,
    upload_write,
    upload_done
};

void webnet_demo(void)
{
    /* 注册 CGI 处理函数 */
    webnet_cgi_register("hello", cgi_hello_handler);
    webnet_cgi_register("calc", cgi_calc_handler);

    /* 设置 AUTH 验证 */
    webnet_auth_set("/admin", "admin:admin");

    /* 添加上传入口 */
    webnet_upload_add(&upload_entry_upload);

    /* 启动 WebNet */
    webnet_init();
}
