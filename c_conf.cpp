#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<vector>

#include"c_func.h"
#include"c_conf.h"

using std::vector;

CConfig *CConfig::m_instance = nullptr;

//构造
CConfig::CConfig(){}

//析构函数
CConfig::~CConfig()
{
    vector<LPCConfItem>::iterator pos;
    for(pos=m_ConfigItemList.begin();pos!=m_ConfigItemList.end();++pos){
        delete (*pos);
    }
    m_ConfigItemList.clear();
}

//装载配置文件
bool CConfig::Load(const char *pconfName)
{
    FILE *fp;
    fp = fopen(pconfName,"r");
    if(fp==nullptr){
        return false;
    }

    //保存每行配置文件
    char linebuf[501];
    
    while(!feof(fp)){
        if(fgets(linebuf,500,fp)==nullptr){
            continue;
        }
        if(linebuf[0]==0){
            continue;
        }
        //处理注释行
        if(*linebuf==';'||*linebuf==' '||*linebuf=='#'||*linebuf=='\t'||*linebuf=='\n'){
            continue;
        }


    lblprocstring:
        //将后面的空行 回车 空格都截取
        if(strlen(linebuf)>0){
            if(linebuf[strlen(linebuf)-1]==10||linebuf[strlen(linebuf)-1] == 13 || linebuf[strlen(linebuf)-1] == 32)
            {
                linebuf[strlen(linebuf)-1] = 0;
                goto lblprocstring;
            }
        }
        
        if(linebuf[0]==0)
            continue;
        if(*linebuf=='[')
            continue;
        
        //ListenPort = 8888
        char *ptmp = strchr(linebuf,'=');
        if(ptmp != NULL){
            LPCConfItem p_confitem = new CConfItem;
            memset(p_confitem,0,sizeof(CConfItem));
            strncpy(p_confitem->ItemName,linebuf,(int)(ptmp-linebuf));//等号左侧的拷贝到p_confitem->ItemName
            strcpy(p_confitem->ItemContent,ptmp+1); //等号右侧的拷贝到p_confitem->ItemContent

            Rtrim(p_confitem->ItemName);
            Ltrim(p_confitem->ItemName);
			Rtrim(p_confitem->ItemContent);
			Ltrim(p_confitem->ItemContent);
           printf("itemname=%s | itemcontent=%s\n",p_confitem->ItemName,p_confitem->ItemContent);
            m_ConfigItemList.push_back(p_confitem);  //内存要释放，因为这里是new出来的
        }
    }
    fclose(fp);
    return true;
}

//根据ItemName获取配置信息字符串，不修改不用互斥
const char*CConfig::getString(const char*p_itemname)
{
    vector<LPCConfItem>::iterator pos;
    for(pos = m_ConfigItemList.begin();pos!=m_ConfigItemList.end();++pos)
    {
        if(strcasecmp((*pos)->ItemName,p_itemname)==0){
            //printf("%s",(*pos)->ItemContent);
            return (*pos)->ItemContent;
        }
        //printf("%s  %s",(*pos)->ItemName,(*pos)->ItemContent);
    }
    return nullptr;
}
//根据ItemName获取数字类型配置信息
int CConfig::GetIntDefault(const char *p_itemname,const int def)
{
    vector<LPCConfItem>::iterator pos;
    for(pos = m_ConfigItemList.begin();pos!=m_ConfigItemList.end();++pos){
        if(strcasecmp((*pos)->ItemName,p_itemname)==0){
            //printf("%d", (*pos)->ItemContent);
            return atoi((*pos)->ItemContent);
        }
    }
    return def;
}