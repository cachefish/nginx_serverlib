#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

#include"c_global.h"

//设置可执行程序标题相关函数：分配内存，并把环境标量拷贝到新内存中
void cc_init_setproctitle()
{
    int i;
    //统计环境变量所占内存
    for(i=0;environ[i];i++){
        g_environlen += strlen(environ[i])+1;
    }
    gp_envmem = new char[g_environlen];
    memset(gp_envmem,0,g_environlen);

    char *ptmp = gp_envmem;

    //将原来的内存保存到新的地方
    for(i=0;environ[i];i++){
        size_t size = strlen(environ[i])+1;
        strcpy(ptmp,environ[i]);
        environ[i] = ptmp;
        ptmp += size;
    }
    return;
}


//设置可执行程序标题
void cc_setproctitle(const char *title)
{
    //计算新标题的长度
    size_t itittlelen = strlen(title);

    //计算总的原始的argv那块内存的总长度
    size_t e_environlen = 0;    
    for(int i=0;g_os_argv[i];i++){
        e_environlen +=strlen(g_os_argv[i])+1;
    }

    size_t esy = e_environlen + g_environlen;   //argv 和environ内存总和
    if(esy<=itittlelen){
        return; //太长
    }
    
    g_os_argv[1] = nullptr;
    char *ptmp = g_os_argv[0];  //让ptmp指向g_os_argv所指向的内存
    strcpy(ptmp,title);
    ptmp += itittlelen;
    
    //将剩余的原argv以及environ所占的内存全部清零
    size_t cha = esy - itittlelen;
    memset(ptmp,0,cha);
    return;
}
