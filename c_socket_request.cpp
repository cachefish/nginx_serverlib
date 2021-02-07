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
#include <pthread.h>   //多线程

#include "c_conf.h"
#include "c_macro.h"
#include "c_global.h"
#include "c_func.h"
#include "c_socket.h"
#include "c_memory.h"
#include "c_lockmutex.h"  //自动释放互斥量的一个类

//来数据时候的处理，当连接上有数据来的时候，本函数会被cc_epoll_process_events()所调用  ,官方的类似函数为cc_http_wait_request_handler();
void CSocket::cc_wait_request_handler(lpcc_connection_t c)
{  
    //收包，注意我们用的第二个和第三个参数，我们用的始终是这两个参数，因此我们必须保证 c->precvbuf指向正确的收包位置，保证c->irecvlen指向正确的收包宽度
    ssize_t reco = recvproc(c,c->precvbuf,c->irecvlen); 
    if(reco <= 0)  
    {
        return;//该处理的上边这个recvproc()函数处理过了，这里<=0是直接return        
    }

    //走到这里，说明成功收到了一些字节（>0），就要开始判断收到了多少数据了     
    if(c->curStat == _PKG_HD_INIT) //连接建立起来时肯定是这个状态，因为在cc_get_connection()中已经把curStat成员赋值成_PKG_HD_INIT了
    {        
        if(reco == m_iLenPkgHeader)//正好收到完整包头，这里拆解包头
        {   
            cc_wait_request_handler_proc_p1(c); //那就调用专门针对包头处理完整的函数去处理把。
        }
        else
		{
			//收到的包头不完整--我们不能预料每个包的长度，也不能预料各种拆包/粘包情况，所以收到不完整包头【也算是缺包】是很可能的；
            c->curStat        = _PKG_HD_RECVING;                 //接收包头中，包头不完整，继续接收包头中	
            c->precvbuf       = c->precvbuf + reco;              //注意收后续包的内存往后走
            c->irecvlen       = c->irecvlen - reco;              //要收的内容当然要减少，以确保只收到完整的包头先
        } //end  if(reco == m_iLenPkgHeader)
    } 
    else if(c->curStat == _PKG_HD_RECVING) //接收包头中，包头不完整，继续接收中，这个条件才会成立
    {
        if(c->irecvlen == reco) //要求收到的宽度和我实际收到的宽度相等
        {
            //包头收完整了
            cc_wait_request_handler_proc_p1(c); //那就调用专门针对包头处理完整的函数去处理把。
        }
        else
		{
			//包头还是没收完整，继续收包头
            //c->curStat        = _PKG_HD_RECVING;                 //没必要
            c->precvbuf       = c->precvbuf + reco;              //注意收后续包的内存往后走
            c->irecvlen       = c->irecvlen - reco;              //要收的内容当然要减少，以确保只收到完整的包头先
        }
    }
    else if(c->curStat == _PKG_BD_INIT) 
    {
        //包头刚好收完，准备接收包体
        if(reco == c->irecvlen)
        {
            //收到的宽度等于要收的宽度，包体也收完整了
            cc_wait_request_handler_proc_plast(c);
        }
        else
		{
			//收到的宽度小于要收的宽度
			c->curStat = _PKG_BD_RECVING;					
			c->precvbuf = c->precvbuf + reco;
			c->irecvlen = c->irecvlen - reco;
		}
    }
    else if(c->curStat == _PKG_BD_RECVING) 
    {
        //接收包体中，包体不完整，继续接收中
        if(c->irecvlen == reco)
        {
            //包体收完整了
            cc_wait_request_handler_proc_plast(c);
        }
        else
        {
            //包体没收完整，继续收
            c->precvbuf = c->precvbuf + reco;
			c->irecvlen = c->irecvlen - reco;
        }
    }  //end if(c->curStat == _PKG_HD_INIT)
    return;
}

//接收数据专用函数--引入这个函数是为了方便，如果断线，错误之类的，这里直接 释放连接池中连接，然后直接关闭socket，以免在其他函数中还要重复的干这些事
//参数c：连接池中相关连接
//参数buff：接收数据的缓冲区
//参数buflen：要接收的数据大小
//返回值：返回-1，则是有问题发生并且在这里把问题处理完毕了，调用本函数的调用者一般是可以直接return
//        返回>0，则是表示实际收到的字节数
ssize_t CSocket::recvproc(lpcc_connection_t c,char *buff,ssize_t buflen)  //ssize_t是有符号整型，在32位机器上等同与int，在64位机器上等同与long int，size_t就是无符号型的ssize_t
{
    ssize_t n;
    
    n = recv(c->fd, buff, buflen, 0); //recv()系统函数， 最后一个参数flag，一般为0；     
    if(n == 0)
    {
        //客户端关闭【应该是正常完成了4次挥手】，我这边就直接回收连接连接，关闭socket即可 
        cc_close_connection(c);
        return -1;
    }
    //客户端没断，走这里 
    if(n < 0) //这被认为有错误发生
    {
        //EAGAIN和EWOULDBLOCK[【这个应该常用在hp上】应该是一样的值，表示没收到数据，一般来讲，在ET模式下会出现这个错误，因为ET模式下是不停的recv肯定有一个时刻收到这个errno，但LT模式下一般是来事件才收，所以不该出现这个返回值
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            cc_log_stderr(errno,"CSocket::recvproc()中errno == EAGAIN || errno == EWOULDBLOCK成立");//epoll为LT模式不应该出现这个返回值，所以直接打印出来瞧瞧
            return -1; //不当做错误处理，只是简单返回
        }
        //EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。
        //例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        if(errno == EINTR)  //这个不算错误，是我参考官方nginx，官方nginx这个就不算错误；
        {
            //我认为LT模式不该出现这个errno，而且这个其实也不是错误，所以不当做错误处理
            cc_log_stderr(errno,"CSocket::recvproc()中errno == EINTR成立");//epoll为LT模式不应该出现这个返回值，所以直接打印出来瞧瞧
            return -1; //不当做错误处理，只是简单返回
        }

        if(errno == ECONNRESET)  //#define ECONNRESET 104 /* Connection reset by peer */
        {

        }
        else
        {
            cc_log_stderr(errno,"CSocket::recvproc()中发生错误，我打印出来看看是啥错误！");  //正式运营时可以考虑这些日志打印去掉
        } 
        
        //cc_log_stderr(0,"连接被客户端 非 正常关闭！");

        cc_close_connection(c);
        return -1;
    }

    //能走到这里的，就认为收到了有效数据
    return n; //返回收到的字节数
}


//包头收完整后的处理，我们称为包处理阶段1【p1】：写成函数，方便复用
void CSocket::cc_wait_request_handler_proc_p1(lpcc_connection_t c)
{
    CMemory *p_memory = CMemory::GetInstance();		

    LPCOMM_PKG_HEADER pPkgHeader;
    pPkgHeader = (LPCOMM_PKG_HEADER)c->dataHeadInfo; //正好收到包头时，包头信息肯定是在dataHeadInfo里；

    unsigned short e_pkgLen; 
    e_pkgLen = ntohs(pPkgHeader->pkgLen);  //注意这里网络序转本机序，所有传输到网络上的2字节数据，都要用htons()转成网络序，所有从网络上收到的2字节数据，都要用ntohs()转成本机序
                                                //ntohs/htons的目的就是保证不同操作系统数据之间收发的正确性，【不管客户端/服务器是什么操作系统，发送的数字是多少，收到的就是多少】
    //恶意包或者错误包的判断
    if(e_pkgLen < m_iLenPkgHeader) 
    {
        //伪造包/或者包错误
        //报文总长度 < 包头长度，认定非法用户，废包
        //状态和接收位置都复原，这些值都有必要，因为有可能在其他状态比如_PKG_HD_RECVING状态调用这个函数；
        c->curStat = _PKG_HD_INIT;      
        c->precvbuf = c->dataHeadInfo;
        c->irecvlen = m_iLenPkgHeader;
    }
    else if(e_pkgLen > (_PKG_MAX_LENGTH-1000))   //客户端发来包居然说包长度 > 29000?肯定是恶意包
    {
        //恶意包，太大，认定非法用户，废包【包头中说这个包总长度这么大，这不行】
        //状态和接收位置都复原，这些值都有必要，因为有可能在其他状态比如_PKG_HD_RECVING状态调用这个函数；
        c->curStat = _PKG_HD_INIT;
        c->precvbuf = c->dataHeadInfo;
        c->irecvlen = m_iLenPkgHeader;
    }
    else
    {
        //合法的包头，继续处理
        //我现在要分配内存开始收包体，因为包体长度并不是固定的，所以内存肯定要new出来；
        char *pTmpBuffer  = (char *)p_memory->AllocMemory(m_iLenMsgHeader + e_pkgLen,false); //分配内存【长度是 消息头长度  + 包头长度 + 包体长度】，最后参数先给false，表示内存不需要memset;
        c->ifnewrecvMem   = true;        //标记我们new了内存，将来在cc_free_connection()要回收的
        c->pnewMemPointer = pTmpBuffer;  //内存开始指针

        //a)先填写消息头内容
        LPSTRUC_MSG_HEADER ptmpMsgHeader = (LPSTRUC_MSG_HEADER)pTmpBuffer;
        ptmpMsgHeader->pConn = c;
        ptmpMsgHeader->iCurrsequence = c->iCurrsequence; //收到包时的连接池中连接序号记录到消息头里来，以备将来用；
        //b)再填写包头内容
        pTmpBuffer += m_iLenMsgHeader;                 //往后跳，跳过消息头，指向包头
        memcpy(pTmpBuffer,pPkgHeader,m_iLenPkgHeader); //直接把收到的包头拷贝进来
        if(e_pkgLen == m_iLenPkgHeader)
        {
            //该报文只有包头无包体【我们允许一个包只有包头，没有包体】
            //这相当于收完整了，则直接入消息队列待后续业务逻辑线程去处理吧
            cc_wait_request_handler_proc_plast(c);
        } 
        else
        {
            //开始收包体，注意我的写法
            c->curStat = _PKG_BD_INIT;                   //当前状态发生改变，包头刚好收完，准备接收包体	    
            c->precvbuf = pTmpBuffer + m_iLenPkgHeader;  //pTmpBuffer指向包头，这里 + m_iLenPkgHeader后指向包体 weizhi
            c->irecvlen = e_pkgLen - m_iLenPkgHeader;    //e_pkgLen是整个包【包头+包体】大小，-m_iLenPkgHeader【包头】  = 包体
        }                       
    }  //end if(e_pkgLen < m_iLenPkgHeader) 

    return;
}

//收到一个完整包后的处理【plast表示最后阶段】，放到一个函数中，方便调用
void CSocket::cc_wait_request_handler_proc_plast(lpcc_connection_t c)
{
    //把这段内存放到消息队列中来；
    int irmqc = 0;  //消息队列当前信息数量
    inMsgRecvQueue(c->pnewMemPointer,irmqc); //返回消息队列当前信息数量irmqc，是调用本函数后的消息队列中消息数量

    //激发线程池中的某个线程来处理业务逻辑
    g_threadpool.Call(irmqc);
    
    c->ifnewrecvMem    = false;            //内存不再需要释放，因为你收完整了包，这个包被上边调用inMsgRecvQueue()移入消息队列，那么释放内存就属于业务逻辑去干，不需要回收连接到连接池中干了
    c->pnewMemPointer  = NULL;
    c->curStat         = _PKG_HD_INIT;     //收包状态机的状态恢复为原始态，为收下一个包做准备                    
    c->precvbuf        = c->dataHeadInfo;  //设置好收包的位置
    c->irecvlen        = m_iLenPkgHeader;  //设置好要接收数据的大小
    return;
}

//---------------------------------------------------------------
//当收到一个完整包之后，将完整包入消息队列，这个包在服务器端应该是 消息头+包头+包体 格式
//参数：返回 接收消息队列当前信息数量irmqc，因为临界着，所以这个值也是OK的；
void CSocket::inMsgRecvQueue(char *buf,int &irmqc) //buf这段内存 ： 消息头 + 包头 + 包体
{
    CLock lock(&m_recvMessageQueueMutex);	 //自动加锁解锁很方便，不需要手工去解锁了
    m_MsgRecvQueue.push_back(buf);	         //入消息队列
    ++m_iRecvMsgQueueCount;                  //收消息队列数字+1，个人认为用变量更方便一点，比 m_MsgRecvQueue.size()高效
    irmqc = m_iRecvMsgQueueCount;            //接收消息队列当前信息数量保存到irmqc


    //收到了一个完整的数据包，打印一个信息
    //cc_log_stderr(0,"非常好，收到了一个完整的数据包【包头+包体】！");  
    
}

//从消息队列中把一个包提取出来以备后续处理
char *CSocket::outMsgRecvQueue() 
{
    CLock lock(&m_recvMessageQueueMutex);	//互斥
    if(m_MsgRecvQueue.empty())
    {
        return NULL; //也许会存在这种情形： 消息本该有，但被干掉了，这里可能为NULL的？        
    }
    char *sTmpMsgBuf = m_MsgRecvQueue.front(); //返回第一个元素但不检查元素存在与否
    m_MsgRecvQueue.pop_front();                //移除第一个元素但不返回	
    --m_iRecvMsgQueueCount;                    //收消息队列数字-1
    return sTmpMsgBuf;                         
}


//消息处理线程主函数，专门处理各种接收到的TCP消息
//pMsgBuf：发送过来的消息缓冲区，消息本身是自解释的，通过包头可以计算整个包长
//         消息本身格式【消息头+包头+包体】 
void CSocket::threadRecvProcFunc(char *pMsgBuf)
{

    return;
}
