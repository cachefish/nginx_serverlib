//和日志相关的函数之类
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

#include "c_global.h"
#include "c_macro.h"
#include "c_func.h"
#include "c_conf.h"

//错误等级，和cc_macro.h里定义的日志等级宏是一一对应关系
static u_char err_levels[][20]  = 
{
    {"stderr"},    //0：控制台错误
    {"emerg"},     //1：紧急
    {"alert"},     //2：警戒
    {"crit"},      //3：严重
    {"error"},     //4：错误
    {"warn"},      //5：警告
    {"notice"},    //6：注意
    {"info"},      //7：信息
    {"debug"}      //8：调试
};
cc_log_t   cc_log;

//----------------------------------------------------------------------------------------------------------------------
//描述：通过可变参数组合出字符串【支持...省略号形参】，自动往字符串最末尾增加换行符【所以调用者不用加\n】， 往标准错误上输出这个字符串；
//     如果err不为0，表示有错误，会将该错误编号以及对应的错误信息一并放到组合出的字符串中一起显示；
//调用格式比如：cc_log_stderr(0, "invalid option: \"%s\",%d", "testinfo",123);
void cc_log_stderr(int err, const char *fmt, ...)
{    
    va_list args;                        //创建一个va_list类型变量
    u_char  errstr[CC_MAX_ERROR_STR+1]; //2048  
    u_char  *p,*last;

    memset(errstr,0,sizeof(errstr));    

    last = errstr + CC_MAX_ERROR_STR;        //last指向整个buffer最后去了【指向最后一个有效位置的后面也就是非有效位】，作为一个标记，防止输出内容超过这么长,
                                                
    p = cc_cpymem(errstr, "cc: ", 4);     //p指向"cc: "之后    
    
    va_start(args, fmt); //使args指向起始的参数
    p = cc_vslprintf(p,last,fmt,args); //组合出这个字符串保存在errstr里
    va_end(args);        //释放args

    if (err)  //如果错误代码不是0，表示有错误发生
    {
        //错误代码和错误信息也要显示出来
        p = cc_log_errno(p, last, err);
    }
    
    if (p >= (last - 1))
    {
        p = (last - 1) - 1; 
    }
    *p++ = '\n'; //增加个换行符    

    //往标准错误【一般是屏幕】输出信息    
    write(STDERR_FILENO,errstr,p - errstr);
    if(cc_log.fd > STDERR_FILENO)
    {
        cc_log_error_core(CC_LOG_STDERR,err,(const char*)errstr);
    }
    return;
}

//----------------------------------------------------------------------------------------------------------------------
//描述：给一段内存，一个错误编号，要组合出一个字符串，形如：   (错误编号: 错误原因)，放到给的这段内存中去
//buf：是个内存，要往这里保存数据
//last：放的数据不要超过这里
//err：错误编号，是要取得这个错误编号对应的错误字符串，保存到buffer中
u_char *cc_log_errno(u_char *buf, u_char *last, int err)
{
    
    char *perrorinfo = strerror(err); 
    size_t len = strlen(perrorinfo);

    //然后我还要插入一些字符串： (%d:)  
    char leftstr[10] = {0}; 
    sprintf(leftstr," (%d: ",err);
    size_t leftlen = strlen(leftstr);

    char rightstr[] = ") "; 
    size_t rightlen = strlen(rightstr);
    
    size_t extralen = leftlen + rightlen; //左右的额外宽度
    if ((buf + len + extralen) < last)
    {
        buf = cc_cpymem(buf, leftstr, leftlen);
        buf = cc_cpymem(buf, perrorinfo, len);
        buf = cc_cpymem(buf, rightstr, rightlen);
    }
    return buf;
}

//----------------------------------------------------------------------------------------------------------------------
//往日志文件中写日志，代码中有自动加换行符，所以调用时字符串不用刻意加\n；
//    日过定向为标准错误，则直接往屏幕上写日志【比如日志文件打不开，则会直接定位到标准错误，此时日志就打印到屏幕上，参考cc_log_init()】
//level:一个等级数字，我们把日志分成一些等级，以方便管理、显示、过滤等等，如果这个等级数字比配置文件中的等级数字"LogLevel"大，那么该条信息不被写到日志文件中
//err：是个错误代码，如果不是0，就应该转换成显示对应的错误信息,一起写到日志文件中，
//cc_log_error_core(5,8,"这个XXX工作的有问题,显示的结果是=%s","YYYY");
void cc_log_error_core(int level,  int err, const char *fmt, ...)
{
    u_char  *last;
    u_char  errstr[CC_MAX_ERROR_STR+1];   //这个+1也是我放入进来的，本函数可以参考cc_log_stderr()函数的写法；

    memset(errstr,0,sizeof(errstr));  
    last = errstr + CC_MAX_ERROR_STR;   
    
    struct timeval   tv;
    struct tm        tm;
    time_t           sec;   //秒
    u_char           *p;    //指向当前要拷贝数据到其中的内存位置
    va_list          args;

    memset(&tv,0,sizeof(struct timeval));    
    memset(&tm,0,sizeof(struct tm));

    gettimeofday(&tv, NULL);     //获取当前时间，返回自1970-01-01 00:00:00到现在经历的秒数【第二个参数是时区，一般不关心】        

    sec = tv.tv_sec;             //秒
    localtime_r(&sec, &tm);      //把参数1的time_t转换为本地时间，保存到参数2中去，带_r的是线程安全的版本，尽量使用
    tm.tm_mon++;                 //月份要调整下正常
    tm.tm_year += 1900;          //年份要调整下才正常
    
    u_char strcurrtime[40]={0};  //先组合出一个当前时间字符串，格式形如：2019/01/08 19:57:11
    cc_slprintf(strcurrtime,  
                    (u_char *)-1,                       //若用一个u_char *接一个 (u_char *)-1,则 得到的结果是 0xffffffff....，这个值足够大
                    "%4d/%02d/%02d %02d:%02d:%02d",     //格式是 年/月/日 时:分:秒
                    tm.tm_year, tm.tm_mon,
                    tm.tm_mday, tm.tm_hour,
                    tm.tm_min, tm.tm_sec);
    p = cc_cpymem(errstr,strcurrtime,strlen((const char *)strcurrtime));  //日期增加进来，得到形如：     2019/01/08 20:26:07
    p = cc_slprintf(p, last, " [%s] ", err_levels[level]);                //日志级别增加进来，得到形如：  2019/01/08 20:26:07 [crit] 
    p = cc_slprintf(p, last, "%P: ",cc_pid);                             //支持%P格式，进程id增加进来，得到形如：   2019/01/08 20:50:15 [crit] 2037:

    va_start(args, fmt);                     //使args指向起始的参数
    p = cc_vslprintf(p, last, fmt, args);   //把fmt和args参数弄进去，组合出来这个字符串
    va_end(args);                            //释放args 

    if (err)  //如果错误代码不是0，表示有错误发生
    {
        //错误代码和错误信息也要显示出来
        p = cc_log_errno(p, last, err);
    }
    if (p >= (last - 1))
    {
        p = (last - 1) - 1; 
    }
    *p++ = '\n'; //增加个换行符       

    ssize_t   n;
    while(1) 
    {        
        if (level > cc_log.log_level) 
        {
            //这种日志就不打印了
            break;
        }

        //写日志文件        
        n = write(cc_log.fd,errstr,p - errstr);  //文件写入成功后，如果中途
        if (n == -1) 
        {
            //写失败有问题
            if(errno == ENOSPC) //写失败，且原因是磁盘没空间了
            {
            
            }
            else
            {
                if(cc_log.fd != STDERR_FILENO) 
                {
                    n = write(STDERR_FILENO,errstr,p - errstr);
                }
            }
        }
        break;
    } //end while    
    return;
}

//----------------------------------------------------------------------------------------------------------------------
//描述：日志初始化，
void cc_log_init()
{
    u_char *plogname = NULL;
    size_t nlen;

    //从配置文件中读取和日志相关的配置信息
    CConfig *p_config = CConfig::GetInstance();
    plogname = (u_char *)p_config->getString("Log");
    if(plogname == NULL)
    {
        plogname = (u_char *) CC_ERROR_LOG_PATH; //"logs/error.log" ,logs目录需要提前建立出来
    }
    cc_log.log_level = p_config->GetIntDefault("LogLevel",CC_LOG_NOTICE);//缺省日志等级为6【注意】 ，如果读失败，就给缺省日志等级
    //nlen = strlen((const char *)plogname);

    //只写打开|追加到末尾|文件不存在则创建【这个需要跟第三参数指定文件访问权限】
    //mode = 0644：文件访问权限， 6: 110    , 4: 100：     【用户：读写， 用户所在组：读，其他：读】 老师在第三章第一节介绍过
    cc_log.fd = open((const char *)plogname,O_WRONLY|O_APPEND|O_CREAT,0644);  
    if (cc_log.fd == -1)  //如果有错误，则直接定位到 标准错误上去 
    {
        cc_log_stderr(errno,"[alert] could not open error log file: open() \"%s\" failed", plogname);
        cc_log.fd = STDERR_FILENO; //直接定位到标准错误去了        
    } 
    return;
}
