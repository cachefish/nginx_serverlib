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

CSocket::CSocket():m_ListenPortCount(1)
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
    bool ret = cc_open_listening_sockets();
    return ret;
}

//监听端口，支持多个端口
bool CSocket::cc_open_listening_sockets()
{
    CConfig *p_config = CConfig::GetInstance();
    m_ListenPortCount = p_config->GetIntDefault("ListenPortCount",m_ListenPortCount);

    int isock;
    struct sockaddr_in serv_addr;
    int iport;
    char strinfo[100];

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    for(int i =0;i < m_ListenPortCount;++i){
        isock = socket(AF_INET,SOCK_STREAM,0);
        if(isock == -1){
            cc_log_stderr(errno,"CSocket::Initialize()中的socket()失败，i=%d ",i);
            return false;
        }

        //setsockopt
        int reuseaddr = 1;
        if(setsockopt(isock,SOL_SOCKET,SO_REUSEADDR,(const void*)&reuseaddr,sizeof(reuseaddr))==-1){
            cc_log_stderr(errno,"SCocket::Initialize()中的setsockopt()失败,i = %d ",i);
            close(isock);
            return false;
        }
        
        //设置本服务器要监听的地址和端口，
        strinfo[0] = 0;
        sprintf(strinfo,"ListenPortt%d",i);
        iport = p_config->GetIntDefault(strinfo,10000);
        serv_addr.sin_port = htons((in_port_t)iport);

        //绑定服务器地址结构体
        if(bind(isock,(struct sockaddr*)&serv_addr,sizeof(serv_addr))==-1)
        {
            cc_log_stderr(errno,"SCocket::Initialize()中的bind()失败,i = %d ",i);
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