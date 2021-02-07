#ifndef __C_COMM_H__
#define __C_COMM_H__

#define _PKG_MAX_LENGTH         30000   //每个包的最长长度 [包头+包体]

//通信 收包状态
#define _PKG_HD_INIT                    0           //初始状态，准备接收数据包头
#define _PKG_HD_RECVING          1           //接收包头中，包头不完整，继续接收中
#define _PKG_BD_INIT                    2           //包头刚好收完，准备接收包体
#define _PKG_BD_RECVING          3           //接收包体中，包体不完整，继续接收中，处理后直接回到_PKG_HD_INIT状态
#define _PKG_RV_FINISHED         4            //完整包收完

#define _DATA_BUFSIZE_               20         //固定大小的数组用来收包头


#pragma pack (1)

typedef struct _COMM_PKG_HEADER
{
    unsigned short pkgLen;      //报文总长度
    unsigned short msgCode; //消息类型

    int                             crc32;       //校验

}COMM_PKG_HEADER,*LPCOMM_PKG_HEADER;


#pragma pack()

#endif