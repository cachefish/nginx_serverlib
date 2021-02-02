#ifndef __FUNC_H__
#define __FUNC_H__

//字符串相关函数  左右切割
void Rtrim(char*string);
void Ltrim(char*string);
int compareStr(const char *des,const char *src);

//设置可执行程序标题相关函数
void cc_init_setproctitle();
void cc_setproctitle(const char *title);


//日志输出
void cc_log_init();
void cc_log_stderr(int err,const char *fmt,...);
void cc_log_error_core(int level,int err,const char *fmt,...);

u_char *cc_log_errno(u_char *buf,u_char *last, int err);
u_char *cc_snprintf(u_char *buf, size_t max, const char *fmt, ...);
u_char *cc_slprintf(u_char *buf,u_char *last,const char *fmt,...);
u_char *cc_vslprintf(u_char *buf,u_char *last,const char *fmt,va_list args);  //va_llis 用于获取不确定个数的参数


//信号
int cc_init_signals();

//主进程流程
void cc_master_process_cycle();

int cc_daemon();
#endif