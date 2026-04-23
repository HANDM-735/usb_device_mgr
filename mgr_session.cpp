#include "mgr_session.h"
#include <math.h>
#include <algorithm>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <sstream>
#include "usbadapter_package.h"
#include "mgr_log.h"

#define MAX_THREAD_WAIT_MS 100

usbadapter_msg::usbadapter_msg()
{
}

usbadapter_msg::usbadapter_msg(int msgtype, const std::string& port_name, boost::shared_ptr<xusbadapter_package> package) : \
    m_msgtype(msgtype), m_port_name(port_name), packdata(package)
{
}

usbadapter_msg::~usbadapter_msg()
{
}

usbadapter_msg::usbadapter_msg(const usbadapter_msg& other)
{
}

usbadapter_msg& usbadapter_msg::operator=(const usbadapter_msg& other)
{
    if (this != &other) {
        this->m_msgtype = other.m_msgtype;
        this->m_port_name = other.m_port_name;
        this->packdata = other.packdata;
    }
    return *this;
}

usbadapter_msg::usbadapter_msg(usbadapter_msg&& other)
{
    if (this != &other) {
        std::swap(other.m_msgtype, this->m_msgtype);
        m_port_name = std::move(other.m_port_name);
        std::swap(other.packdata, this->packdata);
    }
}

usbadapter_msg::usbadapter_msg(const usbadapter_msg&& other)
{
    if (this != &other) {
        this->m_msgtype = other.m_msgtype;
        m_port_name = std::move(other.m_port_name);
        this->packdata = other.packdata;
    }
}

usbadapter_msg& usbadapter_msg::operator=(usbadapter_msg&& other)
{
    if (this != &other) {
        std::swap(other.m_msgtype, this->m_msgtype);
        m_port_name = std::move(other.m_port_name);
        std::swap(other.packdata, this->packdata);
    }
    return *this;
}

usbadapter_msg& usbadapter_msg::operator=(const usbadapter_msg&& other)
{
    if (this != &other) {
        this->m_msgtype = other.m_msgtype;
        m_port_name = std::move(other.m_port_name);
        this->packdata = other.packdata;
    }
    return *this;
}

mgr_session::mgr_session() : m_msg_queue(0)
{
}

mgr_session::~mgr_session()
{
    //m_network_mgr->del_listener(this);
    m_sys_config = NULL;
    m_network_mgr = NULL;
}

void mgr_session::init() //初始化
{
    const char *c_filt_port[] = {PORT_ADAPTER};
    for (int i = 0; i < sizeof(c_filt_port) / sizeof(char *); i++) this->add_filter(c_filt_port[i]); //设置网络消息筛选端口
    m_sys_config = xconfig::get_instance();
    m_network_mgr = mgr_network::get_instance();
    m_network_mgr->add_listener(this); //将自己加入网络消息监听器
}

void mgr_session::work(unsigned long ticket) //工作函数
{
    //to do
    main_process();
}

int mgr_session::main_process()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_session::main_process()");

    usbadapter_msg msg;
    bool ret = m_msg_queue.Pop_Wait(&msg, 500);
    if (!ret) {
        LOG_MSG(WRN_LOG, "mgr_session::main_process() failed to fetch message from message queue len(%d)", m_msg_queue.Size());
        return -1;
    }

    if ((msg.m_msgtype == NET_DATA) && (msg.packdata == NULL)) {
        LOG_MSG(WRN_LOG, "mgr_session::main_process() packet is null.");
        return -1;
    }

    xusbadapter_package *pack = dynamic_cast<xusbadapter_package *>(msg.packdata.get());
    if (msg.m_msgtype == NET_DATA) //网络端来数据
    {
        //消息请求
        boost::shared_ptr<xsession> session = add_session(msg.m_port_name.c_str(), pack->m_session);
        session->on_recv(msg.m_port_name.c_str(), pack); //回调到指定会话

        //删除会话
        std::ostringstream oss;
        oss << pack->m_session;
        std::string real_session = msg.m_port_name + std::string(":") + oss.str();
        del_session(real_session);
    }
    else if (msg.m_msgtype == NET_CONNECTED) //网络端刚连接
    {
        //to do ....
    }
    else if (msg.m_msgtype == NET_DISCONNECTED) //网络断开
    {
    }
    LOG_MSG(MSG_LOG, "Exited mgr_session::main_process()");
    return 0;
}

int mgr_session::on_network(NET_MSG msg_type, const char *port_name, xpacket *packet) //网络接收通知
{
    LOG_MSG(MSG_LOG, "Enter into mgr_session::on_network()");
    usbadapter_msg msgq;
    msgq.m_msgtype = msg_type;
    msgq.m_port_name = port_name;
    if (packet != NULL) {
        xusbadapter_package *pack = dynamic_cast<xusbadapter_package *>(packet);
        boost::shared_ptr<xusbadapter_package> packagedata(dynamic_cast<xusbadapter_package *>(pack->clone()));
        *packagedata = *pack;
        msgq.packdata = packagedata;
        LOG_MSG(MSG_LOG, "mgr_session::on_network() push usbadapter msg pack->m_msg_type:%d pack->m_msg_cmd:%d", msgq.packdata->m_msg_type, msgq.packdata->m_msg_cmd);
    }
    if (!m_msg_queue.Push(msgq)) {
        LOG_MSG(WRN_LOG, "mgr_session::on_network() push usbadapter msg into queue failed!");
        return -1;
    }
    LOG_MSG(MSG_LOG, "Exited mgr_session::on_network()");
    return 0;
}

int mgr_session::on_network_send(const char *port_name, xpacket *packet) //代理消息发送接口
{
    LOG_MSG(MSG_LOG, "Enter into mgr_session::on_network_send() port_name=%s", port_name);
    xusbadapter_package *pack = dynamic_cast<xusbadapter_package *>(packet);
    if (strncmp(port_name, PORT_ADAPTER, strlen(PORT_ADAPTER)) == 0) //发向适配层的库或测试程序
    {
        std::string cli_port(port_name + strlen(PORT_ADAPTER));
        int send_len = 0;

        //发送数据到网络底层
        send_len = m_network_mgr->send_to_adapter(cli_port, pack);
        if (send_len != 0) {
            LOG_MSG(MSG_LOG, "mgr_session::on_network_send() port_name=%s send successfully.", port_name);
            return 0;
        }
        else {
            LOG_MSG(WRN_LOG, "mgr_session::on_network_send() port_name=%s msg session_id=%d", port_name, pack->m_session);
            return -1;
        }
    }
    LOG_MSG(MSG_LOG, "Exited mgr_session::on_network_send() port_name=%s", port_name);
    return -1;
}

boost::shared_ptr<xsession> mgr_session::find_session(const std::string& session)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_session::find_session() session=%s", session.c_str());
    boost::shared_lock<boost::shared_mutex> lock(m_mux_session); //读锁
    boost::unordered_map<std::string, boost::shared_ptr<xsession> >::iterator iter = m_map_session.find(session);
    if (iter != m_map_session.end()) {
        LOG_MSG(MSG_LOG, "mgr_session::find_session() session=%s founded", session.c_str());
        return iter->second;
    }
    LOG_MSG(MSG_LOG, "Exited mgr_session::find_session() session=%s", session.c_str());
    return boost::shared_ptr<xsession>();
}

boost::shared_ptr<xsession> mgr_session::add_session(const char *port_name, int session_id)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_session::add_session() port_name=%s, sess_id=%d", port_name, session_id);
    std::ostringstream oss;
    oss << session_id;
    std::string real_session = std::string(port_name) + std::string(":") + oss.str();

    boost::shared_ptr<xsession> the_session = find_session(real_session);
    if (the_session != NULL) {
        LOG_MSG(MSG_LOG, "mgr_session::add_session() session have existed");
        return the_session; //已经存在
    }
    LOG_MSG(MSG_LOG, "mgr_session::add_session() --debug1");
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session); //写锁
    boost::shared_ptr<xsession> new_session(new xsession(session_id, port_name)); //新建单板对象
    new_session->set_proxy_send(std::bind(&mgr_session::on_network_send, this, std::placeholders::_1, std::placeholders::_2)); //设置代理发送函数
    m_map_session.insert(std::make_pair(real_session, new_session)); //将单板加入到集合中
    LOG_MSG(MSG_LOG, "Exited mgr_session::add_session() add new session: %s, port_name=%s, sess_id=%d", real_session.c_str(), port_name, session_id);
    return new_session;
}

void mgr_session::del_session(const std::string& sess) //删除会话
{
    LOG_MSG(MSG_LOG, "Enter into mgr_session::del_session() sess=%s", sess.c_str());
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session); //写锁
    boost::unordered_map<std::string, boost::shared_ptr<xsession> >::iterator iter = m_map_session.find(sess);
    if (iter != m_map_session.end()) {
        boost::shared_ptr<xsession> session = iter->second;
        m_map_session.erase(iter);
        LOG_MSG(MSG_LOG, "mgr_session::del_session() delete session: %s", sess.c_str());
    }
    LOG_MSG(MSG_LOG, "Exited mgr_session::del_session() sess=%s", sess.c_str());
}

int mgr_session::find_session_from_port(const std::string& port_name, std::vector<boost::shared_ptr<xsession> >& vects) //根据端口寻找会话
{
    LOG_MSG(MSG_LOG, "Enter into mgr_session::find_session_from_port() port_name=%s", port_name.c_str());
    boost::shared_lock<boost::shared_mutex> lock(m_mux_session); //读锁
    for (boost::unordered_map<std::string, boost::shared_ptr<xsession> >::iterator iter = m_map_session.begin(); iter != m_map_session.end(); ++iter) {
        boost::shared_ptr<xsession> session = iter->second;
        if (session->get_port_name() == port_name) vects.push_back(session); //对应的会话
    }
    int vectsize = vects.size();
    LOG_MSG(MSG_LOG, "Exited mgr_session::find_session_from_port() port_name=%s size=%d", port_name.c_str(), vectsize);
    return vectsize;
}

int mgr_session::release_portname_session(const std::string& port_name)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_session::release_portname_session() port_name=%s", port_name.c_str());
    std::vector<boost::shared_ptr<xsession> > vects;

    int size = find_session_from_port(port_name, vects); //根据端口寻找会话
    if (size != 0) {
        for (int i = 0; i < size; i++) {
            boost::shared_ptr<xsession> session = vects[i];
            std::ostringstream oss;
            oss << session->get_sessionid();
            std::string real_session = port_name + std::string(":") + oss.str();
            del_session(real_session); //删除会话
            LOG_MSG(MSG_LOG, "mgr_session::release_portname_session() delete session: %s", real_session.c_str());
        }
    }
    LOG_MSG(MSG_LOG, "Exited mgr_session::release_portname_session() port_name=%s", port_name.c_str());
    return 0;
}
