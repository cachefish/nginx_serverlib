#include<string.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<errno.h>

#include"c_macro.h"
#include"c_func.h"

typedef struct{
    int signo;  //信号对应的数字编号
    const char*signame; //信号对应的中文名

    void (*handler)(int signo,siginfo_t *siginfo,void *ucontext);  //函数指针
}cc_signal_t;

//声明一个信号处理函数
static void cc_signal_handler(int signo, siginfo_t *siginfo, void *ucontext);

cc_signal_t signals[] = {
    {SIGHUP,"SIGHUP",cc_signal_handler},                        //终端断开信号
    {SIGINT,"SIGINT",cc_signal_handler},                            //标识2
    {SIGTERM,"SIGTERM",cc_signal_handler},                  //标识15
    {SIGCHLD,"SIGCHLD",cc_signal_handler},                  //子进程退出时，父进程会收到这个信号--标识17
    {SIGQUIT,"SIGQUIT",cc_signal_handler},                      //标识3
    {SIGIO,"SIGIO",cc_signal_handler},                                  //指示一个异步I/O事件【通用异步I/O信号】
    {SIGSYS,"SIGSYS,SIG_IGN",NULL},                                     //我们想忽略这个信号，SIGSYS表示收到了一个无效系统调用，如果我们不忽略，进程会被操作系统杀死，--标识31

    {0, NULL,NULL}

};

//初始化信号,用于注册信号处理程序
int cc_init_signals()
{
    cc_signal_t *sig;
    struct sigaction sa;

    for(sig = signals;sig->signo != 0;sig++){
        memset(&sa,0,sizeof(struct sigaction));

        if(sig->handler){
            sa.sa_sigaction = sig->handler;
            sa.sa_flags = SA_SIGINFO; //sa_flags：int型，指定信号的一些选项，设置了该标记(SA_SIGINFO)，就表示信号附带的参数可以被传递到信号处理函数中
        }else{
            sa.sa_handler = SIG_IGN;//sa_handler:这个标记SIG_IGN给到sa_handler成员，表示忽略信号的处理程序，否则操作系统的缺省信号处理程序很可能把这个进程杀掉
        }
        sigemptyset(&sa.sa_mask);

        if(sigaction(sig->signo,&sa,NULL)==-1){
            cc_log_error_core(CC_LOG_EMERG,errno,"sigaction(%s) filed",sig->signame);
            return -1;
        }else{
             cc_log_stderr(0,"sigaction(%s) succed!",sig->signame);
        }
    }
    return 0;
}


//信号处理函数
static void cc_signal_handler(int signo, siginfo_t *siginfo, void *ucontext)
{
    printf("来信号了\n");
}