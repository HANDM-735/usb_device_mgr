#ifndef MGR_SESSION_H
#define MGR_SESSION_H
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include "xbasic.h"
#include "xbasicmgr.hpp"
#include "xconfig.h"
#include "xsession.h"
#include "mgr_network.h"
#include "queue.h"

class usbadapter_msg
{
public:
    usbadapter_msg();
    usbadapter_msg(int msgtype,const std::string& port_name,boost::shared_ptr<xusbadapter_package> package);
    usbadapter_msg(const usbadapter_msg& other);
    usbadapter_msg& operator=(const usbadapter_msg& other);
    usbadapter_msg(usbadapter_msg&& other);
    usbadapter_msg& operator=(usbadapter_msg&& other);
    usbadapter_msg& operator=(const usbadapter_msg&& other);
    ~usbadapter_msg();
public:
    int     m_msgtype;
    std::string   m_port_name;
    boost::shared_ptr<xusbadapter_package> packdata;
};

class mgr_session : public xmgr_basic<mgr_session> //会话管理器
{
public:
    mgr_session() ;
    virtual ~mgr_session();
public:
    virtual void init();                    //初始化
    virtual void work(unsigned long ticket);  //工作函数

public:
    boost::shared_ptr<xsession> find_session(const std::string& session);        //查找会话
    virtual int on_network_send(const char *port_name,xpacket *packet);    //代理消息发送接口

protected:
    virtual int on_network(NET_MSG msg_type,const char *port_name,xpacket *packet); //网络接收通知
    virtual int main_process();

protected:
    int find_session_from_port(const std::string& port_name,std::vector<boost::shared_ptr<xsession> >& vects);  //根据端口寻找会话
    boost::shared_ptr<xsession> add_session(const char* port_name,int session_id);    //添加会话
    void del_session(const std::string& sess);                //删除会话
    int release_portname_session(const std::string& port_name);            //释放与端口关联的相关会话

protected:
    xconfig                *m_sys_config;        //系统配置
    mgr_network            *m_network_mgr;        //网络管理器

private:
    boost::unordered_map<std::string,boost::shared_ptr<xsession> >  m_map_session;        //会话集合
    boost::shared_mutex            m_mux_session;            //会话集合读写锁
    SafeQueue<usbadapter_msg>        m_msg_queue;            //网络消息队列
};

#endif
