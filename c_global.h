#ifndef __GLOBAL_H__
#define __GLOBAL_H__
#include<signal.h>
#include"c_socket.h"
#include"c_threadpool.h"
//读配置结构体
typedef struct 
{
    /* data */
    char ItemName[50];
    char ItemContent[500];
}CConfItem,*LPCConfItem;

//日志类型
typedef struct{
        int log_level;  //日志等级
        int fd; //日志文件描述符
}cc_log_t;


//外部全局量声明
extern size_t g_argvneedmem;
extern size_t g_envneedmem;
extern int g_os_argc;
extern char **g_os_argv;
extern char *gp_envmem;
extern int g_environlen;
extern int g_daemonized;

extern CSocket g_socket;
extern CThreadPool g_threadpool;


extern pid_t cc_pid;
extern pid_t cc_parent;
extern cc_log_t cc_log;

extern int           cc_process;   
extern sig_atomic_t  cc_reap;    //原子操作


#endif