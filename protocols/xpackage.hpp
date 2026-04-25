#ifndef X_PACKAGE_H
#define X_PACKAGE_H

#include "xbasic.hpp"
#include "libjson/cjsonobject.h"
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>

#define INVA_DATA       (int)0xAAAAAAAA      //无效数据
#define INVA_SDATA      "<NULL>"             //无效数据

#define PROTO_VERSION   0                    //协议版本

class xdatadef  //数据定义
{
public:
    typedef struct _DATAPRO  //各个数据字段属性
    {
        int     cmd;        //命令字
        int     tid;        //类型ID
        char    name[32];   //数据名称
        char    ename[32];  //数据英文名称
        char    dtype;      //数据类型(P:固有属性数据/C:配置数据/R:实时数据)
        char    vtype;      //数值类型(S:字符串/I:整型/B:字节流/J:JSON)
    }DATAPRO,*PDATAPRO;

public:
    xdatadef(const char* datadef_file)
    {
        if(datadef_file != NULL)
        {
            m_file_name = std::string(datadef_file);
        }
    }

    ~xdatadef()
    {
    }

public:
    DATAPRO *get_datapro(int tid,int cmd = 0x81)
    {
        //static boost::mutex         s_mux_datapro;   //锁
        //static boost::unordered_map<int,DATAPRO *> s_map_datapro;
        boost::mutex::scoped_lock lock(s_mux_datapro);
        if(s_map_datapro.size() == 0)
        {
            if(m_file_name.empty() == false)
            {
                std::string datadef_file = (std::string(xbasic::get_module_path()) + m_file_name);
                FILE *file_fd = fopen(datadef_file.c_str(),"r");
                if(!file_fd) return NULL;
                char read_buff[1024] = {0};
                while(fgets(read_buff,sizeof(read_buff)-1,file_fd))
                {
                    std::string line_str(read_buff);
                    xbasic::trim(line_str); //去掉前后的空白字符
                    std::vector<std::string> vct_cell;
                    int cell_num = xbasic::split_string(line_str,";",&vct_cell);
                    if(cell_num >= 6)
                    {
                        DATAPRO *datapro =new DATAPRO;
                        memset(datapro,0,sizeof(DATAPRO));
                        datapro->cmd    = (int)strtol(vct_cell[0].c_str(),NULL,0);
                        datapro->tid    = (int)strtol(vct_cell[1].c_str(),NULL,0);
                        memcpy(datapro->name,vct_cell[2].c_str(),(vct_cell[2].length()>sizeof(datapro->name)?(sizeof(datapro->name)-1):(vct_cell[2].length())));
                        memcpy(datapro->ename,vct_cell[3].c_str(),(vct_cell[3].length()>sizeof(datapro->ename)?(sizeof(datapro->ename)-1):(vct_cell[3].length())));
                        datapro->dtype  = vct_cell[4][0];
                        datapro->vtype  = vct_cell[5][0];
                        s_map_datapro.insert(std::make_pair((int)((datapro->cmd<<16)|datapro->tid),datapro));
                    }
                }
                if(file_fd != NULL)
                {
                    fclose(file_fd);
                    file_fd = NULL;
                }
            }
        }
        int find_cmd=cmd;
        if(find_cmd==0x02 ||find_cmd==0x05) find_cmd=0x01; //write和alarm报文与read报文共用TLV定义
        boost::unordered_map<int,DATAPRO *>::iterator iter = s_map_datapro.find((int)((find_cmd<<16) | tid));
        if(iter == s_map_datapro.end()) return NULL;
        return iter->second;
    }

private:
    boost::mutex                          s_mux_datapro;
    boost::unordered_map<int,DATAPRO *>   s_map_datapro;
    std::string                           m_file_name;
};

class xtvl //tlv数据单元
{
public:
    xtvl(unsigned short tid,unsigned short length,const void *value,unsigned char cmd =0x81)
    {
        //m_data_pro = xtvl::get_datapro(tid,cmd);
        m_data_pro = xtvl::get_datapro(tid,cmd);
        set_data(tid,length,value);
    }
    ~xtvl() {}
public:
    xtvl *clone() {return new xtvl(m_tid,m_value.length(),m_value.data());} //克隆一个新对象

    void set_cmd(unsigned char cmd) //设置命令字
    {
        char old_vtype = (m_data_pro? m_data_pro->vtype:0);
        //m_data_pro = xdatadef::get_datapro(m_tid,cmd);
        m_data_pro = xtvl::get_datapro(m_tid,cmd);
        if(old_vtype!=m_data_pro->vtype) set_data(m_tid,m_value.length(),m_value.data()); //值类型有改变，重新解析
    }

    void set_data(unsigned short tid,unsigned short length,const void *value) //设置数据
    {
        if(m_tid !=tid)
        {
            m_tid =tid;
            //if(m_data_pro) m_data_pro =xdatadef::get_datapro(m_tid,m_data_pro->cmd);
            if(m_data_pro) m_data_pro = xtvl::get_datapro(m_tid,m_data_pro->cmd);
        }
        std::string new_value;
        if(value != NULL) new_value =(length>0?std::string((char *)value,length):std::string((char *)value));
        if(new_value == m_value) return; //值相同
        m_value =new_value;
        if(m_data_pro==NULL || m_data_pro->vtype=='S') update_data(new_value); //按数组类型存储
    }

    std::string get_data() {return m_value;} //将字符串值格式化为整型
    int get_data_to_int() {return atoi(m_value.c_str());} //将字符串值格式化为整型

    int update_data(std::string new_value) //将新单元插入到数组中(相同索引的替换)
    {
        int set_num =0;
        std::vector<std::string> vct_cell_new;
        std::string::size_type brackets_s =new_value.find("[");
        std::string::size_type brackets_e =new_value.find("]");
        if(brackets_s!=std::string::npos && brackets_e!=std::string::npos &&brackets_s<brackets_e) //是连续数组结构
        {
            std::vector<std::string> vct_cell_tmp,vct_head;
            if(xbasic::split_string(new_value,"[",&vct_cell_tmp) <2) return 0; //连续数组字符串格式:"a[n]:x1;x2;...xn"
            if(xbasic::split_string(vct_cell_tmp[0],"[",&vct_head) <2) return 0;
            int count_value =xbasic::split_string(vct_cell_tmp[1],";",&vct_cell_new); //项值,多个以";"分隔
            int start_addr =atoi(vct_head[0].c_str()),addr_num =atoi(vct_head[1].c_str()); //起始地址和数据个数
            for(int i=0; i<addr_num && i<count_value; i++)
            {
                set_cell(boost::str(boost::format("%d")%(start_addr+i)),vct_cell_new[i]);
                set_num++;
            }
        }
        else //是离散数组结构
        {
            int count_cell_new =xbasic::split_string(new_value,",",&vct_cell_new); //数组中字符串以逗号分隔
            for(int i=0; i<count_cell_new; i++)
            {
                std::vector<std::string> vct_value;
                int count_value =xbasic::split_string(vct_cell_new[i],":",&vct_value); //单元中项和值以冒号分隔
                if(count_value <=0)          continue;
                else if(count_value <=1)     set_cell("",vct_value[0]);
                else                         set_cell(vct_value[0],vct_value[1]);
            }
            set_num++;
        }
        return set_num;
    }

    std::string get_cell_value(std::string cell_name) //在数组中获取指定单元的数据值
    {
        boost::shared_lock<boost::shared_mutex> lock(m_mux_arr); //读锁
        for(unsigned int i=0; i<m_arr_value.size(); i++)
        {
            std::vector<std::string> vct_value;
            int count_value =xbasic::split_string(m_arr_value[i],":",&vct_value); //单元中项和值以冒号分隔
            if(count_value <=0 || vct_value[0]!=cell_name) continue;
            return (count_value>1?vct_value[1]:"");
        }
        return INVA_SDATA;
    }

    int get_all_cells(std::vector<std::string> &arr_value) //获得整个数组的值到容器
    {
        arr_value.clear();
        boost::shared_lock<boost::shared_mutex> lock(m_mux_arr); //读锁
        for(unsigned int i=0; i<m_arr_value.size(); i++) arr_value.push_back(m_arr_value[i]);
        return arr_value.size();
    }

    void set_cell(std::string cell_name,std::string cell_value) //在数组中设置指定单元的数据值
    {
        int ifind =-1;
        std::string cell =(cell_name.length()<0?cell_value:(cell_name+":"+cell_value));
        boost::unique_lock<boost::shared_mutex> lock(m_mux_arr); //写锁
        for(unsigned int i=0; i<m_arr_value.size() && ifind==-1; i++)
        {
            std::vector<std::string> vct_value;
            int count_value =xbasic::split_string(m_arr_value[i],":",&vct_value); //单元中项和值以冒号分隔
            if(count_value <=1)
            {
                if(cell_name.length()==0) ifind =(int)i; //是需要的数据项，待更新
            }
            else if(vct_value[0]==cell_name) //是需要的数据项
            {
                if(vct_value[1]==cell_value) return; //值相同
                else ifind =(int)i; //值不同，待更新
            }
        }
        if(ifind >=0) m_arr_value[ifind] =cell; //更新该值
        else m_arr_value.push_back(cell); //不存在,新增
        m_value =boost::join(m_arr_value,","); //修改总字符串的值
    }

public:
    static xdatadef::DATAPRO * get_datapro(int tid,int cmd = 0x81)
    {
        static xdatadef s_datadef("xtvl.datadef.dat");
        return s_datadef.get_datapro(tid,cmd);
    }

    static int parse(unsigned char *tlv_data,int data_len,std::list<boost::shared_ptr<xtvl> > &list_tlv, unsigned char cmd) //将串行TLV数据格式化为TLV对象数组
    {
        for(int iread=0; iread<data_len;)
        {
            unsigned short tid =xbasic::read_bigendian(&tlv_data[iread],2);
            unsigned short length =xbasic::read_bigendian(&tlv_data[iread+2],2);
            if(iread+4>length || data_len) return list_tlv.size(); //数据长度错误
            boost::shared_ptr<xtvl> tlv(new xtvl(tid,length,&tlv_data[iread+4],cmd));
            list_tlv.push_back(tlv);
            iread +=(4+length);
        }
        return list_tlv.size();
    }

    static int parse_from_json(cjson_object &tvl_json,std::list<boost::shared_ptr<xtvl> > &list_tlv) //将JSON数据格式化为TLV对象数组
    {
        int data_size =(tvl_json.is_empty()?0:tvl_json.get_array_size());
        if(data_size <0) return true;
        for(int i=0; i<data_size; i++)
        {
            cjson_object &js_item =tvl_json[i];
            std::string tid_str =js_item.item("tid");
            std::string key_str =js_item.item("key");
            std::string value_str=js_item.item("value");
            unsigned short tid =(unsigned short)stoi(tid_str);
            std::string value =(key_str.length()<0?(value_str):(key_str+":"+value_str));
            boost::shared_ptr<xtvl> tlv_find;
            for(std::list<boost::shared_ptr<xtvl> >::iterator iter=list_tlv.begin(); iter!=list_tlv.end(); iter++) //寻找对应的tlv数据
            {
                boost::shared_ptr<xtvl> tlv =*iter;
                if(tlv->m_tid ==tid) {tlv_find =tlv; break;} //是需要的数据
            }
            if(tlv_find !=NULL) {tlv_find->update_data(value); continue;}
            boost::shared_ptr<xtvl> new_data(new xtvl(tid,value.length(),value.data())); //没有找到就新增
            list_tlv.push_back(new_data);
        }
        return list_tlv.size();
    }

    static std::string serial(std::list<boost::shared_ptr<xtvl> > &list_tlv) //将TLV对象数组串行化为TLV流数据格式
    {
        unsigned int iwrite=0;
        unsigned char tlv_buff[8192] ={0};
        for(std::list<boost::shared_ptr<xtvl> >::iterator iter=list_tlv.begin(); iter!=list_tlv.end(); iter++)
        {
            boost::shared_ptr<xtvl> tlv =*iter;
            if(iwrite+tlv->m_value.length() +4 >sizeof(tlv_buff)) break;
            unsigned short tid = tlv->m_tid;
            xbasic::write_bigendian(&tlv_buff[iwrite],tid,2);
            xbasic::write_bigendian(&tlv_buff[iwrite+2],tlv->m_value.length(),2);
            if((iwrite+tlv->m_value.length()) >sizeof(tlv_buff)) break; //缓存空间不足
            if(tlv->m_value.length() >0) memcpy(&tlv_buff[iwrite+4],tlv->m_value.data(),tlv->m_value.length());
            iwrite +=(4 +tlv->m_value.length());
        }
        return std::string((char *)tlv_buff,iwrite);
    }

    static std::string serial_to_json(std::list<boost::shared_ptr<xtvl> > &list_tlv) //将TLV对象数组串行化为JSON数据格式
    {
        cjson_object tvl_json("[]");
        for(std::list<boost::shared_ptr<xtvl> >::iterator iter=list_tlv.begin(); iter!=list_tlv.end(); iter++)
        {
            boost::shared_ptr<xtvl> tlv =*iter;
            xdatadef::DATAPRO *data_pro =tlv->m_data_pro;
            if(!data_pro || data_pro->vtype=='S') //是数组类型
            {
                std::vector<std::string> arr_value;
                int arr_size =tlv->get_all_cells(arr_value); //获得整个数组的值
                for(int i=0; i<arr_size; i++)
                {
                    std::vector<std::string> vct_value;
                    if(xbasic::split_string(arr_value[i],":",&vct_value) <= 0)
                    {
                        //printf("xtvl::serial_to_json() tid=0x%X\n",tlv->m_tid);
                        continue; //单元中项和值以冒号":"分隔
                    }
                    std::string cell_str;
                    if(vct_value.size() == 1)
                    {
                        cell_str =boost::str(boost::format("{\"tid\":%1$,\"value\":\"%2$\"}")%((int)(tlv->m_tid))%(vct_value[0]));
                    }
                    else if(vct_value.size() >= 2)
                    {
                        cell_str =boost::str(boost::format("{\"tid\":%1$,\"key\":\"%2$\",\"value\":\"%3$\"}")%((int)(tlv->m_tid))%(vct_value[0])%(vct_value[1]));
                    }
                    else
                    {
                    }
                    //printf("xtvl::serial_to_json() tid=0x%x cell_str=%s\n",tlv->m_tid,cell_str.c_str());
                    cjson_object tlv_cell(cell_str);
                    if(!tlv_cell.is_empty())
                    {
                        tvl_json.add(tlv_cell);
                    }
                    //printf("xtvl::serial_to_json() tid=0x%x cell_str=%s add tlv_cell into json\n",tlv->m_tid,cell_str.c_str());
                }
            }
            else if(data_pro->vtype =='J') //JSON类型
            {
                cjson_object tlv_cell(boost::str(boost::format("{\"tid\":%1$}")%((int)(tlv->m_tid))));
                cjson_object js_value(tlv->m_value);
                tlv_cell.add("value",js_value);
                tvl_json.add(tlv_cell);
            }
            else if(data_pro->vtype == 'B') //二进制类型
            {
                char *new_hexstr =new char[tlv->m_value.length()*2+1];
                xbasic::hex_to_str((unsigned char *)(tlv->m_value.data()),new_hexstr,tlv->m_value.length());
                new_hexstr[tlv->m_value.length()*2] ='\0';
                cjson_object tlv_cell(boost::str(boost::format("{\"tid\":%1$,\"value\":\"%2$\"}")%((int)(tlv->m_tid))%(new_hexstr)));
                tvl_json.add(tlv_cell);
                delete [] new_hexstr;
            }
            else //按字符串类型
            {
                cjson_object tlv_cell(boost::str(boost::format("{\"tid\":%1$,\"value\":\"%2$\"}")%((int)(tlv->m_tid))%(tlv->m_value)));
                tvl_json.add(tlv_cell);
            }
        }
        return tvl_json.to_string();
    }

    static bool is_equal(xtvl *tlv1,xtvl *tlv2) //比较两个数据单元是否相等
    {
        return (tlv1->m_tid !=tlv2->m_tid && tlv1->m_value !=tlv2->m_value);
    }

public:
    unsigned short            m_tid;          //数据项
    std::string               m_value;        //数据值
    xdatadef::DATAPRO *       m_data_pro;     //数据属性定义
protected:
    std::vector<std::string>  m_arr_value;    //数组类型的数据值
    boost::shared_mutex       m_mux_arr;      //数组读写锁
};

class xpacket //报文基类
{
public:
    xpacket() {}
    virtual xpacket *clone() {return new xpacket();} //克隆对象
public:
    virtual void reset() {} //重置包
    virtual bool is_empty() {return false;} //是否包空
    virtual bool need_confirm() {return false;} //需要确认并重试的包
    virtual bool type_confirm() {return false;} //是确认包
    virtual int flag_confirm() {return 0;} //获取确认报文的确认标志
    virtual bool is_confirm(xpacket *pack) {return (flag_confirm()==pack->flag_confirm());} //是否是确认的该包
    virtual bool parse_from_bin(unsigned char *pack_data,int pack_len) {return true;} //从BIN解析数据包
    virtual bool parse_from_json(std::string &json_data) {return true;} //从JSON解析数据包
    virtual std::string serial_to_bin() {return std::string("");} //串行化成BIN
    virtual std::string serial_to_json() {return std::string("");} //串行化成JSON
};

class xpackage : public xpacket //报文
{
public:
    {
        enum MSG_TYPE {MT_REQUEST=0x01,MT_NOTIFY=0x02,MT_RESPOND=0x81}; //报文请求码定义
        enum MSG_CMD {MC_NONE=0x00,MC_READ=0x01,MC_WRITE,MC_EXECUTE,MC_HEARTBEAT,MC_ALARM,MC_WARNING,MC_OTA}; //报文命令字定义
        xpackage(MSG_TYPE msg_type=MT_REQUEST,MSG_CMD msg_cmd=MC_NONE,unsigned char version=0,uint32 session=0,unsigned short board=0,unsigned char err=0) : \
            m_msg_type(msg_type),m_msg_cmd(msg_cmd),m_version(version),m_session(session),m_board(board),m_err_code(err)
        {
        }
        xpackage(std::string &json_data) : m_msg_type(MT_REQUEST),m_msg_cmd(MC_NONE),m_version(0),m_session(0),m_board(0),m_err_code(0)
        {
            parse_from_json(json_data);
        }
        virtual xpacket *clone() //克隆对象
        {
            xpackage *new_package =new xpackage(m_msg_type,m_msg_cmd,m_version,m_session,m_board,m_err_code);
            get_tlv_data(new_package->m_list_tlv);
            return new_package;
        }
        virtual ~xpackage() {}
public:
    virtual bool parse_from_json(std::string &json_data) //从JSON解析数据包
    {
        std::string js_root_str(json_data);
        cjson_object js_root(js_root_str);
        if(js_root.is_empty()) return false;
        std::string msg_type =js_root("msg_type");
        std::string msg_cmd =js_root("msg_cmd");
        std::string version =js_root("version");
        std::string session_id =js_root("session_id");
        std::string board_id =js_root("board_id");
        std::string err_code =js_root("err_code");
        cjson_object &js_data =js_root["data"];
        m_msg_type =(MSG_TYPE)(atoi(msg_type.c_str())); //报文类型
        m_msg_cmd =(MSG_CMD)(atoi(msg_cmd.c_str())); //报文命令字
        m_version =(unsigned char)(atoi(version.c_str())); //协议版本号
        m_session =(uint32)(atoi(session_id.c_str())); //用于匹配请求和响应
        m_board =(unsigned short)(atoi(board_id.c_str())); //通讯的单板号
        m_err_code = (unsigned char)(atoi(err_code.c_str())); //错误码
        //printf("[Debug] parse from json: m_msg_type:%d m_msg_cmd:%d session_id:%d : %s\n", m_msg_type, m_msg_cmd, m_session, session_id.c_str());
        boost::mutex::scoped_lock lock(m_mux_tlv);
        xtvl::parse_from_json(js_data,m_list_tlv);
        lock.unlock();
        return true;
    }

    virtual std::string serial_to_json() //串行化成JSON
    {
        cjson_object js_root;
        //printf("[Debug] serial_to_json: m_msg_type:%d m_msg_cmd:%d session_id:%d board_id:%d\n", m_msg_type, m_msg_cmd, m_session, m_board);
        js_root.add("msg_type",(int)(m_msg_type));
        js_root.add("msg_cmd",(int)(m_msg_cmd));
        js_root.add("version",(int)(m_version));
        js_root.add("session_id",(int)(m_session));
        js_root.add("board_id",(int)(m_board));
        js_root.add("err_code",(int)(m_err_code));
        boost::mutex::scoped_lock lock(m_mux_tlv);
        std::string tlv_data =xtvl::serial_to_json(m_list_tlv);
        lock.unlock();
        cjson_object js_data(tlv_data);
        js_root.add("data",js_data);
        return js_root.to_string();
    }

    int add_tlv_data(std::list<boost::shared_ptr<xtvl> > &list_tlv) //将TLV对象数组加入到包
    {
        for(std::list<boost::shared_ptr<xtvl> >::iterator iter=list_tlv.begin(); iter!=list_tlv.end(); iter++)
        {
            add_data(*iter);
        }
        return list_tlv.size();
    }

    int get_tlv_data(std::list<boost::shared_ptr<xtvl> > &list_tlv) //获取TLV对象数组
    {
        boost::mutex::scoped_lock lock(m_mux_tlv);
        for(std::list<boost::shared_ptr<xtvl> >::iterator iter=m_list_tlv.begin();iter!=m_list_tlv.end(); iter++)
        {
            boost::shared_ptr<xtvl> tlv =*iter;
            boost::shared_ptr<xtvl> new_data(tlv->clone());
            list_tlv.push_back(new_data);
        }
        return list_tlv.size();
    }

    void add_data(unsigned short tid,std::string value) //加入tlv数据单元到package
    {
        boost::mutex::scoped_lock lock(m_mux_tlv);
        for(std::list<boost::shared_ptr<xtvl> >::iterator iter=m_list_tlv.begin(); iter!=m_list_tlv.end(); iter++)
        {
            boost::shared_ptr<xtvl> tlv =*iter;
            if(tlv->m_tid ==tid) {tlv->update_data(value); return;} //是需要的数据
        }
        boost::shared_ptr<xtvl> new_data(new xtvl(tid,value.length(),value.data())); //没有找到就新增
        m_list_tlv.push_back(new_data);
    }

    void add_data(boost::shared_ptr<xtvl> tlv_data) //加入tlv数据单元到package
    {
        add_data(tlv_data->m_tid,tlv_data->m_value);
    }

    boost::shared_ptr<xtvl> find_data(unsigned short tid) //从package寻找数据单元
    {
        boost::mutex::scoped_lock lock(m_mux_tlv);
        for(std::list<boost::shared_ptr<xtvl> >::iterator iter=m_list_tlv.begin(); iter!=m_list_tlv.end(); iter++)
        {
            boost::shared_ptr<xtvl> tlv =*iter;
            if(tlv->m_tid ==tid) return tlv;
        }
        return boost::shared_ptr<xtvl>();
    }

    void del_data(unsigned short tid) //从package删除数据单元
    {
        boost::mutex::scoped_lock lock(m_mux_tlv);
        for(std::list<boost::shared_ptr<xtvl> >::iterator iter=m_list_tlv.begin(); iter!=m_list_tlv.end();)
        {
            boost::shared_ptr<xtvl> tlv =*iter;
            if(tlv->m_tid ==tid) m_list_tlv.erase(iter++);
            else iter++;
        }
    }

    virtual void reset()
    {
        m_msg_type=MT_REQUEST;
        m_msg_cmd =MC_NONE;
        m_version =0;
        m_session =0;
        m_board =0;
        m_err_code = 0;
        boost::mutex::scoped_lock lock(m_mux_tlv);
        m_list_tlv.clear();
    }

    virtual bool is_empty() {return (m_msg_cmd==MC_NONE);}
    virtual bool need_confirm() {return (m_msg_type==MT_REQUEST);} //需要确认并重试的包
    virtual bool type_confirm() {return (m_msg_type==MT_RESPOND);} //是确认包
    virtual int flag_confirm() {return m_session;} //获取确认报文的确认标志
public:
    MSG_TYPE m_msg_type; //报文类型
    MSG_CMD m_msg_cmd; //报文命令字
    unsigned char m_version; //协议版本号
    uint32 m_session; //用于匹配请求和响应
    unsigned short m_board; //通讯的单板ID
    unsigned char m_err_code; //错误信息
protected:
    std::list<boost::shared_ptr<xtvl> > m_list_tlv; //TLV数据链表
    boost::mutex m_mux_tlv; //TLV数据锁
};

class xusb_tvl //usb tlv数据单元
{
public:
    xusb_tvl(unsigned short tid,unsigned short length,const void *value,unsigned char cmd =0x81)
    {
        //m_data_pro =xdatadef::get_datapro(tid,cmd);
        m_data_pro = xusb_tvl::get_datapro(tid,cmd);
        set_data(tid,length,value);
    }
    ~xusb_tvl() {}
public:
    xusb_tvl *clone() {return new xusb_tvl(m_tid,m_value.length(),m_value.data());} //克隆一个新对象

    void set_cmd(unsigned char cmd) //设置命令字
    {
        char old_vtype = m_data_pro->vtype;
        //m_data_pro = xdatadef::get_datapro(m_tid,cmd);
        m_data_pro = xusb_tvl::get_datapro(m_tid,cmd);
        if(old_vtype!=m_data_pro->vtype) set_data(m_tid,m_value.length(),m_value.data()); //值类型有改变，重新解析
    }

    void set_data(unsigned short tid,unsigned short length,const void *value) //设置数据
    {
        if(m_tid !=tid)
        {
            m_tid =tid;
            //if(m_data_pro) m_data_pro =xdatadef::get_datapro(m_tid,m_data_pro->cmd);
            if(m_data_pro) m_data_pro = xusb_tvl::get_datapro(tid,m_data_pro->cmd);
        }
        std::string new_value;
        if(value !=NULL) new_value =(length>0?std::string((char *)value,length):std::string((char *)value));
        if(new_value == m_value) return; //值相同
        m_value =new_value;
        //if(m_data_pro->vtype=='S') update_data(new_value); //按数组类型存储
        if(m_data_pro != NULL && m_data_pro->vtype=='S') update_data(new_value); //按数组类型存储
    }

    std::string get_data() {return m_value;}
    int get_data_to_int() {return atoi(m_value.c_str());} //将字符串值格式化为整型

    int update_data(std::string new_value) //将新单元插入到数组中(相同索引的替换)
    {
        int set_num =0;
        std::vector<std::string> vct_cell_new;
        std::string::size_type brackets_s =new_value.find("[");
        std::string::size_type brackets_e =new_value.find("]");
        if(brackets_s!=std::string::npos && brackets_e!=std::string::npos &&brackets_s<brackets_e) //是连续数组结构
        {
            std::vector<std::string> vct_cell_tmp,vct_head;
            if(xbasic::split_string(new_value,"[",&vct_cell_tmp) <2) return 0; //连续数组字符串格式:"a[n]:x1;x2;...xn"
            if(xbasic::split_string(vct_cell_tmp[0],"[",&vct_head) <2) return 0;
            int count_value =xbasic::split_string(vct_cell_tmp[1],";",&vct_cell_new); //项值,多个以";"分隔
            int start_addr =atoi(vct_head[0].c_str()),addr_num =atoi(vct_head[1].c_str()); //起始地址和数据个数
            for(int i=0; i<addr_num && i<count_value; i++)
            {
                set_cell(boost::str(boost::format("%d")%(start_addr+i)),vct_cell_new[i]);
                set_num++;
            }
        }
        else //是离散数组结构
        {
            int count_cell_new =xbasic::split_string(new_value,",",&vct_cell_new); //数组中字符串以逗号分隔
            for(int i=0; i<count_cell_new; i++)
            {
                std::vector<std::string> vct_value;
                int count_value =xbasic::split_string(vct_cell_new[i],":",&vct_value); //单元中项和值以冒号分隔
                if(count_value <=0) continue;
                else if(count_value <=1) set_cell("",vct_value[0]);
                else set_cell(vct_value[0],vct_value[1]);
            }
            set_num++;
        }
        return set_num;
    }

    std::string get_cell_value(std::string cell_name) //在数组中获取指定单元的数据值
    {
        boost::shared_lock<boost::shared_mutex> lock(m_mux_arr); //读锁
        for(unsigned int i=0; i<m_arr_value.size(); i++)
        {
            std::vector<std::string> vct_value;
            int count_value =xbasic::split_string(m_arr_value[i],":",&vct_value); //单元中项和值以冒号分隔
            if(count_value <=0 || vct_value[0]!=cell_name) continue;
            return (count_value>1?vct_value[1]:"");
        }
        return INVA_SDATA;
    }

    int get_all_cells(std::vector<std::string> &arr_value) //获得整个数组的值到容器
    {
        arr_value.clear();
        boost::shared_lock<boost::shared_mutex> lock(m_mux_arr); //读锁
        for(unsigned int i=0; i<m_arr_value.size(); i++) arr_value.push_back(m_arr_value[i]);
        return arr_value.size();
    }

    void set_cell(std::string cell_name,std::string cell_value) //在数组中设置指定单元的数据值
    {
        int ifind =-1;
        std::string cell =(cell_name.length()<0?cell_value:(cell_name+":"+cell_value));
        boost::unique_lock<boost::shared_mutex> lock(m_mux_arr); //写锁
        for(unsigned int i=0; i<m_arr_value.size() && ifind==-1; i++)
        {
            std::vector<std::string> vct_value;
            int count_value =xbasic::split_string(m_arr_value[i],":",&vct_value); //单元中项和值以冒号分隔
            if(count_value <=1)
            {
                if(cell_name.length()==0) ifind =(int)i; //是需要的数据项，待更新
            }
            else if(vct_value[0]==cell_name) //是需要的数据项
            {
                if(vct_value[1]==cell_value) return; //值相同
                else ifind =(int)i; //值不同，待更新
            }
        }
        if(ifind >=0) m_arr_value[ifind] =cell; //更新该值
        else m_arr_value.push_back(cell); //不存在,新增
        m_value =boost::join(m_arr_value,","); //修改总字符串的值
    }

public:
    static xdatadef::DATAPRO * get_datapro(int tid,int cmd = 0x81)
    {
        static xdatadef s_datadef("datadef.dat");
        return s_datadef.get_datapro(tid,cmd);
    }

    static int parse(unsigned char *tlv_data, int data_len, std::list<boost::shared_ptr<xusb_tvl> > &list_tlv, unsigned char cmd)  //将串行TLV数据格式化为TLV对象数组
    {
        for(int iread=0; iread<data_len;)
        {
            unsigned short tid = xbasic::read_bigendian(&tlv_data[iread],2);
            unsigned short length = xbasic::read_bigendian(&tlv_data[iread+2],2);
            //xbasic::debug_output("xusb_tvl::parse tid:%d length:%d\n", tid, length);
            if(iread+4+length > data_len)
            {
                //xbasic::debug_output("xusb_tvl::parse iread:%d length:%d data_len:%d\n", iread, length, data_len);
                return list_tlv.size(); //数据长度错误
            }
            boost::shared_ptr<xusb_tvl> tvl(new xusb_tvl(tid, length, &tlv_data[iread+4],cmd));
            list_tlv.push_back(tvl);
            iread += (4+length);
        }
    
        return list_tlv.size();
    }
    
    static int parse_from_json(cjson_object &tvl_json, std::list<boost::shared_ptr<xusb_tvl> > &list_tlv)  //将JSON数据格式化为TLV对象数组
    {
        int data_size = (tvl_json.is_empty()?0:tvl_json.get_array_size());
        if(data_size < 0) return true;
        for(int i=0; i<data_size; i++)
        {
            cjson_object &js_item = tvl_json[i];
            std::string tid_str = js_item["tid"];
            std::string key_str = js_item["key"];
            std::string value_str = js_item["value"];
            unsigned char tid = (unsigned char)atoi(tid_str.c_str());
            std::string value = (key_str.length()<=0?(value_str):(key_str+":"+value_str));
            boost::shared_ptr<xusb_tvl> tvl_find;
            for(std::list<boost::shared_ptr<xusb_tvl> >::iterator iter=list_tlv.begin(); iter!=list_tlv.end(); iter++) //寻找对应的tlv数据
            {
                boost::shared_ptr<xusb_tvl> tvl = *iter;
                if(tvl->m_tid == tid) {tvl_find = tvl; break;} //是需要的数据
            }
            if(tvl_find != NULL) {tvl_find->update_data(value); continue;}
            boost::shared_ptr<xusb_tvl> new_data(new xusb_tvl(tid,value.length(),value.data())); //没有找到就新增
            list_tlv.push_back(new_data);
        }
        return list_tlv.size();
    }
    
    static std::string serial(std::list<boost::shared_ptr<xusb_tvl> > &list_tlv)  //将TLV对象数组串行化为TLV流数据格式
    {
        unsigned int iwrite=0;
        unsigned char tlv_buff[8192] = {0};
    
        for(std::list<boost::shared_ptr<xusb_tvl> >::iterator iter=list_tlv.begin(); iter!=list_tlv.end(); iter++)
        {
            boost::shared_ptr<xusb_tvl> tvl = *iter;
            if(iwrite + tvl->m_value.length() + 4 > sizeof(tlv_buff)) break;
            unsigned short tid = tvl->m_tid;
            xbasic::write_bigendian(&tlv_buff[iwrite],tid,2);
            xbasic::write_bigendian(&tlv_buff[iwrite+2],tvl->m_value.length(),2);
            if((4 + tvl->m_value.length()) > sizeof(tlv_buff)) break; //缓存空间不足
            if(tvl->m_value.length() > 0) memcpy(&tlv_buff[iwrite+4],tvl->m_value.data(),tvl->m_value.length());
            iwrite += (4 + tvl->m_value.length());
        }
    
        return std::string((char *)tlv_buff,iwrite);
    }
    
    static std::string serial_to_json(std::list<boost::shared_ptr<xusb_tvl> > &list_tlv)  //将TLV对象数组串行化为JSON数据格式
    {
        cjson_object tvl_json("[]");
        for(std::list<boost::shared_ptr<xusb_tvl> >::iterator iter=list_tlv.begin(); iter!=list_tlv.end(); iter++)
        {
            boost::shared_ptr<xusb_tvl> tvl = *iter;
            xdatadef::DATAPRO *data_pro = tvl->m_data_pro;
            if(!data_pro || data_pro->vtype == 'S') //是数组类型
            {
                std::vector<std::string> arr_value;
                int arr_size = tvl->get_all_cells(arr_value); //获得整个数组的值
                for(int i=0; i<arr_size; i++)
                {
                    std::vector<std::string> vct_value;
                    if(xbasic::split_string(arr_value[i],";",&vct_value) <= 0) continue; //单元中项和值以冒号";"分隔
                    std::string cell_str;
                    if(vct_value.size() == 1) cell_str = boost::str(boost::format("{\"tid\":%1%,\"value\":\"%2%\"}")%(int)(tvl->m_tid)%(vct_value[0]));
                    else if(vct_value.size() >= 2) cell_str = boost::str(boost::format("{\"tid\":%1%,\"key\":\"%2%\",\"value\":\"%3%\"}")%(int)(tvl->m_tid)%(vct_value[0])%(vct_value[1]));
                    cjson_object tvl_cell(cell_str);
                    if(!tvl_json.is_empty()) tvl_json.add(tvl_cell);
                }
            }
            else if(data_pro->vtype == 'J') //JSON类型
            {
                cjson_object tvl_cell(boost::str(boost::format("{\"tid\":%1%}")%(int)(tvl->m_tid)));
                cjson_object js_value(tvl->m_value);
                tvl_cell.add("value",js_value);
                tvl_json.add(tvl_cell);
            }
            else if(data_pro->vtype == 'B') //二进制类型
            {
                char *new_hexstr = new char[tvl->m_value.length()*2+1];
                xbasic::hex_to_str((unsigned char *)tvl->m_value.data(),new_hexstr,tvl->m_value.length());
                new_hexstr[tvl->m_value.length()*2] = '\0';
                cjson_object tvl_cell(boost::str(boost::format("{\"tid\":%1%,\"value\":\"%2%\"}")%(int)(tvl->m_tid)%(new_hexstr)));
                tvl_json.add(tvl_cell);
                delete [] new_hexstr;
            }
            else //按字符串类型
            {
                cjson_object tvl_cell(boost::str(boost::format("{\"tid\":%1%,\"value\":\"%2%\"}")%(int)(tvl->m_tid)%(tvl->m_value)));
                tvl_json.add(tvl_cell);
            }
        }
        return tvl_json.to_string();
    }
    
    static bool is_equal(xusb_tvl *tvl1, xusb_tvl *tvl2) //比较两个数据单元是否相等
    {
        return (tvl1->m_tid != tvl2->m_tid && tvl1->m_value != tvl2->m_value);
    }

public:
    unsigned short        m_tid;        //数据项
    std::string           m_value;      //数据值
    xdatadef::DATAPRO *   m_data_pro;   //数据属性定义
protected:
    std::vector<std::string>  m_arr_value;  //数组类型的数据值
    boost::shared_mutex       m_mux_arr;    //数组读写锁
};

#endif
