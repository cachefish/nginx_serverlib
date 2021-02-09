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
//m_pconnections(NULL),
//m_pfree_connections(NULL),
//m_pread_events(NULL),
//m_pwrite_events(NULL)
m_iLenPkgHeader(sizeof(COMM_PKG_HEADER)),
m_iLenMsgHeader(sizeof(STRUC_MSG_HEADER)),
m_iSendMsgQueueCount(0),
m_total_recyconnection_n(0)
{   
    
}


CSocket::~CSocket()
{
    std::vector<lpcc_listening_t>::iterator pos;

    for(pos = m_ListenSocketList.begin();pos!=m_ListenSocketList.end();++pos){
        delete (*pos);
    }
    m_ListenSocketList.clear();

    //连接池相关释放
    // if(m_pwrite_events != NULL){
    //     delete [] m_pwrite_events;
    // }
    // if(m_pread_events != NULL){
    //     delete [] m_pread_events;
    // }
}

// void CSocket::clearMsgRecvQueue()                                           //清理接收消息队列
// {
//     char *sTmpMempoint;
//     CMemory *p_memory = CMemory::GetInstance();

//     while (!m_MsgRecvQueue.empty())
//     {
//         sTmpMempoint = m_MsgRecvQueue.front();
//         m_MsgRecvQueue.pop_front();
//         p_memory->FreeMemory(sTmpMempoint);
//     }
    
// }

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
    //信号量初始化
    if(sem_init(&m_semEventSendQueue,0,0)==-1)
    {
        cc_log_stderr(0,"CSocket::initial_subproc()中的sem_init()函数失败");
        return false;
    }
    //开始创建线程
    int err;
    ThreadItem *pRecyconn= new ThreadItem(this);
    m_threadVector.push_back(pRecyconn );
    err = pthread_create(&pRecyconn->_Handle,NULL,ServerRecyConnectionThread,pRecyconn);
    if(err != 0)
    {
        return false;
    }
    return true;

}
//关闭退出函数[子进程中执行]
void CSocket::Shutdown_subproc()
{
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

    //多线程相关
    pthread_mutex_destroy(&m_connectionMutex);
    pthread_mutex_destroy(&m_sendMessageQueueMutex);
    pthread_mutex_destroy(&m_recyconnqueueMutex);
    sem_destroy(&m_semEventSendQueue);
}

//清理TCP发送消息队列
void CSocket::clearMsgSendQueue()
{
    char * sTmpMempoint;
    CMemory *p_memory = CMemory::GetInstance();
    while(!m_MsgRecvQueue.empty()){
        sTmpMempoint = m_MsgRecvQueue.front();
        m_MsgRecvQueue.pop_front();
        p_memory->FreeMemory(sTmpMempoint);
    }
}


void CSocket::ReadConf()
{
    CConfig *p_config = CConfig::GetInstance();
    m_worker_connections = p_config->GetIntDefault("worker_connections",m_worker_connections);
    m_ListenPortCount          = p_config->GetIntDefault("ListenPortCount",m_ListenPortCount);
    m_RecyConnectionWaitTime = p_config->GetIntDefault("Sock_RecyConnectionWaitTime",m_RecyConnectionWaitTime);
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
    CLock lock(&m_sendMessageQueueMutex);
    m_MsgRecvQueue.push_back(psendbuf);
    ++m_iSendMsgQueueCount;

    if(sem_post(&m_semEventSendQueue)==-1)
    {
        cc_log_stderr(0,"CSocket::msgSend()sem_post()函数失败");
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

    //m_connection_n = m_worker_connections;      //记录当前连接池中连接总数
    
    //m_pconnections = new cc_connection_t[m_connection_n];   

    // m_pread_events = new cc_event_t[m_connection_n];
    // m_pwrite_events = new cc_event_t[m_connection_n];
    // for(int i =0;i < m_connection_n; i++){
    //     m_pconnections[i].instance = 1;     //失效标志位设置为1
    // }
/*
    int i = m_connection_n;
    lpcc_connection_t next = NULL;
    lpcc_connection_t c = m_pconnections;

    do{
        i--;
        c[i].next = next;                    //设置连接对象的next指针
        c[i].fd = -1;                              //初始化连接，无socket和该连接池中的连接【对象】绑定
        c[i].instance = 1;                  //失效标志位设置为1【失效】
        c[i].iCurrsequence = 0;     //当前序号统一从0开始

        next = &c[i];                           //next指针前移
    }while(i);

    m_pfree_connections = next; //设置空闲连接链表头指针,因为现在next指向c[0]，现在整个链表都是空的
    m_free_connection_n = m_connection_n;       //空闲连接链表长度，因为现在整个链表都是空的，这两个长度相等；
*/
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

/*
//epoll增加事件，可能被cc_epoll_init()等函数调用
//fd:句柄，一个socket
//readevent：表示是否是个读事件，0是，1不是
//writeevent：表示是否是个写事件，0是，1不是
//otherflag：其他需要额外补充的标记，弄到这里
//eventtype：事件类型  ，一般就是用系统的枚举值，增加，删除，修改等;
//c：对应的连接池中的连接的指针
//返回值：成功返回1，失败返回-1；

int CSocket::cc_epoll_add_event(int fd,
                                int readevent,int writeevent,
                                uint32_t otherflag, 
                                uint32_t eventtype, 
                                lpcc_connection_t c
                                )
{
        struct epoll_event ev;
        
        memset(&ev,0,sizeof(ev));

        if(readevent == 1){
            ev.events = EPOLLIN|EPOLLRDHUP;
        }else{
            //其他事件类型

        }

        if(otherflag != 0){
            ev.events = otherflag;
        }

        ev.data.ptr = (void*)((uintptr_t)c | c->instance);//把对象弄进去，后续来事件时，用epoll_wait()后，这个对象能取出来用
        if(epoll_ctl(m_epollhandle,eventtype,fd,&ev) == -1){
            cc_log_stderr(errno,"CSocekt::cc_epoll_add_event()中epoll_ctl(%d,%d,%d,%ud,%ud)失败.",fd,readevent,writeevent,otherflag,eventtype);
            return -1;
        }
        return 1;
}
*/

//对epoll事件的具体操作
int CSocket::cc_epoll_oper_event( int                fd,               //句柄，一个socket
                        uint32_t           eventtype,        //事件类型，一般是EPOLL_CTL_ADD，EPOLL_CTL_MOD，EPOLL_CTL_DEL ，说白了就是操作epoll红黑树的节点(增加，修改，删除)
                        uint32_t           flag,             //标志，具体含义取决于eventtype
                        int                bcaction,         //补充动作，用于补充flag标记的不足  :  0：增加   1：去掉
                        lpcc_connection_t pConn     )        //pConn：一个指针【其实是一个连接】，EPOLL_CTL_ADD时增加到红黑树中去，将来epoll_wait时能取出来用)
{
    struct epoll_event ev;
    memset(&ev,0,sizeof(ev));

    if(eventtype == EPOLL_CTL_ADD)
    {
        ev.data.ptr = (void*)pConn;
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
    lpcc_connection_t c;
  //  uintptr_t                     instance;
    uint32_t                     revents;
    for(int i =0;i<events;++i){         //遍历本次epoll_wait返回的所有事件，events才是返回的实际事件数量
        c = (lpcc_connection_t)(m_events[i].data.ptr);      //将地址的最后一位取出来，用instance变量标识, 见ngx_epoll_add_event，该值是当时随着连接池中的连接一起给进来的
       /* instance = (uintptr_t)c&1;
        c = (lpcc_connection_t)((uintptr_t)c&(uintptr_t)~1);//最后1位干掉，得到真正的c地址

        if(c->fd == -1){
            cc_log_error_core(CC_LOG_DEBUG,0,"CSocekt::cc_epoll_process_events()中遇到了fd=-1的过期事件:%p.",c); 
            continue; //这种事件就不处理即可
        }
        if(c->instance != instance){
            cc_log_error_core(CC_LOG_DEBUG,0,"CSocekt::cc_epoll_process_events()中遇到了instance值改变的过期事件:%p.",c); 
            continue; //这种事件就不处理即可
        }
        */
        //事件未过期，开始处理
        revents = m_events[i].events;

        // if(revents&(EPOLLERR|EPOLLHUP)){        //如果发生了错误或者客户端断连
        //     revents |= EPOLLIN|EPOLLOUT;
        // }


        if(revents&EPOLLIN){                                        //如果是读事件
            (this->*(c->rhandler))(c);                             //如果新连接进入，这里执行的应该是CSocekt::cc_event_accept(c)】
                                                                                            //如果是已经连入，发送数据到这里，则这里执行的应该是 CSocekt::cc_wait_request_handler   
        }
        if(revents&EPOLLOUT)                                      //如果是读事件
        {
            //....待扩展
        }
    }
    return 1;
}