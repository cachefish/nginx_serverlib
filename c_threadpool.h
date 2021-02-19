#ifndef __C_THREADPOOL_H__
#define __C_THREADPOOL_H__

#include <vector>
#include <pthread.h>
#include <atomic>   

//线程池相关类
class CThreadPool
{
public:
    //构造函数
    CThreadPool();               
    
    //析构函数
    ~CThreadPool();                           

public:
    bool Create(int threadNum);                 //创建该线程池中的所有线程
    void StopAll();                             //使线程池中的所有线程退出

    void inMsgRecvQueueAndSingal(char *buf);//收到一个完整消息后，入消息队列，并触发线程池中线程来处理该消息
    void Call();                       //来任务了，调一个线程池中的线程下来干活
    int  getRecvMsgQueueCount(){return m_iRecvMsgQueueCount;} //获取接收消息队列大小

private:
    static void* ThreadFunc(void *threadData);  //新线程的线程回调函数

    void clearMsgRecvQueue();               //清理接收消息队列

private:
    //定义一个 线程池中的 线程 的结构，以后可能做一些统计之类的 功能扩展，所以引入这么个结构来 代表线程 感觉更方便一些；    
    struct ThreadItem   
    {
        pthread_t   _Handle;                              //线程句柄
        CThreadPool *_pThis;                              //记录线程池的指针	
        bool        ifrunning;                            //标记是否正式启动起来，启动起来后，才允许调用StopAll()来释放

        //构造函数
        ThreadItem(CThreadPool *pthis):_pThis(pthis),ifrunning(false){}                             
        //析构函数
        ~ThreadItem(){}        
    };

private:
    static pthread_mutex_t     m_pthreadMutex;      //线程同步互斥量/也叫线程同步锁
    static pthread_cond_t      m_pthreadCond;       //线程同步条件变量
    static bool                m_shutdown;          //线程退出标志，false不退出，true退出

    int                        m_iThreadNum;        //要创建的线程数量

    std::atomic<int>           m_iRunningThreadNum; //线程数, 运行中的线程数，原子操作
    time_t                     m_iLastEmgTime;      //上次发生线程不够用【紧急事件】的时间,防止日志报的太频繁

    std::vector<ThreadItem *>  m_threadVector;      //线程 容器 

    std::list<char *>               m_MsgRecvQueue;         //接收数据消息队列 
    int                                          m_iRecvMsgQueueCount;          //收消息队列大小
};

#endif
