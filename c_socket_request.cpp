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
#include<pthread.h>

#include "c_conf.h"
#include "c_macro.h"
#include "c_global.h"
#include "c_func.h"
#include "c_socket.h"
#include"c_memory.h"
#include"c_lockmutex.h"

//来数据时候的处理，当连接上有数据来的时候，本函数会被ngx_epoll_process_events()所调用
void CSocket::cc_wait_request_handler(lpcc_connection_t c)
{
        cc_log_stderr(errno,"2222222222222222");
        ssize_t reco = recvproc(c,c->precvbuf,c->irecvlen);
        if(reco <= 0){
                return;
        }

        if(c->curStat == _PKG_HD_INIT){
                if(reco == m_iLenPkgHeader){
                        cc_wait_request_handler_proc_p1(c);     //调用专门处理包头函数
                }else{
                        //收到的包头不完整，调整当前包头信息
                        c->curStat            =        _PKG_HD_RECVING;                //继续接收
                        c->precvbuf         =         c->precvbuf + reco;
                        c->irecvlen           =         c->irecvlen - reco;
                }
        }else if(c->curStat == _PKG_HD_RECVING){
                if(c->irecvlen == reco){                //收到的长度与实际长度相等
                        cc_wait_request_handler_proc_p1(c);     //调用专门处理包头函数
                }else{
                        c->precvbuf             == c->precvbuf + reco;
                        c->irecvlen               == c->irecvlen   - reco;
                }
        }else if(c->curStat == _PKG_BD_INIT){           //包头接收完成，开始接收包体
                if(reco == c->irecvlen){
                        cc_wait_request_handler_proc_plast(c);
                }else{
                        c->curStat    =      _PKG_BD_RECVING;
                        c->precvbuf =      c->precvbuf + reco;
                        c->irecvlen   =      c->irecvlen - reco;
                }
        }else if(c->curStat == _PKG_BD_RECVING){
                //继续接收包体
                if(c->irecvlen == reco){
                        cc_wait_request_handler_proc_plast(c);
                }else{
                        c->precvbuf =      c->precvbuf + reco;
                        c->irecvlen   =      c->irecvlen - reco;
                }
        }
        return;
}

//参数c：连接池中相关连接
//参数buf：接收数据的缓冲区
//参数buflen：要接收的数据大小
//返回值：返回-1，则是有问题发生并且在这里把问题处理完毕了，调用本函数的调用者一般是可以直接return
//        返回>0，则是表示实际收到的字节数
ssize_t CSocket::recvproc(lpcc_connection_t c,char *buf,ssize_t buflen)      //接收从客户端来的数据
{
        ssize_t n;
        n = recv(c->fd,buf,buflen,0);
        if(n == 0){
                cc_log_stderr(0,"连接被客户端正常关闭[4路挥手关闭]！");
                cc_close_connection(c);
                return -1;
        }

        if(n < 0){      //有错误
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                        cc_log_stderr(errno,"CSocket::recvproc()中errno == EAGAIN || errno == EWOULDBLOCK成立");
                        return -1;
                }

                if(errno == EINTR){
                        cc_log_stderr(errno,"CSocket::recvproc()中errno == EINTR成立");
                        return -1;
                }
                if(errno == ECONNRESET)//在服务器崩溃后重启的时候，因为之前的连接都无效了，所以服务器端会发送一个RST响应，此时客户端产生ECONNRESET错误
                {

                }else{
                        cc_log_stderr(errno,"CSocekt::recvproc()中发生错误，错误码为");
                }

                cc_close_connection(c);
                return -1;
       }
        return n;
}

//处理包头
void CSocket::cc_wait_request_handler_proc_p1(lpcc_connection_t c)          //包头收完整后的处理
{
        CMemory *p_memory = CMemory::GetInstance();
        LPCOMM_PKG_HEADER pPkgHeader;   //保存信息
        pPkgHeader = (LPCOMM_PKG_HEADER)c->dataHeadInfo;      
       
        unsigned short e_pkgLen;
        e_pkgLen = ntohs(pPkgHeader->pkgLen);

        //判断e_gkglen
        if(e_pkgLen < m_iLenPkgHeader){
                //整个包长是包头+包体，就算包体为0字节，那么至少e_pkgLen == m_iLenPkgHeader
                c->curStat      = _PKG_HD_INIT;
                c->precvbuf   = c->dataHeadInfo;
                c->irecvlen     = m_iLenPkgHeader;
        }
        else if(e_pkgLen > (_PKG_MAX_LENGTH-100)){
                
                c->curStat      = _PKG_HD_INIT;
                c->precvbuf   = c->dataHeadInfo;
                c->irecvlen     = m_iLenPkgHeader;
        }else{
                //合法包
                //分配内存收包
                char *pTmpBuffer = (char *)p_memory->AllocMemory(m_iLenPkgHeader + e_pkgLen,false);
                c->ifnewrecvMem = true; //标记new出的内存，方便到时释放
                c->pnewMemPointer = pTmpBuffer; //指向内存

                //填写消息头
                LPSTRUC_MSG_HEADER ptmpMsgHeader = (LPSTRUC_MSG_HEADER)pTmpBuffer;
                ptmpMsgHeader->pConn = c;
                ptmpMsgHeader->iCurrsequence = c->iCurrsequence;//收到包时的连接池中连接序号记录到消息头里来，以备将来用；
                //填写消息头
                pTmpBuffer += m_iLenMsgHeader;
                memcpy(pTmpBuffer,pPkgHeader,m_iLenPkgHeader);
                if(e_pkgLen == m_iLenPkgHeader){
                         //该报文只有包头无包体【我们允许一个包只有包头，没有包体】
                        //入消息队列待后续业务逻辑线程去处理
                        cc_wait_request_handler_proc_plast(c);
                }else{
                         c->curStat = _PKG_BD_INIT;                   //当前状态发生改变，包头刚好收完，准备接收包体	    
                         c->precvbuf = pTmpBuffer + m_iLenPkgHeader;  //pTmpBuffer指向包头，这里 + m_iLenPkgHeader后指向包体位置
                         c->irecvlen = e_pkgLen - m_iLenPkgHeader;    //e_pkgLen是整个包【包头+包体】大小，-m_iLenPkgHeader【包头】  = 包体
                }
        }       
        return;
}

void CSocket::cc_wait_request_handler_proc_plast(lpcc_connection_t c)     //收到一个完整包后的处理
{
        int irmqc = 0;
        inMegRecvQueue(c->pnewMemPointer,irmqc);

        //选取线程池中的某个线程来处理业务逻辑
        g_threadpool.Call(irmqc);

        c->ifnewrecvMem = false;
        c->pnewMemPointer = NULL;
        c->curStat = _PKG_HD_INIT;              //收包状态机的状态恢复为原始态，为收下一个包做准备
        c->precvbuf = c->dataHeadInfo;       //设置好收包的位置
        c->irecvlen = m_iLenPkgHeader;     //设置好要接收数据的大小
        return;
}


void CSocket::inMegRecvQueue(char *buf, int &irmqc)                                                                    //收到一个完整消息后，入消息队列
{
        CLock lock(&m_recvMessageQueueMutex);
        m_MsgRecvQueue.push_back(buf);
        ++m_iRecvMsgQueueCount;
        irmqc = m_iRecvMsgQueueCount;

        //tmpoutMsgRecvQueue();

        cc_log_stderr(0,"收到了一个完整的数据包[包头+包体]");
}
// void CSocket::tmpoutMsgRecvQueue()                                     //临时清除队列中消息函数
// {      
//         if(m_MsgRecvQueue.empty())
//         {
//                 return;
//         }
//         int size = m_MsgRecvQueue.size();
//         if(size < 1000){
//                 return;
//         }
//         //消息数过多
//         CMemory *p_memory = CMemory::GetInstance();
//         int cha = size - 500;
//         for(int i =0;i<cha;++i){
//                 char *sTmpMsgBuf = m_MsgRecvQueue.front();//返回第一个元素但不检查元素存在与否
//                 m_MsgRecvQueue.pop_front();               //移除第一个元素但不返回	
//                 p_memory->FreeMemory(sTmpMsgBuf);         //先释放掉把；
//         }
//         return;
// }

char *CSocket::outMsgRecvQueue()
{
        CLock lock(&m_recvMessageQueueMutex);
        if(m_MsgRecvQueue.empty())
        {
                return NULL;
        }

        char *sTmpMsgBuf = m_MsgRecvQueue.front();
        m_MsgRecvQueue.pop_front();
        --m_iRecvMsgQueueCount;
        return sTmpMsgBuf;
}

//消息处理线程主函数，专门处理各种接收到的TCP消息
//pMsgBuf：发送过来的消息缓冲区，消息本身是自解释的，通过包头可以计算整个包长
//         消息本身格式【消息头+包头+包体】 
void CSocket::threadRecvProcFunc(char *pMsgBuf)
{

    return;
}
