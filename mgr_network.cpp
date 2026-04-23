#include "mgr_network.h"
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include "xcrypto.hpp"
#include "xpackage.hpp"
#include "xconfig.h"
#include "mgr_platform_tid_def.h"
#include "mgr_log.h"

mgr_network::mgr_network()
{
    m_sys_config = xconfig::get_instance();

    m_adapter_svr.reset(new xtcp_server);
    m_adapter_svr->set_callback(std::bind(&mgr_network::on_tcp_svr_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)); //设置回调函数

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
    m_sync_cli.reset(new xtcp_client);
    m_sync_cli->set_callback(std::bind(&mgr_network::on_tcp_cli_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); //设置回调函数
    m_sync_cli->set_packer_unpacker(new usb_bin_packer, new usb_bin_unpacker); //重定义编解码器

    m_sitctrl_synccli.reset(new xtcp_client);
    m_sitctrl_synccli->set_callback(std::bind(&mgr_network::on_tcp_cli_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); //设置回调函数
    m_sitctrl_synccli->set_packer_unpacker(new sitejson_packer, new sitejson_unpacker); //重定义编解码器

#elif defined(SYNC_BUILD)
    m_sync_svr.reset(new xtcp_server);
    m_sync_svr->set_callback(std::bind(&mgr_network::on_tcp_svr_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)); //设置回调函数
    m_sitctrlsync_svr.reset(new xtcp_server);
    m_sitctrlsync_svr->set_callback(std::bind(&mgr_network::on_tcp_svr_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)); //设置回调函数

    m_platform_cli.reset(new xtcp_client);
    m_platform_cli->set_callback(std::bind(&mgr_network::on_tcp_cli_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); //设置回调函数
    m_platform_cli->set_packer_unpacker(new json_packer, new json_unpacker); //重定义编解码器
    m_register_sucess = 0;

    if(m_sys_config->get_data("monitor_enable") == std::string("yes"))
    {
        m_th_monitor_cli.reset(new xtcp_client);
        m_th_monitor_cli->set_callback(std::bind(&mgr_network::on_tcp_cli_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); //设置回调函数
        m_th_monitor_cli->set_packer_unpacker(new usb_bin_packer, new usb_bin_unpacker); //重定义编解码器
        m_mf_monitor_cli.reset(new xtcp_client);
        m_mf_monitor_cli->set_callback(std::bind(&mgr_network::on_tcp_cli_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); //设置回调函数
        m_mf_monitor_cli->set_packer_unpacker(new usb_bin_packer, new usb_bin_unpacker); //重定义编解码器
    }

    m_sitctrl_cli.reset(new xtcp_client);
    m_sitctrl_cli->set_callback(std::bind(&mgr_network::on_tcp_cli_msg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); //设置回调函数
    m_sitctrl_cli->set_packer_unpacker(new sitejson_packer, new sitejson_unpacker); //重定义编解码器

#endif
}

mgr_network::~mgr_network()
{
    //close_connection();
}

//关闭连接
void mgr_network::close_connection()
{
    m_adapter_svr->stop_service();

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
    m_sync_cli->close_socket();
    m_sitctrl_synccli->close_socket();

#elif defined(SYNC_BUILD)
    m_sync_svr->stop_service();
    m_sitctrlsync_svr->stop_service();

    m_platform_cli->close_socket();

    if(m_sys_config->get_data("monitor_enable") == std::string("yes"))
    {
        m_th_monitor_cli->close_socket();
        m_mf_monitor_cli->close_socket();
    }

    m_sitctrl_cli->close_socket();

#endif

    if (!m_map_port.empty()) {
        m_map_port.clear();
    }
}

void mgr_network::init() //初始化
{
    m_sys_config = xconfig::get_instance();
#if defined(SYNC_BUILD)
    if(m_sys_config->get_data("monitor_enable") == std::string("yes"))
    {
        monitor_interval = m_sys_config->get_data("monitor_interval",15);
        monitor_timeout = m_sys_config->get_data("monitor_timeout",30);
    }
#endif
}

//工作函数
void mgr_network::work(unsigned long ticket)
{
    //LOG_MSG(MSG_LOG,"Enter mgr_network::work()");

    //服务器没有运行
    if(m_adapter_svr->is_stoped()) m_adapter_svr->start_service();

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
    if(!m_sync_cli->is_connected())
    {
        //sync客户端还没有连接
        m_sync_cli->start_connect();
        //此处无法判断是否连接成功，将发送心跳包放置在连接成功之后，此函数中on_tcp_cli_msg
        //send_heartbeat_toplatform();
        LOG_MSG(MSG_LOG,"mgr_network::work() sync client start connect");
    }
    else
    {
        //sync客户端已经连接
        //最后接收数据时间与当前间隔秒数
        long tm_recv_interval = (long)abs(time(NULL) - m_sync_cli->get_last_recv_tm());
        if(tm_recv_interval > 30) //心跳超时
        {
            LOG_MSG(MSG_LOG,"mgr_network::work() sync client heartbeat timeout tm_recv_interval=%ld", tm_recv_interval);
            m_sync_cli->disconnect();
        }
        else if((ticket % 15) == 0)
        {
            //sync client心跳即将超时
            send_heartbeat_tosync();
        }
    }

    //sitctrl sync客户端还没有连接
    if(!m_sitctrl_synccli->is_connected())
    {
        m_sitctrl_synccli->start_connect();
    }
    else
    {
        //sitctrl sync客户端已经连接
        //最后接收数据时间与当前间隔秒数
        long tm_recv_interval = (long)abs(time(NULL) - m_sitctrl_synccli->get_last_recv_tm());
        if(tm_recv_interval > 30) //心跳超时
        {
            LOG_MSG(MSG_LOG,"mgr_network::work() sitectrl sync client heartbeat timeout tm_recv_interval=%ld", tm_recv_interval);
            m_sitctrl_synccli->disconnect();
        }
        else if(ticket % 15 == 0)
        {
            //sitctrl sync心跳即将超时
            send_heartbeat_to_sitctrlsync();
        }
    }

#elif defined(SYNC_BUILD)
    //服务器没有运行
    if(m_sync_svr->is_stoped()) m_sync_svr->start_service();
    //服务器没有运行
    if(m_sitctrlsync_svr->is_stoped()) m_sitctrlsync_svr->start_service();

    //平台客户端还没有连接
    if(!m_platform_cli->is_connected())
    {
        m_platform_cli->start_connect();
        //此处无法判断是否连接成功，将发送心跳包放置在连接成功之后，此函数中on_tcp_cli_msg
        //send_heartbeat_toplatform();
        LOG_MSG(MSG_LOG,"mgr_network::work() platform client start connect");
    }
    else
    {
        //平台客户端已经连接
        long curr_time = time(NULL);
        long last_recv_time = m_platform_cli->get_last_recv_tm();
        //最后接收数据时间与当前间隔秒数
        long tm_recv_interval = (long)abs(curr_time-last_recv_time);
        LOG_MSG(MSG_LOG,"mgr_network::work() platform client curr_time=%ld last_recv_time=%ld tm_recv_interval=%ld.",curr_time,last_recv_time,tm_recv_interval);
        if(tm_recv_interval > 30) //心跳超时
        {
            LOG_MSG(MSG_LOG,"mgr_network::work() platform client heartbeat timeout tm_recv_interval=%ld", tm_recv_interval);
            m_platform_cli->disconnect();
            //重置上次接收时间，防止当client一直不给心跳，心跳频率退化成1s一次
            m_platform_cli->reset_last_recv_tm(curr_time);
        }
        else if((ticket % 15) == 0)
        {
            //platform client心跳即将超时
            send_register_toplatform();
            send_heartbeat_toplatform();
        }
    }

    if(m_sys_config->get_data("monitor_enable") == std::string("yes"))
    {
        //TH监控板客户端还没有连接
        if(!m_th_monitor_cli->is_connected())
        {
            m_th_monitor_cli->start_connect();
            //连接成功时，就发送心跳包
            send_heartbeat_tothmonitor();
            LOG_MSG(MSG_LOG,"mgr_network::work() TH board client start connect");
        }
        else
        {
            //TH监控板客户端已经连接
            long curr_time = time(NULL);
            long last_recv_time = m_th_monitor_cli->get_last_recv_tm();
            //最后接收数据时间与当前间隔秒数
            long tm_recv_interval = (long)abs(curr_time-last_recv_time);
            LOG_MSG(MSG_LOG,"mgr_network::work() TH board curr_time=%ld last_recv_time=%ld tm_recv_interval=%ld.",curr_time,last_recv_time,tm_recv_interval);

            if(tm_recv_interval > monitor_timeout)
            {
                //心跳超时
                LOG_MSG(WRN_LOG,"mgr_network::work() TH board heartbeat timeout tm_recv_interval=%ld", tm_recv_interval);
                m_th_monitor_cli->disconnect();
                //重置上次接收时间，防止当client一直不给心跳，心跳频率退化成1s一次
                m_th_monitor_cli->reset_last_recv_tm(curr_time);
            }
            else if((ticket % monitor_interval) == 0)
            {
                //TH监控板心跳即将超时
                send_heartbeat_tothmonitor();
            }
        }

        //MF监控板客户端还没有连接
        if(!m_mf_monitor_cli->is_connected())
        {
            m_mf_monitor_cli->start_connect();
            //连接成功时，就发送心跳包
            send_heartbeat_tomfmonitor();
            LOG_MSG(MSG_LOG,"mgr_network::work() MF board client start connect");
        }
        else //MF监控板客户端已经连接
        {
            long curr_time = time(NULL);
            long last_recv_time = m_mf_monitor_cli->get_last_recv_tm(); //最后接收数据时间与当前间隔秒数
            long tm_recv_interval = (long)abs(curr_time-last_recv_time); //最后接收数据时间与当前间隔秒数
            LOG_MSG(MSG_LOG,"mgr_network::work() MF board curr_time=%ld last_recv_time=%ld tm_recv_interval=%ld.",curr_time,last_recv_time,tm_recv_interval);

            if(tm_recv_interval > monitor_timeout) //心跳超时
            {
                LOG_MSG(WRN_LOG,"mgr_network::work() MF board heartbeat timeout tm_recv_interval=%ld", tm_recv_interval);
                m_mf_monitor_cli->disconnect();
                //重置上次接收时间，防止当client一直不给心跳，心跳频率退化成1s一次
                m_mf_monitor_cli->reset_last_recv_tm(curr_time);
            }
            else if((ticket % monitor_interval) == 0) //MF监控板心跳即将超时
            {
                send_heartbeat_tomfmonitor();
            }
        }
    }

    //SiteControl客户端还没有连接
    if(!m_sitctrl_cli->is_connected())
    {
        m_sitctrl_cli->start_connect();
        LOG_MSG(MSG_LOG,"mgr_network::work() sitecontrol client start connect");
    }
    else
    {
        //SiteControl客户端已经连接
        long curr_time = time(NULL);
        long last_recv_time = m_sitctrl_cli->get_last_recv_tm(); //最后接收数据时间与当前间隔秒数
        long tm_recv_interval = (long)abs(curr_time-last_recv_time);
        LOG_MSG(MSG_LOG,"mgr_network::work() sitecontrol client curr_time=%ld last_recv_time=%ld tm_recv_interval=%ld.",curr_time,last_recv_time,tm_recv_interval);

        if(tm_recv_interval > 30)
        {
            //心跳超时
            //LOG_MSG(MSG_LOG,"mgr_network::work() sitecontrol client heartbeat timeout tm_recv_interval=%ld", tm_recv_interval);
            LOG_MSG(WRN_LOG,"mgr_network::work() sitecontrol client heartbeat timeout tm_recv_interval=%ld", tm_recv_interval);
            m_sitctrl_cli->disconnect();
            //重置上次接收时间，防止当client一直不给心跳，心跳频率退化成1s一次
            m_sitctrl_cli->reset_last_recv_tm(curr_time);
        }
        else if((ticket % 15) == 0)
        {
            //SiteControl心跳即将超时
            send_heartbeat_tositectrl();
        }
    }

#endif
    //LOG_MSG(MSG_LOG,"Exited mgr_network::work()");
}

void mgr_network::on_tcp_cli_msg(xtcp_client *cli,xtcp_client::CBKTCPMSG tcp_msg,char *pdata,int data_len)
{
    if(tcp_msg == xtcp_client::TCP_DATA)
    {

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
        if(cli == m_sync_cli.get())
        {
            //sync客户端
            xusbpackage pack((unsigned char *)pdata,data_len);
            tell_listener(NET_DATA,PORT_SYNC_CLI,&pack); //回调
        }
        else if(cli == m_sitctrl_synccli.get())
        {
            //sitctrl sync 客户端
            int len_js = xbasic::read_bigendian(pdata+2,4);
            if(len_js <= 0 || len_js > 1024*1024) return; //JSON长度不对
            std::string msg_cont(pdata+6,len_js);
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_cli_msg() sitecontrol sync xpackage %s", msg_cont.c_str());
            xsite_package pack(msg_cont);
            tell_listener(NET_DATA,PORT_SITECTRL_CLI,&pack); //回调
        }
        else
        {
            //nothing to do
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() cli tcp data is invaild");
        }

#elif defined(SYNC_BUILD)
        if(cli==m_platform_cli.get()) //平台端
        {
            int len_js = xbasic::read_bigendian(pdata+2,4);
            if(len_js <=0 || len_js >1024*1024) return; //JSON长度不对
            std::string msg_cont(pdata+6,len_js);
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_cli_msg() platform xpackage %s", msg_cont.c_str());
            xpackage pack(msg_cont);
            tell_listener(NET_DATA,PORT_PLATFROM,&pack); //回调
        }
        else if(cli == m_th_monitor_cli.get())
        {
            //TH监控板客户端
            xusbpackage pack((unsigned char *)pdata,data_len);
            tell_listener(NET_DATA,PORT_TH_MONITOR,&pack); //回调
        }
        else if(cli == m_mf_monitor_cli.get())
        {
            //MF监控板客户端
            xusbpackage pack((unsigned char *)pdata,data_len);
            tell_listener(NET_DATA,PORT_MF_MONITOR,&pack);
        }
        else if(cli == m_sitctrl_cli.get())
        {
            //SiteControl客户端
            int len_js = xbasic::read_bigendian(pdata+2,4);
            if(len_js <= 0 || len_js > 1024*1024) return; //JSON长度不对
            std::string msg_cont(pdata+6,len_js);
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_cli_msg() sitecontrol xpackage %s", msg_cont.c_str());
            xsite_package pack(msg_cont);
            tell_listener(NET_DATA,PORT_SITECONTROL,&pack); //回调
        }
        else
        {
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() cli tcp data is invaild");
        }
#endif
    }
    else if(tcp_msg == xtcp_client::TCP_CONNECTED)     //连接建立
    {
        if(xbasic::read_littleendian(pdata,sizeof(int)) != 0) return; //连接错误

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
        if(cli == m_sync_cli.get())
        {
            //sync客户端
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_cli_msg() m_sync_cli connected !!!");
            send_heartbeat_tosync();
            tell_listener(NET_CONNECTED,PORT_SYNC_CLI,NULL); //回调
        }
        else if(cli == m_sitctrl_synccli.get())
        {
            //sitectrl sync客户端
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_cli_msg() m_sitctrl_synccli connected !!!");
            send_heartbeat_to_sitctrlsync();
            tell_listener(NET_CONNECTED,PORT_SITECTRL_CLI,NULL); //回调
        }
        else
        {
            //nothing to do
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() cli tcp connect is invaild");
        }

#elif defined(SYNC_BUILD)
        if(cli==m_platform_cli.get()) //平台端
        {
            // 此处发送心跳包并告知监听
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_cli_msg() m_platform_cli connected !!!");
            send_register_toplatform();
            send_heartbeat_toplatform();
            tell_listener(NET_CONNECTED,PORT_PLATFROM,NULL); //回调
        }
        else if(cli == m_th_monitor_cli.get())
        {
            //TH监控板客户端
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() TH Board connected success!!!");
            tell_listener(NET_CONNECTED,PORT_TH_MONITOR,NULL); //回调
        }
        else if(cli == m_mf_monitor_cli.get())
        {
            //MF监控板客户端
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() MF Board connected success!!!");
            tell_listener(NET_CONNECTED,PORT_MF_MONITOR,NULL); //回调
        }
        else if(cli == m_sitctrl_cli.get())
        {
            //MF监控板客户端
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() SiteControl connected success!!!");
            tell_listener(NET_CONNECTED,PORT_SITECONTROL,NULL); //回调
        }
        else
        {
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() cli tcp connect is invaild");
        }
#endif
    }
    else if(tcp_msg == xtcp_client::TCP_DISCONNECTED) //连接断开
    {
        LOG_MSG(MSG_LOG,"mgr_network::on_tcp_cli_msg() xtcp_client disconnected !!!");

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
        if(cli == m_sync_cli.get())
        {
            //sync客户端
            m_sync_cli->disconnect();
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() sync client disconnected!!!");
            tell_listener(NET_DISCONNECTED,PORT_SYNC_CLI, NULL); //回调
        }
        else if(cli == m_sitctrl_synccli.get())
        {
            //sitectrl sync客户端
            m_sitctrl_synccli->disconnect();
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() sitectrl sync client disconnected!!!");
            tell_listener(NET_DISCONNECTED,PORT_SITECTRL_CLI, NULL); //回调
        }
        else
        {
            //nothing to do
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() cli tcp disconnect is invaild");
        }

#elif defined(SYNC_BUILD)
        if(cli == m_platform_cli.get())
        {
            //平台客户端
            m_platform_cli->disconnect();
            m_register_sucess = 0;
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() platform client disconnected!!!");
            tell_listener(NET_DISCONNECTED,PORT_PLATFROM, NULL); //回调
        }
        else if(cli == m_th_monitor_cli.get())
        {
            //TH监控板客户端
            m_th_monitor_cli->disconnect();
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() TH Board disconnected!!!");
            tell_listener(NET_DISCONNECTED,PORT_TH_MONITOR,NULL); //回调
        }
        else if(cli == m_mf_monitor_cli.get())
        {
            //MF监控板客户端
            m_mf_monitor_cli->disconnect();
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() MF Board disconnected!!!");
            tell_listener(NET_DISCONNECTED,PORT_MF_MONITOR,NULL); //回调
        }
        else if(cli == m_sitctrl_cli.get())
        {
            //MF监控板客户端
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() SiteControl disconnected!!!");
            tell_listener(NET_DISCONNECTED,PORT_SITECONTROL,NULL); //回调
        }
        else
        {
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_cli_msg() cli tcp disconnect is invaild");
        }
#endif
    }
}

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)

void mgr_network::set_sync_cli_addr(std::string synccli_addr) //设置sync板的服务器地址
{
    std::vector<std::string> vct_addr;
    LOG_MSG(WRN_LOG,"mgr_network::set_sync_cli_addr() synccli_addr=%s", synccli_addr.c_str());
    if(xbasic::split_string(synccli_addr,":",&vct_addr) < 2) return ; //不是地址格式
    m_sync_cli->set_server_addr(vct_addr[0].c_str(),atoi(vct_addr[1].c_str()));
}

void mgr_network::send_heartbeat_tosync() //向sync板发送心跳包
{
    //1、构造心跳包
    xusbpackage pack(xusbpackage::MC_HEARTBEAT,PROTO_VERSION,0,xconfig::self_id(),0);
    //2、发送心跳包
    send_to_sync_svr(&pack);
}

int mgr_network::send_to_sync_svr(xusbpackage *pack)
{
    if(m_sync_cli == NULL)
    {
        LOG_MSG(WRN_LOG, "mgr_network::send_to_sync_svr() m_sync_cli is null");
        return -1;
    }
    std::string pack_data = pack->serial_to_bin(); //将包串行化
    char print_buff[4096] = {0};
    xbasic::hex_to_str((unsigned char *)pack_data.data(),print_buff,pack_data.length()>2000?2000:pack_data.length());
    LOG_MSG(WRN_LOG,"mgr_network::send_to_sync_svr() cmd:%d:%s", pack->m_msg_cmd, print_buff);
    return m_sync_cli->send_data(pack_data.data(),pack_data.length());
}

//设置sitectrl sync客户端连接的服务器地址
void mgr_network::set_sitctrlsync_cli_addr(std::string synccli_addr)
{
    std::vector<std::string> vct_addr;
    LOG_MSG(WRN_LOG,"mgr_network::set_sitctrlsync_cli_addr() synccli_addr=%s", synccli_addr.c_str());
    if(xbasic::split_string(synccli_addr,":",&vct_addr) < 2) return ; //不是地址格式
    m_sitctrl_synccli->set_server_addr(vct_addr[0].c_str(),atoi(vct_addr[1].c_str()));
}

//向sitectrl sync的服务端发送心跳包
void mgr_network::send_heartbeat_to_sitctrlsync()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_heartbeat_to_sitctrlsync()");

    //构造心跳请求包
    int len = 0;
    std::string site_id = std::to_string(xconfig::self_id());
    xsite_package pack(SITE_MT_REQUEST,"",site_id);
    boost::shared_ptr<xcmd> cmd(new xcmd(SITE_MC_HEARTBEAT,""));
    pack.add_cmd(cmd);

    //发送心跳包
    len = send_to_sitectrl_syncsvr(&pack);

    LOG_MSG(MSG_LOG, "Exited mgr_network::send_heartbeat_to_sitctrlsync() len=%d",len);
    return ;
}

//向sitectrl sync的服务端发送注册包
int mgr_network::send_register_to_sitctrlsync()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_register_to_sitctrlsync()");

    //构造注册请求包
    int len = 0;
    boost::shared_ptr<xsite_package> pack = make_sitctrlsync_register_pack();

    //发送注册包
    len = send_to_sitectrl_syncsvr(pack.get());

    LOG_MSG(MSG_LOG, "Exited mgr_network::send_register_to_sitctrlsync() len=%d",len);
    return len;
}

//向sitectrl sync的服务端发送消息
int mgr_network::send_to_sitectrl_syncsvr(xsite_package* pack)
{
    std::string pack_data = pack->serial_to_json(); //将包串行化
    LOG_MSG(WRN_LOG,"mgr_network::send_to_sitectrl_syncsvr() pack_data :%s",pack_data.c_str());
    return m_sitctrl_synccli->send_data(pack_data.data(),pack_data.length());
}

//构造site sync同步注册包
boost::shared_ptr<xsite_package> mgr_network::make_sitctrlsync_register_pack()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::make_sitctrlsync_register_pack()");

    std::string site_id = std::to_string(xconfig::self_id());
    std::string timeout = std::to_string(30);
    boost::shared_ptr<xsite_package> pack(new xsite_package(SITE_MT_REQUEST,"",site_id));
    boost::shared_ptr<xcmd> cmd(new xcmd(SITE_MC_REGISTER,""));
    cmd->add_param("proto_version",SITE_PROTO_VERSION);
    cmd->add_param("heartbeat_timeout",timeout);
    pack->add_cmd(cmd);

    LOG_MSG(MSG_LOG, "Exited mgr_network::make_sitctrlsync_register_pack()");
    return pack;
}

#elif defined(SYNC_BUILD)

//设置平台的服务器地址
void mgr_network::set_platform_addr(std::string platform_addr)
{
    std::vector<std::string> vct_addr;
    LOG_MSG(WRN_LOG,"mgr_network::set_platform_addr() plat_addr=%s",platform_addr.c_str());
    if(xbasic::split_string(platform_addr,":",&vct_addr) <2) return ; //不是地址格式
    m_platform_cli->set_server_addr(vct_addr[0].c_str(),atoi(vct_addr[1].c_str()));
}

//向平台发送注册消息
void mgr_network::send_register_toplatform()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_network::send_register_toplatform()");

    std::string monitor_enable = m_sys_config->get_data("monitor_enable");
    std::string soft_ver = std::to_string(1)+ std::string(".") + std::string(STR(SOFT_VERSION));

    if(m_register_sucess == 0)
    {
        xpackage pack(xpackage::MT_NOTIFY,xpackage::MC_READ,PROTO_VERSION,0,xconfig::site_id());
        if(monitor_enable == std::string("yes"))
        {
            int th_id = m_sys_config->get_data("th_id",64);
            int mf_id = m_sys_config->get_data("mf_id",79);
            LOG_MSG(MSG_LOG,"mgr_network::send_register_toplatform() soft_ver=%s mf_id=%d th_id=%d",soft_ver.c_str(),mf_id,th_id);

            std::string mf_boardid = std::to_string(2)+ std::string(".") + std::string(mf_id);
            std::string th_boardid = std::to_string(3)+ std::string(".") + std::string(th_id);

            boost::shared_ptr<xtlv> key1_data(new xtlv(BOARD_REGISTER_SOFT_VER,soft_ver.length(),soft_ver.data(),true));
            boost::shared_ptr<xtlv> key2_data(new xtlv(BOARD_REGISTER_SOFT_VER,mf_boardid.length(),mf_boardid.data(),true));
            boost::shared_ptr<xtlv> key3_data(new xtlv(BOARD_REGISTER_SOFT_VER,th_boardid.length(),th_boardid.data(),true));

            list_tlv.push_back(key1_data);
            list_tlv.push_back(key2_data);
            list_tlv.push_back(key3_data);

            pack.add_tlv_data(list_tlv);
        }
        else
        {
            // 软件版本
            pack.add_data(BOARD_REGISTER_SOFT_VER, soft_ver);
        }

        int len = send_to_platform(&pack);
        if (-1 == len)
        {
            LOG_MSG(ERR_LOG,"mgr_network::send_register_toplatform() monitor_enable=%s soft_ver=%s len=%d send register message failed!",soft_ver.c_str(),monitor_enable.c_str(),len);
        }
        else
        {
            m_register_sucess = 1;
            LOG_MSG(WRN_LOG,"mgr_network::send_register_toplatform() monitor_enable=%s soft_ver=%s len=%d m_register_success=%d send register message success!",soft_ver.c_str(),monitor_enable.c_str(),len,m_register_sucess.load());
        }
    }
    LOG_MSG(MSG_LOG,"Exited mgr_network::send_register_toplatform() monitor_enable=%s soft_ver=%s m_register_success=%d",monitor_enable.c_str(),soft_ver.c_str(),m_register_sucess.load());
}

//向平台发送心跳包
void mgr_network::send_heartbeat_toplatform()
{
    // 1、构造心跳包
    xpackage pack(xpackage::MT_REQUEST,xpackage::MC_HEARTBEAT,PROTO_VERSION,0,xconfig::site_id()); //构造心跳请求包
    // 2、发送心跳包
    send_to_platform(&pack);
}

int mgr_network::send_to_platform(xpackage *pack)
{
    std::string pack_data = pack->serial_to_json(); //将包串行化
    LOG_MSG(WRN_LOG,"mgr_network::send_to_platform() pack_data :%s",pack_data.c_str());
    return m_platform_cli->send_data(pack_data.data(),pack_data.length());
}

//发送SiteControl消息
int mgr_network::send_to_sitectrl(xsite_package *pack)
{
    std::string pack_data = pack->serial_to_json(); //将包串行化
    LOG_MSG(WRN_LOG,"mgr_network::send_to_sitectrl() pack_data :%s",pack_data.c_str());
    return m_sitctrl_cli->send_data(pack_data.data(),pack_data.length());
}

//设置设备服务器的绑定地址
void mgr_network::set_sync_svr_addr(std::string syncsvr_addr)
{
    std::vector<std::string> vct_addr;
    LOG_MSG(WRN_LOG,"mgr_network::set_sync_svr_addr() syncsvr_addr=%s", syncsvr_addr.c_str());
    if(xbasic::split_string(syncsvr_addr,":",&vct_addr) < 2) return ; //不是地址格式
    m_sync_svr->set_server_addr(vct_addr[0].c_str(),atoi(vct_addr[1].c_str()));
}
void mgr_network::set_sitctrlsync_svr_addr(std::string syncsvr_addr)
{
    std::vector<std::string> vct_addr;
    LOG_MSG(WRN_LOG,"mgr_network::set_sitctrlsync_svr_addr() syncsvr_addr=%s", syncsvr_addr.c_str());
    if(xbasic::split_string(syncsvr_addr,":",&vct_addr) < 2) return ; //不是地址格式
    m_sitctrlsync_svr->set_server_addr(vct_addr[0].c_str(),atoi(vct_addr[1].c_str()));
}

int mgr_network::send_to_dst_board(std::string cli_port, xusbpackage *pack)
{
    std::string pack_data = pack->serial_to_bin(); //将包串行化
    char print_buff[4096] = {0};
    xbasic::hex_to_str((unsigned char *)pack_data.data(),print_buff,pack_data.length()>2000?2000:pack_data.length());
    LOG_MSG(WRN_LOG,"mgr_network::send_to_dst_board() cli_port=%s cmd=%d msg=%s", cli_port.c_str(),pack->m_msg_cmd, print_buff);
    return m_sync_svr->send_data(cli_port.c_str(), pack_data.data(), pack_data.length());
}

//设置TH监控板的服务器地址
void mgr_network::set_thmonitor_board_addr(std::string th_addr)
{
    std::vector<std::string> vct_addr;
    LOG_MSG(WRN_LOG,"mgr_network::set_thmonitor_board_addr() th_addr=%s", th_addr.c_str());
    if(xbasic::split_string(th_addr,":",&vct_addr) < 2) return ; //不是地址格式
    m_th_monitor_cli->set_server_addr(vct_addr[0].c_str(),atoi(vct_addr[1].c_str()));
}

//向TH监控板发送心跳包
void mgr_network::send_heartbeat_tothmonitor()
{
    //1、构造心跳包
    xusbpackage pack(xusbpackage::MC_HEARTBEAT,PROTO_VERSION,0,xconfig::self_id(),0);
    //2、发送心跳包
    send_to_thmonitor_board(&pack);
}

//设置MF监控板的服务器地址
void mgr_network::set_mfmonitor_board_addr(std::string mf_addr)
{
    std::vector<std::string> vct_addr;
    LOG_MSG(WRN_LOG,"mgr_network::set_mfmonitor_board_addr() mf_addr=%s", mf_addr.c_str());
    if(xbasic::split_string(mf_addr,":",&vct_addr) < 2) return ; //不是地址格式
    m_mf_monitor_cli->set_server_addr(vct_addr[0].c_str(),atoi(vct_addr[1].c_str()));
}

//设置sitecontrol平台服务器地址
void mgr_network::set_sitecontrol_addr(std::string sitectrl_addr)
{
    std::vector<std::string> vct_addr;
    LOG_MSG(WRN_LOG,"mgr_network::set_mfmonitor_board_addr() sitectrl_addr=%s", sitectrl_addr.c_str());
    if(xbasic::split_string(sitectrl_addr,":",&vct_addr) < 2) return ; //不是地址格式
    m_sitctrl_cli->set_server_addr(vct_addr[0].c_str(),atoi(vct_addr[1].c_str()));
}

//向MF监控板发送心跳包
void mgr_network::send_heartbeat_tomfmonitor()
{
    //1、构造心跳包
    xusbpackage pack(xusbpackage::MC_HEARTBEAT,PROTO_VERSION,0,xconfig::self_id(),0);
    //2、发送心跳包
    send_to_mfmonitor_board(&pack);
}

//向SiteControl平台服务发送心跳
void mgr_network::send_heartbeat_tositectrl()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_heartbeat_tositectrl()");

    //构造心跳请求包
    int len = 0;
    std::string site_id = std::to_string(xconfig::site_id());
    xsite_package pack(SITE_MT_REQUEST,"",site_id);
    boost::shared_ptr<xcmd> cmd(new xcmd(SITE_MC_HEARTBEAT,""));
    pack.add_cmd(cmd);

    //发送心跳包
    len = send_to_sitectrl(&pack);

    LOG_MSG(MSG_LOG, "Exited mgr_network::send_heartbeat_tositectrl() len=%d",len);
    return ;
}

//构造SiteControl平台注册包
boost::shared_ptr<xsite_package> mgr_network::make_register_pack()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::make_register_pack()");

    int siteid = xconfig::site_id();
    std::string site_id = std::to_string(siteid);
    std::string board_slot = std::to_string(siteid-64);
    std::string timeout = std::to_string(30);
    boost::shared_ptr<xsite_package> pack(new xsite_package(SITE_MT_REQUEST,"",site_id));
    boost::shared_ptr<xcmd> cmd(new xcmd(SITE_MC_REGISTER,""));
    cmd->add_param("proto_version",SITE_PROTO_VERSION);
    cmd->add_param("heartbeat_timeout",timeout);
    cmd->add_param("board_slot",board_slot);
    pack->add_cmd(cmd);

    LOG_MSG(MSG_LOG, "Exited mgr_network::make_register_pack()");
    return pack;
}

//发送SiteControl平台注册包
int mgr_network::send_register_tositectrl()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_register_tositectrl()");

    //构造注册请求包
    int len = 0;
    boost::shared_ptr<xsite_package> pack = make_register_pack();

    //发送注册包
    len = send_to_sitectrl(pack.get());

    LOG_MSG(MSG_LOG, "Exited mgr_network::send_register_tositectrl() len=%d",len);
    return len;
}

//向sitctrl sync的客户端发送消息
int mgr_network::send_to_sitectlr_synccli(std::string sync_cli_addr,xsite_package* pack)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_to_sitectlr_synccli()");

    //将包串行化
    std::string pack_data = pack->serial_to_json();
    int len = 0;
    len = m_sitctrlsync_svr->send_data(sync_cli_addr.c_str(),pack_data.data(),pack_data.length());

    LOG_MSG(MSG_LOG, "Exited mgr_network::send_to_sitectlr_synccli() len=%d",len);
    return len;
}

//向所有sitctrl sync的客户端发送消息
int mgr_network::send_to_allsitectrl_synccli(xsite_package* pack)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_to_allsitectrl_synccli()");

    //将包串行化
    std::string pack_data = pack->serial_to_json();
    int len = 0;
    len = m_sitctrlsync_svr->send_data_to_all(pack_data.data(),pack_data.length());

    LOG_MSG(MSG_LOG, "Exited mgr_network::send_to_allsitectrl_synccli() len=%d",len);
    return len;
}

int mgr_network::send_to_all_synccli(xusbpackage *pack)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_to_all_synccli()");

    //将包串行化
    std::string pack_data = pack->serial_to_bin();
    char print_buff[4096] = {0};
    xbasic::hex_to_str((unsigned char *)pack_data.data(),print_buff,pack_data.length()>2000?2000:pack_data.length());
    LOG_MSG(WRN_LOG,"mgr_network::send_to_all_synccli() cmd=0x%x pack_data:%d msg=%s",pack->m_msg_cmd, pack_data.size(), print_buff);
    int len = 0;
    len = m_sync_svr->send_data_to_all(pack_data.data(),pack_data.length());

    LOG_MSG(MSG_LOG, "Exited mgr_network::send_to_all_synccli() len=%d",len);
    return len;
}

int mgr_network::send_to_thmonitor_board(xusbpackage *pack)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_to_thmonitor_board()");
    //将包串行化
    std::string pack_data = pack->serial_to_bin();
    int len = 0;
    if(m_th_monitor_cli != NULL)
    {
        len = m_th_monitor_cli->send_data(pack_data.data(),pack_data.length());
    }
    else
    {
        LOG_MSG(WRN_LOG, "mgr_network::send_to_thmonitor_board() len=%d",len);
    }
    LOG_MSG(MSG_LOG, "Exited mgr_network::send_to_thmonitor_board() len=%d",len);
    return len;
}

int mgr_network::send_to_mfmonitor_board(xusbpackage *pack)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_to_mfmonitor_board()");
    //将包串行化
    std::string pack_data = pack->serial_to_bin();
    int len = 0;
    if(m_mf_monitor_cli != NULL)
    {
        len = m_mf_monitor_cli->send_data(pack_data.data(),pack_data.length());
    }
    else
    {
        LOG_MSG(WRN_LOG, "mgr_network::send_to_mfmonitor_board() len=%d",len);
    }
    LOG_MSG(MSG_LOG, "Exited mgr_network::send_to_mfmonitor_board() len=%d",len);
    return len;
}

#endif

void mgr_network::on_tcp_svr_msg(xtcp_server *svr,xtcp_client_node *cli,xtcp_client_node::CBKTCPMSG tcp_msg,char *pdata,int data_len) //TCP服务端回调函数
{
    LOG_MSG(MSG_LOG,"Enter into mgr_network::on_tcp_svr_msg()");
    if(tcp_msg == xtcp_client_node::TCP_DATA)
    {
        if(svr == m_adapter_svr.get())
        {
            std::string port_name = std::string(PORT_ADAPTER)+cli->get_cli_info()->addr_info;
            LOG_MSG(MSG_LOG, "mgr_network::on_tcp_svr_msg() port_name=%s",port_name.c_str());
            xusbadapter_package pack((unsigned char *)pdata,data_len);
            if(pack.n_msg_cmd == xusbadapter_package::MC_HEARTBEAT)
            {
                tell_listener(NET_DATA,port_name.c_str(),&pack); //回调
                LOG_MSG(MSG_LOG,"mgr_network::on_tcp_svr_msg() recevice heartbeat message!");
            }
            else
            {
                LOG_MSG(MSG_LOG,"mgr_network::on_tcp_svr_msg() recevice logic message!");
                tell_listener(NET_DATA,port_name.c_str(),&pack); //回调
            }
        }

#if defined(SYNC_BUILD)
        if(svr == m_sync_svr.get())
        {
            std::string port_name = std::string(PORT_SYNC_SVR)+cli->get_cli_info()->addr_info;
            //LOG_MSG(MSG_LOG,"mgr_network::on_tcp_svr_msg() port_name=%s",port_name.c_str());
            xusbpackage pack((unsigned char *)pdata,data_len);
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_svr_msg() port_name=%s src:0x%x.",port_name.c_str(), pack.m_srcid);
            add_port(pack.m_srcid, cli->get_cli_info()->addr_info);
            tell_listener(NET_DATA,port_name.c_str(),&pack); //回调
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_svr_msg() recevice message!");
        }
        else if(svr == m_sitctrlsync_svr.get())
        {
            std::string port_name = std::string(PORT_SITECTRL_SVR)+cli->get_cli_info()->addr_info;
            int len_js = xbasic::read_bigendian(pdata+2,4);
            if(len_js <= 0 || len_js > 1024*1024) return; //JSON长度不对
            std::string msg_cont(pdata+6,len_js);
            xsite_package pack(msg_cont);
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_svr_msg() sitectrl sync svr port_name=%s src:0x%x.",port_name.c_str(), std::stoi(pack.m_board_id));
            tell_listener(NET_DATA,port_name.c_str(),&pack); //回调
            LOG_MSG(MSG_LOG,"mgr_network::on_tcp_svr_msg() sitectrl sync svr recevice message!");
        }
        else
        {
            //nothing to do
            LOG_MSG(WRN_LOG, "mgr_network::on_tcp_svr_msg() recevice message is invalid");
        }
#endif
    }
    else if(tcp_msg == xtcp_client_node::TCP_CONNECTED) //连接建立
    {
        if(svr == m_adapter_svr.get())
        {
            std::string port_name = std::string(PORT_ADAPTER)+cli->get_cli_info()->addr_info;
            LOG_MSG(MSG_LOG, "mgr_network::on_tcp_svr_msg() port_name=%s.",port_name.c_str());
            cli->set_packer_unpacker(new usbadapter_bin_packer,new usbadapter_bin_unpacker); //重设解包打包器
            tell_listener(NET_CONNECTED,port_name.c_str(),NULL); //回调
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_svr_msg() recevice tcp connect message from portname=%s!",port_name.c_str());
        }

#if defined(SYNC_BUILD)
        if(svr == m_sync_svr.get())
        {
            std::string port_name = std::string(PORT_SYNC_SVR)+cli->get_cli_info()->addr_info;
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_svr_msg() port_name=%s.",port_name.c_str());
            cli->set_packer_unpacker(new usb_bin_packer,new usb_bin_unpacker); //重设解包打包器
            tell_listener(NET_CONNECTED,port_name.c_str(),NULL); //回调
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_svr_msg() recevice tcp connect message from portname=%s!",port_name.c_str());
        }
        else if(svr == m_sitctrlsync_svr.get())
        {
            std::string port_name = std::string(PORT_SITECTRL_SVR)+cli->get_cli_info()->addr_info;
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_svr_msg() sitectrl sync svr recevice tcp connect message from portname=%s!",port_name.c_str());
            cli->set_packer_unpacker(new sitejson_packer,new sitejson_unpacker); //重设解包打包器
            tell_listener(NET_CONNECTED,port_name.c_str(),NULL); //回调
        }
        else
        {
            //nothing to do
            LOG_MSG(WRN_LOG, "mgr_network::on_tcp_svr_msg() recevice tcp connect message is invalid");
        }
#endif
    }
    else if(tcp_msg == xtcp_client_node::TCP_DISCONNECTED) //连接断开
    {
        if(svr == m_adapter_svr.get())
        {
            std::string port_name = std::string(PORT_ADAPTER)+cli->get_cli_info()->addr_info;
            LOG_MSG(MSG_LOG, "mgr_network::on_tcp_svr_msg() port_name=%s.",port_name.c_str());
            tell_listener(NET_DISCONNECTED,port_name.c_str(),NULL); //回调
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_svr_msg() recevice tcp disconnect message from portname=%s!",port_name.c_str());
        }

#if defined(SYNC_BUILD)
        if(svr == m_sync_svr.get())
        {
            std::string port_name = std::string(PORT_SYNC_SVR)+cli->get_cli_info()->addr_info;
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_svr_msg() port_name=%s.",port_name.c_str());
            tell_listener(NET_DISCONNECTED,port_name.c_str(),NULL); //回调
            del_port(cli->get_cli_info()->addr_info);
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_svr_msg() recevice tcp disconnect message from portname=%s!",port_name.c_str());
        }
        else if(svr == m_sitctrlsync_svr.get())
        {
            std::string port_name = std::string(PORT_SITECTRL_SVR)+cli->get_cli_info()->addr_info;
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_svr_msg() sitectrl sync svr port_name=%s.",port_name.c_str());
            tell_listener(NET_DISCONNECTED,port_name.c_str(),NULL); //回调
            LOG_MSG(WRN_LOG,"mgr_network::on_tcp_svr_msg() sitectrl sync svr recevice tcp disconnect message from portname=%s!",port_name.c_str());
        }
        else
        {
            //nothing to do
            LOG_MSG(WRN_LOG, "mgr_network::on_tcp_svr_msg() recevice tcp disconnect message is invalid");
        }
#endif
    }

    LOG_MSG(MSG_LOG,"Exited mgr_network::on_tcp_svr_msg()");
    return ;
}

void mgr_network::set_adapter_addr(std::string devsvr_addr) //设置设备服务器的绑定地址
{
    std::vector<std::string> vct_addr;
    LOG_MSG(WRN_LOG,"mgr_network::set_adapter_addr() devsvr_addr=%s", devsvr_addr.c_str());
    if(xbasic::split_string(devsvr_addr,":",&vct_addr) < 2) return ; //不是地址格式
    m_adapter_svr->set_server_addr(vct_addr[0].c_str(),atoi(vct_addr[1].c_str()));
}

int mgr_network::send_to_adapter(std::string cli_port,xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_network::send_to_adapter() cli_port=%s",cli_port.c_str());
    std::string pack_data = pack->serial_to_bin(); //将包串行化
    int len = 0;
    len = m_adapter_svr->send_data(cli_port.c_str(),pack_data.data(),pack_data.length());
    LOG_MSG(MSG_LOG, "Exited mgr_network::send_to_adapter() cli_port=%s",cli_port.c_str());
    return len;
}

int mgr_network::add_listener(xlistener *new_listener) //添加监听器
{
    boost::unique_lock<boost::shared_mutex> lock(m_mux_listener); //写锁
    for(std::list<xlistener *>::iterator iter=m_lst_listener.begin(); iter!=m_lst_listener.end(); iter++)
    {
        if((xlistener *)(*iter) == new_listener)
        {
            return 0; //已经存在
        }
    }
    m_lst_listener.push_back(new_listener);
    return 1;
}

void mgr_network::del_listener(xlistener *del_listener) //移除监听器
{
    boost::unique_lock<boost::shared_mutex> lock(m_mux_listener); //写锁
    for(std::list<xlistener *>::iterator iter=m_lst_listener.begin(); iter!=m_lst_listener.end();)
    {
        if((xlistener *)(*iter) != del_listener)
        {
            iter++;
            continue;
        }
        else
        {
            m_lst_listener.erase(iter++);
            return;
        }
    }
}

int mgr_network::tell_listener(NET_MSG msg_type,const char *port_name,xpacket *packet) //告知监听器
{
    const char *flag_end = strstr(port_name,">");
    std::string port_flag = (flag_end)? std::string(port_name,flag_end-port_name+1): ""; //获取端口标志
    //LOG_MSG(MSG_LOG, "mgr_network::tell_listener() port_name=%s port_flag=%s.",port_name,port_flag.c_str());
    boost::shared_lock<boost::shared_mutex> lock(m_mux_listener); //读锁
    for(std::list<xlistener *>::iterator iter=m_lst_listener.begin(); iter!=m_lst_listener.end(); iter++)
    {
        xlistener * listener = *iter;
        if(listener->judge_filter(port_flag)) listener->on_network(msg_type,port_name,packet); //向监听器回调消息
    }
    return m_lst_listener.size();
}

std::string mgr_network::find_port(int board_id) //寻找端口
{
    LOG_MSG(MSG_LOG,"Enter into mgr_network::find_port() board_id=0x%x",board_id);
    boost::shared_lock<boost::shared_mutex> lock(m_mux_port); //读锁
    boost::unordered_map<int,std::string>::iterator iter = m_map_port.find(board_id);
    if(iter != m_map_port.end())
    {
        return iter->second;
    }
    LOG_MSG(MSG_LOG,"Exited mgr_network::find_port()");
    return "";
}

int mgr_network::add_port(int board_id, std::string port) //添加端口
{
    LOG_MSG(MSG_LOG,"Enter into mgr_network::add_port()");
    std::string the_port = find_port(board_id);
    if(!the_port.empty())
    {
        if (the_port != port)
        {
            boost::unique_lock<boost::shared_mutex> lock(m_mux_port); //写锁
            m_map_port[board_id] = port;
        }
    }
    else
    {
        boost::unique_lock<boost::shared_mutex> lock(m_mux_port); //写锁
        m_map_port.insert(std::make_pair(board_id,port));
    }

    LOG_MSG(MSG_LOG,"Exited mgr_network::add_port()");
    return 0;
}

void mgr_network::del_port(std::string port) //删除端口
{
    boost::unique_lock<boost::shared_mutex> lock(m_mux_port); //写锁
    for(auto& iter : m_map_port)
    {
        if (iter.second == port)
        {
            m_map_port.erase(iter.first);
        }
    }
}
