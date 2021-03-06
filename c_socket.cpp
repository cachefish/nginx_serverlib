#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<stdarg.h>
#include<unistd.h>
#include<sys/time.h>
#include<time.h>
#include<fcntl.h>
#include<errno.h>
#include<sys/socket.h>
#include<sys/ioctl.h>
#include<arpa/inet.h>

#include"c_conf.h"
#include"c_macro.h"
#include"c_global.h"
#include"c_func.h"
#include"c_socket.h"
#include"c_memory.h"
#include"c_lockmutex.h"
CSocket::CSocket():m_worker_connections(1),
m_epollhandle(-1),
m_ListenPortCount(1),
m_RecyConnectionWaitTime(60),
m_iLenPkgHeader(sizeof(COMM_PKG_HEADER)),
m_iLenMsgHeader(sizeof(STRUC_MSG_HEADER)),
m_iSendMsgQueueCount(0),
m_total_recyconnection_n(0),
m_cur_size_(0),
m_timer_value_(0),
m_iDiscardSendPkgCount(0),
m_onlineUserCount(0),
m_lastprintTime(0)
{   
    
}


CSocket::~CSocket()
{
    std::vector<lpcc_listening_t>::iterator pos;

    for(pos = m_ListenSocketList.begin();pos!=m_ListenSocketList.end();++pos){
        delete (*pos);
    }
    m_ListenSocketList.clear();

}


//初始化
bool CSocket::Initialize()
{
    ReadConf();
    bool ret = cc_open_listening_sockets();
    return ret;
}

bool CSocket::Initialize_subproc()
{
    //发消息互斥量初始化
    if(pthread_mutex_init(&m_sendMessageQueueMutex,NULL) != 0){
        cc_log_stderr(0,"CSocket::initial_subproc()中的pthread_mutex_init()函数失败");
        return false;
    }
    //连接池相关互斥量初始化
    if(pthread_mutex_init(&m_connectionMutex,NULL)!=0)
    {
        cc_log_stderr(0,"CSocket::initial_subproc()中的pthread_mutex_init()函数失败");
        return false;
    }
    //连接回收队列相关互斥量初始化
    if(pthread_mutex_init(&m_recyconnqueueMutex,NULL)!=0)
    {
        cc_log_stderr(0,"CSocket::initial_subproc()中的pthread_mutex_init()函数失败");
        return false;
    }
    //时间处理互斥量
    if(pthread_mutex_init(&m_timerqueueMutex,NULL) != 0)
    {
        cc_log_stderr(0,"CSocket::initial_subproc()中的pthread_mutex_init()函数失败");
        return false;
    }
    //信号量初始化
    if(sem_init(&m_semEventSendQueue,0,0)==-1)
    {
        cc_log_stderr(0,"CSocket::initial_subproc()中的sem_init()函数失败");
        return false;
    }
    //开始创建线程
    int err;
    ThreadItem *pSendQueue; //专门用来发送数据线程
     m_threadVector.push_back(pSendQueue = new ThreadItem(this));
     err = pthread_create(&pSendQueue->_Handle,NULL,ServerSendQueueThread,pSendQueue);
     if(err != 0)
    {
        cc_log_stderr(0,"CSocket::Initialize_subproc()中pthread_create(ServerSendQueueThread)失败.");
        return false;
    }

    ThreadItem *pRecyconn= new ThreadItem(this);//专门用来回收连接的线程
    m_threadVector.push_back(pRecyconn );
    err = pthread_create(&pRecyconn->_Handle,NULL,ServerRecyConnectionThread,pRecyconn);
    if(err != 0)
    {
            cc_log_stderr(0,"CSocket::Initialize_subproc()中pthread_create(ServerRecyConnectionThread)失败.");
        return false;
    }

    if(m_ifkickTimeCount == 1)  //是否开启踢人时钟
    {
        ThreadItem *pTimemonitor;
        m_threadVector.push_back(pTimemonitor = new ThreadItem(this));
        err = pthread_create(&pTimemonitor->_Handle,NULL,ServerTimerQueueMonitorThread,pTimemonitor);
        if(err != 0)
        {
            cc_log_stderr(0,"CSocket::Initialize_subproc()中pthread_create(ServerTimerQueueMonitorThread)失败.");
            return false;
        }
    }
    return true;

}
//关闭退出函数[子进程中执行]
void CSocket::Shutdown_subproc()
{
    if(sem_post(&m_semEventSendQueue)==-1){//让ServerSendQueueThread()流程走下来干活
        cc_log_stderr(0,"CSocket::Shutdown_subproc()中sem_post(&m_semEventSendQueue)失败.");
    }

    std::vector<ThreadItem*>::iterator iter;
    for(iter = m_threadVector.begin();iter != m_threadVector.end();++iter){
        pthread_join((*iter)->_Handle,NULL);
    }

    for(iter = m_threadVector.begin();iter != m_threadVector.end();++iter){
        if(*iter){
            delete *iter;
        }
    }

    m_threadVector.clear();

    //队列相关
    clearMsgSendQueue();
    clearconnection();
    clearAllFromTimerQueue();
    //多线程相关
    pthread_mutex_destroy(&m_connectionMutex);
    pthread_mutex_destroy(&m_sendMessageQueueMutex);
    pthread_mutex_destroy(&m_recyconnqueueMutex);
    pthread_mutex_destroy(&m_timerqueueMutex);
    sem_destroy(&m_semEventSendQueue);
}

//清理TCP发送消息队列
void CSocket::clearMsgSendQueue()
{
    char * sTmpMempoint;
    CMemory *p_memory = CMemory::GetInstance();
    while(!m_MsgSendQueue.empty()){
        sTmpMempoint = m_MsgSendQueue.front();
        m_MsgSendQueue.pop_front();
        p_memory->FreeMemory(sTmpMempoint);
    }
}


void CSocket::ReadConf()
{
    CConfig *p_config = CConfig::GetInstance();
    m_worker_connections = p_config->GetIntDefault("worker_connections",m_worker_connections);
    m_ListenPortCount          = p_config->GetIntDefault("ListenPortCount",m_ListenPortCount);
    m_RecyConnectionWaitTime = p_config->GetIntDefault("Sock_RecyConnectionWaitTime",m_RecyConnectionWaitTime);
    m_ifkickTimeCount         = p_config->GetIntDefault("Sock_WaitTimeEnable",0);                                //是否开启踢人时钟，1：开启   0：不开启
	m_iWaitTime               = p_config->GetIntDefault("Sock_MaxWaitTime",m_iWaitTime);                         //多少秒检测一次是否 心跳超时，只有当Sock_WaitTimeEnable = 1时，本项才有用	
	m_iWaitTime               = (m_iWaitTime > 5)?m_iWaitTime:5;
    
    m_ifTimeOutKick           = p_config->GetIntDefault("Sock_TimeOutKick",0);                                   //当时间到达Sock_MaxWaitTime指定的时间时，直接把客户端踢出去，只有当Sock_WaitTimeEnable = 1时，本项才有用 

    m_floodAkEnable          = p_config->GetIntDefault("Sock_FloodAttackKickEnable",0);                          //Flood攻击检测是否开启,1：开启   0：不开启
	m_floodTimeInterval      = p_config->GetIntDefault("Sock_FloodTimeInterval",100);                            //表示每次收到数据包的时间间隔是100(毫秒)
	m_floodKickCount         = p_config->GetIntDefault("Sock_FloodKickCounter",10);                              //累积多少次踢出此人
    
    return;
}


//监听端口，支持多个端口
bool CSocket::cc_open_listening_sockets()
{

    int                                               isock;                    //socket
    struct sockaddr_in              serv_addr;          //服务器的地址结构体
    int                                               iport;                     //端口
    char                                            strinfo[100];       //临时字符串

    //初始化
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    CConfig *p_config = CConfig::GetInstance();
    for(int i =0;i < m_ListenPortCount;++i){            //监听端口

        //参数1：AF_INET：使用ipv4协议
        //参数2：SOCK_STREAM：使用TCP，表示可靠连接【相对还有一个UDP套接字，表示不可靠连接】
        //参数3：给0
        isock = socket(AF_INET,SOCK_STREAM,0);
        if(isock == -1){
            cc_log_stderr(errno,"CSocket::cc_open_listening_sockets()中的socket()失败，i=%d ",i);
            return false;
        }
        //setsockopt（）:设置一些套接字参数选项；
        //参数2：是表示级别，和参数3配套使用，也就是说，参数3如果确定了，参数2就确定了;
        //参数3：允许重用本地地址
        //设置 SO_REUSEADDR，目的第五章第三节讲解的非常清楚：主要是解决TIME_WAIT这个状态导致bind()失败的问题
        int reuseaddr = 1;
        if(setsockopt(isock,SOL_SOCKET,SO_REUSEADDR,(const void*)&reuseaddr,sizeof(reuseaddr))==-1){
            cc_log_stderr(errno,"SCocket::cc_open_listening_sockets()中的setsockopt()失败,i = %d ",i);
            close(isock);
            return false;
        }
        //处理惊群
        int reuseport = 1;
        if(setsockopt(isock,SOL_SOCKET,SO_REUSEPORT,(const void*)&reuseport,sizeof(int)==-1))
        {
            cc_log_stderr(errno,"CSocket::Initialize()中setsockopt(SO_REUSEPORT)失败");
        }
        //设置非阻塞
        if(setnonblock(isock) == false)
        {
            cc_log_stderr(errno,"SCocket::cc_open_listening_sockets()中的setnonblock()失败,i = %d ",i);
            close(isock);
            return false;
        }
        //设置本服务器要监听的地址和端口，
        strinfo[0] = 0;
        sprintf(strinfo,"ListenPort%d",i);
        iport = p_config->GetIntDefault(strinfo,10000);
        serv_addr.sin_port = htons((in_port_t)iport);

        //绑定服务器地址结构体
        if(bind(isock,(struct sockaddr*)&serv_addr,sizeof(serv_addr))==-1)
        {
            cc_log_stderr(errno,"SCocket::cc_open_listening_sockets()中的bind()失败,i = %d ",i);
            close(isock);
            return false;
        }        
        //开始监听
        if(listen(isock,CC_LISTEN_BACKLOG)==-1)
        {
            cc_log_stderr(errno,"SCocket::Initialize()中的listen()失败,i = %d ",i);
            close(isock);
            return false;
        }
        lpcc_listening_t p_listensocketitem = new cc_listening_t;
        memset(p_listensocketitem,0,sizeof(cc_listening_t));
        p_listensocketitem->port = iport;
        p_listensocketitem->fd = isock;
        cc_log_error_core(CC_LOG_INFO,0,"监听%d端口成功！",iport);
        m_ListenSocketList.push_back(p_listensocketitem);
    }
    if(m_ListenSocketList.size() <= 0)
        return false;
    return true;
}
//设置socket连接为非阻塞模式
bool CSocket::setnonblock(int sockfd)
{
    // int nb = 1;
    // if(ioctl(sockfd,FIONBIO,&nb) == -1)
    // {
    //     return false;
    // }
    // return true;

    //fcntl:file control【文件控制】相关函数，执行各种描述符控制操作
    //参数1：所要设置的描述符，这里是套接字【也是描述符的一种】
    int opts = fcntl(sockfd, F_GETFL);  //用F_GETFL先获取描述符的一些标志信息
    if(opts < 0) 
    {
        cc_log_stderr(errno,"CSocket::setnonblocking()中fcntl(F_GETFL)失败.");
        return false;
    }
    opts |= O_NONBLOCK; //把非阻塞标记加到原来的标记上，标记这是个非阻塞套接字【如何关闭非阻塞呢？opts &= ~O_NONBLOCK,然后再F_SETFL一下即可】
    if(fcntl(sockfd, F_SETFL, opts) < 0) 
    {
        cc_log_stderr(errno,"CSocket::setnonblocking()中fcntl(F_SETFL)失败.");
        return false;
    }
    return true;
}

void CSocket::cc_close_listening_sockets()
{
    for(int i =0;i<m_ListenPortCount;++i)
    {
        close(m_ListenSocketList[i]->fd);
        cc_log_error_core(CC_LOG_INFO,0,"关闭监听端口%d!",m_ListenSocketList[i]->port);
    }
    return ;
}
//将一个待发送消息入到发消息队列
void CSocket::msgSend(char *psendbuf)
{   
    CMemory *p_memory = CMemory::GetInstance();

    CLock lock(&m_sendMessageQueueMutex);
    //发送消息队列过大
    if(m_iSendMsgQueueCount > 50000)
    {
        m_iDiscardSendPkgCount++;
        p_memory->FreeMemory(psendbuf);
    }

    LPSTRUC_MSG_HEADER pMsgHeader = (LPSTRUC_MSG_HEADER)psendbuf;
    lpcc_connection_t p_Conn = pMsgHeader->pConn;
    if(p_Conn->iSendCount > 400)
    {
        //该用户收消息太慢,累积的该用户的发送队列中有的数据条目数过大，认为是恶意用户
        cc_log_stderr(0,"CSocket::msgSend()中发现某用户%d积压了大量待发送数据包，切断与他的连接！",p_Conn->fd);      
        m_iDiscardSendPkgCount++;
        p_memory->FreeMemory(psendbuf);
        ActClosesocketProc(p_Conn); //直接关闭
		return;
    }

    ++p_Conn->iSendCount;   //发送队列中的数据条目+1
    m_MsgSendQueue.push_back(psendbuf);
    ++m_iSendMsgQueueCount;

    if(sem_post(&m_semEventSendQueue)==-1)
    {
        cc_log_stderr(0,"CSocket::msgSend()sem_post()函数失败");
    }
    return;
}

//主动关闭一个连接时的要做些善后的处理函数
void CSocket::ActClosesocketProc(lpcc_connection_t p_Conn)
{
    if(m_ifkickTimeCount == 1){
        DeleteFromTimerQueue(p_Conn);   //从时间队列中将连接干掉
    }
    if(p_Conn->fd == -1)
    {
        close(p_Conn->fd);
        p_Conn->fd = -1;
    }
    if(p_Conn->iThrowsendCount > 0)
    {
        --p_Conn->iThrowsendCount;
    }
    inRecyConnectQueue(p_Conn);
    return;
}
//测试是否flood攻击成立，成立则返回true，否则返回false
bool CSocket::TestFlood(lpcc_connection_t pConn)
{
    struct timeval sCurrTime;       
    uint64_t iCurrTime; //当前时间
    bool reco = false;

    gettimeofday(&sCurrTime,NULL);
    iCurrTime = (sCurrTime.tv_sec*1000+sCurrTime.tv_usec/1000);  //毫秒
    if((iCurrTime-pConn->FloodkickLastTime)< m_floodTimeInterval)
    {
        //记录发包数
        pConn->FloodAttackCount++;
        pConn->FloodkickLastTime = iCurrTime;
    }else{
        pConn->FloodAttackCount = 0;
        pConn->FloodkickLastTime = iCurrTime;
    }

    if(pConn->FloodAttackCount >= m_floodKickCount)
    {
        reco = true;
    }
    return reco;
}

void CSocket::printInfo()
{
    time_t currtime = time(NULL);
    if((currtime - m_lastprintTime) > 10)
    {
        int tmprmqc = g_threadpool.getRecvMsgQueueCount();  //收消息队列

        m_lastprintTime = currtime;
        int tmpoLUC = m_onlineUserCount;        //在线用户数
        int tmpsmqc = m_iSendMsgQueueCount; //发送消息队列数
        cc_log_stderr(0,"------------------------------------begin--------------------------------------");
        cc_log_stderr(0,"当前在线人数/总人数(%d/%d)。",tmpoLUC,m_worker_connections);        
        cc_log_stderr(0,"连接池中空闲连接/总连接/要释放的连接(%d/%d/%d)。",m_freeconnectionList.size(),m_connectionList.size(),m_recyconnectionList.size());
        cc_log_stderr(0,"当前时间队列大小(%d)。",m_timerQueuemap.size());        
        cc_log_stderr(0,"当前收消息队列/发消息队列大小分别为(%d/%d)，丢弃的待发送数据包数量为%d。",tmprmqc,tmpsmqc,m_iDiscardSendPkgCount);        
        if( tmprmqc > 100000)
        {
            //接收队列过大，报一下，这个属于应该 引起警觉的，考虑限速等等手段
            cc_log_stderr(0,"接收队列条目数量过大(%d)，要考虑限速或者增加处理线程数量了！！！！！！",tmprmqc);
        }
        cc_log_stderr(0,"-------------------------------------end---------------------------------------");
    }
    return;
}


int CSocket::cc_epoll_init()
{
    m_epollhandle = epoll_create(m_worker_connections);
    if(m_epollhandle == -1){
        cc_log_stderr(errno,"CSocket::cc_epoll_init()中epollcreate()失败");
        exit(2);
    }
     //创建连接池【数组】、创建出来，这个东西后续用于处理所有客户端的连接
    initconnection();

    
    //(3)遍历所有监听socket【监听端口】，为每个监听socket增加一个 连接池中的连接
    //让一个socket和一个内存绑定，以方便记录该sokcet相关的数据、状态等等
    std::vector<lpcc_listening_t>::iterator  pos;
    for(pos = m_ListenSocketList.begin();pos != m_ListenSocketList.end();++pos){
        lpcc_connection_t pConn = cc_get_connection((*pos)->fd);
        if(pConn == NULL){
            cc_log_stderr(errno,"CSocket::cc_epoll_init()中cc_get_connection失败");
            exit(2);
        }
        pConn->listening = (*pos);                  //连接对象 和监听对象关联，方便通过连接对象找监听对象
        (*pos)->connection = pConn;            //监听对象 和连接对象关联，方便通过监听对象找连接对象

        //对监听端口的读事件设置处理方法，因为监听端口是用来等对方连接的发送三路握手的，所以监听端口关心的就是读事件
        pConn->rhandler=&CSocket::cc_event_accept;

        //往监听socket上增加监听事件
         if(cc_epoll_oper_event(
                                (*pos)->fd,       //socekt句柄
                                EPOLL_CTL_ADD,
                                EPOLLIN|EPOLLRDHUP,             //读，写【只关心读事件，所以参数2：readevent=1,而参数3：writeevent=0】
                                0,               //其他补充标记
                                pConn                //连接池中的连接 
                                ) == -1) 
        {
            exit(2); //有问题，直接退出，日志 已经写过了
        }
    }
    return 1;
}

//对epoll事件的具体操作
int CSocket::cc_epoll_oper_event( int                fd,               //句柄，一个socket
                        uint32_t           eventtype,        //事件类型，一般是EPOLL_CTL_ADD，EPOLL_CTL_MOD，EPOLL_CTL_DEL ，说白了就是操作epoll红黑树的节点(增加，修改，删除)
                        uint32_t           flag,             //标志，具体含义取决于eventtype
                        int                bcaction,         //补充动作，用于补充flag标记的不足  :  0：增加   1：去掉 2：完全覆盖 
                        lpcc_connection_t pConn     )        //pConn：一个指针【其实是一个连接】，EPOLL_CTL_ADD时增加到红黑树中去，将来epoll_wait时能取出来用)
{
    struct epoll_event ev;
    memset(&ev,0,sizeof(ev));

    if(eventtype == EPOLL_CTL_ADD)
    {
        //ev.data.ptr = (void*)pConn;
        ev.events = flag;
        pConn->events = flag;
    }else if(eventtype == EPOLL_CTL_MOD){
        //修改节点事件
        //节点已经在红黑树中，修改节点的事件信息
        ev.events = pConn->events;  //先把标记恢复回来
        if(bcaction == 0)
        {
            //增加某个标记            
            ev.events |= flag;
        }
        else if(bcaction == 1)
        {
            //去掉某个标记
            ev.events &= ~flag;
        }
        else
        {
            //完全覆盖某个标记            
            ev.events = flag;      //完全覆盖            
        }
        pConn->events = ev.events; //记录该标记
    }else{
        return 1;
    }
    ev.data.ptr = (void *)pConn;
    
    if(epoll_ctl(m_epollhandle,eventtype,fd,&ev) == -1){
        cc_log_stderr(errno,"CSocket::cc_epoll_oper_event()中epoll_ctl(%d,%ud,%ud,%d)失败.",fd,eventtype,flag,bcaction);
        return -1;
    }
    return 1;
}

//开始获取发生的事件消息
//参数unsigned int timer：epoll_wait()阻塞的时长，单位是毫秒；
//返回值，1：正常返回  ,0：有问题返回，一般不管是正常还是问题返回，都应该保持进程继续运行
//本函数被cc_process_events_and_timers()调用，而cc_process_events_and_timers()是在子进程的死循环中被反复调用
int CSocket::cc_epoll_process_events(int timer)
{

    int events = epoll_wait(m_epollhandle,m_events,CC_MAX_EVENTS,timer);

    if(events == -1){
        //有错误发生，发送某个信号给本进程就可以导致这个条件成立

        if(errno == EINTR)
        {
            cc_log_error_core(CC_LOG_INFO,errno,"CSocket::cc_epoll_process_events中epoll_wait()失败!");
            return 1;
        }else{
            cc_log_error_core(CC_LOG_ALERT,errno,"CSocket::cc_epoll_process_events中epoll_wait()失败!");
            return 0;   //非正常返回
        }
    }
    
    if(events == 0) //超时,无事件来
    {
        if(timer != -1){
            return 1;
        }
        cc_log_error_core(CC_LOG_ALERT,errno,"CSocket::cc_epoll_process_events中epoll_wait()没超时却没返回任何事件!");
        return 0;
    }
    //有事件收到
    lpcc_connection_t pConn;
  //  uintptr_t                     instance;
    uint32_t                     revents;
    for(int i =0;i<events;++i){         //遍历本次epoll_wait返回的所有事件，events才是返回的实际事件数量
        pConn = (lpcc_connection_t)(m_events[i].data.ptr);      
    
        //事件未过期，开始处理
        revents = m_events[i].events;


        if(revents&EPOLLIN){                                        //如果是读事件
            (this->*(pConn->rhandler))(pConn);                             //如果新连接进入，这里执行的应该是CSocekt::cc_event_accept(c)】
                                                                                            //如果是已经连入，发送数据到这里，则这里执行的应该是 CSocekt::cc_wait_request_handler   
        }
        if(revents&EPOLLOUT)                                      //如果是读事件
        {
            if(revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                --pConn->iThrowsendCount;
            }else{
                (this->*(pConn->whandler))(pConn);//如果有数据没有发送完毕，由系统驱动来发送，则这里执行的应该是 CSocket::cc_write_request_handler()
            }
        }
    }
    return 1;
}

//--------------------------------------------------------------------
//处理发送消息队列的线程
void* CSocket::ServerSendQueueThread(void* threadData)
{
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CSocket*pSocketobj = pThread->_pThis;
    int err;
    std::list<char*>::iterator pos,pos2,posend;
    
    char *pMsgbuf;
    LPSTRUC_MSG_HEADER pMsgHeader;
    LPCOMM_PKG_HEADER   pPkgHeader;
    lpcc_connection_t              p_Conn;
    unsigned short                      itmp;
    ssize_t                                        sendsize;

    CMemory *p_memory = CMemory::GetInstance();

    while(g_stopEvent == 0) //未退出
    {
        if(sem_wait(&pSocketobj->m_semEventSendQueue)==-1){
            if(errno != EINTR){
                cc_log_stderr(errno,"CSocket::ServerSendQueueThread()中sem_wait(&pSocketObj->m_semEventSendQueue)失败.");
            }
        }
        if(g_stopEvent != 0){
                break;
        }
        if(pSocketobj->m_iSendMsgQueueCount > 0){
                err = pthread_mutex_lock(&pSocketobj->m_sendMessageQueueMutex);
                if(err!=0) cc_log_stderr(err,"CSocket::ServerSendQueueThread()中pthread_mutex_lock()失败，返回的错误码为%d!",err);

                pos = pSocketobj->m_MsgSendQueue.begin();
                posend = pSocketobj->m_MsgSendQueue.end();

                while (pos != posend)
                {
                    pMsgbuf = (*pos);
                    pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgbuf;
                    pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgbuf+pSocketobj->m_iLenMsgHeader);
                    p_Conn = pMsgHeader->pConn;

                    ////包过期，因为如果 这个连接被回收，比如在cc_close_connection(),inRecyConnectQueue()中都会自增iCurrsequence
                     //而且这里有没必要针对 本连接 来用m_connectionMutex临界 ,只要下面条件成立，肯定是客户端连接已断，要发送的数据肯定不需要发送了
                    if(p_Conn->iCurrsequence != pMsgHeader->iCurrsequence)
                    {
                        pos2 = pos;
                        pos++;
                        pSocketobj->m_MsgSendQueue.erase(pos2);
                        --pSocketobj->m_iSendMsgQueueCount;
                        p_memory->FreeMemory(pMsgbuf);
                        continue;
                    }
                    if(p_Conn->iThrowsendCount > 0)
                    {
                        pos++;
                        continue;
                    }
                    --p_Conn->iSendCount;   //消息队列中的数量条数-1

                    //开始发送消息
                    p_Conn->psendMemPointer = pMsgbuf;
                    pos2 = pos;
                    pos++;
                    pSocketobj->m_MsgSendQueue.erase(pos2);
                    pSocketobj->m_iSendMsgQueueCount;
                    p_Conn->psendbuf = (char *)pPkgHeader;
                    itmp = ntohs(pPkgHeader->pkgLen);
                    p_Conn->isendlen = itmp;


                    //采用 epoll水平触发的策略，能走到这里的，都应该是还没有投递 写事件 到epoll中
                    sendsize = pSocketobj->sendproc(p_Conn,p_Conn->psendbuf,p_Conn->isendlen);
                    if(sendsize > 0){
                        if(sendsize == p_Conn->isendlen ){
                            p_memory->FreeMemory(p_Conn->psendMemPointer);
                            p_Conn->psendMemPointer = NULL;
                            p_Conn->iThrowsendCount = 0;    
                            //cc_log_stderr(0,"CSocket::ServerSendQueueThread()中数据发送完毕");
                        }else{//没有全部发送完毕
                            p_Conn->psendbuf = p_Conn->psendbuf + sendsize;
                            p_Conn->isendlen = p_Conn->isendlen-sendsize;
                            ++p_Conn->iThrowsendCount;
                            if(pSocketobj->cc_epoll_oper_event(p_Conn->fd,EPOLL_CTL_MOD,EPOLLOUT,0,p_Conn)==-1)
                            {
                                    cc_log_stderr(errno,"CSocket::ServerSendQueueThread()cc_epoll_oper_event()失败.");
                            }
                            cc_log_stderr(errno,"CSock3t::ServerSendQueueThread()中数据没发送完毕【发送缓冲区满】，整个要发送%d，实际发送了%d。",p_Conn->isendlen,sendsize);
                        }
                        continue;
                    }//end if(sendsize>0)
                    else if(sendsize == 0){
                        p_memory->FreeMemory(p_Conn->psendMemPointer);  //释放内存
                        p_Conn->psendMemPointer = NULL;
                        p_Conn->iThrowsendCount = 0;    
                        continue;
                    }
                    else if(sendsize == -1){
                    //发送缓冲区已经满了【一个字节都没发出去，说明发送 缓冲区当前正好是满的】
                        ++p_Conn->iThrowsendCount;
                        //标记发送缓冲区满了，需要通过epoll事件来驱动消息的继续发送
                        if(pSocketobj->cc_epoll_oper_event(
                                p_Conn->fd,         //socket句柄
                                EPOLL_CTL_MOD,      //事件类型，这里是增加【因为我们准备增加个写通知】
                                EPOLLOUT,           //标志，这里代表要增加的标志,EPOLLOUT：可写【可写的时候通知我】
                                0,                  //对于事件类型为增加的，EPOLL_CTL_MOD需要这个参数, 0：增加   1：去掉 2：完全覆盖
                                p_Conn              //连接池中的连接
                                ) == -1)
                        {
                            cc_log_stderr(errno,"CSocket::ServerSendQueueThread()中cc_epoll_add_event()_2失败.");
                        }
                        continue;
                    }
                    else{
                        //能走到这里的，应该就是返回值-2了，一般就认为对端断开了，等待recv()来做断开socket以及回收资源
                        p_memory->FreeMemory(p_Conn->psendMemPointer);  //释放内存
                        p_Conn->psendMemPointer = NULL;
                        p_Conn->iThrowsendCount = 0;  
                        continue;
                    }
                }//end while(pos != posend)
                 err = pthread_mutex_unlock(&pSocketobj->m_sendMessageQueueMutex); 
                if(err != 0)  cc_log_stderr(err,"CSocket::ServerSendQueueThread()pthread_mutex_unlock()失败，返回的错误码为%d!",err);
            }//if(pSocketObj->m_iSendMsgQueueCount > 0)
    }//end while
    return (void*)0;
}