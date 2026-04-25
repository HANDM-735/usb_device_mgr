#ifndef MGR_NETWORK_H
#define MGR_NETWORK_H

#include <string>
#include <list>
#include <boost/array.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include "xconfig.h"
#include "xbasic.hpp"
#include "xbasicmgr.hpp"
#include "xbasicasio.hpp"
#include "xpackage.hpp"
#include "xuspackage.h"
#include "usbadapter_package.h"
#include "xsite_package.h"

#define PORT_ADAPTER        "<adapter>"         //单板通讯端口名称定义
#define PORT_SYNC_CLI       "<synccli>"         //单板通讯端口名称定义
#define PORT_SYNC_SVR       "<syncsvr>"         //单板通讯端口名称定义
#define PORT_PLATFORM       "<platform>"        //平台通讯端口名称定义
#define PORT_TH_MONITOR     "<thmonitor>"       //TH监控板端口名称定义
#define PORT_MF_MONITOR     "<mfmonitor>"       //MF监控板端口名称定义
#define PORT_MGRPLATFORM    "<mgrplatform>"     //平台管理器端口名称定义
#define PORT_SITECONTROL    "<mgrsitecontrol>"  //SiteControl端口名称定义
#define PORT_SITECTRL_CLI   "<sctrlsynccli>"    //SiteControl同步客户端端口名称定义
#define PORT_SITECTRL_SVR   "<sctrlsyncsvr>"    //SiteControl同步服务端端口名称定义

#define PLATFORM_HEADER     0xFEA5  //平台包头标识
#define PLATFORM_HEADER_LEN 6       //平台包头长度
#define PLATFORM_MAXMSG_LEN 32768   //平台包最大包长

class json_packer : public xpacker  //JSON消息打包器类
{
public:
    virtual boost::shared_ptr<const std::string> pack_data(const char *pdata,size_t datalen,PACKWAY pack_way)
    {
        boost::shared_ptr<const std::string> ppack;
        if(NULL == pdata || 0 == datalen) return ppack;
        int totallen = (int)datalen;
        std::string *raw_str = new std::string();
        if(pack_way == PACK_BASIC)
        {
            char header[PLATFORM_HEADER_LEN] = {0};
            xbasic::write_bigendian(header,PLATFORM_HEADER,2);
            xbasic::write_bigendian(header+2,totallen,4);
            raw_str->reserve(totallen + PLATFORM_HEADER_LEN); //申请指定长度空间的字符串
            raw_str->append(header,PLATFORM_HEADER_LEN);
            raw_str->append(pdata, totallen);
        }
        else //按原始数据打包
        {
            raw_str->reserve(totallen); //申请指定长度空间的字符串
            raw_str->append(pdata,totallen);
        }
        ppack.reset(raw_str);
        return ppack;
    }
};

class json_unpacker : public xunpacker  //JSON消息解包器类
{
public:
    json_unpacker() {m_signed_len = (size_t)-1; m_data_len = 0;}
public:
    virtual void reset_data() {m_signed_len = (size_t)-1; m_data_len = 0;}
    virtual bool unpack_data(size_t bytes_data, boost::container::list<boost::shared_ptr<const std::string> >& list_pack)
    {
        m_data_len += bytes_data; //本次收到的字节数
        bool unpack_ok = true;
        char *buff_head = m_raw_buff.begin();
        char *buff_data = buff_head;
        while(unpack_ok)
        {
            if(m_signed_len != (size_t)-1) //包头解析完，现在解析包体
            {
                if(xbasic::read_bigendian(buff_data,2) != PLATFORM_HEADER)
                {
                    //包头错误
                    unpack_ok = false;
                    break;
                }
                size_t pack_len = m_signed_len + PLATFORM_HEADER_LEN;
                if(m_data_len < pack_len)
                {
                    //不够一个包长,退出,在现有基础上继续接收
                    break;
                }

                //将这个包返回上层
                list_pack.push_back(boost::shared_ptr<std::string>(new std::string(buff_data, pack_len)));
                //增加指定偏移继续分析
                std::advance(buff_data,pack_len);
                m_data_len -= pack_len;
                m_signed_len = (size_t)-1;
            }
            else if(m_data_len >=PLATFORM_HEADER_LEN) //已经收到了部分数据
            {
                if(xbasic::read_bigendian(buff_data,2) ==PLATFORM_HEADER)
                {
                    //获得长度标识
                    m_signed_len = (size_t)xbasic::read_bigendian(buff_data+2,4);
                }
                else
                {
                    unpack_ok = false; //包头错误
                }

                if(m_signed_len != (size_t)-1 &&m_signed_len> (PLATFORM_MAXMSG_LEN-1024))
                {
                    unpack_ok = false; //包头中长度错误
                    break;
                }
            }
            else
            {
                //包头都还没有收完，继续收
                break;
            }
        }

        if(!unpack_ok)
        {
            xbasic::debug_bindata("json unpacker error:",buff_data,m_data_len%512);
            //解包错误，复位解包器
            reset_data();
            return unpack_ok;
        }

        if(m_data_len > 0 && buff_data > buff_head) //拷贝剩余断包到缓冲区头部
        {
            for(unsigned int i=0; i<m_data_len; i++) buff_head[i] = buff_data[i];
        }

        return unpack_ok;
    }

    virtual boost::asio::mutable_buffers_1 prepare_buff(size_t& min_recv_len) //准备下一个数据接收缓冲区
    {
        if(m_data_len >= PLATFORM_MAXMSG_LEN)
        {
            reset_data(); //接收缓冲区即将溢出
        }
        if(m_data_len >= PLATFORM_HEADER_LEN) //已经收到了部分数据
        {
            char *next_buff = m_raw_buff.begin();
            if(xbasic::read_bigendian(next_buff,2) == PLATFORM_HEADER) //包头正确
            {
                min_recv_len = (m_signed_len == (size_t)-1? PLATFORM_HEADER_LEN-m_data_len:m_signed_len);
            }
        }
        else //还没有收到数据
        {
            min_recv_len = PLATFORM_HEADER_LEN-m_data_len;
        }

        if(min_recv_len == (size_t)-1 || min_recv_len > (PLATFORM_MAXMSG_LEN-1024))
        {
            reset_data();
            min_recv_len = PLATFORM_HEADER_LEN;
        }

        return boost::asio::buffer(boost::asio::buffer(m_raw_buff) +m_data_len); //使用mutable_buffer能防止接受缓冲区溢出
    }
private:
    boost::array<char, PLATFORM_MAXMSG_LEN>    m_raw_buff;     //socket缓冲区
    size_t                                     m_signed_len;   //数据包头中标识的长度，-1标识还没有收到包头
    size_t                                     m_data_len;     //已经收到的数据的长度(包括头)
};

//网络管理器
class mgr_network : public xmgr_basic<mgr_network>
{
public:
    mgr_network();
    ~mgr_network();

public:
    //设置设备服务器的绑定地址
    void set_adapter_addr(std::string devsvr_addr);
    int send_to_adapter(std::string cli_port,usbadapter_package *pack);
    //添加监听器
    int add_listener(xlistener *new_listener);
    //移除监听器
    void del_listener(xlistener *del_listener);
    //告知监听器
    int tell_listener(NET_MSG msg_type,const char *port_name,xpacket *packet);
    int add_port(int board_id, std::string port);
    void del_port(std::string port);
    std::string find_port(int board_id);

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
    void set_sync_cli_addr(std::string synccli_addr);
    void send_heartbeat_tosync();
    int send_to_sync_svr(xuspackage *pack);

    //设置sitectrl sync客户端连接的服务器地址
    void set_sitctrlsync_cli_addr(std::string synccli_addr);
    //向sitectrl sync的服务端发送心跳包
    void send_heartbeat_to_sitctrlsync();
    //向sitectrl sync的服务端发送注册包
    int send_register_to_sitctrlsync();
    //向sitectrl sync的服务端发送消息
    int send_to_sitectrl_syncsvr(xsite_package* pack);
    boost::shared_ptr<xsite_package> make_sitctrlsync_register_pack();

#elif defined(SYNC_BUILD)
    //sync同步服务器地址
    void set_sync_svr_addr(std::string syncsvr_addr);
    //设置sitectrl sync服务端连接的服务器地址
    void set_sitctrlsync_svr_addr(std::string syncsvr_addr);
    //设置平台服务器地址
    void set_platform_addr(std::string platform_addr);
    //设置TH监控板的服务器地址
    void set_thmonitor_board_addr(std::string th_addr);
    //设置MF监控板的服务器地址
    void set_mfmonitor_board_addr(std::string mf_addr);
    //设置sitecontrol平台服务器地址
    void set_sitecontrol_addr(std::string sitectrl_addr);
    //向平台发送注册消息
    void send_register_toplatform();
    //向平台发送心跳包
    void send_heartbeat_toplatform();
    //向TH监控板发送心跳包
    void send_heartbeat_tothmonitor();
    //向MF监控板发送心跳包
    void send_heartbeat_tomfmonitor();
    //向SiteControl平台发送心跳
    void send_heartbeat_tositectrl();
    //发送SiteControl平台注册包
    int send_register_tositectrl();
    //构造SiteControl平台注册包
    boost::shared_ptr<xsite_package> make_register_pack();

    //向TH板发送数据包
    int send_to_thmonitor_board(xuspackage *pack);
    //向MF板发送数据包
    int send_to_mfmonitor_board(xuspackage *pack);
    //向平台发送数据包
    int send_to_platform(xpackage *pack);
    //发送SiteControl消息
    int send_to_sitectrl(xsite_package *pack);
    //向sitectrl sync的客户端发送消息
    int send_to_sitectrl_synccli(std::string sync_cli_addr,xsite_package* pack);
    //向所有sitectrl sync的客户端发送消息
    int send_to_allsitectrl_synccli(xsite_package* pack);
    //向所有 sync 的客户端发送消息
    int send_to_all_sync(xuspackage *pack);
    //向特定的客户端发送消息
    int send_to_dst_board(std::string cli_port, xuspackage *pack);
#endif

public:
    //初始化
    virtual void init();
    //工作函数
    virtual void work(unsigned long ticket);
    //关闭连接
    void close_connection();

protected:
    //TCP服务端回调函数
    void on_tcp_svr_msg(xtcp_server *svr,xtcp_client_node *cli,xtcp_client_node::CBKTCPMSG tcp_msg,char *pdata,int data_len);
    //TCP客户端回调函数
    void on_tcp_cli_msg(xtcp_client *cli,xtcp_client::CBKTCPMSG tcp_msg,char *pdata,int data_len);

private:
    //监听器集合
    std::list<xlistener *>                     m_lst_listener;
    //监听器集合读写锁
    boost::shared_mutex                        m_mux_listener;
    //端口集合读写锁
    boost::shared_mutex                        m_mux_port;
    //即端口集合,保存每个tcpclient单板的端口信息
    boost::unordered_map<int,std::string>      m_map_port;

protected:
    //系统配置
    xconfig                                   *m_sys_config;
    //测试程序lib库服务端
    boost::shared_ptr<xtcp_server>             m_adapter_svr;

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
    //pgb板连接sync板同步pgb板数据的client
    boost::shared_ptr<xtcp_client>             m_sync_cli;
    //sitectrl sync同步的客户端
    boost::shared_ptr<xtcp_client>             m_sitctrl_synccli;

#elif defined(SYNC_BUILD)
    //TH MF监控板心跳间隔时间
    int                                        monitor_interval;
    //TH MF监控板心跳超时时间
    int                                        monitor_timeout;

    //接收pgb板同步数据的sync板服务端
    boost::shared_ptr<xtcp_server>             m_sync_svr;
    //sitectrl sync服务端
    boost::shared_ptr<xtcp_server>             m_sitctrlsync_svr;

    //平台客户端
    boost::shared_ptr<xtcp_client>             m_platform_cli;
    boost::atomic<int>                         m_register_sucess;
    //TH监控板客户端
    boost::shared_ptr<xtcp_client>             m_th_monitor_cli;
    //MF监控板客户端
    boost::shared_ptr<xtcp_client>             m_mf_monitor_cli;
    //SiteControl平台客户端
    boost::shared_ptr<xtcp_client>             m_sitectrl_cli;
#endif
};

#endif
