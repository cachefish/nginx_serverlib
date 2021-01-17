#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "c_conf.h"  //和配置文件处理相关的类,名字带c_表示和类有关
#include "c_func.h"    //各种函数声明

//设置标题相关的全局变量
char **g_os_argv; //原始命令行参数数组，在mian中赋值
char *gp_envmem = nullptr;  //指向自己分配的env环境变量的内存
int g_environlen = 0; //环境变量所占内存大小

int main(int argc,char *const *argv)
{
     CConfig *p_config = CConfig::GetInstance(); //单例类
    if(p_config->Load("nginx.conf") == false) //把配置文件内容载入到内存
    {
        printf("配置文件载入失败，退出!\n");
        exit(1);
    }
    //  //获取配置文件信息的用法    
    // int port = p_config->GetIntDefault("ListenPort",0); //0是缺省值
    // printf("port=%d\n",port);
    // const char *pDBInfo = p_config->getString("DBInfo");
    // if(pDBInfo != NULL)
    // {
    //   printf("DBInfo=%s\n",pDBInfo);
    // }
   
    return 0;
}