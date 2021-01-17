#include<stdio.h>
#include<string.h>
#include<math.h>
#include"c_func.h"

//截取字符串尾部空格
void Rtrim(char *string)
{
    size_t len = 0;
    if(string==nullptr){
        return;
    }
    len = strlen(string);
    while (len>0&&string[len-1]==' ')
    {
        string[--len] = 0;
    }
    return;
}

void Ltrim(char *string)
{
    size_t len = 0;
    len = strlen(string);
    char *p_tmp = string;
    if((*p_tmp) != ' '){   //不是以空格开头
        return;
    }
    while((*p_tmp) != '\0'){
        if((*p_tmp) == ' ')   //将指针向右移
            p_tmp++;
        else
            break;
    }
    if((*p_tmp) =='\0'){  //如果全是空格
        *string = '\0';
        return;
    }
    char *p_tmp2 = string;
    while ((*p_tmp) != '\0')
    {
        (*p_tmp2) = (*p_tmp);
        p_tmp++;
        p_tmp2++;
    }
    (*p_tmp2) = '\0';
    return;

}

/*******************************************
//功能：字符串匹配不区分大小写 
//返回值： 匹配 1，不匹配  0
//作者：小龙仔
//微信号：L-G-Q-06 
******************************************/
int compareStr(const char *des,const char *src)
{
	int id_len,param_len;
	int times,i,j;
	
	id_len = strlen(src);
	param_len = strlen(des);
	
	if(param_len<id_len)
	{
		return 0;
	}
	else
	{	
		//比较次数 
		times = param_len - id_len+1;
	}
//	printf("id_len = %d ,param_len = %d ,times = %d\n",id_len,param_len,times);
	for(i=0;i<times;i++)
	{
		for(j=0;j<id_len;j++)
		{
			if(des[i+j] == src[j])
			{
				if(j == id_len-1)
				{
					return 1;
				}
			}//判断两个字符是否都是字母 
			else if(((des[i+j]>=97&&des[i+j]<=122)||(des[i+j]>=65&&des[i+j]<=90)) && ((src[j]>=97&&src[j]<=122)||(src[j]>=65&&src[j]<=90)))      
			{	
				//判断两个字母是不是大小写相反 
				if(32	== abs(des[i+j]-src[j])) 
				{
					if(j == id_len-1)
					{
						return 1;
					}
				}
				else
				{
					break;
				}	 
			}
			else
			{
				break;
			}
		}
	}
	return 0;
}