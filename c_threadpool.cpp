#include<stdarg.h>
#include<unistd.h>

#include"c_global.h"
#include"c_func.h"
#include"c_threadpool.h"
#include"c_memory.h"
#include"c_macro.h"

pthread_mutex_t CThreadPool::m_pthreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t CThreadPool::m_pthreadCond = PTHREAD_COND_INITIALIZER;
bool CThreadPool::m_shutdown = false;

CThreadPool::CThreadPool():
m_iRunningThreadNum(0),
m_iLastEmgTime(0)
{}

CThreadPool::~CThreadPool()
{

}

bool CThreadPool::Create(int threadNum)
{
    ThreadItem *pNew;
    int err;

    m_iThreadNum = threadNum; //保存要创建的线程数量    
    
    for(int i = 0; i < m_iThreadNum; ++i)
    {
        pNew = new ThreadItem(this);
        m_threadVector.push_back(pNew);             //创建 一个新线程对象 并入到容器中         
        err = pthread_create(&pNew->_Handle, NULL, ThreadFunc, pNew);      //创建线程，错误不返回到errno，一般返回错误码
        if(err != 0)
        {
            //创建线程有错
            cc_log_stderr(err,"CThreadPool::Create()创建线程%d失败，返回的错误码为%d!",i,err);
            return false;
        }
        else
        {
            //创建线程成功
            //ngx_log_stderr(0,"CThreadPool::Create()创建线程%d成功,线程id=%d",pNew->_Handle);
        }        
    } //end for

lblfor:
    for( auto iter = m_threadVector.begin();iter != m_threadVector.end();++iter)
    {
        if((*iter)->isrunning == false){
            usleep(100*1000);
            goto lblfor;
        } 
    }
    return true;
}

//线程入口函数
void* CThreadPool::ThreadFunc(void* threadData)
{
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CThreadPool *pThreadPoolObj = pThread->_pThis;

    char *jobbuf = NULL;
    CMemory *p_memory = CMemory::GetInstance();

    int ret;

    pthread_t tid = pthread_self();
    while (true)
    {
        ret = pthread_mutex_lock(&m_pthreadMutex);
        if(ret != 0) cc_log_stderr(ret,"CThreadPool::ThreadFunc()中pthread_create()失败,返回的错误码为%d",ret);

        while((jobbuf = g_socket.outMsgRecvQueue()) == NULL && m_shutdown == false)
        {
            if(pThread->isrunning == false){
                pThread->isrunning = true;
            }

            pthread_cond_wait(&m_pthreadCond,&m_pthreadMutex);//所有线程卡在这里等待的

        }

        ret = pthread_mutex_unlock(&m_pthreadMutex);
        if(ret != 0) cc_log_stderr(ret,"CThreadPool::ThreadFunc()中pthread_cond_wait()失败，返回的错误码为%d!",ret);

        if(m_shutdown){
            if(jobbuf != NULL)
            {
                p_memory->FreeMemory(jobbuf);
            }
            break;
        }
        //有数据处理
        ++pThreadPoolObj->m_iRunningThreadNum;  //线程数+1

        p_memory->FreeMemory(jobbuf);              //释放消息内存 
        --pThreadPoolObj->m_iRunningThreadNum;     //原子-1

    }

    return (void*)0;
}


//停止所有线程
void CThreadPool::StopAll()
{
    if(m_shutdown == true)
    {
        return;
    }
    m_shutdown = true;
    int err = pthread_cond_broadcast(&m_pthreadCond);
    if(err != 0)
    {
        cc_log_stderr(err,"CThreadPool::StopAll()中pthread_cond_broadcast()失败，返回的错误码为%d!",err);
        return;
    }
    for(auto iter = m_threadVector.begin();iter != m_threadVector.end();++iter)
    {
            pthread_join((*iter)->_Handle,NULL);
    }

    pthread_mutex_destroy(&m_pthreadMutex);
    pthread_cond_destroy(&m_pthreadCond);

    for(auto iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
	{
		if(*iter)
			delete *iter;
	}
	m_threadVector.clear();

    cc_log_stderr(0,"CThreadPool::StopAll()成功返回，线程池中线程全部正常结束!");
    return;    

}

//来任务了，调一个线程池中的线程下来干活
void CThreadPool::Call(int irmqc)
{
    int err = pthread_cond_signal(&m_pthreadCond); //唤醒一个等待该条件的线程，也就是可以唤醒卡在pthread_cond_wait()的线程
    if(err != 0 )
    {
        cc_log_stderr(err,"CThreadPool::Call()中pthread_cond_signal()失败，返回的错误码为%d!",err);
    }

    if(m_iThreadNum == m_iRunningThreadNum) //线程池中线程总量，跟当前正在干活的线程数量一样，说明所有线程都忙碌起来，线程不够用了
    {        
        //线程不够用了
        //ifallthreadbusy = true;
        time_t currtime = time(NULL);
        if(currtime - m_iLastEmgTime > 10) //最少间隔10秒钟才报一次线程池中线程不够用的问题；
        {
            //两次报告之间的间隔必须超过10秒，不然如果一直出现当前工作线程全忙，但频繁报告日志也够烦的
            m_iLastEmgTime = currtime;  //更新时间
            //写日志，通知这种紧急情况给用户，用户要考虑增加线程池中线程数量
            cc_log_stderr(0,"CThreadPool::Call()中发现线程池中当前空闲线程数量为0，要考虑扩容线程池了!");
        }
    }
    return;
}