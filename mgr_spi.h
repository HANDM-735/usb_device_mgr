#ifndef MGR_SPI_H_H
#define MGR_SPI_H_H

#include "xbasicmgr.h"
#include "basethread.h"
#include <boost/thread/mutex.hpp>
#include <memory>
#include <string>
#include <unordered_map>

#if (defined(PGB_BUILD) && defined(DEV_TYPE_FT)) || (defined(SYNC_BUILD) && defined(DEV_TYPE_CP))
    //此处添加SPI驱动升级接口头文件,to do ...
    #include "libMidDig.h"
#endif

class spi_strategy : public commstrategy
{
public:
    //Serdes升级状态定义
    enum EXITSTATUS{SUCCESS = 0,FAILED};
    //回调函数定义
    typedef std::function<void(commstrategy*,EXITSTATUS,int,int,const char*)> CALLBK_FN;

public:
    spi_strategy();
    ~spi_strategy();

public:
    virtual void execute() override ;

public:
    //设置执行状态回调函数
    void set_callback(CALLBK_FN fn);
    void set_targetid(const int& targetid);
    void set_spitype(const int& serdestype);
    void set_spifile(const std::string& filename);

private:
    //spi驱动target_id(板类型+槽位号)
    int             m_target_id;
    //spi的文件类型
    int             m_spi_type;
    //spi文件名称(包含路径)
    std::string     m_filename;
    //执行是否结束
    int             m_exited;
    //执行状态回调函数
    CALLBK_FN       m_fn_callback;
};

class xspi
{
public:
    //SPI升级状态定义
    enum SPISTATUS{SPI_STATRTED = 1,SPI_COMPLETE};
    enum SPIRESULT{SPI_NONE = 0,SPI_SUCSESS = 1, SPI_FAILED};
    //SPI升级线程类定义
    typedef basethread<commstrategy> SPIThread;
    //回调函数定义
    typedef std::function<void(xspi*,SPISTATUS,SPIRESULT,int,const char*)> CALLBK_FN;

public:
    xspi();
    xspi(int boardid);
    ~xspi();

public:
    //SPI执行状态回调函数
    void on_spi_exec(commstrategy* strategy,spi_strategy::EXITSTATUS status,int targetid ,int spitype, const char* filename);
    //设置SPI升级状态回调函数
    void set_callback(CALLBK_FN fn);
    //启动SPI升级
    void send_ota_start(const int& boardid,int ota_type,const std::string& filename);

private:
    //通过boardid,获得SPI驱动接口所需的targetid
    bool get_spi_targetid(const int& boardid,int& targetid);
    //通过ota_type, 获得SPI驱动接口所需fpga type
    bool get_spi_type(const int& ota_type,int& spi_type);

private:
    //spi线程
    std::shared_ptr<SPIThread>      m_thread;
    std::shared_ptr<commstrategy>   m_strategy;
    //线程start标识
    int                             m_started;
    //spi对应boardid
    int                             m_boardid;
    //Serdes升级状态回调函数
    CALLBK_FN                       m_fn_callback;
};

//spi管理类
class mgr_spi : public xmgr_basic<mgr_spi>
{
public:
    typedef std::shared_ptr<xspi> XSpiPtr;

public:
    mgr_spi();
    ~mgr_spi();

public:

public:
    //初始化
    virtual void init();
    //工作函数
    virtual void work(unsigned long ticket);
    //开始工作
    virtual int start_work(int work_cycle = 1000);
    //停止工作
    virtual void stop_work();

public:
    void on_spi_status(xspi* obj,xspi::SPISTATUS status,xspi::SPIRESULT res,int boardid,const char* filename);
    int send_all_start(int boardtype, int ota_type,const std::string& filename);
    int send_ota_start(int boardid, int ota_type, const std::string& filename);
    XSpiPtr add_spi(int boardid);
    XSpiPtr find_spi(int boardid);

private:
    std::unordered_map<int,XSpiPtr>         m_spi_map;
    boost::shared_mutex                     m_spi_mutex;
};

#endif
