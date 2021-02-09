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
#include"c_lockmutex.h"
//----------------------------------------
//连接池成员函数
cc_connection_s::cc_connection_s()
{
    iCurrsequence = 0;
    pthread_mutex_init(&logicPorcMutex,NULL);
}
cc_connection_s::~cc_connection_s()
{
    pthread_mutex_destroy(&logicPorcMutex);
}

void cc_connection_s::GetOneToUse()
{
    ++iCurrsequence;

    curStat = _PKG_HD_INIT;
    precvbuf = dataHeadInfo;
    irecvlen = sizeof(COMM_PKG_HEADER);

    precvMemPointer = NULL;
    iThrowsendCount = 0;
    psendMemPointer = NULL;             //发送数据头指针记录
    events = 0;                                               //epoll事件先给0
}

void cc_connection_s::PutOneToFree()
{
    ++iCurrsequence;
    if(precvMemPointer != NULL){
        CMemory::GetInstance()->FreeMemory(precvMemPointer);
        precvMemPointer = NULL;
    }
    if(psendMemPointer != NULL){
        CMemory::GetInstance()->FreeMemory(psendMemPointer);\
        psendMemPointer = NULL;
    }
    iThrowsendCount = 0;
}

//---------------------------------------
//初始化连接池
void CSocket::initconnection()
{
    lpcc_connection_t p_Conn;
    CMemory *p_memory =  CMemory::GetInstance();

    int ilenconnpool = sizeof(cc_connection_t);
    for(int i =0;i<m_worker_connections;++i){
        p_Conn = (lpcc_connection_t)p_memory->AllocMemory(ilenconnpool,true);

        p_Conn = new(p_Conn) cc_connection_t();
        p_Conn->GetOneToUse();
        m_connectionList.push_back(p_Conn);
        m_freeconnectionList.push_back(p_Conn);
    }
    m_free_connection_n = m_total_connection_n = m_connectionList.size();
    return;
}

void CSocket::clearconnection()
{
    lpcc_connection_t p_Conn;
    CMemory *p_memory = CMemory::GetInstance();

    while(!m_connectionList.empty())
    {
            p_Conn = m_connectionList.front();
            m_connectionList.pop_front();
            p_Conn->~cc_connection_t();
            p_memory->FreeMemory(p_Conn);
    }
}

//---------------------------------------------


//从连接池中获取一个空闲连接
lpcc_connection_t CSocket::cc_get_connection(int isock)
{
    CLock lock(&m_connectionMutex);

    if(!m_freeconnectionList.empty())
    {   
        lpcc_connection_t p_Conn = m_freeconnectionList.front();
        m_freeconnectionList.pop_front();
        p_Conn->GetOneToUse();;
        --m_free_connection_n;
        p_Conn->fd = isock;
        return p_Conn;
    }

    //没有空闲的连接，则需要重新建立连接
    CMemory *p_memory = CMemory::GetInstance();
    lpcc_connection_t p_Conn = (lpcc_connection_t)p_memory;

    p_Conn = new(p_Conn) cc_connection_t();

    p_Conn->GetOneToUse();
    m_connectionList.push_back(p_Conn);
    ++m_total_connection_n;
    p_Conn->fd = isock;
    return p_Conn;

    /*
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
    */
    //return c;
}

//归还参数c所代表的连接到到连接池中
void CSocket::cc_free_connection(lpcc_connection_t pConn)
{
    CLock lock(&m_connectionMutex);

    pConn->PutOneToFree();

    m_freeconnectionList.push_back(pConn);
    ++m_free_connection_n;
    /*
    if(c->ifnewrecvMem == true){
        CMemory::GetInstance()->FreeMemory(c->pnewMemPointer);
        c->pnewMemPointer = NULL;
        c->ifnewrecvMem        = false;
    }

    c->next = m_pfree_connections; //回收的节点指向原来串起来的空闲链的链头

    ++c->iCurrsequence; //回收后，该值就增加1

    m_pfree_connections = c;        //修改 原来的链头使链头指向新节点
    ++m_free_connection_n;          //空闲连接+1
    return;*/
    return;
}

//有些连接，我们不希望马上释放,将隔一段时间才释放的连接先放到一个队列中来

void CSocket::inRecyConnectQueue(lpcc_connection_t pConn)
{
    CLock lock(&m_recyconnqueueMutex);

    pConn->inRecyTime = time(NULL);
    ++pConn->iCurrsequence;
    m_recyconnectionList.push_back(pConn);
    ++m_total_connection_n;
    return;
}

//处理连接回收的线程
void* CSocket::ServerRecyConnectionThread(void *threadData)
{
    ThreadItem *pThread =  static_cast<ThreadItem*>(threadData);
    CSocket *pSocketobj = pThread->_pThis;

    time_t currtime;
    int err;
    std::list<lpcc_connection_t>::iterator pos,posend;
    lpcc_connection_t p_Conn;

    while(1)
    {
        usleep(200*1000);

        if(pSocketobj->m_total_recyconnection_n>0)
        {
            currtime = time(NULL);
            err = pthread_mutex_lock(&pSocketobj->m_recyconnqueueMutex);
            if(err !=0 ) cc_log_stderr(err,"CSocket::ServerRecyConnectionThread()中pthread_mutex_lock()失败，返回的错误码为%d!",err);

lblRRTD:
            pos = pSocketobj->m_recyconnectionList.begin();
            posend = pSocketobj->m_recyconnectionList.end();

            for(;pos!=posend;++pos){
                p_Conn = (*pos);
                if((p_Conn->inRecyTime + pSocketobj->m_RecyConnectionWaitTime)>currtime && (g_stopEvent == 0)){
                    continue;
                }
                --pSocketobj->m_total_recyconnection_n;
                pSocketobj->m_recyconnectionList.erase(pos);

                pSocketobj->cc_free_connection(p_Conn);
                goto lblRRTD;

            }
            err = pthread_mutex_unlock(&pSocketobj->m_recyconnqueueMutex);
            if(err != 0 ) cc_log_stderr(err,"CSocekt::ServerRecyConnectionThread()pthread_mutex_unlock()失败，返回的错误码为%d!",err);
        }
        if(g_stopEvent==1)//要退出整个程序
        {
            if(pSocketobj->m_total_recyconnection_n>0)
            {
                err = pthread_mutex_lock(&pSocketobj->m_recyconnqueueMutex);
                if(err != 0) cc_log_stderr(err,"CSocket::ServerRecyConnectionThread()中pthread_mutex_lock2()失败，返回的错误码为%d!",err);
            
        lblRRTD2:
                pos = pSocketobj->m_recyconnectionList.begin();
                posend = pSocketobj->m_recyconnectionList.end();
                for(;pos != posend;++pos)
                {
                    p_Conn = (*pos);
                    --pSocketobj->m_total_recyconnection_n;
                    pSocketobj->m_recyconnectionList.erase(pos);
                    goto lblRRTD2;
                }
                err = pthread_mutex_unlock(&pSocketobj->m_recyconnqueueMutex);
                if(err != 0) cc_log_stderr(err,"CSocket::ServerRecyConnectionThread()中pthread_mutex_unlock2()失败，返回的错误码为%d!",err);
            }
            break;
        }
    }
    return (void*)0;
}


//用户连入，我们accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放
void CSocket::cc_close_connection(lpcc_connection_t pConn)
{
    cc_free_connection(pConn);

    if(close(pConn->fd)==-1){
        cc_log_error_core(CC_LOG_ALERT,errno,"CSocket::cc_close_accept_connection()中close(%d)失败!",pConn->fd);
    }
    return;
}