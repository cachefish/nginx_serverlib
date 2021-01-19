#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdarg.h>
#include<stdint.h>

#include"c_global.h"
#include"c_macro.h"
#include"c_func.h"

//只用于本文件的一些函数声明就放在本文件中
static u_char *cc_sprintf_num(u_char *buf, u_char *last, uint64_t ui64,u_char zero, uintptr_t hexadecimal, uintptr_t width);

u_char *cc_slprintf(u_char *buf,u_char *last,const char *fmt,...)
{
    va_list args;
    u_char *p;

    va_start(args,fmt); //使args指向起始的参数
    p = cc_vslprintf(buf,last,fmt,args);
    va_end(args);   //释放args
    return p;

}

//"abc = %d",13   ,最终buf里得到的应该是   abc=13 这种结果
//buf：往这里放数据
//last：放的数据不要超过这里
//fmt：以这个为首的一系列可变参数
//支持的格式： %d【%Xd/%xd】:数字,    %s:字符串      %f：浮点,  %P：pid_t
    //对于：cc_log_stderr(0, "invalid option: \"%s\",%d", "testinfo",123);
       //fmt = "invalid option: \"%s\",%d"
       //args = "testinfo",123
u_char *cc_vslprintf(u_char *buf,u_char *last,const char *fmt,va_list args) //va_list 用于获取不确定个数的参数
{
    u_char zero;
      /*
    #ifdef _WIN64
        typedef unsigned __int64  uintptr_t;
    #else
        typedef unsigned int uintptr_t;
    #endif
    */
    uintptr_t  width,sign,hex,frac_width,scale,n;  //临时用到的一些变量

    int64_t    i64;   //保存%d对应的可变参
    uint64_t   ui64;  //保存%ud对应的可变参，临时作为%f可变参的整数部分也是可以的 
    u_char     *p;    //保存%s对应的可变参
    double     f;     //保存%f对应的可变参
    uint64_t   frac;  //%f可变参数,根据%.2f等，取得小数部分的2位后的内容；

    while(*fmt&&buf<last){
        if(*fmt == '%'){
            zero = (u_char)((*++fmt == '0')?'0':' ');//判断%后边接的是否是个'0',如果是zero = '0'，否则zero = ' '，一般比如显示10位，而实际数字7位，前头填充三个字符，就是这里的zero用于填充

            width = 0;                                       //格式字符% 后边如果是个数字，这个数字最终会弄到width里边来 ,这东西目前只对数字格式有效，比如%d,%f这种
            sign  = 1;                                       //显示的是否是有符号数，这里给1，表示是有符号数，除非你 用%u，这个u表示无符号数 
            hex   = 0;                                       //是否以16进制形式显示(比如显示一些地址)，0：不是，1：是，并以小写字母显示a-f，2：是，并以大写字母显示A-F
            frac_width = 0;                                  //小数点后位数字，一般需要和%.10f配合使用，这里10就是frac_width；
            i64 = 0;                                         //一般用%d对应的可变参中的实际数字，会保存在这里
            ui64 = 0;                                        //一般用%ud对应的可变参中的实际数字，会保存在这里    

            while(*fmt >= '0'&&*fmt <= '9') //如果%后边接的字符是 '0' --'9'之间的内容   ，比如  %16这种
            {
                width = width*10 + (*fmt++ - '0');    //1   16
            }

            //对于特殊格式，u X x .
            for(;;){
                switch (*fmt)
                {
                case 'u':
                    sign = 0;
                    fmt++;
                    continue;
                case 'X':
                    hex = 2;
                    sign = 0;
                    fmt++;
                    continue;
                case 'x':
                    hex = 1;
                    sign =0;
                    fmt++;
                    continue;
                case '.':
                    fmt++;
                    while(*fmt >= '0'&&*fmt<='9'){
                        frac_width = frac_width*10+(*fmt++ - '0');
                    }
                    break;

                default:
                    break;
                }
            } 

            switch (*fmt)
            {
            case '%':
                *buf++ = '%';
                fmt++;
                continue;
            
            case 'd':
                if(sign){
                    i64 = (int64_t)va_arg(args,int);    //va_arg遍历可变参数，var_arg的第二个参数表示遍历的这个可变的参数的类型
                }else{
                    ui64 = (uint64_t)va_arg(args,u_int);
                }
                break;

            case 's':
                p = va_arg(args,u_char*); 
                while(*p&&buf<last){
                    *buf++ = *p++;
                }
                fmt++;
                continue;
                
            case 'P':
                i64 = (int64_t)va_arg(args,pid_t);
                sign = 1;
                break;
            case 'f':
                f = va_arg(args,double);
                if(f<0){
                    *buf++ = '-';
                    f = -f;
                }
                //f>=0
                ui64 = (int64_t)f;
                frac = 0;

                //如果要求小数点后显示多少小数
                if(frac_width){
                    scale = 1;
                    for(n=frac_width;n;n--){
                        scale *= 10;
                    }
                    //把小数部分取出来 ，比如如果是格式    %.2f   ，对应的参数是12.537
                        // (uint64_t) ((12.537 - (double) 12) * 100 + 0.5);  
                                    //= (uint64_t) (0.537 * 100 + 0.5)  = (uint64_t) (53.7 + 0.5) = (uint64_t) (54.2) = 54
                    frac = (uint64_t) ((f - (double) ui64) * scale + 0.5);   //取得保留的那些小数位数，【比如  %.2f   ，对应的参数是12.537，取得的就是小数点后的2位四舍五入，也就是54】
                                                                                //如果是"%.6f", 21.378，那么这里frac = 378000
                    if(frac == scale){
                        ui64++;
                        frac = 0;
                    }
                }
                buf = cc_sprintf_num(buf,last,ui64,zero,0,width);
                if(frac_width)  //制定了显示多少位小数
                {
                    if(buf<last){
                        *buf++='.';
                    }
                    buf = cc_sprintf_num(buf,last,frac,'0',0,frac_width);
                }
                fmt++;
                continue;

            default:
                *buf++=*fmt++;  //往下移动字符
                continue;
            }
            //有符号数
            if(sign){
                if(i64<0){
                    *buf++ = '-';
                    ui64 = (uint64_t)-i64;
                }else{
                    ui64 = (uint64_t)i64;
                }
            }
            //把一个数字 比如“1234567”弄到buffer中显示，如果是要求10位，则前边会填充3个空格比如“   1234567”
            //注意第5个参数hex，是否以16进制显示，比如如果你是想以16进制显示一个数字则可以%Xd或者%xd，此时hex = 2或者1
            buf = cc_sprintf_num(buf, last, ui64, zero, hex, width); 
            fmt++;
        }else{
            *buf++ = *fmt++;
        }
    }
    return buf;
}

//----------------------------------------------------------------------------------------------------------------------
//以一个指定的宽度把一个数字显示在buf对应的内存中, 如果实际显示的数字位数 比指定的宽度要小 ,比如指定显示10位，而你实际要显示的只有“1234567”，那结果可能是会显示“   1234567”
     //当然如果你不指定宽度【参数width=0】，则按实际宽度显示
     //你给进来一个%Xd之类的，还能以十六进制数字格式显示出来
//buf：往这里放数据
//last：放的数据不要超过这里
//ui64：显示的数字         
//zero:显示内容时，格式字符%后边接的是否是个'0',如果是zero = '0'，否则zero = ' ' 【一般显示的数字位数不足要求的，则用这个字符填充】，比如要显示10位，而实际只有7位，则后边填充3个这个字符；
//hexadecimal：是否显示成十六进制数字 0：不
//width:显示内容时，格式化字符%后接的如果是个数字比如%16，那么width=16，所以这个是希望显示的宽度值【如果实际显示的内容不够，则后头用0填充】

static u_char*cc_sprintf_num(u_char *buf,u_char *last,uint64_t ui64,u_char zero,uintptr_t hexzdecimal,uintptr_t width)
{
    u_char *p,temp[CC_INT64_LEN+1];
    size_t len;
    uint32_t ui32;

    static u_char hex[] = "0123456789abcdef";
    static u_char HEX[] = "0123456789ABCDEF";

    p = temp + CC_INT64_LEN;//CC_INT64_LEN = 20,所以 p指向的是temp[20]那个位置，也就是数组最后一个元素位置

    if(hexzdecimal == 0){
        if(ui64 <= (uint64_t)CC_MAX_UINT32_VALUE){
            ui32 = (uint32_t)ui64;
            do{
                *--p = (u_char)(ui32%10  + '0');
            }while(ui32/=10);      
        }else{
            do{
                *--p = (u_char)(ui64%10 + '0');
            }while (ui64/=10);            
        }
    }else if(hexzdecimal == 1)
    {
        //显示一个1,234,567【十进制数】，对应的二进制数实际是 12 D687
        do{
            //0xf就是二进制的1111,大家都学习过位运算，ui64 & 0xf，就等于把 一个数的最末尾的4个二进制位拿出来；
            //ui64 & 0xf  其实就能分别得到 这个16进制数也就是 7,8,6,D,2,1这个数字，转成 (uint32_t) ，然后以这个为hex的下标，找到这几个数字的对应的能够显示的字符；
            *--p = hex[(uint32_t)(ui64&0xf)];

            //ui64 >>= 4     --->   ui64 = ui64 >> 4 ,而ui64 >> 4是啥，实际上就是右移4位，就是除以16,因为右移4位就等于移动了1111；
                                 //相当于把该16进制数的最末尾一位干掉，原来是 12 D687, >> 4后是 12 D68，如此反复，最终肯定有=0时导致while不成立退出循环
                                  //比如 1234567 / 16 = 77160(0x12D68) 
                                  // 77160 / 16 = 4822(0x12D6)

        }while(ui64>>=4);
    }else{  //hexzdecimal==2
        do{
            *--p = HEX[(uint32_t)(ui64&0xf)];
        }while(ui64 >>= 4);
    }
    
    len = (temp + CC_INT64_LEN)-p;  //得到这个数字的宽度，比如 “7654321”这个数字 ,len = 7

    while(len++ < width &&buf<last){ //如果你希望显示的宽度是10个宽度【%12f】，而实际想显示的是7654321，只有7个宽度，那么这里要填充5个0进去到末尾，凑够要求的宽度
            *buf++ = zero;//填充0进去到buffer中（往末尾增加）
    }

    len = (temp + CC_INT64_LEN)-p;//还原这个len，也就是要显示的数字的实际宽度【因为上边这个while循环改变了len的值】
    if((buf+len)>=last){//发现如果往buf里拷贝“7654321”后，会导致buf不够长【剩余的空间不够拷贝整个数字】
            len = last -buf;//剩余的buf有多少就拷贝多少
    }

    return cc_cpymem(buf,p,len); //把最新buf返回去；
    
}