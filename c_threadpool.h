#ifndef __C_THREADPOOL_H__
#define __C_THREADPOOL_H__

#include<vector>
#include<pthread.h>
#include<atomic>

class CThreadPool
{

    public:
        CThreadPool();

        ~CThreadPool();
    public:
        bool Create(int threadNum);                                     //创建该线程池中的所有线程
        void StopAll();                                                                   //使线程池中的所有线程退出
        void Call(int irmqc);                                                       //调线程池中的线程下干活
    private:
        static void* ThreadFunc(void *threadData);  //线程回调函数
    
    private:
        //线程结构
        struct ThreadItem
        {
            pthread_t   _Handle;                            //线程句柄
            CThreadPool *_pThis;                         //记录线程池的指针
            bool isrunning;


            ThreadItem(CThreadPool *pThis):_pThis(pThis),isrunning(false){}
            ~ThreadItem(){}
        };

    private:
        static pthread_mutex_t         m_pthreadMutex;                               //同步锁
        static pthread_cond_t            m_pthreadCond;                                //条件变量
        static bool                                    m_shutdown;                                       //线程退出标志，false不退出，true退出
 
        int m_iThreadNum;                                                                                  //创建的线程数量

        std::atomic<int>                        m_iRunningThreadNum;                //线程数
        time_t                                            m_iLastEmgTime;                              //上次发生线程不够用【紧急事件】的时间,防止日志报的太频繁

        std::vector<ThreadItem *>    m_threadVector;                                 //线程 容器

};


#endif