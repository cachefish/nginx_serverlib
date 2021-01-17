#ifndef __FUNC_H__
#define __FUNC_H__

//字符串相关函数  左右切割
void Rtrim(char*string);
void Ltrim(char*string);
int compareStr(const char *des,const char *src);

void cc_init_setproctitle();
void cc_setproctitle(const char *title);

#endif