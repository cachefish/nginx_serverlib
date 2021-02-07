# 客户端给服务器发送数据后，会出现malloc():memory corruption
```
    导致发送第一次数据后，work进程会终止
    原因：发生了内存异常使用，malloc在申请内存的时候，发现内存冲突，接收到SIGABRT信号退出。
    
    使用gdb、valgrind工具调试 已修改

```