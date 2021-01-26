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

cc_log_error_core(CC_LOG_EMERG,errno,"sigaction(%s) filed",sig->signame);

```
# 守护进程
```
cc_daemon();

```