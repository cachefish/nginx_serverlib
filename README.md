## 基于事件驱动框架的服务
``
    由一些事件发生源【三次握手内核通知，事件发生源就是客户端】，通过事件收集器来收集和分发事件【调用函数处理】
	//【事件收集器：epoll_wait()函数】【cc_event_accept（），cc_wait_request_handler（）都属于事件处理器，用来消费事件】
``
# conf   配置文件
[配置文件详解](https://github.com/cachefish/nginx_serverlib/blob/master/analysis/%E9%85%8D%E7%BD%AE%E6%96%87%E4%BB%B6.md "配置文件"),用来通过接口查找配置信息
``` 

使用[Socket]
ListenPort = 5678    

DBInfo = 127.0.0.1

    CConfig *p_config = CConfig::GetInstance(); 
    p_config->Load("nginx.conf") == false //把配置文件内容载入到内存

    p_config->GetIntDefault("Daemon",0) //获取信息
```
#  setprotitle   设置程序后台标题
[设置可执行程序标题](https://github.com/cachefish/nginx_serverlib/blob/master/analysis/%E8%AE%BE%E7%BD%AE%E5%8F%AF%E6%89%A7%E8%A1%8C%E7%A8%8B%E5%BA%8F%E6%A0%87%E9%A2%98.md "设置程序标题"),用来设置可执行程序标题
```
cc_init_setproctitle();

```
# log 日志文件
[日志文件](https://github.com/cachefish/nginx_serverlib/blob/master/analysis/%E6%97%A5%E5%BF%97%E6%96%87%E4%BB%B6.md "日志输出"), 用于数据输出、日志输出的函数
```
//向日志文件中输入
cc_log_error_core(CC_LOG_EMERG,errno,"sigaction(%s) filed",sig->signame);

```
# 守护进程
[守护进程](https://github.com/cachefish/nginx_serverlib/blob/master/analysis/%E5%AE%88%E6%8A%A4%E8%BF%9B%E7%A8%8B.md "守护进程"),用来设置守护进程
```
//设置守护进程
cc_daemon();
```
# 网络相关结构
```
待改进

```



# 程序运行流程
```
		//描述：创建worker子进程
void cc_master_process_cycle()
	清空信号集
		将一些信号通过sigaddset设置进入信号集，
			通过sigprocmask(SIG_BLOCK,&set,NULL)设置阻塞信号
	开始设置该进程标题(主进程master)
		 cc_setproctitle(title); //设置标题
	从配置文件中读取要创建的worker进程数量
		int workprocess = p_config->GetIntDefault("WorkerProcesses",1); //worker进程数量
	           cc_start_worker_processes(workprocess);  //这里要创建worker子进程
			        cc_spawn_process(i,"worker process");//每个子进程执行该函数,(在该函数中才会执行fork函数，子进程去执行专门的函数，父进程直接break掉)
				      cc_worker_process_cycle(inum,pprocname);    //所有worker子进程，在这个函数里不断循环着不出来
					           cc_worker_process_init(inum);//子进程创建初始化工作
								1、清空信号集
								2、初始化线程池
								3、socket初始化
								4、初始化epoll相关内容
						    cc_setproctitle(pprocname); //设置子进程标题 、
								for(;;)
									cc_process_events_and_timers();子进程都执行该函数  

								 //从for这个循环跳出来在这里停止线程池；
								g_threadpool.StopAll(); 
								g_socket.Shutdown_subproc();
								
	
	//创建子进程后，父进程的执行流程会返回到这里，子进程不会走进来    
	sigemptyset(&set); //信号屏蔽字为空，表示不屏蔽任何信号
	for(;;){
	//master主进程量持续挂起，
	 sigsuspend(&set); //阻塞在这里，等待一个信号，此时进程是挂起的，不占用cpu时间，只有收到信号才会被唤醒（返回）；
	}


	```