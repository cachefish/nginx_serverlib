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
    



```