#ifndef __C_LOGICCOMM_H__
#define __C_LOGICCOMM_H__

//收发命令宏定义

#define _CMD_START	                    0  
#define _CMD_PING                            _CMD_START+0       //心跳包
#define _CMD_REGISTER 		            _CMD_START + 5   //注册
#define _CMD_LOGIN 		                _CMD_START + 6   //登录



#pragma pack(1)

typedef struct _STRUCT_PRGSTER
{
    int iType;      //类型
    char username[56];
    char password[40];
}STRUCT_REGISTER,*LPSTRUCT_REGISTER;

typedef struct  _STRUCT_LOGIN
{
    char username[56];
    char password[40];
}_STRUCT_LOGIN,*LPSTRUCT_LOGIN;

#pragma pack()

#endif