#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<signal.h>
#include<errno.h>

#include"c_func.h"
#include"c_macro.h"
#include"c_conf.h"

static void cc_start_worker_processes(int threadnums);
static int cc_spawn_process(int threadnums,const char *pprocname);
static void cc_worker_process_cycle(int inum,const char *pprocname);
static void cc_worker_process_init(int inum);

//标题
static u_char master_process[] = "master process";


//创建worker子进程
void cc_master_process_cycle()
{
    sigset_t set;   //信号集

    sigemptyset(&set);

    sigaddset(&set,SIGCHLD);    //子进程状态改变
     sigaddset(&set, SIGALRM);     //定时器超时
    sigaddset(&set, SIGIO);       //异步I/O
    sigaddset(&set, SIGINT);      //终端中断符
    sigaddset(&set, SIGHUP);      //连接断开
    sigaddset(&set, SIGUSR1);     //用户定义信号
    sigaddset(&set, SIGUSR2);     //用户定义信号
    sigaddset(&set, SIGWINCH);    //终端窗口大小改变
    sigaddset(&set, SIGTERM);     //终止
    sigaddset(&set, SIGQUIT);     //终端退出符

    if(sigprocmask(SIG_BLOCK,&set,NULL) == -1){
        cc_log_error_core(CC_LOG_ALERT,errno,"cc_master_process_cycle()中的sigprocmask()失败");
    }

    //设置主进程标题
    size_t size;
    int i;
    size = sizeof(master_process);
    size += g_argvneedmem;
    if(size < 1000)
    {
        char title[1000] = {0};
        strcpy(title,(const char *)master_process);
        strcat(title," ");
        for(i=0;i<g_os_argc;i++){
            strcat(title,g_os_argv[i]);
        }
        cc_setproctitle(title);

        cc_log_error_core(CC_LOG_NOTICE,0,"%s %P启动并开始运行......!",title,cc_pid);
    }
    //从配置文件中读取要创建的worker进程数量
    CConfig *p_config = CConfig::GetInstance();
    int workprocess = p_config->GetIntDefault("WorkerProcesses",1);
    cc_start_worker_processes(workprocess);

    sigemptyset(&set);//不在屏蔽任何信号。

     for ( ;; ) 
    {

    //    usleep(100000);
       //cc_log_error_core(0,0,"haha--这是父进程，pid为%P",cc_pid);

        //a)根据给定的参数设置新的mask 并 阻塞当前进程【因为是个空集，所以不阻塞任何信号】
        //b)此时，一旦收到信号，便恢复原先的信号屏蔽【我们原来的mask在上边设置的，阻塞了多达10个信号，从而保证我下边的执行流程不会再次被其他信号截断】
        //c)调用该信号对应的信号处理函数
        //d)信号处理函数返回后，sigsuspend返回，使程序流程继续往下走

        sigsuspend(&set); //阻塞在这里，等待一个信号，此时进程是挂起的，不占用cpu时间，只有收到信号才会被唤醒（返回）；
                         //此时master进程完全靠信号驱动干活    

//        printf("执行到sigsuspend()下边来了\n");
        
///        printf("master进程休息1秒\n");      
        sleep(1); //休息1秒        
        //以后扩充.......

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
        cc_log_error_core(CC_LOG_ALERT,errno,"ngx_spawn_process()fork()产生子进程num=%d,procname=\"%s\"失败!",inum,pprocname);
        return -1;

    case 0:  //子进程分支
        cc_parent = cc_pid;              //因为是子进程了，所有原来的pid变成了父pid
        cc_pid = getpid();                //重新获取pid,即本子进程的pid
        cc_worker_process_cycle(inum,pprocname);    //我希望所有worker子进程，在这个函数里不断循环着不出来，也就是说，子进程流程不往下边走;
        break;

    default: //这个应该是父进程分支，直接break;，流程往switch之后走        
        break;
    }//end switch

    //父进程分支会走到这里，子进程流程不往下边走-------------------------
    //若有需要，以后再扩展增加其他代码......
    return pid;
}

//描述：worker子进程的功能函数，每个woker子进程，就在这里循环着了（无限循环【处理网络事件和定时器事件以对外提供web服务】）
//     子进程分叉才会走到之类
//inum：进程编号【0开始】
static void cc_worker_process_cycle(int inum,const char *pprocname) 
{

    cc_process = CC_PROCESS_WORKER;     //设置进程的类型
    //重新为子进程设置进程名，不能与父进程重复
    cc_worker_process_init(inum);
    cc_setproctitle(pprocname); //设置标题   
    cc_log_error_core(CC_LOG_NOTICE,0,"%s %P 启动并开始运行......!",pprocname,cc_pid);
    
    for(;;)
    {

        //先sleep一下 以后扩充.......
        //printf("worker进程休息1秒");       
        //fflush(stdout); //刷新标准输出缓冲区，把输出缓冲区里的东西打印到标准输出设备上，则printf里的东西会立即输出；
        sleep(1); //休息1秒       
        //usleep(100000);
        //cc_log_error_core(0,0,"good--这是子进程，编号为%d,pid为%P！",inum,cc_pid);
        //printf("1212");
        //if(inum == 1)
        //{
            //ngx_log_stderr(0,"good--这是子进程，编号为%d,pid为%P",inum,ngx_pid); 
            //printf("good--这是子进程，编号为%d,pid为%d\r\n",inum,ngx_pid);
            //ngx_log_error_core(0,0,"good--这是子进程，编号为%d",inum,ngx_pid);
            //printf("我的测试哈inum=%d",inum++);
            //fflush(stdout);
        //}
            
        //ngx_log_stderr(0,"good--这是子进程，pid为%P",ngx_pid); 
        //ngx_log_error_core(0,0,"good--这是子进程，编号为%d,pid为%P",inum,ngx_pid);
        cc_process_events_and_timers();
    } //end for(;;)
    return;
}

//描述：子进程创建时调用本函数进行一些初始化工作
static void cc_worker_process_init(int inum)
{
    sigset_t  set;      //信号集

    sigemptyset(&set);  //清空信号集
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1)  //原来是屏蔽那10个信号【防止fork()期间收到信号导致混乱】，现在不再屏蔽任何信号【接收任何信号】
    {
        cc_log_error_core(CC_LOG_ALERT,errno,"ngx_worker_process_init()中sigprocmask()失败!");
    }
    g_socket.cc_epoll_init();
    //....将来再扩充代码
    //....
    return;
}



