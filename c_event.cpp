#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<signal.h>
#include<errno.h>

#include"c_func.h"
#include"c_macro.h"
#include"c_conf.h"


//处理网络事件和定时器事件
void cc_process_events_and_timers()
{
    g_socket.cc_epoll_process_events(-1); //-1表示卡着等待
    
    g_socket.printInfo();

    //...再完善
}