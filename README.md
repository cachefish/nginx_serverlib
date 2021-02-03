## 基于事件驱动框架的服务
```
    由一些事件发生源【三次握手内核通知，事件发生源就是客户端】，通过事件收集器来收集和分发事件【调用函数处理】
	//【事件收集器：epoll_wait()函数】【cc_event_accept（），cc_wait_request_handler（）都属于事件处理器，用来消费事件】
```
# conf   配置文件
``` 
使用[Socket]
ListenPort = 5678    

DBInfo = 127.0.0.1

    CConfig *p_config = CConfig::GetInstance(); 
    p_config->Load("nginx.conf") == false //把配置文件内容载入到内存

    p_config->GetIntDefault("Daemon",0) //获取信息
```
#  setprotitle   设置程序后台标题
```
cc_init_setproctitle();

```
# log 日志文件
```
//向日志文件中输入
cc_log_error_core(CC_LOG_EMERG,errno,"sigaction(%s) filed",sig->signame);

```
# 守护进程
```
//设置守护进程
cc_daemon();
```
# 网络相关结构
```
//监听端口号和套接字 结构体
typedef struct cc_listening_s
{
    int                                        fd;
    int                                        port;
    lpcc_connection_t       connection;     //连接池中的一个连接
}cc_listening_t,*lpcc_listening_t;

struct cc_connection_s
{
    int                                  fd;                        //套接字句柄socket
    lpcc_listening_t       listening;          //如果这个连接被分配给了一个监听套接字，那么这个里边就指向监听套接字对应的那个lpcc_listening_t的内存首地址
	
    
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

        int cc_epoll_init();                                                 //epoll功能初始化

        int cc_epoll_add_event(int fd,int readevent,int wrireevent,uint32_t otherflag,uint32_t eventtype,lpcc_connection_t c);                    //epoll增加事件

        int cc_epoll_process_events(int timer);     //epoll等待接收和处理事件


    
        //业务处理函数handler
        void cc_event_accept(lpcc_connection_t oldc);                                          //建立新连接
        void cc_wait_request_handler(lpcc_connection_t c);                              //设置数据来时的读处理函数
        void cc_close_accepted_connection(lpcc_connection_t c);                  //用户连入，我们accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放
        //获取对端信息相关 
        size_t cc_sock_ntop(struct sockaddr *sa,int port,u_char *text,size_t len);

        //连接池  连接 相关
        lpcc_connection_t cc_get_connection(int isock);                                         //从连接池中获取一个空闲连接
        void cc_free_connection(lpcc_connection_t c);                                             //归还参数c所代表的连接到到连接池中

```



# 程序运行流程

    //(i)cc_master_process_cycle()        //创建子进程等一系列动作
	//(i)    cc_setproctitle()            //设置进程标题    
	//(i)    cc_start_worker_processes()  //创建worker子进程   
	//(i)        for (i = 0; i < threadnums; i++)   //master进程在走这个循环，来创建若干个子进程
	//(i)            cc_spawn_process(i,"worker process");
	//(i)                pid = fork(); //分叉，从原来的一个master进程（一个叉），分成两个叉（原有的master进程，以及一个新fork()出来的worker进程
	//(i)                //只有子进程这个分叉才会执行cc_worker_process_cycle()
	//(i)                cc_worker_process_cycle(inum,pprocname);  //子进程分叉
	//(i)                    cc_worker_process_init();
	//(i)                        sigemptyset(&set);  
	//(i)                        sigprocmask(SIG_SETMASK, &set, NULL); //允许接收所有信号
	//(i)                        g_socket.cc_epoll_init();  //初始化epoll相关内容，同时 往监听socket上增加监听事件，从而开始让监听端口履行其职责
	//(i)                            m_epollhandle = epoll_create(m_worker_connections); 
	//(i)                            cc_epoll_add_event((*pos)->fd....);
	//(i)                                epoll_ctl(m_epollhandle,eventtype,fd,&ev);
	//(i)                    cc_setproctitle(pprocname);          //重新为子进程设置标题为worker process
	//(i)                    for ( ;; ) 
    //(i)                    {
	//(i)                        //子进程开始在这里不断的死循环
	//(i)                        cc_process_events_and_timers(); //处理网络事件和定时器事件
	//(i)                            g_socket.cc_epoll_process_events(-1); //-1表示卡着等待吧
    //(i)                    }

	//(i)    sigemptyset(&set); 
	//(i)    for ( ;; ) {}.                //父进程[master进程]会一直在这里循环