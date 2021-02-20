#ifndef __C_MACRO_H__
#define __C_MACRO_H__
//用来存放宏定义
#define CC_MAX_ERROR_STR 2048   //显示的错误信息最大数组长度
//功能函数
#define cc_cpymem(dst,src,n) (((u_char *)memcpy(dst,src,n))+(n))
#define cc_min(val1,val2) ((val1>val2)?(val2):(val1))
//数字相关
#define CC_MAX_UINT32_VALUE (uint32_t) 0xffffffff
#define CC_INT64_LEN  (sizeof("-9223372036854775808")-1)
//日志相关--------------------
//日志一共分成八个等级【级别从高到低，数字最小的级别最高，数字大的级别最低】，以方便管理、显示、过滤等等
#define CC_LOG_STDERR            0    //控制台错误【stderr】：最高级别日志，日志的内容不再写入log参数指定的文件，而是会直接将日志输出到标准错误设备比如控制台屏幕
#define CC_LOG_EMERG             1    //紧急 【emerg】
#define CC_LOG_ALERT             2    //警戒 【alert】
#define CC_LOG_CRIT              3    //严重 【crit】
#define CC_LOG_ERR               4    //错误 【error】：属于常用级别
#define CC_LOG_WARN              5    //警告 【warn】：属于常用级别
#define CC_LOG_NOTICE            6    //注意 【notice】
#define CC_LOG_INFO              7    //信息 【info】
#define CC_LOG_DEBUG             8    //调试 【debug】：最低级别

#define CC_ERROR_LOG_PATH       "error.log"   //定义日志存放的路径和文件名 


//进程相关
//标记当前进程类型
#define CC_PROCESS_MASTER 0    //管理进程
#define CC_PROCESS_WORKER 1     //工作进程



#endif