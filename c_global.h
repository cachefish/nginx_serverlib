#ifndef __GLOBAL_H__
#define __GLOBAL_H__

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
extern char **g_os_argv;
extern char *gp_envmem;
extern int g_environlen;

extern pid_t cc_pid;
extern cc_log_t cc_log;

#endif