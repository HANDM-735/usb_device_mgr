#include "mgr_spi.h"
#include "xbasic_def.h"
#include "mgr_usb_def.h"
#include "mgr_sitask.h"
#include "xota_session.h"
#include "xboard.h"
#include "mgr_log.h"

#define CPDIG_SPITYPE_FPGA      0
#define CPDPS_MASTER_MASK       0x1C
#define CPDPS_MASTER            0x00
#define CPDPS_SLAVE             0x08
#define FT_DIGINDEX_NUM         2

spi_strategy::spi_strategy()
{
    m_target_id   = -1;
    m_spi_type    = -1;
    m_exited      = -1;
}

spi_strategy::~spi_strategy()
{

}

void spi_strategy::execute()
{
    LOG_MSG(MSG_LOG,"Enter into spi_strategy::execute() m_target_id=0x%x m_spi_type=%d m_filename=%s",m_target_id,m_spi_type,m_filename.c_str());

#if (defined(SYNC_BUILD) && defined(DEV_TYPE_CP))
    //下面函数将由强渡云提供，暂时没有提供
    //强渡云只提供了接口定义，实现待实现，，先注释掉调用

    dig_up_t dig;
    dig.master_dps_slotid   = m_target_id;
    dig.dig_index           = 0;
    dig.filename            = const_cast<char*>(m_filename.c_str());

    LOG_MSG(WRN_LOG,"spi_strategy::execute() master_dps_slotid=0x%x dig_index=%d filename=%s",dig.master_dps_slotid ,dig.dig_index,dig.filename);
    m_exited = DIG_dig_Upgrade(&dig);

    if(m_exited == 0)
    {
        m_fn_callbk(this,SUCCESS,m_target_id,m_spi_type,m_filename.c_str());
        LOG_MSG(WRN_LOG,"spi_strategy::execute() m_target_id=0x%x m_spi_type=%d m_filename=%s success",m_target_id,m_spi_type,m_filename.c_str());
    }
    else
    {
        m_fn_callbk(this,FAILED,m_target_id,m_spi_type,m_filename.c_str());
        LOG_MSG(WRN_LOG,"spi_strategy::execute() m_target_id=0x%x m_spi_type=%d m_filename=%s failed",m_target_id,m_spi_type,m_filename.c_str());
    }

#elif (defined(PGB_BUILD) && defined(DEV_TYPE_FT))

    bool flag = true;
    for(int i = 0; i < FT_DIGINDEX_NUM; i++)
    {
        dig_up_t dig;
        dig.pgb_slotid    = m_target_id;
        dig.dig_index     = 0;
        dig.filename      = const_cast<char*>(m_filename.c_str());

        LOG_MSG(WRN_LOG,"spi_strategy::execute() pgb_slotid=0x%x dig_index=%d filename=%s",dig.pgb_slotid ,dig.dig_index,dig.filename);
        m_exited = DIG_dig_Upgrade(&dig);

        if(m_exited == 0)
        {
            LOG_MSG(WRN_LOG,"spi_strategy::execute() m_target_id=0x%x m_spi_type=%d dig_index=%d m_filename=%s success",m_target_id,m_spi_type,dig.dig_index,m_filename.c_str());
        }
        else
        {
            m_fn_callbk(this,FAILED,m_target_id,m_spi_type,m_filename.c_str());
            flag = false;
            LOG_MSG(WRN_LOG,"spi_strategy::execute() m_target_id=0x%x m_spi_type=%d dig_index=%d m_filename=%s failed",m_target_id,m_spi_type,dig.dig_index,m_filename.c_str());
            break;
        }
    }
    if(flag = true)
    {
        m_fn_callbk(this,SUCCESS,m_target_id,m_spi_type,m_filename.c_str());
    }
#else
#endif

    LOG_MSG(MSG_LOG,"Exited spi_strategy::execute()");
}

//设置执行状态回调函数
void spi_strategy::set_callback(CALLBK_FN fn)
{
    m_fn_callbk = fn;
}

void spi_strategy::set_targetid(const int& targetid)
{
    m_target_id = targetid;
    return;
}

void spi_strategy::set_spitype(const int& spitype)
{
    m_spi_type = spitype;
}

void spi_strategy::set_spifile(const std::string& filename)
{
    m_filename = filename;
}

xspi::xspi()
{
    m_started = 0;
}

xspi::xspi(int boardid)
{
    m_started = 0;
    m_boardid = boardid;
}

xspi::~xspi()
{

}

//SPI执行状态回调函数
void xspi::on_spi_exec(commstrategy* strategy,spi_strategy::EXITSTATUS status,int targetid ,int serdestype, const char* filename)
{
    LOG_MSG(MSG_LOG,"Enter into xspi::on_spi_exec() status=%d targetid=0x%x spitype=%d filename=%s",status,targetid,serdestype,filename);

    if(m_fn_callbk != NULL)
    {
        if(status == spi_strategy::SUCCESS)
        {
            m_fn_callbk(this,SPI_COMPLETE,SPI_SUCCESS,m_boardid,filename);
        }
        else
        {
            m_fn_callbk(this,SPI_COMPLETE,SPI_FAILED,m_boardid,filename);
        }
    }

    //释放线程共享指针对象
    m_thread.reset();
    //释放执行策略
    m_strategy.reset();
    m_started = 0;

    LOG_MSG(MSG_LOG,"Exited xspi::on_spi_exec()");
}

//设置SPI升级状态回调函数
void xspi::set_callback(CALLBK_FN fn)
{
    m_fn_callbk = fn;
}

int xspi::send_ota_start(const int& boardid,int ota_type,const std::string& filename)
{
    LOG_MSG(MSG_LOG,"Enter into xspi::send_ota_start() boardid=0x%x ota_type=%d filename=%s",boardid,ota_type,filename.c_str());

    int ret = 0;
    int drv_targetid = 0;
    int spi_type = 0;
    bool flag1 = get_spi_targetid(boardid,drv_targetid);
    if(!flag1)
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xspi::send_ota_start() boardid=0x%x filename=%s get_spi_targetid failed",boardid,filename.c_str());
        return ret;
    }
    bool flag2 = get_spi_type(ota_type,spi_type);
    if(!flag2)
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xspi::send_ota_start() boardid=0x%x filename=%s get_spi_type failed",boardid,filename.c_str());
        return ret;
    }

    //1、启动线程
    if(m_started == 0)
    {
        auto new_strategy = std::make_shared<spi_strategy>();

        //2、设置spi执行状态回调
        new_strategy->set_callback(std::bind(&xspi::on_spi_exec,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
        //3、设置spi_strategy相关属性成员
        new_strategy->set_targetid(drv_targetid);
        new_strategy->set_spitype(spi_type);
        new_strategy->set_spifile(filename);

        //保存执行策略，以延长生命周期
        m_strategy = new_strategy;

        m_thread = std::make_shared<SPIThread>(new_strategy);
        if(m_thread == NULL)
        {
            ret = -1;
        }
        else
        {
            //4、启动线程
            m_thread->start();
            m_thread->detach();
            if(m_fn_callbk != NULL)
            {
                m_fn_callbk(this,SPI_STATRTED,SPI_NONE,boardid,filename.c_str());
            }
            m_started = 1;
        }
    }
    else
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xspi::send_ota_start() serdes thread is upgrading,can't start ota boardid=0x%x filename=%s ",boardid,filename.c_str());
        return ret;
    }

    LOG_MSG(MSG_LOG,"Exited xspi::send_ota_start() drv_targetid=0x%x spi_type=%d ret=%d",drv_targetid,spi_type,ret);

    return ret;
}

//通过boardid,获得SPI驱动接口所需的targetid
bool xspi::get_spi_targetid(const int& boardid,int& targetid)
{
    LOG_MSG(MSG_LOG,"Enter into xspi::get_spi_targetid() boardid=0x%x",boardid);

    bool ret = false;
    int boardtype = (boardid & BOARDTYPE_MASK);
    int boardslot = ((boardid & SLOT_MASK) >> 8);
#if (defined(SYNC_BUILD) && defined(DEV_TYPE_CP))
    //目前只需支持CP DIG
    if(boardtype == BOARDTYPE_CP_DIG)
    {
        //计算规则，targetid为主DSP槽位号
        xboardmgr* mgr_board = xboardmgr::get_instance();
        boost::unordered_map<int, boost::shared_ptr<xboard>> board_map = mgr_board->get_all_board();

        for(auto it : board_map)
        {
            int dps_boardid = it.first;
            int dps_boardtype = (dps_boardid & BOARDTYPE_MASK);
            int dps_boardslot = ((dps_boardid & SLOT_MASK) >> 8);

            LOG_MSG(MSG_LOG,"xspi::get_spi_targetid() dps_boardid=0x%x dps_boardslot=0x%x",dps_boardid,dps_boardslot);

            if((dps_boardtype == BOARDTYPE_CP_DPS) && ((dps_boardslot & CPDPS_MASTER_MASK) == CPDPS_MASTER))
            {
                targetid = dps_boardslot;
                ret = true;
                LOG_MSG(WRN_LOG,"xspi::get_spi_targetid() dps_boardid=0x%x dps_boardslot=0x%x targetid=%d",dps_boardid,dps_boardslot,targetid);
                break;
            }
        }
    }
#else
    //待实现
#endif

    LOG_MSG(MSG_LOG,"Exited xspi::get_spi_targetid() boardid=0x%x targetid=0x%x ret=%d",boardid,targetid,ret);

    return ret;
}

//通过ota_type, 获得spi驱动接口所需fpga_type
bool xspi::get_spi_type(const int& ota_type,int& spi_type)
{
    LOG_MSG(MSG_LOG,"Enter into xspi::get_spi_type() ota_type=%d",ota_type);

    bool ret = false;
#if (defined(SYNC_BUILD) && defined(DEV_TYPE_CP))
    //目前只支持CP DIG的FPGA
    if(ota_type == OTA_TYPE_CPDIG_CDCU_FPGA)
    {
        //CPDIG板的FPGA type 暂时定义为0.
        //后续根据实际再修改,to do ...
        spi_type = CPDIG_SPITYPE_FPGA;
        ret = true;
    }
    else
    {
    }
#else
    //待实现
#endif

    LOG_MSG(MSG_LOG,"Exited xspi::get_spi_type() ota_type=%d spi_type=%d ret=%d",ota_type,spi_type,ret);
    return ret;
}

mgr_spi::mgr_spi()
{

}

mgr_spi::~mgr_spi()
{

}

//初始化
void mgr_spi::init()
{
    //to do ...
}

//工作函数
void mgr_spi::work(unsigned long ticket)
{
    //to do ...
}

//开始工作
int mgr_spi::start_work(int work_cycle)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_spi::start_work()");

    int ret = 0;
    //to do ...

    LOG_MSG(MSG_LOG, "Exited mgr_spi::start_work() ret=%d",ret);
    return ret;
}

//停止工作
void mgr_spi::stop_work()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_spi::stop_work()");
    //to do ...

    LOG_MSG(MSG_LOG, "Exited mgr_spi::stop_work()");
}

void mgr_spi::on_spi_status(xspi* obj,xspi::SPISTATUS status,xspi::SPIRESULT res,int boardid,const char* filename)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_spi::on_spi_status() boardid=0x%x status=%d res=%d filename=%s",boardid,status,res,filename);
    //to do ...
    if(status == xspi::SPI_COMPLETE)
    {
        xtaskprogress* xtask_progress = xtaskprogress::get_instance();
        xtask_progress->set_completestatus(boardid);
        if(res == xspi::SPI_SUCCESS)
        {
            xtask_progress->set_taskresult(boardid,xtaskprogress::TASKRESULT_SUCCESS);
        }
        else
        {
            xtask_progress->set_taskresult(boardid,xtaskprogress::TASKRESULT_FAILED);
        }
    }

    LOG_MSG(MSG_LOG,"Exited mgr_spi::on_spi_status()");
    return ;
}

int mgr_spi::send_all_start(int boardtype, int ota_type,const std::string& filename)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_spi::send_all_start() boardtype=0x%x ota_type=%d filename=%s",boardtype,ota_type,filename.c_str());

    int ret = 0;

    //1、获取所有boardid,并筛选符合board_type的地址
    std::vector<int> vect_boardid;
    int boardid;
    xboardmgr* mgr_board = xboardmgr::get_instance();
    boost::unordered_map<int, boost::shared_ptr<xboard>> board_map = mgr_board->get_all_board();

    for(auto it : board_map)
    {
        int board_id = it.first;
        if((board_id & BOARDTYPE_MASK) == boardtype)
        {
            vect_boardid.push_back(board_id);
        }
    }

    //2、逐个往对应的SPI地址发送升级指令
    int boardid_sz = 0;
    boardid_sz = vect_boardid.size();
    if(boardid_sz > 0)
    {
        for(int j = 0; j < boardid_sz; j++)
        {
            int dstid = vect_boardid[j];
            int is_ok = send_ota_start(dstid,ota_type,filename);
            if(is_ok != 0)
            {
                LOG_MSG(WRN_LOG,"mgr_spi::send_all_start() dstid=0x%x ota_type=%d failed",dstid,ota_type);
            }
        }
    }
    else
    {
        ret = -1;
        LOG_MSG(WRN_LOG,"mgr_spi::send_all_start() failed,board_type=%d ota_type=%d boardid_sz=%d ret=%d",boardtype,ota_type,boardid_sz,ret);
    }

    LOG_MSG(MSG_LOG,"Exited mgr_spi::send_all_start() boardid_sz=%d ret=%d",boardid_sz,ret);
    return ret;
}

int mgr_spi::send_ota_start(int boardid, int ota_type, const std::string& filename)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_spi::send_ota_start() boardid=0x%x ota_type=%d filename=%s",boardid,ota_type,filename.c_str());

    int ret = 0;
    //1、通过boardid查找对应spi对象
    XSpiPtr the_spi = add_spi(boardid);
    //2、设置xspi对象升级状态回调函数
    the_spi->set_callback(std::bind(&mgr_spi::on_spi_status,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    //3、开启spi对象升级操作线程
    ret = the_spi->send_ota_start(boardid,ota_type,filename);
    if(ret != 0)
    {
        LOG_MSG(ERR_LOG,"mgr_spi::send_ota_start() failed,ret=%d",ret);
    }

    LOG_MSG(MSG_LOG,"Exited mgr_spi::send_ota_start() ret=%d",ret);
    return ret;
}

mgr_spi::XSpiPtr mgr_spi::add_spi(int boardid)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_spi::add_spi() boardid=0x%x",boardid);

    XSpiPtr the_spi = find_spi(boardid);
    if(the_spi != NULL)
    {
        LOG_MSG(MSG_LOG,"mgr_spi::add_spi() board(boardid=0x%x) has existed.",boardid);
        //已经存在
        return the_spi;
    }

    LOG_MSG(WRN_LOG,"mgr_spi::add_spi() add new board:%02x",boardid);
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_spi_mutex);
    //新建serdes对象
    XSpiPtr new_spi(new xspi(boardid));
    //将新spi加入到集合中
    m_spi_map.insert(std::make_pair(boardid,new_spi));

    LOG_MSG(MSG_LOG,"Exited mgr_spi::add_spi()");

    return new_spi;
}

mgr_spi::XSpiPtr mgr_spi::find_spi(int boardid)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_spi::find_spi() board_id=0x%x",boardid);
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_spi_mutex);
    std::unordered_map<int,XSpiPtr>::iterator iter = m_spi_map.find(boardid);
    if(iter != m_spi_map.end())
    {
        return iter->second;
    }

    LOG_MSG(MSG_LOG,"Exited mgr_spi::find_spi()");

    return XSpiPtr();
}
