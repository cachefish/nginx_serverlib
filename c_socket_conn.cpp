#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>    //uintptr_t
#include <stdarg.h>    //va_start....
#include <unistd.h>    //STDERR_FILENO等
#include <sys/time.h>  //gettimeofday
#include <time.h>      //localtime_r
#include <fcntl.h>     //open
#include <errno.h>     //errno
//#include <sys/socket.h>
#include <sys/ioctl.h> //ioctl
#include <arpa/inet.h>

#include "c_conf.h"
#include "c_macro.h"
#include "c_global.h"
#include "c_func.h"
#include "c_socket.h"
#include"c_memory.h"
//从连接池中获取一个空闲连接
lpcc_connection_t CSocket::cc_get_connection(int isock)
{
    lpcc_connection_t c = m_pfree_connections;  //空闲连接链表头

    if(c == NULL){
        cc_log_stderr(0,"CSocket::cc_get_connection()中空闲链表为空");
        return NULL;
    }

    m_pfree_connections =  c->next;
    m_free_connection_n--;

    //把c指向的对象中有用的东西搞出来保存成变量
    uintptr_t   instance = c->instance;
    uint64_t    iCurrsequence = c->iCurrsequence;
    //...


    memset(c,0,sizeof(cc_connection_t));
    c->fd = isock;   
    c->curStat = _PKG_HD_INIT;                                              //收包状态处于 初始状态，准备接收数据包头
    c->precvbuf = c->dataHeadInfo;                                      //收包我要先收到这里来，因为我要先收包头，所以收数据的buff直接就是dataHeadInfo
    c->irecvlen = sizeof(COMM_PKG_HEADER);                //指定收数据的长度
    c->ifnewrecvMem = false;
    c->pnewMemPointer = NULL;
    //...

    c->instance = !instance;//分配内存时候，连接池里每个连接对象这个变量给的值都为1，所以这里取反应该是0【有效】；
    c->iCurrsequence = iCurrsequence;
    ++c->iCurrsequence; //每次取用该值都增加1

    return c;
}

//归还参数c所代表的连接到到连接池中
void CSocket::cc_free_connection(lpcc_connection_t c)
{
    if(c->ifnewrecvMem == true){
        CMemory::GetInstance()->FreeMemory(c->pnewMemPointer);
        c->pnewMemPointer = NULL;
        c->ifnewrecvMem        = false;
    }

    c->next = m_pfree_connections; //回收的节点指向原来串起来的空闲链的链头

    ++c->iCurrsequence; //回收后，该值就增加1

    m_pfree_connections = c;        //修改 原来的链头使链头指向新节点
    ++m_free_connection_n;          //空闲连接+1
    return;
}

//用户连入，我们accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放
void CSocket::cc_close_connection(lpcc_connection_t c)
{
    if(close(c->fd)==-1){
        cc_log_error_core(CC_LOG_ALERT,errno,"CSocket::cc_close_accept_connection()中close(%d)失败!",c->fd);
    }
    c->fd = -1;
    cc_free_connection(c);
    return;
}