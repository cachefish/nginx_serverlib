#include <stdarg.h>
#include <unistd.h>  //usleep

#include "c_global.h"
#include "c_func.h"
#include "c_threadpool.h"
#include "c_memory.h"
#include "c_macro.h"

pthread_mutex_t CThreadPool::m_pthreadMutex = PTHREAD_MUTEX_INITIALIZER;  //#define PTHREAD_MUTEX_INITIALIZER ((pthread_mutex_t) -1)
pthread_cond_t CThreadPool::m_pthreadCond = PTHREAD_COND_INITIALIZER;     //#define PTHREAD_COND_INITIALIZER ((pthread_cond_t) -1)
bool CThreadPool::m_shutdown = false;    //刚开始标记整个线程池的线程是不退出的      

//构造函数
CThreadPool::CThreadPool()
{
    m_iRunningThreadNum = 0; 
    m_iLastEmgTime = 0;      
   
}

//析构函数
CThreadPool::~CThreadPool()
{
    //资源释放在StopAll()里统一进行
}

//创建线程池中的线程，要手工调用，不在构造函数里调用了
//返回值：所有线程都创建成功则返回true，出现错误则返回false
bool CThreadPool::Create(int threadNum)
{    
    ThreadItem *pNew;
    int err;

    m_iThreadNum = threadNum; //保存要创建的线程数量    
    
    for(int i = 0; i < m_iThreadNum; ++i)
    {
        m_threadVector.push_back(pNew = new ThreadItem(this));             //创建 一个新线程对象 并入到容器中         
        err = pthread_create(&pNew->_Handle, NULL, ThreadFunc, pNew);      //创建线程，错误不返回到errno，一般返回错误码
        if(err != 0)
        {
            //创建线程有错
            return false;
        }
        else
        {
            //创建线程成功
        }        
    } //end for

    std::vector<ThreadItem*>::iterator iter;
lblfor:
    for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
    {
        if( (*iter)->ifrunning == false) //这个条件保证所有线程完全启动起来，以保证整个线程池中的线程正常工作；
        {
            //这说明有 没有启动完全的线程
            usleep(100 * 1000);  
            goto lblfor;
        }
    }
    return true;
}

//线程入口函数，当用pthread_create()创建线程后，这个ThreadFunc()函数都会被立即执行；
void* CThreadPool::ThreadFunc(void* threadData)
{
    //这个是静态成员函数，是不存在this指针的；
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CThreadPool *pThreadPoolObj = pThread->_pThis;

    char *jobbuf = NULL;    
    CMemory *p_memory = CMemory::GetInstance();	    
    int err;

    pthread_t tid = pthread_self(); //获取线程自身id，以方便调试打印信息等    
    while(true)
    {
        //线程用pthread_mutex_lock()函数去锁定指定的mutex变量，若该mutex已经被另外一个线程锁定了，该调用将会阻塞线程直到mutex被解锁。  
        err = pthread_mutex_lock(&m_pthreadMutex);  
        if(err != 0) cc_log_stderr(err,"CThreadPool::ThreadFunc()pthread_mutex_lock()失败，返回的错误码为%d!",err);//有问题，要及时报告
        

        while( (jobbuf = g_socket.outMsgRecvQueue()) == NULL && m_shutdown == false)
        {
            //如果这个pthread_cond_wait被唤醒
              //那么会立即再次执行g_socket.outMsgRecvQueue()，如果拿到了一个NULL，则继续在这里wait着();
            if(pThread->ifrunning == false)            
                pThread->ifrunning = true; //标记为true了才允许调用StopAll()：测试中发现如果Create()和StopAll()紧挨着调用，就会导致线程混乱，所以每个线程必须执行到这里，才认为是启动成功了      
            pthread_cond_wait(&m_pthreadCond, &m_pthreadMutex); //整个服务器程序刚初始化的时候，所有线程必然是卡在这里等待的；
        }
        err = pthread_mutex_unlock(&m_pthreadMutex); //先解锁mutex
        if(err != 0)  cc_log_stderr(err,"CThreadPool::ThreadFunc()pthread_cond_wait()失败，返回的错误码为%d!",err);//有问题，要及时报告
        
        //先判断线程退出这个条件
        if(m_shutdown)
        {            
            if(jobbuf != NULL)
            {
                p_memory->FreeMemory(jobbuf);   
            }
            break; 
        }

        //有数据可以处理
        ++pThreadPoolObj->m_iRunningThreadNum;    //原子+1，这比互斥量要快很多

cc_log_stderr(0,"执行开始---begin,tid=%ui!",tid);
sleep(5); //临时测试代码
cc_log_stderr(0,"执行结束---end,tid=%ui!",tid);

        p_memory->FreeMemory(jobbuf);              //释放消息内存 
        --pThreadPoolObj->m_iRunningThreadNum;     //原子-1

    } //end while(true)

    return (void*)0;
}

//停止所有线程【等待结束线程池中所有线程，该函数返回后，应该是所有线程池中线程都结束了】
void CThreadPool::StopAll() 
{
    if(m_shutdown == true)
    {
        return;
    }
    m_shutdown = true;

    //(2)唤醒等待该条件【卡在pthread_cond_wait()的】的所有线程，一定要在改变条件状态以后再给线程发信号
    int err = pthread_cond_broadcast(&m_pthreadCond); 
    if(err != 0)
    {
        cc_log_stderr(err,"CThreadPool::StopAll()中pthread_cond_broadcast()失败，返回的错误码为%d!",err);
        return;
    }

    //(3)等等线程，让线程真返回    
    std::vector<ThreadItem*>::iterator iter;
	for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
    {
        pthread_join((*iter)->_Handle, NULL); //等待一个线程终止
    }

    //流程走到这里，那么所有的线程池中的线程肯定都返回了；
    pthread_mutex_destroy(&m_pthreadMutex);
    pthread_cond_destroy(&m_pthreadCond);    

    //(4)释放一下new出来的ThreadItem【线程池中的线程】    
	for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
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
  
    //(1)如果当前的工作线程全部都忙，则要报警
    //bool ifallthreadbusy = false;
    if(m_iThreadNum == m_iRunningThreadNum) //线程池中线程总量，跟当前正在干活的线程数量一样，说明所有线程都忙碌起来，线程不够用了
    {        
        //线程不够用了
        //ifallthreadbusy = true;
        time_t currtime = time(NULL);
        if(currtime - m_iLastEmgTime > 10) //最少间隔10秒钟才报一次线程池中线程不够用的问题；
        {
            //两次报告之间的间隔必须超过10秒，不然如果一直出现当前工作线程全忙，但频繁报告日志也够烦的
            m_iLastEmgTime = currtime;  //更新时间
            cc_log_stderr(0,"CThreadPool::Call()中发现线程池中当前空闲线程数量为0，要考虑扩容线程池了!");
        }
    } //end if 

    return;
}
