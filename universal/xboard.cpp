#include "xboard.h"
#include "xconfig.h"
#include "mgr_log.h"

xboard::xboard()
{

}

xboard::xboard(int id):m_id(id)
{
    m_recvLastTime = time(NULL);
    m_ip = "127.0.0.1";
}

xboard::~xboard()
{

}

void xboard::set_value(unsigned short tid,std::string value,unsigned char cmd) //设置指定数据
{
    if(xconfig::debug() >= 1)
    {
        XDATAPRO *data_pro = xusb_tvl::get_datapro(tid);
        if(data_pro && (data_pro->vtype=='S' || data_pro->vtype=='J'))
        {
            LOG_MSG(MSG_LOG,"xboard::set_value() vtype=%c board:%02XH setdata:%02XH-%s ",data_pro->vtype,m_tid,value.c_str());
        }
        else
        {
            char print_buff[2052] ={0};
            xbasic::hex_to_str((unsigned char *)value.data(),print_buff,value.length()>1024?1024:value.length());
            LOG_MSG(MSG_LOG,"xboard::set_value() board:%02XH setdata:%02XH<%s> ",m_tid,print_buff);
        }
    }
    boost::shared_ptr<xusb_tvl> tlv = find_data(tid);
    if(tlv != NULL)
    {
        boost::unique_lock<boost::shared_mutex> lock(m_mux_data); //写锁
        tlv->set_data(tid,value.length(),value.data());
        return  //已经存在该数据,修改其值
    }
    boost::unique_lock<boost::shared_mutex> lock(m_mux_data); //写锁
    boost::shared_ptr<xusb_tvl> new_data(new xusb_tvl(tid,value.length(),value.data(),cmd)); //没有该数据项,新增一个
    m_map_data.insert(std::make_pair(tid,new_data));
}

void xboard::set_value(unsigned short tid, int rk3588_slotid,std::string value,unsigned char cmd)
{
    if(xconfig::debug() >= 1)
    {
        XDATAPRO *data_pro = xusb_tvl::get_datapro(tid);
        if(data_pro && (data_pro->vtype=='S' || data_pro->vtype=='J'))
        {
            LOG_MSG(MSG_LOG,"xboard::set_value() vtype=%c board:%02XH rk3588_slotid:%d setdata:%02XH-%s ",data_pro->vtype,m_tid,rk3588_slotid,value.c_str());
        }
        else
        {
            char print_buff[2052] ={0};
            xbasic::hex_to_str((unsigned char *)value.data(),print_buff,value.length()>1024?1024:value.length());
            LOG_MSG(MSG_LOG,"xboard::set_value() board:%02XH rk3588_slotid:%d setdata:%02XH<BIN> ",m_tid,rk3588_slotid,print_buff);
        }
    }
    //1、通过rk3588_slotid从m_rcafpga_temp中查找查找元素
    boost::shared_ptr<xusb_tvl> tlv = find_data(rk3588_slotid);
    if(tlv != NULL)
    {
        boost::unique_lock<boost::shared_mutex> lock(m_mux_rcatemp); //写锁
        tlv->set_data(tid,value.length(),value.data());
        return  //已经存在该数据,修改其值
    }
    boost::unique_lock<boost::shared_mutex> lock(m_mux_rcatemp); //写锁
    boost::shared_ptr<xusb_tvl> new_data(new xusb_tvl(tid,value.length(),value.data(),cmd)); //没有该数据项,新增一个
    m_rcafpga_temp.insert(std::make_pair(rk3588_slotid,new_data));
    return ;
}

int xboard::set_tlv_data(std::list<boost::shared_ptr<xusb_tvl>> &list_tlv,unsigned char cmd) //将TLV结构数组设置到单片机
{
    for(std::list<boost::shared_ptr<xusb_tvl>>::iterator iter=list_tlv.begin(); iter!=list_tlv.end(); iter++)
    {
        boost::shared_ptr<xusb_tvl> tlv =*iter;
        if(tlv->m_value.length() <= 0) {
            continue; //不带值
        }
        set_value(tlv->m_tid,tlv->m_value,cmd);
    }
    return list_tlv.size();
}

int xboard::set_tlv_data(int rk3588_slotid,std::list<boost::shared_ptr<xusb_tvl>> &list_tlv,unsigned char cmd)
{
    for(std::list<boost::shared_ptr<xusb_tvl>>::iterator iter=list_tlv.begin(); iter!=list_tlv.end(); iter++)
    {
        boost::shared_ptr<xusb_tvl> tlv =*iter;
        if(tlv->m_value.length() <= 0) {
            continue; //不带值
        }
        set_value(tlv->m_tid,rk3588_slotid,tlv->m_value,cmd);
    }
    return list_tlv.size();
}

boost::shared_ptr<xusb_tvl> xboard::find_data(unsigned short tid)
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_data); //读锁
    std::map<int,boost::shared_ptr<xusb_tvl>>::iterator itdata =m_map_data.find((int)tid);
    if(itdata == m_map_data.end()) return boost::shared_ptr<xusb_tvl>();
    return itdata->second;
}

boost::shared_ptr<xusb_tvl> xboard::find_rcadata(int rk3588_slotid)
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_rcatemp); //读锁
    std::map<int,boost::shared_ptr<xusb_tvl>>::iterator itdata =m_rcafpga_temp.find(rk3588_slotid);
    if(itdata ==m_rcafpga_temp.end()) return boost::shared_ptr<xusb_tvl>();
    return itdata->second;
}

std::map<int,boost::shared_ptr<xusb_tvl>> xboard::get_rcafpga_temp()
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_rcatemp); //读锁
    return m_rcafpga_temp;
}

int xboard::get_tlv_data_from_type(char dtype,std::list<boost::shared_ptr<xusb_tvl>> &list_tlv) //按数据类型获取单片机的TLV数据
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_data); //读锁
    for(std::map<int,boost::shared_ptr<xusb_tvl>>::iterator itdata=m_map_data.begin(); itdata!=m_map_data.end(); ++itdata)
    {
        boost::shared_ptr<xusb_tvl> tlv = itdata->second;
        XDATAPRO *proper =xusb_tvl::get_datapro(tlv->m_tid);
        if(!proper) continue;
        if(dtype=='A' || proper->dtype ==dtype)
        {
            boost::shared_ptr<xusb_tvl> tlv_clone(tlv->clone());
            list_tlv.push_back(tlv_clone);
        }
    }
    return list_tlv.size();
}

boost::unordered_map<int,boost::shared_ptr<xboard>> xboardmgr::get_all_board() //获取单板所有数据
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_board); //读锁
    return m_map_board;
}

boost::shared_ptr<xboard> xboardmgr::find_board(int board_id) //寻找单板
{
    LOG_MSG(MSG_LOG,"Enter into xboardmgr::find_board() board_id=0x%x",board_id);
    boost::shared_lock<boost::shared_mutex> lock(m_mux_board); //读锁
    boost::unordered_map<int,boost::shared_ptr<xboard>>::iterator iter = m_map_board.find(board_id);
    if(iter != m_map_board.end())
    {
        return iter->second;
    }
    LOG_MSG(MSG_LOG,"Exited xboardmgr::find_board()");
    return boost::shared_ptr<xboard>();
}

boost::shared_ptr<xboard> xboardmgr::add_board(int board_id) //加入单板,如果已经存在,则返回该单板
{
    LOG_MSG(MSG_LOG,"Enter into xboardmgr::add_board()");

    boost::shared_ptr<xboard> the_board = find_board(board_id);
    if(the_board != NULL)
    {
        LOG_MSG(MSG_LOG,"xboardmgr::add_board(),board(boardid=0x%x) has existed.",board_id);
        the_board->set_last_recv_tm();
        return the_board; //已经存在
    }

    LOG_MSG(WRN_LOG,"xboardmgr::add_board() add new board:%02x",board_id);
    boost::unique_lock<boost::shared_mutex> lock(m_mux_board);    //写锁
    boost::shared_ptr<xboard> new_board(new xboard(board_id)); //新建单板对象
    m_map_board.insert(std::make_pair(board_id,new_board));    //将新单板加入到集合中

    LOG_MSG(MSG_LOG,"Exdit xboardmgr::add_board()");
    return new_board;
}

void xboardmgr::del_board(int board_id) //删除单板，mgr_board中单独管理单板删除操作
{
    boost::unique_lock<boost::shared_mutex> lock(m_mux_board); //写锁
    boost::unordered_map<int,boost::shared_ptr<xboard>>::iterator iter = m_map_board.find(board_id);
    if(iter != m_map_board.end()) m_map_board.erase(iter);
}
