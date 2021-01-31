#ifndef __C_SOCKET_H__
#define __C__SOCKET_H__

#include<vector>

#define CC_LISTEN_BACKLOG  511  //已完成连接队列

//监听端口号和套接字 结构体
typedef struct cc_listening_s
{
    int fd;
    int port;
}cc_listening_t,*lpcc_listening_t;

//socket相关类
class CSocket
{
    public:
        CSocket();
        virtual ~CSocket();
    
    public:
        virtual bool Initialize();
    
    private:
        bool cc_open_listening_sockets();
        void cc_close_listening_sockets();
        bool setnonblock(int sockfd);
    
    private:
        int                                                                             m_ListenPortCount;               //监听端口数
        std::vector<lpcc_listening_t>                       m_ListenSocketList; //监听套接字队列


};






#endif