#ifndef XBASIC_HPP
#define XBASIC_HPP
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <list>
#include <sys/time.h>
#include <ifaddrs.h>
#include <sys/time.h>

#ifdef _WINDOWS //WINDOWS平台
#include <io.h>
#include <cctype>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <libloaderapi.h>
#include <functional>
#pragma comment(lib,"ws2_32.lib")
#define InitNetwork() {WSADATA wsaData;WSAStartup(MAKEWORD(2,2),&wsaData);}
#define UnInitNetwork() {WSACleanup();}
#define ssleep(a) Sleep(a*1000)
#define ussleep(a) Sleep(a/1000)
#define localtime_r(a,b) localtime_s(b,a)
#define syncdisk()
#pragma warning(disable:4996)
#pragma warning(disable:4267)
#pragma warning(disable:4819)
#else //非WINDOWS平台
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <net/if.h>
#include <sys/ioctl.h>
#define __stdcall
#define ssleep(a) sleep(a)
#define ussleep(a) usleep(a)
#define closesocket(a) close(a)
#define syncdisk() sync()
#define InitNetwork()
#define UnInitNetwork()
#endif

#define REAL_STR_DEF(s) #s
#define STR(s) REAL_STR_DEF(s)

#define DRV_SLOT_VALUE(boardtye,slot) (((boardtye << 8) & 0xFF00) | (slot & 0x00FF))
#define BORADID_VALUE(boardtye,slot) (((slot << 8) & 0xFF00) | (boardtye & 0x00FF))

static FILE* stderr_fp = NULL;

class xbasic
{
public:
    typedef struct CPU_PACKED //定义一个cpu occupy的结构体
    {
        char        name[32]; //定义一个char类型的数组名name有20个元素
        unsigned int user;     //定义一个无符号的int类型的user
        unsigned int nice;     //定义一个无符号的int类型的nice
        unsigned int system;   //定义一个无符号的int类型的system
        unsigned int idle;     //定义一个无符号的int类型的idle
        unsigned int iowait;
        unsigned int softirq;
    }CPU_OCCUPY;

public:
    static std::string to_string(float val)
    {
        char buff[65] = {0};
        snprintf(buff,sizeof(buff)-1,"%.2f",val);
        std::string str(buff);
        return str;
    }

    static void debug_output(const char* format,...)
    {
        char writebuff[8192];
        int len =0;
        va_list arglist;
        time_t nowtime =time(NULL);
        struct tm *timeinfo =localtime(&nowtime);
        if(format[0]=='$')
        {
            va_start(arglist,format);
            vsprintf(writebuff+len,format+1,arglist);//处理变长参数
            va_end(arglist);
        }
        else
        {
            len =sprintf(writebuff,"[%02d-%02d %02d:%02d:%02d] ",timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
            va_start(arglist,format);
            vsprintf(writebuff+len,format,arglist);//处理变长参数
            va_end(arglist);
        }
        printf("%s",writebuff);
        if(0)
        {
            char sFileName[32] = {0};
            sprintf(sFileName,"debug%02d-%02d--%d.log",timeinfo->tm_mon+1,timeinfo->tm_mday,1+(timeinfo->tm_hour/8));
            FILE *pFile =fopen(sFileName,"a+");
            if(pFile) return;
            fprintf(pFile,"%s",writebuff);
            fclose(pFile);
        }
    }

    static void debug_bindata(const char *data_name,void *bindata,int data_len)
    {
        char print_buff[4096] = {0};
        xbasic::hex_to_str((unsigned char *)bindata,print_buff,data_len>2000?2000:data_len);
        xbasic::debug_output("%s %d: %s\n",data_name,data_len,print_buff);
    }

    static int exe_sys_cmd(std::string cmd)
    {
        int ret = ::system(cmd.c_str());
        //if(ret==-1) debug_output("system(%s) execution failed!!!\n",cmd.c_str());
        return ret;
    }

    static int hex_to_str(unsigned char *b_data,char *s_data,int data_len) //十六进制转字符串
    {
        const char arrchar[256][4] = {
            "00","01","02","03","04","05","06","07","08","09","0A","0B","0C","0D","0E","0F",
            "10","11","12","13","14","15","16","17","18","19","1A","1B","1C","1D","1E","1F",
            "20","21","22","23","24","25","26","27","28","29","2A","2B","2C","2D","2E","2F",
            "30","31","32","33","34","35","36","37","38","39","3A","3B","3C","3D","3E","3F",
            "40","41","42","43","44","45","46","47","48","49","4A","4B","4C","4D","4E","4F",
            "50","51","52","53","54","55","56","57","58","59","5A","5B","5C","5D","5E","5F",
            "60","61","62","63","64","65","66","67","68","69","6A","6B","6C","6D","6E","6F",
            "70","71","72","73","74","75","76","77","78","79","7A","7B","7C","7D","7E","7F",
            "80","81","82","83","84","85","86","87","88","89","8A","8B","8C","8D","8E","8F",
            "90","91","92","93","94","95","96","97","98","99","9A","9B","9C","9D","9E","9F",
            "A0","A1","A2","A3","A4","A5","A6","A7","A8","A9","AA","AB","AC","AD","AE","AF",
            "B0","B1","B2","B3","B4","B5","B6","B7","B8","B9","BA","BB","BC","BD","BE","BF",
            "C0","C1","C2","C3","C4","C5","C6","C7","C8","C9","CA","CB","CC","CD","CE","CF",
            "D0","D1","D2","D3","D4","D5","D6","D7","D8","D9","DA","DB","DC","DD","DE","DF",
            "E0","E1","E2","E3","E4","E5","E6","E7","E8","E9","EA","EB","EC","ED","EE","EF",
            "F0","F1","F2","F3","F4","F5","F6","F7","F8","F9","FA","FB","FC","FD","FE","FF"
        };
        for(int i=0; i<data_len; i++) {memcpy(s_data+(i<<1),arrchar[b_data[i]],2);}
        return (data_len<<1);
    }

    static int str_to_hex(char *s_data,unsigned char *b_data,int s_len) //字符串转十六进制
    {
        const unsigned char arrbyte[256] = {
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        };
        int byteslen =s_len>>1;
        for(int i=0; i<byteslen; i++) {b_data[i] = ((arrbyte[(unsigned char)s_data[i<<1]]<<4)|arrbyte[(unsigned char)s_data[(i<<1)+1]]);}
        return byteslen;
    }

    static int read_bigendian(void *mem_addr,int bytes_num) //从内存中读取大端整型数据
    {
        unsigned char *tmp_mem = (unsigned char *)mem_addr;
        if(bytes_num ==1) return (int)(tmp_mem[0]);
        else if(bytes_num ==2) return ((int)(tmp_mem[0]<<8)&0xFF00)|(tmp_mem[1]&0xFF);
        else if(bytes_num ==3) return ((int)(tmp_mem[0]<<16)&0xFF0000)|((tmp_mem[1]<<8)&0xFF00)|(tmp_mem[2]&0xFF);
        else
            return ((int)(tmp_mem[0]<<24)&0xFF000000)|((tmp_mem[1]<<16)&0xFF0000)|((tmp_mem[2]<<8)&0xFF00)|(tmp_mem[3]&0xFF);
    }

    static int read_littleendian(void *mem_addr,int bytes_num) //从内存中读取小端整型数据
    {
        unsigned char *tmp_mem = (unsigned char *)mem_addr;
        if(bytes_num ==1) return (int)(tmp_mem[0]);
        else if(bytes_num ==2) return ((int)(tmp_mem[1]<<8)&0xFF00)|(tmp_mem[0]&0xFF);
        else if(bytes_num ==3) return ((int)(tmp_mem[2]<<16)&0xFF0000)|((tmp_mem[1]<<8)&0xFF00)|(tmp_mem[0]&0xFF);
        else
            return ((int)(tmp_mem[3]<<24)&0xFF000000)|((tmp_mem[2]<<16)&0xFF0000)|((tmp_mem[1]<<8)&0xFF00)|(tmp_mem[0]&0xFF);
    }

    static float readfloat_bigendian(void *mem_addr,int bytes_num) //从内存中读取大端float数据
    {
        unsigned char *tmp_mem = (unsigned char *)mem_addr;
        unsigned char buff[4] = {0};

        buff[0] = tmp_mem[3];
        buff[1] = tmp_mem[2];
        buff[2] = tmp_mem[1];
        buff[3] = tmp_mem[0];

        float ret = 0.0f;
        memcpy(&ret,buff,4);
        return ret;
    }

    static double readdouble_bigendian(void *mem_addr,int bytes_num) //从内存中读取大端double数据
    {
        unsigned char *tmp_mem = (unsigned char *)mem_addr;
        unsigned char buff[8] = {0};

        buff[0] = tmp_mem[7];
        buff[1] = tmp_mem[6];
        buff[2] = tmp_mem[5];
        buff[3] = tmp_mem[4];
        buff[4] = tmp_mem[3];
        buff[5] = tmp_mem[2];
        buff[6] = tmp_mem[1];
        buff[7] = tmp_mem[0];

        double ret = 0.0;
        memcpy(&ret,buff,8);
        return ret;
    }

    static long long readlong_bigendian(void *mem_addr,int bytes_num) //从内存中读取大端long long数据
    {
        unsigned char *tmp_mem = (unsigned char *)mem_addr;
        unsigned char buff[8] = {0};

        buff[0] = tmp_mem[7];
        buff[1] = tmp_mem[6];
        buff[2] = tmp_mem[5];
        buff[3] = tmp_mem[4];
        buff[4] = tmp_mem[3];
        buff[5] = tmp_mem[2];
        buff[6] = tmp_mem[1];
        buff[7] = tmp_mem[0];

        long long ret = 0;
        memcpy(&ret,buff,8);
        return ret;
    }

    static float readfloat_littleendian(void *mem_addr,int bytes_num) //从内存中读取大端float数据
    {
        float ret = 0.0f;
        memcpy(&ret,mem_addr,bytes_num);
        return ret;
    }

    static long long readlong_littleendian(void *mem_addr,int bytes_num) //从内存中读取小端long long数据
    {
        long long ret = 0;
        memcpy(&ret,mem_addr,bytes_num);
        return ret;
    }

    static void write_bigendian(void *mem_addr,int value,int bytes_num) //写入内存中大端整型数据
    {
        unsigned char *tmp_mem =(unsigned char *)mem_addr;
        unsigned char *ptr_value = (unsigned char *)&value;
        if(bytes_num ==1)     {tmp_mem[0] =ptr_value[0];}
        else if(bytes_num ==2) {tmp_mem[0] =ptr_value[1];tmp_mem[1] =ptr_value[0];}
        else if(bytes_num ==3) {tmp_mem[0] =ptr_value[2];tmp_mem[1] =ptr_value[1];tmp_mem[2] =ptr_value[0];}
        else
            {tmp_mem[0] =ptr_value[3];tmp_mem[1] =ptr_value[2];tmp_mem[2] =ptr_value[1];tmp_mem[3] =ptr_value[0];}
    }

    static void write_littleendian(void *mem_addr,int value,int bytes_num) //写入内存中小端整型数据
    {
        memcpy(mem_addr,&value,bytes_num);
    }

    static void writefloat_bigendian(void *mem_addr,float value,int bytes_num) //写入内存中大端float数据
    {
        unsigned char *tmp_mem =(unsigned char *)mem_addr;
        unsigned char *ptr_value = (unsigned char *)&value;

        tmp_mem[0] = ptr_value[3];
        tmp_mem[1] = ptr_value[2];
        tmp_mem[2] = ptr_value[1];
        tmp_mem[3] = ptr_value[0];
    }

    static void writedouble_bigendian(void *mem_addr,double value,int bytes_num) //写入内存中大端double数据
    {
        unsigned char *tmp_mem =(unsigned char *)mem_addr;
        unsigned char *ptr_value = (unsigned char *)&value;

        tmp_mem[0] = ptr_value[7];
        tmp_mem[1] = ptr_value[6];
        tmp_mem[2] = ptr_value[5];
        tmp_mem[3] = ptr_value[4];
        tmp_mem[4] = ptr_value[3];
        tmp_mem[5] = ptr_value[2];
        tmp_mem[6] = ptr_value[1];
        tmp_mem[7] = ptr_value[0];
    }

    static void writelong_bigendian(void *mem_addr,long long value,int bytes_num) //写入内存中大端long long数据
    {
        unsigned char *tmp_mem =(unsigned char *)mem_addr;
        unsigned char *ptr_value = (unsigned char *)&value;

        tmp_mem[0] = ptr_value[7];
        tmp_mem[1] = ptr_value[6];
        tmp_mem[2] = ptr_value[5];
        tmp_mem[3] = ptr_value[4];
        tmp_mem[4] = ptr_value[3];
        tmp_mem[5] = ptr_value[2];
        tmp_mem[6] = ptr_value[1];
        tmp_mem[7] = ptr_value[0];
    }

    static void writefloat_littleendian(void *mem_addr,float value,int bytes_num) //写入内存中大端float数据
    {
        memcpy(mem_addr,&value,bytes_num);
    }

    static void writelong_littleendian(void *mem_addr,long long value,int bytes_num) //写入内存中小端long long数据
    {
        memcpy(mem_addr,&value,bytes_num);
    }

    static long long get_time_stamp_ms() //获得当前时间戳毫秒
    {
        struct timeval time_now;
        gettimeofday(&time_now, nullptr);
        return (time_now.tv_sec * 1000 + time_now.tv_usec / 1000);
    }

    static std::string time_to_str(time_t rawtime,const char *format=NULL) //"%Y-%m-%d %H:%M:%S"
    {
        struct tm info;
        char buffer[64] = {0};
#ifdef _WINDOWS //WINDOWS平台
        localtime_s(&info,&rawtime); //将time_t转为tm
#else
        localtime_r(&rawtime,&info); //将time_t转为tm
#endif
        strftime(buffer,sizeof(buffer)-1,(format)? format:"%Y-%m-%d %H:%M:%S",&info);
        return std::string(buffer);
    }

    static void set_sys_time(std::string time_string) //设置系统时间
    {
        char cmd_str[64] = {0};
        sprintf(cmd_str,"date -s \"%s\"\n",time_string.c_str());
        exe_sys_cmd(cmd_str);
        exe_sys_cmd("hwclock -w");
        exe_sys_cmd("sync");
    }

    static void trim(std::string& src_string) //去除字符串首尾空白字符
    {
        src_string.erase(0, src_string.find_first_not_of(" \r\n\t\v\f"));
        src_string.erase(src_string.find_last_not_of(" \r\n\t\v\f") + 1);
    }

    //截取key之后的子串
    static void trim_str(const std::string& key, std::string& src)
    {
        //1、先去除首尾空白字符
        xbasic::trim(src);

        //2、截取key之后字符串
        std::size_t found = src.find(key);
        if(found != std::string::npos)
        {
            std::size_t pos = found + key.length();
            src = src.substr(pos);
            //3、再次去除截取后字符串的首尾空白字符
            xbasic::trim(src);
        }
    }

    //检查文件是否存在
    static bool file_exist(std::string file_path)
    {
        FILE *hfile = fopen(file_path.c_str(), "r");
        if(!hfile)
        {
            //文件不存在
            return false;
        }

        fclose(hfile);
        return true;
    }

    static std::string get_number(std::string& src_string) //获得字符串中的数字
    {
        std::string str_ret;
        for(unsigned int i=0; i<src_string.length(); i++)
        {
            if(src_string[i]>='0' &&src_string[i]<='9') str_ret +=src_string[i];
        }
        return str_ret;
    }

    static std::string get_letter(std::string& src_string) //获得字符串中的字母
    {
        std::string str_ret;
        for(unsigned int i=0; i<src_string.length(); i++)
        {
            if((src_string[i]>='A' &&src_string[i]<='Z')||(src_string[i]>='a' &&src_string[i]<='z')) str_ret +=src_string[i];
        }
        return str_ret;
    }

    static int split_string(std::string src_string, std::string delim,std::vector<std::string> * vct_ret) //分割字符串
    {
        vct_ret->clear();
        size_t last =0, index =src_string.find_first_of(delim,last);
        while(index!=std::string::npos)
        {
            vct_ret->push_back(src_string.substr(last,index-last));
            last =index+delim.length();
            index =src_string.find_first_of(delim,last);
        }
        if((int)(src_string.length()-last)>0)
            vct_ret->push_back(src_string.substr(last,(int)(src_string.length()-last)));
        return (int)vct_ret->size();
    }

    static int version_to_int(std::string version) //字符串版本转整型
    {
        unsigned char bytes[4] = {0};
        std::vector<std::string> vct_num;

        if((version.front() == 'v') || (version.front() == 'V'))
        {
            //如果首字符在V字符则去掉
            version = version.substr(1);
        }

        int count =xbasic::split_string(version,".",&vct_num);
        if(count<=1) return -1; //不是版本字符串
        for(int i=count-1,j=0; i>=0&&j<4; i--,j++) bytes[j] =(unsigned char)atoi(vct_num[i].c_str());
        return (int)((bytes[3]<<24)&0xFF000000)|((bytes[2]<<16)&0xFF0000)|((bytes[1]<<8)&0xFF00)|(bytes[0]&0xFF);
    }

    static std::string version_from_int(int ver_int) //整型版本转字符串
    {
        char buff[16] = {0};
        std::string version;
        unsigned char *ptr_ver_int =(unsigned char *)&ver_int;
        for(int i=3; i>=0; i--)
        {
            if(ptr_ver_int[i]==0) continue;
            sprintf(buff,"%d",(int)ptr_ver_int[i]);
            if(version.length() >0) version+=".";
            version +=buff;
        }
        return version;
    }

    static unsigned int ip_to_int(std::string ip_str) //字符串IP转整型
    {
        unsigned char bytes[4] = {0};
        std::vector<std::string> vct_num;
        int count =xbasic::split_string(ip_str,".",&vct_num);
        if(count<4) return -1; //不是IP字符串
        for(int i=0; i<4; i++) bytes[i] =(unsigned char)atoi(vct_num[i].c_str());
        return ((unsigned int)(bytes[0]<<24)&0xFF000000)|((bytes[1]<<16)&0xFF0000)|((bytes[2]<<8)&0xFF00)|(bytes[3]&0xFF);
    }

    static std::string ip_from_int(unsigned int ip_int) //整型IP转字符串
    {
        char buff[16] = {0};
        unsigned char *ptr_ip = (unsigned char *)&ip_int;
        sprintf(buff,"%d.%d.%d.%d",ptr_ip[3],ptr_ip[2],ptr_ip[1],ptr_ip[0]);
        return std::string(buff);
    }

    static int get_files_in_dir(std::string dir,std::vector<std::string> *vct_files) //获取文件夹内所有文件
    {
        vct_files->clear();
        bool flag = true;
        if(dir.back() != '/')
        {
            //path路径没有'/'结尾
            flag = false;
        }
#ifdef _WINDOWS
        _finddata_t file_info;
        std::string strfind =dir + "\\*";
        long handle =_findfirst(strfind.c_str(),&file_info);
        if(handle ==-1) return -1;
        do
        {
            if(file_info.attrib & _A_SUBDIR) continue;//是子文件夹
            vct_files->push_back(dir + "\\" +file_info.name);
        }while (_findnext(handle,&file_info) ==0);
        _findclose(handle);
#else
        DIR *dp; struct dirent *entry; struct stat statbuf;
        if((dp =opendir(dir.c_str())) ==NULL) return -1;
        // int ret =chdir(dir.c_str());
        while((entry =readdir(dp)) !=NULL)
        {
            // 构建文件的完整路径
            char file_path[1024];
            memset(file_path,0,sizeof(file_path));
            if(flag)
            {
                snprintf(file_path, sizeof(file_path), "%s%s", dir.c_str(), entry->d_name);
            }
            else
            {
                snprintf(file_path, sizeof(file_path), "%s/%s", dir.c_str(), entry->d_name);
            }

            if(lstat(file_path,&statbuf) < 0)
            {
                // 信息获取失败跳过
                continue;
            }

            if(S_ISDIR(statbuf.st_mode)) continue; //是子文件夹

            vct_files->push_back(file_path);
        }
        // ret =chdir("..");
        closedir(dp);
#endif
        return vct_files->size();
    }

    static char *get_module_path() //获取程序当前路径
    {
        const int MAX_PATHSIZE =256;
        static char cur_app_path[MAX_PATHSIZE] ={0};
        if(cur_app_path[0]) return cur_app_path;
#ifdef _WINDOWS
        ::GetModuleFileNameA(NULL,cur_app_path,MAX_PATHSIZE-1);
#else
        ssize_t length =::readlink("/proc/self/exe", cur_app_path, MAX_PATHSIZE-1); //获取当前程序绝对路径
#endif
        int path_len =strlen(cur_app_path);
        for(int i =path_len; i>=0; --i) //获取当前目录绝对路径，即去掉程序名
        {
            if(cur_app_path[i] == '/' || cur_app_path[i] == '\\')
            {
                cur_app_path[i+1] = '\0';
                break;
            }
        }
        return cur_app_path;
    }

    static int save_to_file(const char *file_path,void *data,int data_len) //将缓冲区数据写道文件
    {
        FILE *pFile =fopen(file_path,"w+b");
        if(!pFile) return -1;
        int write_ret =fwrite(data,data_len,1,pFile);
        fclose(pFile);
        return (write_ret>0?write_ret*data_len:write_ret);
    }

    static int load_from_file(const char *file_path,void *buff,int buff_size) //将文件加载到缓冲区
    {
        FILE *pFile =fopen(file_path,"rb");
        if(!pFile) return -1;
        int read_ret =fread(buff,1,buff_size,pFile);
        fclose(pFile);
        return read_ret;
    }

    static std::string read_data_from_file(std::string file_path,int offset=0,int read_len =0) //从文件读数据
    {
        FILE *fd_file =fopen(file_path.c_str(),"rb");
        if(!fd_file) return std::string("");
        int data_len =0;
        char read_buff[8192] ={0};
        if(read_len<=0 || read_len >8000) read_len =8000;
        if(fseek(fd_file,offset,SEEK_SET)==0) //定位到指定位置
        {
            data_len =fread(read_buff,1,read_len,fd_file);
        }
        fclose(fd_file);
        if(data_len<=0) return std::string("");
        return std::string(read_buff,data_len);
    }

    static void scan_dir(const char *path, std::list<std::string>& lst_files)
    {
#ifdef _WINDOWS
        //待实现
#else
        DIR *dir;
        struct dirent *entry;
        struct stat file_stat;
        if(path == NULL)
        {
            perror("path is null");
            return ;
        }

        // 打开目录
        dir = opendir(path);
        if (dir == NULL)
        {
            perror("opendir");
            return;
        }

        // 读取目录中的每个文件
        while ((entry = readdir(dir)) != NULL)
        {
            // 构建文件的完整路径
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);

            // 获取文件的信息
            if (lstat(file_path, &file_stat) < 0)
            {
                perror("lstat");
                continue;
            }

            // 判断文件类型
            if (S_ISDIR(file_stat.st_mode))
            {
                // 如果是目录，则递归遍历
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                scan_dir(file_path,lst_files);
            }
            else
            {
                // 如果是文件，则打印文件名
                //printf("%s\n", file_path);
                lst_files.push_back(file_path);
            }
        }

        // 关闭目录
        closedir(dir);
#endif
        return ;
    }

#ifdef _WINDOWS //WINDOWS平台
    static float get_mem_occupy() //获取系统内存使用状况
    {
        return (30+0.01f*(rand()%6800));
    }

    static float get_cpu_occupy() //获得CPU利用百分比
    {
        return (5+0.01f*(rand()%2500));
    }

    static unsigned int get_pid() //获取本进程ID
    {
        return 1;
    }
#else
    static float get_mem_occupy() //获取系统内存使用状况
    {
        char buff[256] = {0};
        char name01[32] ={0},name02[32] ={0};
        unsigned long mem_total =0,mem_free =0,mem_available =0;
        FILE *fd =fopen("/proc/meminfo","r");
        if(fgets(buff,sizeof(buff),fd) return -1;
        sscanf(buff,"%s %lu %s\n",name01,&mem_total,name02);
        if(fgets(buff,sizeof(buff),fd) return -1;
        sscanf(buff,"%s %lu %s",name01,&mem_free,name02);
        if(fgets(buff,sizeof(buff),fd) return -1;
        sscanf(buff,"%s %lu %s",name01,&mem_available,name02);
        fclose(fd);
        float mem_used_rate =(1.0f - (float)mem_available/(float)mem_total) *100;
        return mem_used_rate;
    }

    static double cal_cpu_occupy(CPU_OCCUPY *o, CPU_OCCUPY *n)
    {
        double od =(double)(o->user +o->nice +o->system +o->idle +o->softirq +o->iowait +o->irq);//第一次(用户+优先级+系统+空闲)的时间再赋给od
        double nd =(double)(n->user +n->nice +n->system +n->idle +n->softirq +n->iowait +n->irq);//第二次(用户+优先级+系统+空闲)的时间再赋给od
        double id = (double)(n->idle -o->idle);    //用户第一次和第二次的时间之差再赋给sd
        double sd = (double)(o->idle);    //系统第一次和第二次的时间之差再赋给sd
        if((nd-od) !=0) return 100.0 - ((id-sd))/(nd-od)*100.00; //((用户+系统)乘100)除(第一次和第二次的时间差)再赋给g_cpu_used
        else
            return 0;
    }

    static void get_cpu_status(CPU_OCCUPY *cpust)
    {
        char buff[256] ={0};
        CPU_OCCUPY *cpu_occupy =cpust;
        FILE *fd =fopen ("/proc/stat","r");
        fscanf(fd,"%s %u %u %u %u %u %u %u",cpu_occupy->name,&cpu_occupy->user,&cpu_occupy->nice,&cpu_occupy->system,&cpu_occupy->idle,&cpu_occupy->iowait,&cpu_occupy->irq,&cpu_occupy->softirq);
        fclose(fd);
    }

        static float get_cpu_occupy() //获得CPU利用百分比
    {
        static CPU_OCCUPY cpu_stat1;
        static CPU_OCCUPY cpu_stat2;
        static int inited =0;
        if(inited ==0)
        {
            inited =1;
            get_cpu_status((CPU_OCCUPY *)&cpu_stat1);
            ussleep(100*1000);
        }
        get_cpu_status((CPU_OCCUPY *)&cpu_stat2); //第二次获取cpu使用情况
        float cpu_rate =(float)cal_cpu_occupy ((CPU_OCCUPY *)&cpu_stat1, (CPU_OCCUPY *)&cpu_stat2); //计算cpu使用率
        memcpy(&cpu_stat1,&cpu_stat2,sizeof(cpu_stat1));
        return cpu_rate;
    }

    static unsigned int get_pid() //获取本进程ID
    {
        return (unsigned int)getpid();
    }

    static std::string getFirstNonLoopbackIPv4()
    {
        struct ifaddrs *ifaddr, *ifa;
        int family, s;
        char host[NI_MAXHOST];
        std::string ip;

        if (getifaddrs(&ifaddr) == -1)
        {
            perror("getifaddrs");
            return ip;
        }

        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL)
            {
                continue;
            }

            family = ifa->ifa_addr->sa_family;
            if (family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK))
            {
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                if (s != 0)
                {
                    printf("getnameinfo() failed: %s\n", gai_strerror(s));
                    continue;
                }
                ip = host;
                break;
            }
        }

        freeifaddrs(ifaddr);
        return ip;
    }

//重定向stderr到文件
    static void redirect_stderr(const std::string& filename)
    {
        if(filename.empty())
        {
            printf("redirect_stderr() filename is empty \n");
            return;
        }

        // 追加模式
        stderr_fp = freopen(filename.c_str(), "a", stderr);
        if (!stderr_fp)
        {
            printf("redirect_stderr() can't open %s",filename.c_str());
        }

        return ;
    }

//输出到stderr的重定向文件
    static void output_stderr(const char* mgsfmt, ...)
    {
        time_t currt;
        struct tm curr_tm;

        time(&currt);
        localtime_r(&currt, &curr_tm);
        struct timeval tv;
        gettimeofday(&tv,NULL);

        char occur_time[30] = {0};
        sprintf(occur_time,"%4d-%02d-%02d %02d:%02d:%02d.%d",
                curr_tm.tm_year+1900,curr_tm.tm_mon+1,curr_tm.tm_mday,
                curr_tm.tm_hour,curr_tm.tm_min,curr_tm.tm_sec, tv.tv_usec/1000);

        char err_msg[1024] = {0};

        va_list vl;
        va_start(vl, mgsfmt);
        vsnprintf(err_msg,sizeof(err_msg), mgsfmt, vl );
        va_end(vl);

        fprintf(stderr, "[%s] Err: %s\n", occur_time, err_msg);
        fflush(stderr);
        return ;
    }

//关闭stderr的重定向文件
    static void close_stderr()
    {
        if(stderr_fp != NULL)
        {
            fclose(stderr_fp);
            stderr_fp = NULL;
        }

        return ;
    }
#endif
};

#endif
