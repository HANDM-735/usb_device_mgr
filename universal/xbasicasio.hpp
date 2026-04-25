#ifndef X_ASIO_BASIC_H
#define X_ASIO_BASIC_H
#include <functional>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/array.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/unordered_map.hpp>
#include <boost/container/list.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <boost/atomic.hpp>
#include "xbasic.hpp"
#include "mgr_log.h"

#define MAX_MSGLEN       16384   //接收缓冲区尺寸（最大包长）
#define MAX_MSG_NUM      128     //收发缓冲队列最多缓存的包个数
#define MAXIOTHRDNUM    64      //xioservice最大线程数量
#define POST_ACCEPT_NUM  32     //预先投递的ACCEPT数量

class xioservice : public boost::asio::io_service
{
public:
    xioservice() : m_use_count(0) {}
    virtual ~xioservice() {}

public:
    void start_serve() //开始运行IO服务
    {
        LOG_MSG(MSG_LOG,"Enter into xioservice::start_serve() m_use_count=%d",m_use_count.load());

        if(++m_use_count >1)
        {
            LOG_MSG(MSG_LOG,"xioservice::start_serve() m_use_count=%d",m_use_count.load());
            return; //共用对象，直接返回
        }
        reset();
        boost::thread tmp_thread(boost::bind(&xioservice::run_threads,this));
        boost::this_thread::yield();
        tmp_thread.swap(m_main_thread);

        LOG_MSG(MSG_LOG,"Exited xioservice::start_serve() m_use_count=%d",m_use_count.load());
    }
    void stop_serve() //停止运行IO服务
    {
        LOG_MSG(MSG_LOG,"Enter into xioservice::stop_serve() m_use_count=%d",m_use_count.load());
        if(--m_use_count >0)
        {
            LOG_MSG(MSG_LOG,"xioservice::stop_serve() m_use_count=%d",m_use_count.load());
            return; //还有端口在使用该对象
        }

        stop();
        m_main_thread.timed_join(boost::posix_time::seconds(6));
        m_threads_id.clear();

        LOG_MSG(MSG_LOG,"Exited xioservice::stop_serve() m_use_count=%d",m_use_count.load());
    }

    int get_thread_index(boost::thread::id thread_id) //获得线程索引
    {
        boost::unordered_map<boost::thread::id,int>::iterator iter = m_threads_id.find(thread_id);
        return (iter!=m_threads_id.end())? iter->second : -1;
    }

protected:
    inline void run_threads() //开始运行IO的线程池
    {
        m_work.reset(new boost::asio::io_service::work(*this));
        boost::thread_group thread_group;
        int cpu_num = boost::thread::hardware_concurrency(); //获得CPU线程数
        int threads_num = (cpu_num<=1?1:cpu_num*2);

        LOG_MSG(WRN_LOG,"xioservice::run_threads() cpu_num=%d threads_num",cpu_num,threads_num);
        for(int i =0; i <threads_num; ++i) //默认线程数量=CPU核数×2
        {
            boost::thread *ptr_thread = thread_group.create_thread(boost::bind(&boost::io_service::run,this,boost::system::error_code()));
            m_threads_id.insert(boost::unordered_map<boost::thread::id,int>::value_type(ptr_thread->get_id(),i));
        }
        boost::this_thread::yield();
        thread_group.join_all();
    }

private:
    boost::atomic<int>                          m_use_count;     //引用计数器
    boost::thread                               m_main_thread;   //IO主线程
    boost::shared_ptr<boost::asio::io_service::work>  m_work;    //work对象防止run退出
    boost::unordered_map<boost::thread::id,int> m_threads_id;    //线程号集合
};

class xpacker //打包器
{
public:
    enum PACKWAY{PACK_NATIVE =0,PACK_BASIC,PACK_USER}; //打包方式定义

public:
    xpacker()
    {
        //LOG_MSG(WRN_LOG,"xpacker::xpacker() construct");
    }
    virtual ~xpacker()
    {
        //LOG_MSG(WRN_LOG,"xpacker::~xpacker() deconstruct");
    }

    virtual boost::shared_ptr<const std::string> pack_data(const char *pdata,size_t datalen,PACKWAY pack_way) //打包数据
    {
        boost::shared_ptr<const std::string> ppack;
        if(NULL==pdata || 0==datalen) return ppack;
        std::string *pstring =new std::string();
        ppack.reset(pstring);
        pstring->reserve(datalen); //申请指定长度空间的字符串
        pstring->append(pdata,datalen);
        return ppack;
    }
};

class xunpacker //解包器
{
public:
    xunpacker()
    {
        m_signed_len = (size_t)-1;
        m_data_len =0;
        //LOG_MSG(WRN_LOG,"xunpacker::xunpacker() construct");
    }
    virtual ~xunpacker()
    {
        //LOG_MSG(WRN_LOG,"xunpacker::~xunpacker() deconstruct");
    }

public:
    virtual void reset_data() {m_signed_len = (size_t)-1; m_data_len =0;}
    virtual bool unpack_data(size_t bytes_data, boost::container::list<boost::shared_ptr<const std::string> > &list_pack)
    {
        m_data_len +=bytes_data; //本次收到的字节数
        char *pbegin =m_raw_buff.begin();
        list_pack.push_back(boost::shared_ptr<std::string>(new std::string(pbegin,m_data_len))); //将这个包返回上层
        return true;
    }
    virtual boost::asio::mutable_buffers_1 prepare_buff(size_t& min_recv_len) //准备数据接收缓冲区
    {
        return boost::asio::buffer(m_raw_buff);
    }

protected:
    boost::array<char,MAX_MSGLEN>    m_raw_buff;     //原始缓冲区
    size_t                           m_signed_len;   //数据包头中标识的长度，-1标识还没有收到包头
    size_t                           m_data_len;     //已经收到的数据的长度(包括头)
};

class xtcp_socket : public boost::asio::ip::tcp::socket //tcp套接字类
{
public:
    xtcp_socket(boost::asio::io_service& io_service) : boost::asio::ip::tcp::socket(io_service),m_packer(new xpacker()),m_unpacker(new xunpacker()) //构造函数
    {
        m_is_sending =false;
        m_is_dispatching =false;
        m_tm_last_recv =m_tm_last_send =0;
        //LOG_MSG(WRN_LOG,"xtcp_socket::xtcp_socket()");
    }
    virtual ~xtcp_socket()    //析构函数
    {
        //LOG_MSG(WRN_LOG,"xtcp_socket::~xtcp_socket()");
    }

    void reset_buff() //复位缓冲区状态
    {
        boost::mutex::scoped_lock locks(m_send_mutex);
        m_send_buff.clear();
        m_is_sending = false;
        locks.unlock();

        boost::mutex::scoped_lock lockr(m_recv_mutex);
        m_recv_buff.clear();
        m_is_dispatching = false;
        lockr.unlock();
    }

    void close_socket() //关闭套接字
    {
        reset_buff();
        boost::mutex::scoped_lock locks(m_send_mutex);
        boost::system::error_code err_code;
        shutdown(boost::asio::ip::tcp::socket::shutdown_both, err_code);
        close(err_code);
        m_unpacker->reset_data();
    }

    void recv_data()
    {
        do_recv_data(); //投递接收
    }

    int send_data(const char *pdata, const size_t length, const xpacker::PACKWAY pack_type =xpacker::PACK_BASIC) //打包数据并发送
    {
        if(!is_ok()||!is_open()) return -1;
        return send_native_data(m_packer->pack_data(pdata,length,pack_type));
    }

    void send_pack_in_buff() //发送缓冲区中的数据
    {
        boost::mutex::scoped_lock lock(m_send_mutex);
        do_send_data();
    }

    size_t get_pending_pack_num() //获得缓冲区中待发送的数据包个数
    {
        boost::mutex::scoped_lock lock(m_send_mutex);
        return m_send_buff.size();
    }

    const boost::shared_ptr<const std::string> peek_first_pending_pack() //查看第一个待发送的数据
    {
        boost::mutex::scoped_lock lock(m_send_mutex);
        return m_send_buff.empty() ? boost::shared_ptr<const std::string>() : m_send_buff.front();
    }

    boost::shared_ptr<const std::string> pop_first_pending_pack() //取消第一个待发送的数据并返回该数据
    {
        boost::shared_ptr<const std::string> pack;
        boost::mutex::scoped_lock lock(m_send_mutex);
        if(!m_send_buff.empty())
        {
            pack = m_send_buff.front();
            m_send_buff.pop_front();
        }
        return pack;
    }

    void recv_handler(const boost::system::error_code& err_code, size_t bytes_transferred) //async_read直接触发的回调函数
    {
        if(!err_code && bytes_transferred >0)
        {
            boost::container::list<boost::shared_ptr<const std::string> > pack_list;
            bool unpack_ok =m_unpacker->unpack_data(bytes_transferred, pack_list); //解包器开始解包
            if(!pack_list.empty())
            {
                sync_dispatch_data(pack_list); //同步派发
                if(unpack_ok)
                {
                    start_work(); //开始投递下一次接收
                }
            }
        }
        else
        {
            boost::system::error_code err_code_copy(err_code);
            on_recv_error(err_code_copy);
        }
    }

    void send_handler(const boost::system::error_code& err_code, size_t bytes_transferred, boost::shared_ptr<const std::string>& pack) //async_write直接触发的回调函数
    {
        if(!err_code && bytes_transferred >0)
        {
            on_data_send(pack);
        }
        else
        {
            boost::system::error_code err_code_copy(err_code);
            on_send_error(err_code_copy);
        }

        boost::mutex::scoped_lock lock(m_send_mutex);
        m_is_sending = false;
        do_send_data(); //继续投递下一个包的发送
    }

    void do_send_data() //调用前必须锁定发送缓冲
    {
        if(!is_ok() || !is_open())
        {
            m_is_sending = false;
            m_send_buff.clear();
        }
        else if(!m_is_sending && !m_send_buff.empty())
        {
            m_is_sending = true;
            boost::shared_ptr<const std::string> pack = m_send_buff.front(); //取出第一个待发送数据包执行发送
            async_write(this,boost::asio::buffer(*pack),boost::bind(&xtcp_socket::send_handler,this,boost::placeholders::error,boost::placeholders::bytes_transferred,pack));
            m_send_buff.pop_front(); //将该数据从待发送缓冲区删除
        }
    }

    void do_recv_data() //开始异步接收数据
    {
        size_t min_recv_len =0;
        boost::asio::mutable_buffers_1 recv_buff = m_unpacker->prepare_buff(min_recv_len);
        if(buffer_size(recv_buff)<=0) return;
        if(min_recv_len >0) //接受指定长度才回调
        {
            async_read(*this,recv_buff,boost::asio::transfer_at_least(min_recv_len),boost::bind(&xtcp_socket::recv_handler,this,boost::placeholders::error,boost::placeholders::bytes_transferred));
        }
        else //是否一读到数据就立即回调
        {
            async_read_some(recv_buff,boost::bind(&xtcp_socket::recv_handler,this,boost::placeholders::error,boost::placeholders::bytes_transferred));
        }
    }

    void async_dispatch_data(boost::shared_ptr<const std::string>& pack) //异步数据派发任务定义
    {
        on_data_recv_async(pack); //通知上层异步数据派发
        boost::mutex::scoped_lock lock(m_recv_mutex);
        m_is_dispatching = false;
        do_async_dispatch_data(); //开始下一次异步数据派发
    }

    void do_async_dispatch_data() //开始异步派发数据,调用前必须锁定接收缓存
    {
        boost::asio::io_service& the_io_service =get_io_service();
        if(the_io_service.stopped())
        {
            m_is_dispatching = false;
        }
        else if(!m_is_dispatching && !m_recv_buff.empty())
        {
            m_is_dispatching = true;
            boost::shared_ptr<const std::string> pack = m_recv_buff.front();
            m_recv_buff.pop_front();
            the_io_service.post(boost::bind(&xtcp_socket::async_dispatch_data,this,pack)); //插入一个异步派发任务
        }
    }

private:
    boost::container::list<boost::shared_ptr<const std::string> >    m_send_buff;     //频繁使用该缓冲所以用boost::container
    boost::container::list<boost::shared_ptr<const std::string> >    m_recv_buff;     //频繁使用该缓冲所以用boost::container
    boost::mutex                                                    m_send_mutex;    //发送缓冲互斥量
    boost::mutex                                                    m_recv_mutex;    //接收缓冲互斥量
    bool                                                            m_is_sending;     //发送投递标志
    bool                                                            m_is_dispatching;//派发投递标志

protected:
    boost::shared_ptr<xpacker>     m_packer;        //打包器
    boost::shared_ptr<xunpacker>   m_unpacker;      //解包器
    time_t                          m_tm_last_recv;  //最后收到数据的时间
    time_t                          m_tm_last_send;  //最后发送数据的时间
};

class xtcp_client : public xtcp_socket //TCP客户端类
{
public:
    enum CBKTCPMSG {TCP_CONNECTED =1,TCP_DISCONNECTED,TCP_DATA};
    typedef std::function<void(xtcp_client *,CBKTCPMSG,char *,int)> CALLBK_FN; //回调函数定义

protected:
    enum CLISTATUS {UNCONNECT =0,CONNECTING,CONNECTED}; //当前的连接状态定义

public:
    xtcp_client() : xtcp_socket(*get_share_ioservice())
    {
        m_connect_stat =UNCONNECT;
        memset(m_bak_addr,0,sizeof(m_bak_addr));
        m_bak_port =0;
        get_share_ioservice()->start_serve();
    }
    virtual ~xtcp_client() //析构函数
    {
        reset_socket();
        get_share_ioservice()->stop_serve();
    }

    static xioservice *get_share_ioservice() //所有udp对象共享同一个io异步服务
    {
        static xioservice s_io_service; //使用C++11特性时此处会互斥，所以是线程安全的
        return &s_io_service;
    }

public:
    void set_callback(CALLBK_FN fn) {m_fn_callbk =fn;} //设置客户端回调函数
    int set_server_addr(const char *ip_addr,unsigned short port) //设置服务器地址
    {
        LOG_MSG(MSG_LOG,"Enter into xtcp_client::set_server_addr() ip_addr=%s port=%d",ip_addr,port);

        if(ip_addr && ip_addr[0]!=0)
        {
            strcpy(m_bak_addr,ip_addr);
            m_bak_port = port;
            m_server_addr =boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip_addr),port);
            LOG_MSG(MSG_LOG,"xtcp_client::set_server_addr() ip_addr=%s port=%d",ip_addr,port);
            return 1;
        }
        LOG_MSG(MSG_LOG,"Exited xtcp_client::set_server_addr()");
        return -1;
    }

    char *get_server_addr(char *ip_buff,unsigned short *port) //获得服务器地址
    {
        if(ip_buff) strcpy(ip_buff,m_bak_addr);
        if(port) *port =m_bak_port;
        return m_bak_addr;
    }

    std::string get_server_addr() //获得服务器地址
    {
        char svr_addr[32] ={0};
        sprintf(svr_addr,"%s:%d",m_bak_addr,m_bak_port);
        return svr_addr;
    }

    int start_connect() //开始连接服务器
    {
        LOG_MSG(MSG_LOG,"Enter into xtcp_client::start_connect()");
        if(m_server_addr.port() <= 0 ||m_connect_stat != UNCONNECT)
        {
            LOG_MSG(ERR_LOG,"xtcp_client::start_connect() m_bak_addr=%s m_bak_port=%d",m_bak_addr,m_bak_port);
            return -1;
        }

        start_work();
        LOG_MSG(MSG_LOG,"Exited xtcp_client::start_connect()");
        return 0;
    }

    void disconnect() //主动断开服务器
    {
        LOG_MSG(MSG_LOG,"Enter into xtcp_client::disconnect() server_addr=%s port=%d m_connect_stat=%d", m_bak_addr,m_bak_port,m_connect_stat);
        reset_socket();
        LOG_MSG(MSG_LOG,"Exited xtcp_client::disconnect()");
    }

    bool is_disconnected() {return !get_share_ioservice()->stopped()&&(m_connect_stat==UNCONNECT);} //是否已经停止连接
    bool is_connecting() {return !get_share_ioservice()->stopped()&&(m_connect_stat==CONNECTING);} //是否正在连接
    bool is_connected() {return !get_share_ioservice()->stopped()&&(m_connect_stat==CONNECTED);} //是否连接成功

public:
    virtual void start_work() //开始连接或接收数据
    {
        LOG_MSG(MSG_LOG,"Enter into xtcp_client::start_work()");
        if(m_connect_stat ==UNCONNECT)
        {
            m_connect_stat =CONNECTING;
            async_connect(m_server_addr,boost::bind(&xtcp_client::connected_handler,this,boost::placeholders::error)); //异步连接
            LOG_MSG(MSG_LOG,"xtcp_client::start_work() connected_handler");
        }
        else if(m_connect_stat==CONNECTED)
        {
            recv_data();
            LOG_MSG(MSG_LOG,"xtcp_client::start_work() recv_data");
        }
        LOG_MSG(MSG_LOG,"Exited xtcp_client::start_work()");
    }

protected:
    virtual void on_connect(const boost::system::error_code& err_code) {} //连接消息
    virtual void on_disconnected(const boost::system::error_code& err_code) {} //断开连接消息

protected:
    virtual bool is_ok() //客户端是否正常工作的标准
    {
        return !get_share_ioservice()->stopped() &&(m_connect_stat ==CONNECTED);
    }

    virtual void on_data_recv(boost::shared_ptr<const std::string>& pack)
    {
        xtcp_socket::on_data_recv(pack);
        if(!m_fn_callbk)
        {
            LOG_MSG(WRN_LOG,"xtcp_client::on_data_recv() m_fn_callbk is NULL");
            return false;
        }
        m_fn_callbk(this,TCP_DATA,(char *)pack->data(),pack->length()); //回调到上层
        return false;
    }

    virtual void on_unpack_error() //解码错误
    {
        LOG_MSG(WRN_LOG,"xtcp_client::on_unpack_error()");
        reset_socket();
    }

    virtual void on_recv_error(const boost::system::error_code& err_code) //接收错误，或对方断开连接
    {
        LOG_MSG(WRN_LOG,"xtcp_client::on_recv_error()");
        reset_socket();
        boost::system::error_code err_code_copy(err_code);
        on_disconnected(err_code_copy);
        if(!m_fn_callbk)
        {
            LOG_MSG(WRN_LOG,"xtcp_client::on_recv_error() m_fn_callbk is NULL");
            return;
        }
        int err_no = err_code.value();
        m_fn_callbk(this,TCP_DISCONNECTED,(char *)&err_no,sizeof(int)); //回调到上层
    }

protected:
    void reset_socket() //重置套接字
    {
        LOG_MSG(MSG_LOG,"Enter into xtcp_client::reset_socket() server_addr=%s port=%d m_connect_stat=%d", m_bak_addr,m_bak_port,m_connect_stat);
        if(m_connect_stat == UNCONNECT)
        {
            LOG_MSG(WRN_LOG,"xtcp_client::reset_socket() server_addr=%s port=%d m_connect_stat=%d", m_bak_addr,m_bak_port,m_connect_stat);
            return;
        }

        close_socket();
        LOG_MSG(MSG_LOG,"Exited xtcp_client::reset_socket() m_connect_stat=%d",m_connect_stat);
    }

    void connected_handler(const boost::system::error_code& err_code) //异步连接服务器回调函数
    {
        LOG_MSG(MSG_LOG,"Enter into xtcp_client::connected_handler() server_addr=%s port=%d",m_bak_addr,m_bak_port);
        m_connect_stat = (err_code ? CONNECTED:UNCONNECTED); //标识连接成功或失败

        if(err_code) //连接服务器后返回的错误码
        {
            send_pack_in_buff(); //发送缓冲区中可能剩余的数据
            start_work(); //投递接收
        }

        boost::system::error_code err_code_copy(err_code);
        on_connected(err_code_copy); //通知连接成功或失败

        if(!m_fn_callbk)
        {
            LOG_MSG(WRN_LOG,"xtcp_client::connected_handler() m_fn_callbk is NULL");
            return;
        }
        int err_no = err_code.value();
        if(err_no == 0)
        {
            m_tm_last_recv =time(NULL);
            m_fn_callbk(this,TCP_CONNECTED,(char *)&err_no,sizeof(int)); //回调到上层
        }

        LOG_MSG(MSG_LOG,"Exited xtcp_client::connected_handler() m_connect_stat=%d m_tm_last_recv=%ld err_no=%d",m_connect_stat,m_tm_last_recv,err_no);
    }

private:
    CALLBK_FN                       m_fn_callbk;    //服务器上层回调函数
    CLISTATUS                       m_connect_stat; //连接状态
    char                            m_bak_addr[32];//备份地址
    int                             m_bak_port;    //备份端口
    boost::asio::ip::tcp::endpoint   m_server_addr;//目的服务器地址端点
};

class xtcp_client_node : public xtcp_socket //服务器客户端类
{
public:
    typedef struct _CLIINFO
    {
        char addr_info[24]; //客户端IP地址+端口
        char client_id[32]; //客户端被定义的ID
        char user_data[32]; //用户其他数据
    }CLIINFO,*PCLIINFO;

    enum CBKTCPMSG {TCP_DISCONNECTED,TCP_DATA,TCP_UNPACKERR};
    typedef std::function<void(xtcp_client_node *,CBKTCPMSG,char *,int)> CALLBK_FN; //回调函数定义

public:
    xtcp_client_node(xioservice& ioservice) : xtcp_socket(ioservice)
    {
        memset(&m_client_info,0,sizeof(m_client_info));
        //LOG_MSG(WRN_LOG,"xtcp_client_node::xtcp_client_node()");
    }
    virtual ~xtcp_client_node() //析构函数
    {
        //LOG_MSG(WRN_LOG,"xtcp_client_node::~xtcp_client_node()");
    }

public:
    CLIINFO &get_cli_info()    {return m_client_info;}
    void reset_cli_info()       {memset(&m_client_info,0,sizeof(m_client_info));} //重置客户端信息
    void set_callback(CALLBK_FN fn) {m_fn_callbk =fn;} //设置客户端回调函数

public:
    virtual void start_work() {if(is_open()) recv_data();}

protected:
    virtual bool is_ok() {return !get_io_service().stopped();}

    virtual void on_unpack_error()
    {
        LOG_MSG(WRN_LOG,"xtcp_client_node::on_unpack_error()");
        if(!m_fn_callbk) return;
        m_fn_callbk(this,TCP_UNPACKERR,NULL,0);
    }

    virtual void on_recv_error(const boost::system::error_code& err_code)
    {
        LOG_MSG(WRN_LOG,"xtcp_client_node::on_recv_error()");
        close_socket();
        if(!m_fn_callbk) return;
        int err =err_code.value();
        m_fn_callbk(this,TCP_DISCONNECTED,(char *)&err,sizeof(int));
    }

    virtual void on_data_recv_async(boost::shared_ptr<const std::string>& pack) //异步通知收到一条数据,如果on_data_recv返回false,则该数据包将不会被调用
    {
        if(!m_fn_callbk) return;
        m_fn_callbk(this,TCP_DATA,(char *)pack->data(),pack->length());
    }

private:
    CALLBK_FN       m_fn_callbk;
    CLIINFO         m_client_info;
};

class xtcp_server
{
public:
    typedef boost::shared_ptr<xtcp_client_node> _cliptr;
    typedef boost::container::list<_cliptr>                _cliplist;
    typedef boost::unordered_map<std::string,_cliptr> _climap;
    typedef std::function<void(xtcp_server *,xtcp_client_node *,xtcp_client_node::CBKTCPMSG,char *,int)> CALLBK_FN; //回调函数定义

public:
    xtcp_server() : m_acceptor(*get_share_ioservice())
    {
        memset(m_extern,0,sizeof(m_extern));
        for(int i=0 ;i<MAXIOTHRDNUM; i++) m_cli_now[i] =NULL;
        //LOG_MSG(WRN_LOG,"xtcp_server::xtcp_server()");
    }
    virtual ~xtcp_server() //析构函数
    {
        //LOG_MSG(WRN_LOG,"xtcp_server::~xtcp_server()");
        m_cli_free.clear();
    }

    static xioservice *get_share_ioservice() //所有udp对象共享同一个io异步服务
    {
        static xioservice s_io_service; //使用C++11特性时此处会互斥，所以是线程安全的
        return &s_io_service;
    }

public:
    void *get_extern_data(int len =0) {if(len *len <=sizeof(m_extern)) return m_extern;} //获取外部用户数据存放的指针
    void set_callback(CALLBK_FN fn) {m_fn_callbk =fn;} //设置服务器回调函数
    void set_server_addr(const char *ip_addr,unsigned short port) //设置本地服务端地址
    {
        if(!ip_addr || ip_addr[0]==0)
        {
            m_server_addr =boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
        }
        else
        {
            m_server_addr =boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip_addr), port);
        }
    }

    int start_service() //开启自服务器
    {
        boost::system::error_code errrc;
        m_acceptor.open(m_server_addr.protocol(), errrc);
        m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), errrc);
        m_acceptor.bind(m_server_addr, errrc);
        m_acceptor.listen(boost::asio::socket_base::max_connections, errrc);
        for(int i=0 ;i<POST_ACCEPT_NUM; ++i) post_accept(); //预先投递ACCEPT

        get_share_ioservice()->start_serve();
        return 0;
    }

    void stop_service() //停止服务器
    {
        if(m_acceptor.is_open())
        {
            boost::system::error_code errrc;
            m_acceptor.cancel(errrc);
            m_acceptor.close(errrc);
        }
        del_all_client();
        get_share_ioservice()->stop_serve();
    }

    bool is_stopped() //服务器是否停止
{
    return (!m_acceptor.is_open()) || get_share_ioservice()->stopped();
}

int respond_data(const char *pdata, const size_t len,xpacker::PACKWAY pack_way =xpacker::PACK_BASIC) //应答客户端,只能在回调函数中使用,否则会返回-1
{
    int thrd_index =get_share_ioservice()->get_thread_index(boost::this_thread::get_id());
    if(thrd_index >=0 && m_cli_now[thrd_index] !=NULL)
    {
        return m_cli_now[thrd_index]->send_data(pdata,len,pack_way);
    }
    return -1;
}

int send_data(const char *cli_key,const char *pdata,const size_t len,xpacker::PACKWAY pack_way =xpacker::PACK_BASIC) //主动向客户端发送数据
{
    _cliptr pcli =find_client(cli_key);
    if(pcli !=NULL) return pcli->send_data(pdata,len,pack_way);
    return -1;
}

int send_data_to_all(const char *pdata,const size_t len,xpacker::PACKWAY pack_way =xpacker::PACK_BASIC) //主动向所有客户端发送数据
{
    int send_cli_num =0;
    boost::shared_lock<boost::shared_mutex> lock(m_cli_map_mutex); //读锁
    for(_climap::iterator iter=m_cli_map.begin(); iter!=m_cli_map.end(); ++iter) //切记不可以在这里erase
    {
        // LOG_MSG(MSG_LOG, "send_data_to_all() addr_info=%s client_id=%s user_data:%s", iter->second->get_cli_info()->addr_info, iter->second->get_cli_info()->client_id, iter->second->get_cli_info()->user_data);
        iter->second->send_data(pdata,len,pack_way);
        send_cli_num++;
    }
    return send_cli_num;
}

int disconnect(const char *cli_key) {return del_client(cli_key);} //主动断开连接
bool is_connected(const char *cli_key) {return (find_client(cli_key)==NULL);} //客户端是否连接成功
int get_active_cli_num()       {boost::shared_lock<boost::shared_mutex> lock(m_cli_map_mutex); return m_cli_map.size();} //获得活跃的客户端数量
int get_free_cli_num()        {boost::shared_lock<boost::shared_mutex> lock(m_cli_free_mutex); return m_cli_free.size();} //获得空余的客户端数量

protected:
virtual _cliptr create_client() {return new xtcp_client_node(*get_share_ioservice());} //创建新客户端
virtual bool on_accept(xtcp_client_node *client) {return true;} //接收连接
virtual void on_disconnected(xtcp_client_node *client) {} //断开连接消息
virtual void on_unpack_error(xtcp_client_node *client) {} //客户端解包错误
virtual void on_data_recv(xtcp_client_node *client ,const char *pdata,int data_len) {} //收到客户端数据

virtual void on_client_msg(xtcp_client_node *client,xtcp_client_node::CBKTCPMSG msg_type,char *pdata,int data_len)
{
    int thrd_index =get_share_ioservice()->get_thread_index(boost::this_thread::get_id());
    m_cli_now[thrd_index] =client;

    if(msg_type ==xtcp_client_node::TCP_DISCONNECTED) //断开连接
    {
        on_disconnected(client);
        del_client(client->get_cli_info()->addr_info); //删除客户端
    }
    else if(msg_type ==xtcp_client_node::TCP_DATA) //收到数据
    {
        on_data_recv(client,pdata,data_len);
    }
    else if(msg_type ==xtcp_client_node::TCP_UNPACKERR) //解包错误
    {
        on_unpack_error(client);
    }

    if(m_fn_callbk) m_fn_callbk(this,client,msg_type,pdata,data_len);
    m_cli_now[thrd_index] =NULL;
}

protected:
void post_accept() //投递ACCEPT
{
    _cliptr pcli;
    //LOG_MSG(MSG_LOG,"xtcp_server::post_accept() active client num=%d free client num=%d",get_active_cli_num(),get_free_cli_num());
    if(m_cli_free.size()>MAXIOTHRDNUM)
    {
        boost::unique_lock<boost::shared_mutex> lockfree(m_cli_free_mutex); //写锁
        LOG_MSG(WRN_LOG,"xtcp_server::post_accept() free size(%d) > %d",m_cli_free.size(),MAXIOTHRDNUM);
        pcli =m_cli_free.front();
        m_cli_free.pop_front();
    }
    else
    {
        //LOG_MSG(WRN_LOG,"xtcp_server::post_accept()");
        pcli.reset(create_client());
    }

    pcli->reset_cli_info();
    pcli->set_callback(boost::bind(&xtcp_server::on_client_msg,this,boost::placeholders::_1,boost::placeholders::_2,boost::placeholders::_3,boost::placeholders::_4)); //设置回调函数
    m_acceptor.async_accept(*pcli, boost::bind(&xtcp_server::accept_handler,this,boost::placeholders::error,boost::placeholders::_5,pcli));
}

void accept_handler(const boost::system::error_code& err_code, _cliptr pcli) //ACCEPT直接回调函数
{
    if(!err_code) //连接成功
    {
        try
        {
            xtcp_client_node *client =pcli.get();
            boost::asio::ip::tcp::endpoint endPt =pcli->remote_endpoint(); //获得对端地址和端口
            sprintf(client->get_cli_info()->addr_info,"%s:%d",endPt.address().to_string().c_str(),endPt.port());
            if(m_fn_callbk) m_fn_callbk(this,client,xtcp_client_node::TCP_CONNECTED,0,0); //向上回调客户端连接成功
            if(on_accept(client) && add_client(pcli)!=-1) pcli->start_work();
        }
        catch(boost::system::system_error &ec)
        {
            std::cerr << "xtcp_server function 'accept_handler': endpoint disconnected." << ec.what() << std::endl;
            LOG_MSG(WRN_LOG,"xtcp_server function 'accept_handler' endpoint disconnected %s",ec.what());
            post_accept(); //投递下一个Accept
        }
    }
    else if(m_acceptor.is_open()) //连接出现错误
    {
        boost::system::error_code errc;
        m_acceptor.cancel(errc);
        m_acceptor.close(errc);
    }
    post_accept();
}

int add_client(_cliptr pcli) //增加一个客户端
{
    boost::unique_lock<boost::shared_mutex> lock(m_cli_map_mutex); //写锁
    _climap::iterator iter =m_cli_map.find(pcli->get_cli_info()->addr_info);
    if(iter !=m_cli_map.end()) //存在就先删除
    {
        iter->second->close_socket();
        boost::unique_lock<boost::shared_mutex> lockfree(m_cli_free_mutex); //写锁
        m_cli_free.push_back(iter->second);
        m_cli_map.erase(iter);
        //LOG_MSG(WRN_LOG,"xtcp_server::add_client() delete existed client");
    }
    //LOG_MSG(WRN_LOG,"xtcp_server::add_client() added new client key=%s",pcli->get_cli_info()->addr_info);
    m_cli_map.insert(_climap::value_type(pcli->get_cli_info()->addr_info,pcli));
    return 0;
}

_cliptr find_client(std::string cli_key) //寻找客户端
{
    boost::shared_lock<boost::shared_mutex> lock(m_cli_map_mutex); //读锁
    _climap::iterator iter =m_cli_map.find(cli_key);
    if(iter !=m_cli_map.end())
    {
        return iter->second;
    }
    else
        return _cliptr();
}

int del_client(std::string cli_key) //断开并删除某客户端
{
    boost::unique_lock<boost::shared_mutex> lock(m_cli_map_mutex); //写锁
    _climap::iterator iter =m_cli_map.find(cli_key);
    if(iter !=m_cli_map.end())
    {
        iter->second->reset_buff();
        boost::unique_lock<boost::shared_mutex> lockfree(m_cli_free_mutex); //写锁
        m_cli_free.push_back(iter->second);
        m_cli_map.erase(iter);
        //LOG_MSG(WRN_LOG,"xtcp_server::del_client() delete client key=%s",cli_key.c_str());
        return 1;
    }
    //LOG_MSG(WRN_LOG,"Exited xtcp_server::del_client() delete client key=%s",cli_key.c_str());
    return 0;
}

int del_all_client() //断开并删除所有客户端
{
    if(m_cli_map.size() ==0) return 0;
    boost::unique_lock<boost::shared_mutex> lock(m_cli_map_mutex); //写锁
    boost::unique_lock<boost::shared_mutex> lockfree(m_cli_free_mutex); //写锁
    for(_climap::iterator iter=m_cli_map.begin(); iter!=m_cli_map.end(); ++iter)//切记不可以在这里erase
    {
        iter->second->close_socket();
        m_cli_free.push_back(iter->second);
    }
    m_cli_map.clear();
    return 0;
}

private:
CALLBK_FN                       m_fn_callbk;    //服务器上层回调函数
_climap                         m_cli_map;      //工作中客户端集合
_clilist                        m_cli_free;     //空闲客户端集合
boost::shared_mutex             m_cli_map_mutex;//客户端集合读写锁
boost::shared_mutex             m_cli_free_mutex;//空闲客户端集合读写锁
boost::asio::ip::tcp::endpoint  m_server_addr;  //服务器本地地址
boost::asio::ip::tcp::acceptor  m_acceptor;     //服务器监听套接字
xtcp_client_node *              m_cli_now[MAXIOTHRDNUM];//每个IOSERVICE线程当前正在回调的客户端
unsigned char                   m_extern[64];   //扩展数据
};

#define MAX_UDPMSG_NUM  1024
class xudp_socket : public boost::asio::ip::udp::socket //udp socket类
{
public:
    xudp_socket(boost::asio::io_service& ioservice) : boost::asio::ip::udp::socket(ioservice),m_packer(new xpacker()),m_unpacker(new xunpacker()) //构造函数
    {
        m_is_sending =false;
        m_is_dispatching =false;
    }
    virtual ~xudp_socket() {} //析构函数

public:
    void reset_status() //复位以便重复使用它
    {
        boost::mutex::scoped_lock locks(m_mutex_recv);
        m_list_recv.clear();
        m_is_dispatching = false;
        locks.unlock();

        boost::mutex::scoped_lock lockr(m_mutex_send);
        m_list_send.clear();
        m_is_sending = false;
        lockr.unlock();
    }

    void close_socket() //关闭套接字
    {
        boost::system::error_code err_code;
        shutdown(boost::asio::ip::tcp::socket::shutdown_both,err_code);
        close(err_code);
        m_unpacker->reset_data();
    }

    int send_data(const char *pdata,size_t data_len,const char *addr_to,unsigned short port_to,xpacker::PACKWAY pack_way =xpacker::PACK_NATIVE) //打包数据并发送
    {
        if(!this->is_ok()) return -1;
        boost::shared_ptr<const boost::asio::ip::udp::endpoint> paddr(new boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(addr_to),port_to));
        return send_native_data(m_packer->pack_data(pdata,data_len,pack_way),paddr);
    }

    int send_data(const char *pdata,size_t data_len,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr_to,xpacker::PACKWAY pack_way =xpacker::PACK_NATIVE) //打包数据并发送
    {
        if(!this->is_ok()) return -1;
        return send_native_data(m_packer->pack_data(pdata,data_len,pack_way),addr_to);
    }

    size_t numof_pending_data() //获得缓冲区中待发送的数据包个数
    {
        boost::mutex::scoped_lock lock(m_mutex_send);
        return m_list_send.size();
    }

    const boost::shared_ptr<const std::string> peek_first_pending() //查看第一个待发送的数据
    {
        boost::mutex::scoped_lock lock(m_mutex_send);
        return m_list_send.empty() ? boost::shared_ptr<const std::string>() : m_list_send.front();
    }

    boost::shared_ptr<const std::string> cancel_first_pending() //取消第一个待发送的数据并返回该数据
    {
        boost::shared_ptr<const std::string> pack;
        boost::mutex::scoped_lock lock(m_mutex_send);
        if(m_list_send.empty()) return boost::shared_ptr<const std::string>();
        pack = m_list_send.front();
        m_list_send.pop_front();
        return pack;
    }

    void cancel_all_pending(boost::container::list<boost::shared_ptr<const std::string> > &list_bak) //取消所有待发送的数据
    {
        boost::mutex::scoped_lock lock(m_mutex_send);
        list_bak.splice(list_bak.end(),m_list_send);
    }

    void set_packer(xpacker *new_packer)     {m_packer.reset(new_packer);} //运行时更改编码器
    void set_unpacker(xunpacker *new_unpacker) {m_unpacker.reset(new_unpacker);} //运行时更改解码器
    boost::shared_ptr<xpacker>     get_packer()     {return m_packer;}     //获得编码器
    boost::shared_ptr<xunpacker>   get_unpacker()   {return m_unpacker;}   //获得解码器

public:
    virtual bool is_ok() {return is_open();}
    void start_recv() {do_recv_data();} //启动异步接收

protected:
    virtual void on_unpack_error() {} //数据解包错误
    virtual void on_recv_error(const boost::system::error_code& err_code,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) {} //数据接收错误,一般不用通过靠这个函数来判断连接断开
    virtual bool on_data_recv(boost::shared_ptr<const std::string>& pack,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) {} //同步通知收到一条数据包
    virtual void on_data_recv_async(boost::shared_ptr<const std::string>& pack,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) {} //异步通知收到一条数据包,如果on_data_recv返回false,则该函数将不会被调用
    virtual void on_data_send(boost::shared_ptr<const std::string>& pack,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) {} //一条消息被发送到内核
    virtual void on_recv_buffer_overflow(boost::shared_ptr<const std::string>& pack,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) {} //接收缓冲区溢出

private:
    void handler_dispatch_async(boost::shared_ptr<const std::string>& pack,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) //异步数据派发回调函数
    {
        on_data_recv_async(pack,addr); //通知上层异步数据派发
        boost::mutex::scoped_lock lock(m_mutex_recv);
        m_is_dispatching = false;
        do_dispatch_async(); //开始下一次异步数据派发
    }

    void do_dispatch_async() //开始异步派发数据,调用前必须锁定接收缓存
    {
        boost::asio::io_service& ioservice =get_io_service();
        if(ioservice.stopped())
        {
            m_is_dispatching = false;
        }
        else if(!m_is_dispatching && !m_list_recv.empty())
        {
            m_is_dispatching = true;
            boost::shared_ptr<const std::string> pack = m_list_recv.front();
            boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr = m_addr_recv.front();
            m_list_recv.pop_front();
            m_addr_recv.pop_front();
            ioservice.post(boost::bind(&xudp_socket::handler_dispatch_async,this,pack,addr)); //插入一个异步派发任务
        }
    }

    void do_dispatch(boost::container::list<boost::shared_ptr<const std::string> > &list_data,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) //同步派发收到的数据
    {
        if(list_data.empty()) return;
        boost::mutex::scoped_lock lock(m_mutex_recv);

        unsigned int data_num =m_list_recv.size();
        BOOST_FOREACH(boost::shared_ptr<const std::string>& item,list_data)
        {
            if(on_data_recv(item,addr)) //同步通知用户收到数据包,返回true就把消息加到缓存准备异步派发
            {
                if(data_num <MAX_UDPMSG_NUM) {m_list_recv.push_back(item); m_addr_recv.push_back(addr); ++data_num;} //自身待通知的数据包缓存没满
                else
                    on_recv_buffer_overflow(item,addr); //缓冲区溢出消息
            }
        }
        do_dispatch_async(); //将所有消息执行异步派发
        lock.unlock();
    }

    void handler_recv(const boost::system::error_code& err_code, size_t bytes_data) //async_receive_from直接触发的回调函数
    {
        boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr(new boost::asio::ip::udp::endpoint(m_addr_pack));
        if(!err_code && bytes_data >0)
        {
            boost::container::list<boost::shared_ptr<const std::string> > list_data;
            bool unpack_ok =m_unpacker->unpack_data(bytes_data, list_data); //解包器开始解包
            if(!list_data.empty()) do_dispatch(list_data,addr); //解码成功,开始派发数据
            if(unpack_ok) on_unpack_error(); //通知解包错误,可能需要关闭该套接字
            do_recv_data(); //开始投递下一次接收
        }
        else
        {
            boost::system::error_code err_code_copy(err_code);
            on_recv_error(err_code_copy,addr);
            if(err_code.value() !=0x3E3) do_recv_data(); //非UDP终止,开始投递下一次接收
        }
    }

    void handler_send(const boost::system::error_code& err_code, size_t bytes_data,boost::shared_ptr<const std::string>& pack,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) //async_send_to直接触发的回调函数
    {
        if(!err_code && bytes_data >0)
        {
            on_data_send(pack,addr);
        }
        else
        {
            boost::system::error_code err_code_copy(err_code);
            on_send_error(err_code_copy,addr);
        }

        boost::mutex::scoped_lock lock(m_mutex_send);
        m_is_sending = false;
        do_send_data(); //继续投递下一个包的发送
    }

    void do_send_data() //调用前必须锁定发送缓冲
    {
        if(!this->is_ok())
        {
            m_is_sending = false;
            m_list_send.clear();
        }
        else if(!m_is_sending && !m_list_send.empty())
        {
            m_is_sending = true;
            boost::shared_ptr<const std::string> pack = m_list_send.front(); //取出第一个待发送数据包执行发送
            boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr = m_addr_send.front(); //取出第一个待发送地址执行发送
            async_send_to(*addr,boost::asio::buffer(*pack),boost::bind(&xudp_socket::handler_send,this,boost::placeholders::error,boost::placeholders::bytes_transferred,pack,addr));
            m_list_send.pop_front(); //将该数据从待发送缓冲区删除
            m_addr_send.pop_front(); //将该地址从待发送地址删除
        }
    }

    void do_recv_data() //开始异步接收数据
    {
        size_t min_recv_len =0;
        boost::asio::mutable_buffers_1 recv_buff = m_unpacker->prepare_buff(min_recv_len);
        async_receive_from(recv_buff,m_addr_pack,boost::bind(&xudp_socket::handler_recv,this,boost::placeholders::error,boost::placeholders::bytes_transferred));
    }

    int send_native_data(const boost::shared_ptr<const std::string>& pack,const boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) //发送原始数据包
    {
        if(!pack || pack->empty()) return -1;
        boost::mutex::scoped_lock lock(m_mutex_send);
        if(m_list_send.size() > MAX_UDPMSG_NUM) return -2;
        m_list_send.push_back(pack);
        m_addr_send.push_back(addr);
        do_send_data();
        return pack->length();
    }

private:
    bool                                                            m_is_sending;     //发送投递标志
    bool                                                            m_is_dispatching;//派发投递标志
    boost::mutex                                                    m_mutex_send;    //发送缓冲互斥量
    boost::mutex                                                    m_mutex_recv;    //接收缓冲互斥量
    boost::container::list<boost::shared_ptr<const std::string> >    m_list_send;     //数据发送链表
    boost::container::list<boost::shared_ptr<const std::string> >    m_list_recv;     //数据接收链表
    boost::container::list<boost::shared_ptr<const boost::asio::ip::udp::endpoint> > m_addr_send;//数据发送地址链表
    boost::container::list<boost::shared_ptr<const boost::asio::ip::udp::endpoint> > m_addr_recv;//数据接收地址链表
protected:
    boost::shared_ptr<xpacker>     m_packer;        //打包器
    boost::shared_ptr<xunpacker>   m_unpacker;      //解包器
    boost::asio::ip::udp::endpoint m_addr_pack;     //UDP包裹地址
};

class xudp_terminal : public xudp_socket //UDP终端
{
public:
    typedef std::function<void(xudp_terminal *client,char *ip_addr,unsigned short udp_port,char *data,int len)> CALLBK_FN; //回调函数定义

public:
    xudp_terminal() : xudp_socket(*get_share_ioservice())
    {
        get_share_ioservice()->start_serve();
    }
    virtual ~xudp_terminal() //析构函数
    {
        get_share_ioservice()->stop_serve();
    }

    static xioservice *get_share_ioservice() //所有udp对象共享同一个io异步服务
    {
        static xioservice s_io_service; //使用C++11特性时此处会互斥，所以是线程安全的
        return &s_io_service;
    }

public:
    void set_callback(CALLBK_FN fn) {m_fn_callbk =fn;} //设置客户端回调函数
    void set_local_addr(char *ip_addr,unsigned short udp_port) //设置本地地址
    {
        if(!ip_addr || ip_addr[0]=='\0') m_local_addr =boost::asio::ip::udp::v4(),udp_port;
        std::string now_addr =boost::asio::ip::address::from_string(ip_addr).to_string();
        if(now_addr !=ip_addr || m_local_addr.port()!=udp_port) m_local_addr =boost::asio::ip::address::from_string(ip_addr),udp_port;
    }

    int get_local_addr(char *ip_buff,unsigned short *port) //获得本地地址
    {
        if(ip_buff &&!now_addr.empty()) memcpy(ip_buff,now_addr.c_str(),now_addr.length()>16?16:now_addr.length());
        if(port) *port =m_local_addr.port();
        return 0;
    }

    int start_udp() //开始UDP
    {
        close_socket();
        boost::system::error_code err_code;
        open(m_local_addr.protocol(),err_code);
        if(err_code) return -1;
        set_option(boost::asio::socket_base::reuse_address(true),err_code);
        if(err_code) return -1;
        bind(m_local_addr,err_code);
        if(err_code) return -1;
        if(!is_ok()) return -1;
        start_recv();
        return 0;
    }

    void stop_udp() //停止UDP
    {
        close_socket();
    }

public:
    virtual bool is_ok() {return (xudp_socket::is_ok() &&!get_share_ioservice()->stopped());} //是否可以发送数据了

protected:
    virtual void on_data_recv_async(boost::shared_ptr<const std::string>& pack,boost::shared_ptr<const boost::asio::ip::udp::endpoint> addr) //接收数据异步通知
    {
        if(!m_fn_callbk) return;
        std::string from_addr =addr->address().to_string();
        m_fn_callbk(this,(char *)from_addr.c_str(),addr->port(),(char *)pack->data(),pack->length()); //回调到上层
    }

private:
    CALLBK_FN                       m_fn_callbk;    //服务器上层回调函数
    boost::asio::ip::udp::endpoint  m_local_addr;   //本地绑定地址
};

#endif
