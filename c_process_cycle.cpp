#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>   //信号相关头文件 
#include <errno.h>    //errno
#include <unistd.h>

#include "c_func.h"
#include "c_macro.h"
#include "c_conf.h"

//函数声明
static void cc_start_worker_processes(int threadnums);
static int cc_spawn_process(int threadnums,const char *pprocname);
static void cc_worker_process_cycle(int inum,const char *pprocname);
static void cc_worker_process_init(int inum);

//变量声明
static u_char  master_process[] = "master process";

//描述：创建worker子进程
void cc_master_process_cycle()
{    
    sigset_t set;        //信号集
    sigemptyset(&set);   //清空信号集
    //下列这些信号在执行本函数期间不希望收到,保护不希望由信号中断的代码临界区）
    sigaddset(&set, SIGCHLD);     //子进程状态改变
    sigaddset(&set, SIGALRM);     //定时器超时
    sigaddset(&set, SIGIO);       //异步I/O
    sigaddset(&set, SIGINT);      //终端中断符
    sigaddset(&set, SIGHUP);      //连接断开
    sigaddset(&set, SIGUSR1);     //用户定义信号
    sigaddset(&set, SIGUSR2);     //用户定义信号
    sigaddset(&set, SIGWINCH);    //终端窗口大小改变
    sigaddset(&set, SIGTERM);     //终止
    sigaddset(&set, SIGQUIT);     //终端退出符
    //.........可以根据开发的实际需要往其中添加其他要屏蔽的信号......
    
    //设置，此时无法接受的信号；阻塞期间，你发过来的上述信号，多个会被合并为一个，暂存着，等你放开信号屏蔽后才能收到这些信号。。。
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) //第一个参数用了SIG_BLOCK表明设置 进程 新的信号屏蔽字 为 “当前信号屏蔽字 和 第二个参数指向的信号集的并集
    {        
        cc_log_error_core(CC_LOG_ALERT,errno,"cc_master_process_cycle()中sigprocmask()失败!");
    }
    //设置主进程标题---------begin
    size_t size;
    int    i;
    size = sizeof(master_process);  //注意我这里用的是sizeof，所以字符串末尾的\0是被计算进来了的
    size += g_argvneedmem;          //argv参数长度加进来    
    if(size < 1000) //长度小于这个，我才设置标题
    {
        char title[1000] = {0};
        strcpy(title,(const char *)master_process); //"master process"
        strcat(title," ");  //跟一个空格分开一些，清晰    //"master process "
        for (i = 0; i < g_os_argc; i++)         //"master process ./nginx"
        {
            strcat(title,g_os_argv[i]);
        }//end for
        cc_setproctitle(title); //设置标题
        cc_log_error_core(CC_LOG_NOTICE,0,"%s %P 【master进程】启动并开始运行......!",title,cc_pid); //设置标题时顺便记录下来进程名，进程id等信息到日志
    }    
    //从配置文件中读取要创建的worker进程数量
    CConfig *p_config = CConfig::GetInstance(); //单例类
    int workprocess = p_config->GetIntDefault("WorkerProcesses",1); //从配置文件中得到要创建的worker进程数量
    cc_start_worker_processes(workprocess);  //这里要创建worker子进程

    sigemptyset(&set); //信号屏蔽字为空，表示不屏蔽任何信号
    
    for ( ;; ) 
    {
        sigsuspend(&set); //阻塞在这里，等待一个信号，此时进程是挂起的，不占用cpu时间，只有收到信号才会被唤醒（返回）；
                         //此时master进程完全靠信号驱动干活    
        //printf("master进程休息1秒\n");      
        //cc_log_stderr(0,"haha--这是父进程，pid为%P",cc_pid); 
        sleep(1); //休息1秒        
    }// end for(;;)
    return;
}

//描述：根据给定的参数创建指定数量的子进程，因为以后可能要扩展功能，增加参数，所以单独写成一个函数
//threadnums:要创建的子进程数量
static void cc_start_worker_processes(int threadnums)
{
    int i;
    for (i = 0; i < threadnums; i++)  //master进程在走这个循环，来创建若干个子进程
    {
        cc_spawn_process(i,"worker process");
    } //end for
    return;
}

//描述：产生一个子进程
//inum：进程编号【0开始】
//pprocname：子进程名字"worker process"
static int cc_spawn_process(int inum,const char *pprocname)
{
    pid_t  pid;

    pid = fork(); //fork()系统调用产生子进程
    switch (pid)  //pid判断父子进程，分支处理
    {  
    case -1: //产生子进程失败
        cc_log_error_core(CC_LOG_ALERT,errno,"cc_spawn_process()fork()产生子进程num=%d,procname=\"%s\"失败!",inum,pprocname);
        return -1;

    case 0:  //子进程分支
        cc_parent = cc_pid;              //因为是子进程了，所有原来的pid变成了父pid
        cc_pid = getpid();                //重新获取pid,即本子进程的pid
        cc_worker_process_cycle(inum,pprocname);    //我希望所有worker子进程，在这个函数里不断循环着不出来，也就是说，子进程流程不往下边走;
        break;

    default:             
        break;
    }//end switch
    return pid;
}

//描述：worker子进程的功能函数，每个woker子进程，就在这里循环着了（无限循环【处理网络事件和定时器事件以对外提供web服务】）
//     子进程分叉才会走到这里
//inum：进程编号【0开始】
static void cc_worker_process_cycle(int inum,const char *pprocname) 
{
    cc_process = CC_PROCESS_WORKER;  //设置进程的类型，是worker进程
    //为子进程设置进程名
    cc_worker_process_init(inum);
    cc_setproctitle(pprocname); //设置标题   
    cc_log_error_core(CC_LOG_NOTICE,0,"%s %P 【worker进程】启动并开始运行......!",pprocname,cc_pid); //设置标题时顺便记录下来进程名，进程id等信息到日志

    for(;;)
    {
        //cc_log_stderr(0,"good--这是子进程，编号为%d,pid为%P",inum,cc_pid); 
        //cc_log_error_core(0,0,"good--这是子进程，编号为%d,pid为%P",inum,cc_pid);

        cc_process_events_and_timers(); //处理网络事件和定时器事件


    } //end for(;;)
    //如果从这个循环跳出来，在这里停止线程池；
    g_threadpool.StopAll(); 
    g_socket.Shutdown_subproc();

    return;
}

//描述：子进程创建时调用本函数进行一些初始化工作
static void cc_worker_process_init(int inum)
{
    sigset_t  set;      //信号集

    sigemptyset(&set);  //清空信号集
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1)  //原来是屏蔽那10个信号【防止fork()期间收到信号导致混乱】，现在不再屏蔽任何信号【接收任何信号】
    {
        cc_log_error_core(CC_LOG_ALERT,errno,"cc_worker_process_init()中sigprocmask()失败!");
    }
    //线程池代码，率先创建，至少要比和socket相关的内容优先
    CConfig *p_config = CConfig::GetInstance();
    int tmpthreadnums = p_config->GetIntDefault("ProcMsgRecvWorkThreadCount",5); //处理接收到的消息的线程池中线程数量
    if(g_threadpool.Create(tmpthreadnums) == false)  //创建线程池中线程
    {
        exit(-2);
    }
    sleep(1); 
    if(g_socket.Initialize_subproc()==false){
        exit(-2);
    }
    
    g_socket.cc_epoll_init();           //初始化epoll相关内容，同时 往监听socket上增加监听事件，从而开始让监听端口履行其职责
    return;
}
