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
#include "c_memory.h"
#include "c_lockmutex.h"

//设置踢出时钟(向multimap表中增加内容)，用户三次握手成功连入，然后我们开启了踢人开关【Sock_WaitTimeEnable = 1】，那么本函数被调用；
void CSocket::AddToTimerQueue(lpcc_connection_t pConn)
{
    CMemory *p_memory = CMemory::GetInstance();
    time_t futtime = time(NULL);
    futtime += m_iWaitTime;

    CLock lock(&m_timerqueueMutex);
    LPSTRUC_MSG_HEADER tmpMsgHeader = (LPSTRUC_MSG_HEADER)p_memory->AllocMemory(m_iLenMsgHeader,false);
    tmpMsgHeader->pConn = pConn;
    tmpMsgHeader->iCurrsequence = pConn->iCurrsequence;
    m_timerQueuemap.insert(std::make_pair(futtime,tmpMsgHeader));
    m_cur_size_++;
    m_timer_value_ = GetEarliestTime();
    return;
}

//从multimap中取得最早的时间返回去，调用者负责互斥
time_t CSocket::GetEarliestTime()
{
    std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos;
    pos = m_timerQueuemap.begin();
    return pos->first;
}

//从m_timeQueuemap移除最早的时间，并把最早这个时间所在的项的值所对应的指针 返回
LPSTRUC_MSG_HEADER CSocket::RemoveFirstTimer()
{
    //std::multimap<timer_t,LPSTRUC_MSG_HEADER>::iterator pos;
    LPSTRUC_MSG_HEADER p_tmp;
    if(m_cur_size_ <= 0)
    {
        return NULL;
    }
    auto pos = m_timerQueuemap.begin();
    p_tmp= pos->second;
    m_timerQueuemap.erase(pos);
    --m_cur_size_;
    return p_tmp;
}
//根据给的当前时间，从m_timeQueuemap找到比这个时间更早的节点【1个】返回去
LPSTRUC_MSG_HEADER CSocket::GetOverTimeTimer(time_t cur_time)
{   
    CMemory *p_memory = CMemory::GetInstance();
    LPSTRUC_MSG_HEADER ptmp;

    if(m_cur_size_ == 0 || m_timerQueuemap.empty())
    {
        return NULL;
    }
    time_t earliesttime = GetEarliestTime(); 
    if(earliesttime <= cur_time)
    {
        ptmp = RemoveFirstTimer();
        if(m_ifTimeOutKick != 1)
        {
            time_t newinqueuetime = cur_time + m_iWaitTime;
            LPSTRUC_MSG_HEADER tmpMsgHeader = (LPSTRUC_MSG_HEADER)p_memory->AllocMemory(sizeof(STRUC_MSG_HEADER),false);
            tmpMsgHeader->pConn = ptmp->pConn;
            tmpMsgHeader->iCurrsequence = ptmp->iCurrsequence;
            m_timerQueuemap.insert(std::make_pair(newinqueuetime,tmpMsgHeader));
            m_cur_size_++;
        }
    
        if(m_cur_size_ > 0)
        {
            m_timer_value_ = GetEarliestTime();
        }
        return ptmp;
    }
    return NULL;
}
//把指定用户tcp连接从timer表中踢出去
void CSocket::DeleteFromTimerQueue(lpcc_connection_t pConn)
{
   // std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos,posend;
	CMemory *p_memory = CMemory::GetInstance();
    CLock lock(&m_timerqueueMutex);

lblMTQM:
	auto pos    = m_timerQueuemap.begin();
	auto posend = m_timerQueuemap.end();
	for(; pos != posend; ++pos)	
	{
		if(pos->second->pConn == pConn)
		{			
			p_memory->FreeMemory(pos->second);  //释放内存
			m_timerQueuemap.erase(pos);
			--m_cur_size_; //减去一个元素，必然要把尺寸减少1个;								
			goto lblMTQM;
		}		
	}
	if(m_cur_size_ > 0)
	{
		m_timer_value_ = GetEarliestTime();
	}
    return;    
}

//清理时间队列中所有内容
void CSocket::clearAllFromTimerQueue()
{	
	//std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos,posend;

	CMemory *p_memory = CMemory::GetInstance();	
	auto pos    = m_timerQueuemap.begin();
	auto posend = m_timerQueuemap.end();    
	for(; pos != posend; ++pos)	
	{
		p_memory->FreeMemory(pos->second);		
		--m_cur_size_; 		
	}
	m_timerQueuemap.clear();
}

//时间队列监视和处理线程，处理到期不发心跳包的用户踢出的线程
void* CSocket::ServerTimerQueueMonitorThread(void* threadData)
{
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CSocket *pSocketobj = pThread->_pThis;

    time_t absolute_time,cur_time;
    int err;
    while(g_stopEvent ==0)
    {
        if(pSocketobj->m_cur_size_ > 0) //队列不为空
        {
            absolute_time = pSocketobj->m_timer_value_;
            cur_time = time(NULL);
            if(absolute_time < cur_time)
            {
                //时间到
                std::list<LPSTRUC_MSG_HEADER> m_lsIdleList;  //要处理的内容
                LPSTRUC_MSG_HEADER result;
                err = pthread_mutex_lock(&pSocketobj->m_timerqueueMutex);
                if(err != 0) cc_log_stderr(err,"CSocket::ServerTimerQueueMonitorThread()中pthread_mutex_lock()失败");
                while ((result = pSocketobj->GetOverTimeTimer(cur_time)) != NULL) //一次性的把所有超时节点都拿过来
				{
					m_lsIdleList.push_back(result); 
				}//end while
                err = pthread_mutex_unlock(&pSocketobj->m_timerqueueMutex); 
                if(err != 0) cc_log_stderr(err,"CSocket::ServerTimerQueueMonitorThread()中pthread_mutex_lock()失败");
                LPSTRUC_MSG_HEADER tmpmsg;
                while(!m_lsIdleList.empty())
                {
                    tmpmsg = m_lsIdleList.front();
                    m_lsIdleList.pop_front();
                    pSocketobj->procPingTimeOutChecking(tmpmsg,cur_time);//检查心跳超时问题
                }
            }
        }//end if(pSocketObj->m_cur_size_ > 0)
        usleep(500*1000);
    } 
    return (void*)0;
}

//心跳包检测时间到，该去检测心跳包是否超时的事宜，本函数只是把内存释放，子类应该重新事先该函数以实现具体的判断动作
void CSocket::procPingTimeOutChecking(LPSTRUC_MSG_HEADER tmpmsg,time_t cur_time)
{
	CMemory *p_memory = CMemory::GetInstance();
	p_memory->FreeMemory(tmpmsg);    
}