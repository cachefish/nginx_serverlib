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

#include "c_conf.h"
#include "c_macro.h"
#include "c_global.h"
#include "c_func.h"
#include "c_socket.h"


//建立新连接专用函数，当新连接进入时，本函数会被cc_epoll_process_events()所调用
void CSocket::cc_event_accept(lpcc_connection_t oldc)
{
    struct  sockaddr            mysockaddr;         //远端服务器的socket地址
    socklen_t                         socklen;
    int                                        err;
    int                                        level;
    int                                       s;          
    static int                           use_accept4;         //能够使用accept4()函数
    lpcc_connection_t      newc;                        //代表连接池中的一个连接


    socklen = sizeof(mysockaddr);
    do{
            if(use_accept4){
                s = accept4(oldc->fd,&mysockaddr,&socklen,SOCK_NONBLOCK);       //从内核获取一个用户端连接
            }else{
                s = accept(oldc->fd,&mysockaddr,&socklen);
            }
            if(s == -1){
                err = errno;

                //对accept、send、recv，事件未发生时errno通常被设置为EAGAIN\EWOULDBLOCK
                if(err == EAGAIN){
                        return;
                }

                level = CC_LOG_ALERT;
                if(err == ECONNABORTED)//ECONNRESET错误则发生在对方意外关闭套接字后【您的主机中的软件放弃了一个已建立的连接--由于超时或者其它失败而中止接连(用户插拔网线就可能有这个错误出现)】
                {
                    level = CC_LOG_CRIT;
                }
                cc_log_error_core(level,errno,"CSocket::cc_event_accept()中accept4()失败!");

                if(use_accept4 && err == ENOSYS)
                {
                    use_accept4 = 0;
                    continue;
                }

                if(err == ECONNABORTED){    //对端关闭套接字
                        //忽略
                }
                if(err == EMFILE || err == ENFILE)
                {

                }
                return;
            }//end if(s==-1)

            //accept4成功
            newc = cc_get_connection(s);
            if(newc == NULL){
                if(close(s) == -1){
                    cc_log_error_core(CC_LOG_ALERT,errno,"CSocket::cc_epoll_accept()中close(%d)失败!",s);
                }
                return;
            }
             //成功的拿到了连接池中的一个连接
            memcpy(&newc->s_sockaddr,&mysockaddr,socklen);  //拷贝客户端地址到连接对象
            
            if(!use_accept4){
                if(setnonblock(s) == false){
                    cc_close_connection(newc);
                    return;
                }
            }

            newc->listening = oldc->listening;                                                      //连接对象 和监听对象关联，方便通过连接对象找监听对象【关联到监听端口】
            newc->w_ready = 1;                                                                                   //标记可以写，新连接写事件肯定是ready的
            newc->rhandler = &CSocket::cc_wait_request_handler;         //设置数据来时的读处理函数


            if(cc_epoll_add_event(s,
                                                            1,0,
                                                            0,
                                                            EPOLL_CTL_ADD,
                                                            newc) == -1)
            {
                cc_close_connection(newc);
                return;
            }
            break;
    }while(1);
    return;
}

