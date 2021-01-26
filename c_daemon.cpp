//守护进程相关

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<sys/stat.h>
#include<fcntl.h>

#include"c_func.h"
#include"c_macro.h"
#include"c_conf.h"

//守护进程初始化
int cc_daemon()
{
    //(1)创建子进程
    switch (fork())
    {
    case -1:
        cc_log_error_core(CC_LOG_EMERG,errno,"cc_daemon()中的fork()失败");
        return -1;
        break;
    case 0:
        break;      //子进程
    default:
        //父进程返回主流程中
        return 1;
    }

    //子进程操作
    cc_parent = cc_pid;
    cc_pid = getpid();  
    
    //（2）脱离终端
    if(setsid()==-1) //让进程运行在新的会话里从而成为不属于此终端的子进程
    {
        cc_log_error_core(CC_LOG_EMERG,errno,"cc_daemon()中的setsid()失败");
        return -1;
    }
    //(3)重设文件权限掩码
    umask(0);

    //(4)打开黑洞设备
    int fd = open("/dev/null",O_RDWR);
    if(fd == -1){
        cc_log_error_core(CC_LOG_EMERG,errno,"cc_daemon()中的open(/dev/null)失败");
        return -1;
    }
    if(dup2(fd,STDIN_FILENO)==-1){  //先关闭打开的文件描述符
        cc_log_error_core(CC_LOG_EMERG,errno,"cc_daemon()中的dup2(fd,STDIN_FILENO)失败");
        return -1;
    }
    if(dup2(fd,STDOUT_FILENO)==-1){  //再关闭STDOUT_FILENO，使输出指向/dev/null
        cc_log_error_core(CC_LOG_EMERG,errno,"cc_daemon()中的dup2(fd,STDIN_FILENO)失败");
        return -1;
    }

    if (fd > STDERR_FILENO)  //fd应该是3，这个应该成立
     {
        if (close(fd) == -1)  //释放资源这样这个文件描述符就可以被复用；不然这个数字【文件描述符】会被一直占着；
        {
            cc_log_error_core(CC_LOG_EMERG,errno, "cc_daemon()中close(fd)失败!");
            return -1;
        }
    }
    return 0;
}