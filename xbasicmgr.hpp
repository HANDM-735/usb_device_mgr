#ifndef X_MGR_BASIC_H
#define X_MGR_BASIC_H
#include <set>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include "xbasic.hpp"
#include "xpackage.hpp"

class xlistener //监听器
{
public:
    enum NET_MSG {NET_CONNECTED =1,NET_DISCONNECTED,NET_DATA}; //网络消息类型
public:
    xlistener() {}
    void add_filter(std::string filt_flag) //向筛选器添加过滤选项
    {
        m_set_filter.insert(filt_flag);
    }
    bool judge_filter(std::string flag) //判断是否在筛选项里
    {
        return (m_set_filter.find(flag)!=m_set_filter.end());
    }

public:
    //(注意不可以在下列函数内添加或移除监听器)
    virtual int on_network(NET_MSG msg_type,const char *port_name,xpackage *packet) {return 0;} //网络通知
    virtual int on_message(const char *msg_type,const char *channel,void *msg_data) {return 0;} //消息通知
protected:
    std::set<std::string> m_set_filter;   //过滤器
};

class xtransmitter //传输器
{
public:
    typedef std::function<int(const char *,xpackage *)> SEND_DATA_FN; //代理发送函数定义
public:
    xtransmitter(std::string obj_name) : m_obj_name(obj_name) {}
public:
    void set_proxy_send(SEND_DATA_FN fn)    {m_send_fn =fn;}    //设置代理发送接口
public:
    virtual int send(const char *port_name,xpackage *pack) //发送接口
    {
        int ret =-1;
        if(m_send_fn)  ret =m_send_fn(port_name,pack); //调用代理
        /*if(pack->need_confirm()) //是请启用缓存重发机制的包
        {
            boost::unique_lock<boost::shared_mutex> lock(m_mux_packet); //写锁
            if(m_lst_packet.size() >512) m_lst_packet.pop_front(); //最多缓存512个报文
            m_lst_packet.push_back(boost::shared_ptr<xpackage>(pack->clone()));
        }*/
        return ret;
    }

    virtual int send_cache(const char *port_name) //发送缓存包接口
    {
        if(m_lst_packet.size() <=0) return 0;
        boost::shared_lock<boost::shared_mutex> lock(m_mux_packet); //读锁
        boost::shared_ptr<xpackage> packet =m_lst_packet.front();
        lock.unlock();
        if(m_send_fn) return m_send_fn(port_name,packet.get()); //调用代理
        return -1;
    }

    virtual int on_recv(const char *port_name,xpackage *pack) //消息通知
    {
        if(pack->type_confirm()) //是确认包,检查缓存是否有需要清理的包
        {
            boost::unique_lock<boost::shared_mutex> lock(m_mux_packet); //写锁
            for(std::list<boost::shared_ptr<xpackage> >::iterator iter=m_lst_packet.begin(); iter!=m_lst_packet.end(); iter++)
            {
                boost::shared_ptr<xpackage> packet =*iter;
                if(pack->is_confirm(packet.get())) {m_lst_packet.erase(iter); break;} //找到对应的请求包,删除
            }
        }

        boost::shared_lock<boost::shared_mutex> lock(m_mux_listener); //读锁
        for(std::list<xlistener *>::iterator iter=m_lst_listener.begin(); iter!=m_lst_listener.end(); iter++)
        {
            xlistener *listener =*iter;
            listener->on_message(m_obj_name.c_str(),port_name,pack); //向监听器回调消息
        }
        return m_lst_listener.size();
    }

    virtual int add_listener(xlistener *new_listener) //添加监听器
    {
        boost::unique_lock<boost::shared_mutex> lock(m_mux_listener); //写锁
        for(std::list<xlistener *>::iterator iter=m_lst_listener.begin(); iter!=m_lst_listener.end(); iter++)
        {
            xlistener *listener =*iter;
            if(listener ==new_listener) return 0; //已经存在
        }
        m_lst_listener.push_back(new_listener);
        return 1;
    }

    virtual void del_listener(xlistener *del_listener) //移除监听器
    {
        boost::unique_lock<boost::shared_mutex> lock(m_mux_listener); //写锁
        for(std::list<xlistener *>::iterator iter=m_lst_listener.begin(); iter!=m_lst_listener.end();)
        {
            if((xlistener *)(*iter) !=del_listener) {iter++; continue;}
            else                        {m_lst_listener.erase(iter); return;}
        }
    }

protected:
    std::string                             m_obj_name;      //对象名称
    SEND_DATA_FN                            m_send_fn;       //代理发送函数
    std::list<xlistener * >                 m_lst_listener;  //监听器集合
    boost::shared_mutex                     m_mux_listener;  //监听器集合读写锁
    std::list<boost::shared_ptr<xpackage> > m_lst_packet;    //需要重试的发送包缓存
    boost::shared_mutex                     m_mux_packet;    //需要重试的发送包集合读写锁
};

template<typename T>
class xmgr_basic : public xlistener //管理器基类
{
public:
    static T *get_instance() //单例模式对象获得
    {
        //static T s_instance; //使用C++11特性时此处会互斥,所以是线程安全的
        //return &s_instance;
        if(m_instance == NULL)  //第一重检测, 如果未初始化
        {
            std::lock_guard<std::mutex> lck(m_mtx);  //上锁, RAII,离开if{}自动解锁
            if(m_instance == NULL) { //第二重检测,还未初始化
                m_instance = new T();
            }
        }
        return m_instance;
    }

    static void release_instance()
    {
        std::lock_guard<std::mutex> lck(m_mtx);
        if(m_instance != NULL) {
            delete m_instance;
            m_instance = NULL;
        }
    }

    virtual ~xmgr_basic() {}
protected:
    xmgr_basic()
    {
        m_work_sign =false;
        m_work_cycle =1000;
    }

public:
    virtual int start_work(int work_cycle =1000) //开始工作
    {
        xbasic::debug_output("enter into xmgr_basic::start_work()\n");
        if(m_work_sign) return 0; //已经运行
        this->init(); //初始化
        m_work_sign = true;
        m_work_cycle = work_cycle;
        boost::thread tmp_thread(boost::bind(&xmgr_basic::work_thread,this));
        boost::this_thread::yield();
        tmp_thread.swap(m_work_thread);
        xbasic::debug_output("exited xmgr_basic::start_work()\n");
        return 1;
    }

    virtual void stop_work() //停止工作
    {
        xbasic::debug_output("enter into xmgr_basic::stop_work()\n");
        m_work_sign = false;
        m_work_thread.join();
        xbasic::debug_output("exited xmgr_basic::stop_work()\n");
    }

    virtual bool is_working()               {return m_work_sign;} //是否正在工作中
    virtual void init()                     {} //初始化
    virtual void work(unsigned long ticket) {} //工作函数
private:
    void work_thread() //工作线程
    {
        unsigned long ticket =0;
        while(m_work_sign)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_work_cycle));
            this->work(ticket++);
        }
    }

protected:
    boost::thread       m_work_thread;  //工作线程
    bool                m_work_sign;    //工作标志
    unsigned int        m_work_cycle;   //工作周期ms
private:
    static T*           m_instance;     //单例实例
    static std::mutex   m_mtx;          //单例多线程安全锁
};

template<typename T>
T* xmgr_basic<T>::m_instance = NULL;
template<typename T>
std::mutex xmgr_basic<T>::m_mtx;

#endif
