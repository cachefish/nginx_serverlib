#ifndef __GLOBAL_H__
#define __GLOBAL_H__

//读配置结构体

typedef struct 
{
    /* data */
    char ItemName[50];
    char ItemContent[500];
}CConfItem,*LPCConfItem;

//外部全局量声明
extern char **g_os_argv;
extern char *gp_envmem;
extern int g_environlen;

#endif