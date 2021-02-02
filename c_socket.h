#ifndef __C_SOCKET_H__
#define __C__SOCKET_H__

#include<vector>
#include<sys/epoll.h>
#include<sys/socket.h>

//宏定义
#define CC_LISTEN_BACKLOG  511  //已完成连接队列
#define CC_MAX_EVENTS            512  //epoll_wait一次最多接受这么多连接

typedef struct cc_listening_s     cc_listening_t, *lpcc_listening_t;
typedef struct cc_connection_s   cc_connection_t, *lpcc_connection_t;
typedef class CSocket CSocket;

typedef void (CSocket::*cc_event_handler_pt)(lpcc_connection_t c);//定义成员函数指针


//监听端口号和套接字 结构体
typedef struct cc_listening_s
{
    int                                        fd;
    int                                        port;
    lpcc_connection_t       connection;     //连接池中的一个连接
}cc_listening_t,*lpcc_listening_t;

//一个TCP连接  客户端主动发起

struct cc_connection_s
{
    int                                  fd;                        //套接字句柄socket
    lpcc_listening_t       listening;          //如果这个连接被分配给了一个监听套接字，那么这个里边就指向监听套接字对应的那个lpngx_listening_t的内存首地址
	
    
    //--------------------------------------------------  
    unsigned                    instance:1;        //失效标志位：0：有效，1：失效
    uint64_t                      iCurrsequence;   
    struct sockaddr       s_sockaddr;      //保存对端地址信息     
    //char                      addr_text[100]; //地址的文本信息

    uint8_t                   w_ready;        //写准备好标记

	cc_event_handler_pt      rhandler;       //读事件的相关处理方法
	cc_event_handler_pt      whandler;       //写事件的相关处理方法
	
	//--------------------------------------------------
	lpcc_connection_t        next;           //这是个指针【等价于传统链表里的next成员：后继指针】，指向下一个本类型对象，用于把空闲的连接池对象串起来构成一个单向链表，

};


//每个TCP连接至少需要一个读事件和写事件，事件结构
typedef struct cc_event_s
{
    
}cc_event_t,*lpcc_event_t;



//socket相关类
class CSocket
{
    public:
        CSocket();
        virtual ~CSocket();
    
    public:
        virtual bool Initialize();                                        //初始化

    public:
        int cc_epoll_init();                                                 //epoll功能初始化
        int cc_epoll_add_event(int fd,int readevent,int wrireevent,uint32_t otherflag,uint32_t eventtype,lpcc_connection_t c);//epoll增加事件
        int cc_epoll_process_events(int timer);     //epoll等待接收和处理事件
    
    private:
        void ReadConf();                                                                                                        //读网络配置项

        bool cc_open_listening_sockets();                                                                    //监听必须的端口【支持多个端口】
        void cc_close_listening_sockets();                                                                    //关闭监听套接字
        bool setnonblock(int sockfd);                                                                              //设置非阻塞
    
        //业务处理函数handler
        void cc_event_accept(lpcc_connection_t oldc);                                          //建立新连接
        void cc_wait_request_handler(lpcc_connection_t c);                              //设置数据来时的读处理函数
        void cc_close_accepted_connection(lpcc_connection_t c);                  //用户连入，我们accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放
        //获取对端信息相关 
        size_t cc_sock_ntop(struct sockaddr *sa,int port,u_char *text,size_t len);

        //连接池  连接 相关
        lpcc_connection_t cc_get_connection(int isock);                                         //从连接池中获取一个空闲连接
        void cc_free_connection(lpcc_connection_t c);                                             //归还参数c所代表的连接到到连接池中

    private:
        int                                                                             m_worker_connections;        //epoll连接的最大项数
        int                                                                             m_epollhandle;                            //epoll_create返回的句柄
        int                                                                             m_ListenPortCount;                   //监听端口数
 
        //连接池相关
        lpcc_connection_t                                             m_pconnections;                       //连接池的首地址
        lpcc_connection_t                                             m_pfree_connections;            //空闲连接链表头，连接池中总是有某些连接被占用，为了快速在池中找到一个空闲的连接，把空闲的连接专门用该成员记录;

        lpcc_event_t                                                         m_pread_events;                        //读事件数组
        lpcc_event_t                                                         m_pwrite_events;                       //写事件数组
        int                                                                               m_connection_n;                       //当前进程中所有连接对象的总数【连接池大小】
        int                                                                               m_free_connection_n;             //连接池中可用连接总数

        std::vector<lpcc_listening_t>                       m_ListenSocketList; //监听套接字队列
        struct epoll_event                                               m_events[CC_MAX_EVENTS];  //用于在epoll_wait()中承载返回的所发生的事件

};






#endif