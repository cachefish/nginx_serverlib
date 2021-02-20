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

    fd = -1;
    curStat = _PKG_HD_INIT;
    precvbuf = dataHeadInfo;
    irecvlen = sizeof(COMM_PKG_HEADER);

    precvMemPointer = NULL;
    iThrowsendCount = 0;
    psendMemPointer = NULL;             //发送数据头指针记录
    events = 0;                                               //epoll事件先给0
    lastPingTime = time(NULL);            //上次ping的时间

    FloodkickLastTime = 0;                      //Flood攻击上次收到包的时间
    FloodAttackCount =0;                        //Flood攻击在该时间内收到包的次数统计

    iSendCount        = 0;                            //发送队列中有的数据条目数，若client只发不收，则可能造成此数过大，依据此数做出踢出处理 

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
}

//归还参数c所代表的连接到到连接池中
void CSocket::cc_free_connection(lpcc_connection_t pConn)
{
    CLock lock(&m_connectionMutex);

    pConn->PutOneToFree();

    m_freeconnectionList.push_back(pConn);
    ++m_free_connection_n;
    return;
}

//有些连接，我们不希望马上释放,将隔一段时间才释放的连接先放到一个队列中来  延迟回收
void CSocket::inRecyConnectQueue(lpcc_connection_t pConn)
{
    std::list<lpcc_connection_t>::iterator pos;
    bool isfind = false;
    CLock lock(&m_recyconnqueueMutex);
    for(pos = m_recyconnectionList.begin();pos != m_recyconnectionList.end();++pos)
    {
        if((*pos) == pConn)
        {
            isfind = true;
            break;
        }
    }
    if(isfind = true)
    {
        return;
    }
    pConn->inRecyTime = time(NULL);
    ++pConn->iCurrsequence;
    m_recyconnectionList.push_back(pConn);
    ++m_total_connection_n;
    --m_onlineUserCount;    //连入用户数量-1
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
                    continue;   //没有到释放时间
                }
                //只要到达释放时间,iThrowsendCount就=0
                if(p_Conn->iThrowsendCount > 0){
                        cc_log_stderr(0,"CSocket::ServerRecyConnectionThread()中到释放时间却发现p_Conn.iThrowsendCount!=0,不该发生");
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

    if(pConn->fd != -1){
            close(pConn->fd);
            pConn->fd = -1;
    }
    return;
}