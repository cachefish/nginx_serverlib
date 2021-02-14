# 客户端给服务器发送数据后，会出现malloc():memory corruption
```
    导致发送第一次数据后，work进程会终止
    原因：发生了内存异常使用，malloc在申请内存的时候，发现内存冲突，接收到SIGABRT信号退出。
    
    使用gdb、valgrind工具调试 已修改



    free invalid pointer的问题
    在c_slogic.cpp中
  	101 	pPkgBody = (void *)(pMsgBuf+m_iLenMsgHeader+m_iLenPkgHeader); //跳过消息头 以及 包头 ，指向包体

    开始少加了m_iLenPkgHeader，导致指针指向有问题
    
    c_socket.cpp              ev.data.ptr = (void *)pConn在EPOLL_CTL_ADD中添加，但第二次客户端给服务器发消息不知道怎么回事，会出现ptr失效，故在最后再将指针指向
    

   出现 double free or corruption (out)内存问题　　　在向客户端发送消息时出现内存问题
c_socket.cpp
  
        void* CSocket::ServerSendQueueThread(void* threadData)
           580 指针指向错误，可能导致内存泄露         pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgbuf+pSocketobj->m_iLenMsgHeader);
                    
           605未将发送消息队列中的一个节点删除 ,导致内存问题        pSocketobj->m_MsgSendQueue.erase(pos2);


c_socket_request.cpp
       void CSocket::cc_write_request_handler(lpcc_connection_t pConn)

     336发送完毕才调用cc_epoll_oper_event      if(sendsize > 0 && sendsize == pConn->isendlen) //成功发送完毕
           


```