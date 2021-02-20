#ifndef __CONF_H__
#define __CONF_H__

#include<vector>
#include"c_global.h"
using std::vector;

class CConfig
{
    private:
        CConfig();
    public:
        ~CConfig();
    private:
        static CConfig*m_instance;
    public:
        static CConfig* GetInstance(){
            if(m_instance==nullptr){
                if(m_instance==nullptr){
                    m_instance = new CConfig();
                    static CCrecovery cl;
                }
            }
            return m_instance;
        }
        class CCrecovery{
            public:
                ~CCrecovery(){
                    if(CConfig::m_instance){
                        delete CConfig::m_instance;
                        CConfig::m_instance = nullptr;
                    }
                }
        };
    public:
        bool Load(const char *pconfName);  //装载配置文件
        const char *getString(const char *p_itemName);
        int GetIntDefault(const char*p_itemname,const int def);
    public:
        vector<LPCConfItem> m_ConfigItemList;   //存储配置信息的列表 
};

#endif
