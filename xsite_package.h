#ifndef XSITE_PACKAGE_H_H
#define XSITE_PACKAGE_H_H

#include "xpackage.hpp"
#include "xbasicasio.hpp"
#include "xconfig.h"
#include <boost/thread/shared_mutex.hpp>
#include <string>

#define SITE_PROTO_VERSION              "1.0"           //协议版本
//消息类型
#define SITE_MT_REQUEST                 "request"       //请求消息
#define SITE_MT_NOTIFY                  "notify"        //指示消息
#define SITE_MT_RESPOND                 "respond"       //应答消息
#define SITE_MT_BROADCAST               "broadcast"     //广播消息
//消息命令
#define SITE_MC_NONE                    ""              //空
#define SITE_MC_HEARTBEAT               "heartbeat"     //心跳
#define SITE_MC_SHELL_CMD               "shell_cmd"     //SHELL命令
#define SITE_MC_INTERNAL_CMD            "internal_cmd"  //内部命令
#define SITE_MC_RDBI_CMD                "rdbi_cmd"      //RDBI库命令
#define SITE_MC_PRIVATE_CMD             "private_cmd"   //私有命令
#define SITE_MC_CALL_FUNCTION           "call_function" //平台函数调用
#define SITE_MC_RDBI_FUNCTION           "rdbi_function" //RDBI函数调用
#define SITE_MC_DEVICE_DATA             "device_data"   //设备数据
#define SITE_MC_HARDWARE_DATA           "hardware_data" //硬件数据
#define SITE_MC_REGISTER                "register"      //注册
#define SITE_MC_CONSOLE_LOG             "console_log"   //控制台日志
#define SITE_MC_RDBI_LOG                "rdbi_log"      //RDBI日志
//消息命令内容
#define SITE_CONT_UPGRADE               "upgrade"       //升级
#define SITE_CONT_READATA               "read_data"     //读取版本信息
#define SITE_CONT_SITEINFO              "site_info"     //上报 siteInfo 消息
#define SITE_CONT_SETUP_SITE            "setup_site"    //设置 site
#define SITE_CONT_REBOOT                "reboot"        //重启请求
#define SITE_CONT_REBOOTCONFIRM         "reboot_confirm"//重启确认

//SiteControl包头标识
#define SITE_HEADER                     0xFEAS
//SiteControl平台包头长度
#define SITE_HEADER_LEN                 6


#define SITE_NAME_ITEM                  "name"
#define SITE_VALUE_ITEM                 "value"
#define SITE_SLOT_TYPE_ITEM             "slot_type"
#define SITE_SLOT_NUM_ITEM              "slot_num"
#define SITE_UPGRADE_STATUS             "upgradestatus"
#define SITE_UPGRADE_RESULT             "upgraderesult"

#define SITE_PROCESSORS_ITEM            "site_processors"
#define SITE_RCPUS_ITEM                 "rcpus"
#define SITE_SATELLITE_PROCESSORS_ITEM  "satellite_processors"
#define SITE_OPTION_RCPUS_ITEM          "option_rcpus"
#define SITE_CHAMBER_CPUS_ITEM          "chamber_cpus"
#define SITE_TESTSITES_ITEM             "testsites"
#define SITE_REPAIRSITES_ITEM           "repairsites"
#define SITE_STATION_ITEM               "stations"

//JSON消息打包器
class sitejson_packer : public xpacker
{
public:
    virtual boost::shared_ptr<const std::string> pack_data(const char *pdata,size_t datalen,PACKWAY pack_way);
};

//JSON消息解包器类
class sitejson_unpacker : public xunpacker
{
public:
    sitejson_unpacker();
    ~sitejson_unpacker();

public:
    virtual void reset_data();
    virtual bool unpack_data(size_t bytes_data, boost::container::list<boost::shared_ptr<const std::string> > & list_pack);
    virtual boost::asio::mutable_buffers_1 prepare_buff(size_t & min_recv_len);
};


//数据单元
class xcell
{
public:
    xcell(std::string name,std::string value);
    //克隆对象
    xcell *clone();

public:
    //数据名称
    std::string     m_name;
    //数据值
    std::string     m_value;
};

class xreturn_cell
{
public:
    xreturn_cell();
    xreturn_cell(std::vector<boost::shared_ptr<xcell> > &cellsvect);
    xreturn_cell* clone();
    int add_cell(std::string name, std::string value);
    int get_all_cell(std::vector<boost::shared_ptr<xcell> > &arr_value);
    int get_cellsize();
    //获取指定键名值
    void set_returncell(std::string name,std::string value_set);
    //获取指定返回值
    std::string get_returncell(std::string name,std::string default_val=INVA_SDATA);

public:
    std::vector<boost::shared_ptr<xcell> >    m_returncell_vect;
    boost::shared_mutex                       m_mux_return;
};


//cmd数据
class xcmd
{
public:
    xcmd(std::string cmd_type,std::string cmd_cont);
    //克隆对象
    xcmd *clone();

public:
    //获取参数的个数
    int get_param_size();
    //获取返回值的个数
    int get_return_size();
    //添加参数
    int add_param(std::string name,std::string value);
    //添加返回值
    int add_return(xreturn_cell &return_cell);
    //获取指定参数
    boost::shared_ptr<xcell> get_param(int index);
    boost::shared_ptr<xreturn_cell> get_return(int index);
    //获取指定参数
    std::string get_param(std::string name,std::string default_val=INVA_SDATA);
    //删除指定参数
    void clear_param(const std::string & name);
    //获取指定参数
    void set_param(std::string name,std::string value_set);
    int get_all_param(std::vector<boost::shared_ptr<xcell> > &arr_value);
    int get_all_return(std::vector<boost::shared_ptr<xreturn_cell> > &arr_value);
    void clear_param();
    void clear_return();
    //组合命令
    std::string compose_cmd();
    //设置 return_info信息
    void add_return_info(const std::string &info);
    //设置 return_code信息
    void add_return_code(const std::string &code);
    //获取 return value, 参数 name 是 json 对象中的 name 字段, 返回的是包含该 name 字段的 json 对象中的 value 字段
    std::string get_return_value(const std::string &name);

public:
    //将JSON数据格式化为cmd对象数组
    static int parse_from_json(cjson_object &cmd_json,std::list<boost::shared_ptr<xcmd> > &list_cmd);
    //将cmd对象数组格式化为JSON数据
    static std::string serial_to_json(std::list<boost::shared_ptr<xcmd> > &list_cmd);

public:
    std::string             m_cmd_type;
    std::string             m_cmd_cont;
protected:
    //命令参数
    std::vector<boost::shared_ptr<xcell> >  m_vct_param;
    //命令参数锁
    boost::shared_mutex                     m_mux_param;

    //命令返回值
    std::vector<boost::shared_ptr<xreturn_cell> >   m_vct_return;
    //命令返回值锁
    boost::shared_mutex                     m_mux_return;
};

//SiteControl报文
class xsite_package : public xpacket
{
public:
    xsite_package();
    xsite_package(std::string msg_type,std::string msg_session,std::string board_id);
    xsite_package(std::string msg_type,std::string msg_session,int msg_seq=0,int err_code=0,std::string board_id="");
    xsite_package(std::string &json_data);
    virtual ~xsite_package();

public:
    //产生消息序号
    int create_msg_seq();

public:
    //克隆对象
    virtual xpacket *clone();
    //从JSON解析数据包
    virtual bool parse_from_json(std::string &json_data);
    //串行化成JSON
    virtual std::string serial_to_json();
    virtual void reset();
    //需要确认并重试的包
    virtual bool need_confirm();
    //是确认包
    virtual bool type_confirm();

public:
    //设置头部信息
    void set_header(std::string msg_type,std::string msg_session,int msg_seq,int err_code);
    //获得命令个数
    int get_cmd_size();
    //将TLV对象数据加入到包
    int add_cmd(std::list<boost::shared_ptr<xcmd> > &list_cmd);
    //获取TLV对象数组
    int get_cmd(std::list<boost::shared_ptr<xcmd> > &list_cmd);
    //加入cmd数据单元到package
    void add_cmd(boost::shared_ptr<xcmd> cmd_data);
    //从package寻找数据单元
    boost::shared_ptr<xcmd> find_cmd(int index);
    //从package删除数据单元
    void del_cmd(xcmd *cmd);
    //从package删除命令单元
    void del_cmd(int index = -1);

public:
    //包序号
    int                     m_msg_seq;
    //错误码
    int                     m_err_code;
    //单板ID
    std::string             m_board_id;
    //消息类型
    std::string             m_msg_type;
    //会话ID
    std::string             m_msg_session;
protected:
    //包时间戳
    time_t                  m_timestamp;
    //cmd数据链表
    std::list<boost::shared_ptr<xcmd> >   m_list_cmd;
    //cmd数据锁
    boost::mutex            m_mux_cmd;
    //会话ID互斥锁
    boost::shared_mutex     m_mux_sid;
};

#endif
