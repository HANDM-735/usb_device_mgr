#ifndef XSINGLE_CHIP_H_H
#define XSINGLE_CHIP_H_H

#include "xpackage.hpp"
#include <boost/thread/shared_mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <list>

class xboard        //这个类保存单片机返回实际usb协议格式数据
{
public:
    xboard();
    xboard(int id);
    ~xboard();
public:
    void set_value(unsigned short tid,std::string value,unsigned char cmd);
    void set_value(unsigned short tid, int rk3588_slotid, std::string value,unsigned char cmd);
    int  get_tlv_data(std::list<boost::shared_ptr<xtlv> > &list_tlv,unsigned char cmd);
    int  get_tlv_data_from_type(int board_id,short tlv_type,std::list<boost::shared_ptr<xtlv> > &list_tlv);
    boost::shared_ptr<xtlv> find_data_from_type(short dtype,std::list<boost::shared_ptr<xtlv> > &Allist_tlv);
    std::list<boost::shared_ptr<xtlv> > find_data(unsigned short tid);
    boost::shared_ptr<xtlv> find_rccdata(int rk3588_slotid);
    std::map<int,boost::shared_ptr<xtlv> > get_rccfpgpa_temp();

public:
    time_t  get_last_recv_tm()          {return m_recvlastTime;}      //获得最后接收数据时间
    void    set_last_recv_tm(time_t t)  {m_recvlastTime = t;}
    void    set_board_ip(std::string ip_addr)  { m_ip = ip_addr;}
    void    set_board_port(std::string port)   { m_port = port;}
    std::string get_board_ip()                  {return m_ip;}
    std::string get_board_port()                {return m_port;}

private:
    int                                         m_id;                    //单片机id
    std::list<boost::shared_ptr<xtlv> >         m_recv_data;             //数据集合
    std::string                                 m_ip;                    //单片机端ip地址
    std::map<int,boost::shared_ptr<xtlv> >      m_rccfpgpa_temp;         //RCC板Fpga温度
    boost::shared_mutex                         m_rcc_rctemp;            //RCC板Fpga温度读写锁
    boost::shared_mutex                         m_mux_data;
    time_t                                      m_recvlastTime;          //最后一次接收数据的时间
    std::string                                 m_ip;                    //单片机端所需的ip地址
    std::string                                 m_port;                  //单片机端所需的ip的port端口
};

class xboardmgr
{
private:
    xboardmgr() {}
    ~xboardmgr() {}

public:
    xboardmgr(const xboardmgr&) = delete;
    xboardmgr& operator=(const xboardmgr&) = delete;

    static xboardmgr* get_instance() {
        static xboardmgr kInstance;
        return &kInstance;
    }

    boost::unordered_map<int, boost::shared_ptr<xboard>> get_all_board();
    boost::shared_ptr<xboard> find_board(int board_id);
    boost::shared_ptr<xboard> add_board(int board_id);
    void del_board(int board_id);

private:
    boost::shared_mutex     m_mux_board;       //单片集合读写锁
    boost::unordered_map<int,boost::shared_ptr<xboard>>   m_map_board;    //即单片集合,保存每个单板的实时监控数据
};

#endif
