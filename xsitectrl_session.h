#ifndef XSITECTRL_SESSION_H_H
#define XSITECTRL_SESSION_H_H

#include "xbasicmgr.h"
#include "xconfig.h"
#include "xsite_package.h"
#include <string>
#include <map>
#include <memory>
#include <unordered_map>

//消息类型
#define SITECTRL_MSG_NONE      0x00      //无效
#define SITECTRL_MSG_REQUEST   0x01      //请求消息
#define SITECTRL_MSG_NOTIFY    0x02      //指示消息
#define SITECTRL_MSG_RESPOND   0x03      //应答消息
#define SITECTRL_MSG_BROADCAST 0x04      //广播消息

#define VER_TYPE_NONE     0x00      //无效
#define VER_TYPE_LINUX    0x01      //linux
#define VER_TYPE_MCU      0x02      //MCU
#define VER_TYPE_FPGA     0x03      //逻辑

//PKG文件升级接口类别
#define PKG_TYPE_NONE      0x00      //无效
#define PKG_TYPE_X86       0x01      //X86升级类型
#define PKG_TYPE_USB       0x02      //USB串口升级类型
#define PKG_TYPE_TCP       0x03      //TCP网络升级类型
#define PKG_TYPE_SERDES_DRV 0x04     //SERDES驱动升级类型
#define PKG_TYPE_PCIE_DRV  0x05      //PCIE驱动升级类型
#define PKG_TYPE_SPI_DRV   0x06      //SPI驱动升级类型
#define PKG_TYPE_ARM       0x07      //ARM升级类型

//转发类型
#define PKG_FORWARD_TYPE_NONE   0x00   //无效
#define PKG_FORWARD_TYPE_LOCAL  0x01   //无需转发
#define PKG_FORWARD_TYPE_REMOTE 0x02   //需要转发
#define PKG_FORWARD_TYPE_MASK   0x03

#define STA_IDLE      0
#define STA_UPGRADING 1

enum setup_site_option_type
{
    NETWORKIF,
    SITECNT,
    RCPU,
    OPTRCPU,
    NOCHAMBER,
};

class ver_info
{
public:
    ver_info();
    ver_info(const std::string& name, const std::string& ver);
    ver_info(const ver_info& other);
    ver_info operator=(const ver_info& other);
    ~ver_info();
public:
    //版本信息类型
    int             type;
    //版本名称
    std::string     m_name;
    //版本
    std::string     m_ver;
};

//Linux系统上Lib库、进程的版本信息
class sys_version
{
public:
    typedef std::map<std::string,std::shared_ptr<ver_info> > SysVerMap;

public:
    sys_version();
    sys_version(const sys_version& other);
    sys_version& operator=(const sys_version& other);
    ~sys_version();

public:
    std::shared_ptr<ver_info> add_version(std::string name, std::string ver);
    std::shared_ptr<ver_info> find_version(const std::string& name);
    // 清除 sysver 版本（预留接口，暂未测试）
    bool clear_sysver();
    SysVerMap  get_sysver();

public:
    SysVerMap         m_sys_ver;
    //SysVerMap
    boost::shared_mutex   m_mux_sysver;
};

class board_version
{
public:
    typedef std::map<std::string, std::shared_ptr<ver_info> > LogicVerMap;

public:
    board_version();
    board_version(const board_version& other);
    board_version& operator=(const board_version& other);
    ~board_version();

public:
    void update_info(const board_version& other);

public:
    std::shared_ptr<ver_info> add_logicver(const std::string& name, const ver_info& logic_ver);
    std::shared_ptr<ver_info> find_logicver(const std::string& name);
    LogicVerMap get_logicver();

public:
    //板类型
    int                 m_boardtype;
    //板槽位号
    int                 m_boardslot;
    //单片机软件版本
    ver_info            m_mcu_ver;
    //单片机硬件版本
    ver_info            m_board_ver;
    //不同fpga的逻辑版本
    LogicVerMap         m_logic_ver;
    //LogicVerMap读写锁
    boost::shared_mutex   m_mux_logicver;
};

class site_version
{
public:
    typedef std::map<int,std::shared_ptr<board_version> > BoardVerMap;

public:
    site_version();
    site_version(const site_version& other);
    site_version& operator=(const site_version& other);
    ~site_version();

public:
    std::shared_ptr<board_version> add_boardver(int boardid, const board_version& board_ver);
    // 删除这个区域上某一块单板的版本信息（预留接口，暂未测试）
    bool clear_oneboardver(int boardid);
    // 删除这个区域上所有单板的版本信息（预留接口，暂未测试）
    bool clear_allboardver();
    std::shared_ptr<board_version> find_boardver(int boardid);
    BoardVerMap get_boardver();

public:
    sys_version         m_sys_version;
    BoardVerMap         m_board_version;
    //BoardVerMap读写锁
    boost::shared_mutex   m_mux_boardver;
};

class xsitever_mgr
{
    typedef std::map<int,std::shared_ptr<site_version> > SiteVerMap;
private:
    xsitever_mgr();
    ~xsitever_mgr();

public:
    xsitever_mgr(const xsitever_mgr& ) = delete;
    xsitever_mgr& operator=(const xsitever_mgr& ) = delete;
    static xsitever_mgr* get_instance();

public:
    std::shared_ptr<site_version> add_sysver(int x86_id, const sys_version& sys_ver);
    std::shared_ptr<site_version> add_boardver(int x86_id, int boardid, const board_version& board_ver);
    // 清除一个区域上的 sys ver（预留接口，暂未测试）
    bool clear_sysver(int x86_id);
    // 清除一个区域上某一块单板的 board ver（预留接口，暂未测试）
    bool clear_oneboardver(int x86_id, int boardid);
    // 清除一个区域上保存的所有 board ver（预留接口，暂未测试）
    bool clear_allboardver(int x86_id);
    // 清除一个区域
    bool clear_site(int x86_id);
    std::shared_ptr<site_version> find_sitever(int x86_id);
    SiteVerMap  get_allsitever();
private:
    SiteVerMap        m_sitever_map;
    //集合读写锁
    boost::shared_mutex   m_mux_sitever;
};

//PKG包中升级文件的升级接口类
class xpkg_interface
{
public:
    typedef std::vector<std::string> FileList;
    typedef std::map<int, std::shared_ptr<FileList> > PkgInterfaceMap;

private:
    xpkg_interface();
    ~xpkg_interface();

public:
    xpkg_interface(const xpkg_interface& ) = delete;
    xpkg_interface& operator=(const xpkg_interface& ) = delete;
    static xpkg_interface* get_instance();

public:
    std::shared_ptr<xpkg_interface::FileList> add(const int interface_type, const std::string& filename);
    std::shared_ptr<FileList> find(const int type);
    int clear();
    PkgInterfaceMap get_pkgmap();

public:
    PkgInterfaceMap     m_pkginterface_map;
    //集合读写锁
    boost::shared_mutex   m_mux_pkgmap;
};

// site_processors 信息
class site_processors
{
public:
    site_processors();
    site_processors(const std::string& number, const std::string& test_unit_number, const std::string& test_site_number, const std::string& ip_address,
        const std::string& host_name, const std::string& booted_date_time, const std::string& os_image_version, const std::string& boot_method);
    site_processors(const site_processors& other);
    site_processors& operator=(const site_processors& sp);
    ~site_processors();

public:
    void update_info(const site_processors& other);

public:
    std::string         m_number; //site_processors编号
    std::string         m_test_unit_number;
    std::string         m_test_site_number;
    std::string         m_ip_address;
    std::string         m_host_name;
    std::string         m_booted_date_time;
    std::string         m_os_image_version;
    std::string         m_boot_method;
};

class site_processors_mgr
{
public:
    typedef std::unordered_map<int, std::shared_ptr<site_processors>> site_processors_map_t;
private:
    site_processors_mgr();
    ~site_processors_mgr();

public:
    site_processors_mgr(const site_processors_mgr& spm) = delete;
    site_processors_mgr& operator=(const site_processors_mgr& spm) = delete;
    static site_processors_mgr* get_instance();

public:
    std::shared_ptr<site_processors> add_site_processor(int board_id, const site_processors& site_pro);
    site_processors_map_t get_allsite_processor();
    std::shared_ptr<site_processors> find_site_processor(int board_id);

private:
    site_processors_map_t   m_site_processors_map;
    boost::shared_mutex     m_mux_sitepro;
    static site_processors_mgr* m_instance; // 单例实例
    static std::mutex       m_mtx; // 单例线程安全锁
};

// rcpus 信息
class rcpus
{
public:
    rcpus();
    rcpus(const std::string& rcpu_number, const std::string& test_unit_number, const std::string& test_site_number, const std::string& repair_site_number,
        const std::string& ip_address, const std::string& host_name, const std::string& booted_date_time, const std::string& os_image_version,
        const std::string& boot_method);
    rcpus(const rcpus& other);
    rcpus& operator=(const rcpus& other);
    ~rcpus();

public:
    void update_info(const rcpus& other);

public:
    std::string         m_rcpu_number;
    std::string         m_test_unit_number;
    std::string         m_test_site_number;
    std::string         m_repair_site_number;
    std::string         m_ip_address;
    std::string         m_host_name;
    std::string         m_booted_date_time;
    std::string         m_os_image_version;
    std::string         m_boot_method;
};

// rcpus 管理类
class rcpus_mgr
{
public:
    typedef std::unordered_map<int, std::shared_ptr<rcpus>> rcpus_map_t;
private:
    rcpus_mgr();
    ~rcpus_mgr();

public:
    rcpus_mgr(const rcpus_mgr& other) = delete;
    rcpus_mgr& operator=(const rcpus_mgr& other) = delete;
    static rcpus_mgr* get_instance();

public:
    std::shared_ptr<rcpus> add_rcpu(int board_id, const rcpus& other);
    std::shared_ptr<rcpus> find_rcpu(int board_id);
    rcpus_map_t get_allrcpus();

private:
    rcpus_map_t         m_rcpus_map;
    boost::shared_mutex m_mux_rcpus;
    static rcpus_mgr*   m_instance; // 单例实例
    static std::mutex   m_mtx;
};

// satellite_processors 信息
class satellite_processors
{
public:
    satellite_processors();
    satellite_processors(const std::string& number,const std::string& ip_address, const std::string& host_name, const std::string& booted_date_time,
        const std::string& os_image_version, const std::string& boot_method);
    satellite_processors(const satellite_processors& other);
    satellite_processors& operator=(const satellite_processors& other);
    ~satellite_processors();

public:
    void update_info(const satellite_processors& other);

public:
    std::string         m_number;
    std::string         m_ip_address;
    std::string         m_host_name;
    std::string         m_booted_date_time;
    std::string         m_os_image_version;
    std::string         m_boot_method;
};

// satellite_processors 管理类
class satellite_processors_mgr
{
public:
    typedef std::unordered_map<int, std::shared_ptr<satellite_processors>> satellite_processors_map_t;
private:
    satellite_processors_mgr();
    ~satellite_processors_mgr();

public:
    satellite_processors_mgr(const satellite_processors_mgr& other) = delete;
    satellite_processors_mgr& operator=(const satellite_processors_mgr& other) = delete;
    static satellite_processors_mgr* get_instance();

public:
    std::shared_ptr<satellite_processors> add_satellite_processors(int board_id, const satellite_processors& other);
    std::shared_ptr<satellite_processors> find_satellite_processors(int board_id);
    satellite_processors_map_t get_allsatepro();

private:
    satellite_processors_map_t  m_satellite_processors_map;
    boost::shared_mutex         m_mux_satepro;
    static satellite_processors_mgr* m_instance;
    static std::mutex           m_mtx;
};

// option_rcpus 信息
class option_rcpus
{
public:
    option_rcpus();
    option_rcpus(const std::string& number, const std::string& ip_address, const std::string& host_name, const std::string& booted_date_time,
        const std::string& opt_rcpu_base_pkg_revision);
    option_rcpus(const option_rcpus& other);
    option_rcpus& operator=(const option_rcpus& other);
    ~option_rcpus();

public:
    void update_info(const option_rcpus& other);

public:
    std::string         m_number;
    std::string         m_ip_address;
    std::string         m_host_name;
    std::string         m_booted_date_time;
    std::string         m_opt_rcpu_base_pkg_revision;
};

// option_rcpus 管理类
class option_rcpus_mgr
{
public:
    typedef std::unordered_map<int, std::shared_ptr<option_rcpus>> option_rcpus_map_t;
private:
    option_rcpus_mgr();
    ~option_rcpus_mgr();

public:
    option_rcpus_mgr(const option_rcpus_mgr& other) = delete;
    option_rcpus_mgr& operator=(const option_rcpus_mgr& other) = delete;
    static option_rcpus_mgr* get_instance();

public:
    std::shared_ptr<option_rcpus> add_option_rcpus(int board_id, const option_rcpus& other);
    std::shared_ptr<option_rcpus> find_option_rcpus(int board_id);
    option_rcpus_map_t get_all_option_rcpus();

private:
    option_rcpus_map_t      m_option_rcpu_map;
    boost::shared_mutex     m_mux_optionrcpu;
    static option_rcpus_mgr* m_instance;
    static std::mutex       m_mtx;
};

// chamber_cpus 信息
class chamber_cpus
{
public:
    chamber_cpus();
    chamber_cpus(const std::string& chamber_cpu_number, const std::string& chamber_number, const std::string& ip_address, const std::string& host_name,
        const std::string& booted_date_time, const std::string& software_revision_booted);
    chamber_cpus(const chamber_cpus& other);
    chamber_cpus& operator=(const chamber_cpus& other);
    ~chamber_cpus();

public:
    void update_info(const chamber_cpus& other);

public:
    std::string         m_chamber_cpu_number;
    std::string         m_chamber_number;
    std::string         m_ip_address;
    std::string         m_host_name;
    std::string         m_booted_date_time;
    std::string         m_software_revision_booted;
};

// chamber_cpus 管理类
class chamber_cpus_mgr
{
public:
    typedef std::unordered_map<int, std::shared_ptr<chamber_cpus>> chamber_cpus_map_t;
private:
    chamber_cpus_mgr();
    ~chamber_cpus_mgr();

public:
    chamber_cpus_mgr(const chamber_cpus_mgr& other) = delete;
    chamber_cpus_mgr& operator=(const chamber_cpus_mgr& other) = delete;
    static chamber_cpus_mgr* get_instance();

public:
    std::shared_ptr<chamber_cpus> add_chamber_cpus(int board_id, const chamber_cpus& other);
    std::shared_ptr<chamber_cpus> find_chamber_cpus(int board_id);
    chamber_cpus_map_t get_all_chamber_cpus();

private:
    chamber_cpus_map_t      m_chamber_cpus_map;
    boost::shared_mutex     m_mux_chamber_cpus;
    static chamber_cpus_mgr* m_instance;
    static std::mutex       m_mtx;
};

// testsites 信息
class testsites
{
public:
    testsites();
    testsites(const std::string& test_unit_number, const std::string& test_site_number, const std::string& site_processor_number,
        const std::string& tester_bus_number, const std::string& ip_address, const std::string& host_name, const std::string& physical_stations,
        const std::string& chamber_number);
    testsites(const testsites& other);
    testsites& operator=(const testsites& other);
    ~testsites();

public:
    void update_info(const testsites& other);

public:
    std::string         m_test_unit_number;
    std::string         m_test_site_number;
    std::string         m_site_processor_number;
    std::string         m_tester_bus_number;
    std::string         m_ip_address;
    std::string         m_host_name;
    std::string         m_physical_stations;
    std::string         m_chamber_number;
};

// testsites 管理类
class testsites_mgr
{
public:
    typedef std::unordered_map<int, std::shared_ptr<testsites>> testsites_map_t;
private:
    testsites_mgr();
    ~testsites_mgr();

public:
    testsites_mgr(const testsites_mgr& other) = delete;
    testsites_mgr& operator=(const testsites_mgr& other) = delete;
    static testsites_mgr* get_instance();

public:
    std::shared_ptr<testsites> add_testsites(int board_id, const testsites& other);
    std::shared_ptr<testsites> find_testsites(int board_id);
    testsites_map_t get_all_testsites();

private:
    testsites_map_t     m_testsites_map;
    boost::shared_mutex m_mux_testsites;
    static testsites_mgr*   m_instance;
    static std::mutex       m_mtx;
};

// repairsites 信息
class repairsites
{
public:
    repairsites();
    repairsites(const std::string& test_unit_number, const std::string& test_site_number, const std::string& repair_site_number, const std::string& rcpu_number,
        const std::string& ip_address, const std::string& host_name, const std::string& rcb_numbers);
    repairsites(const repairsites& other);
    repairsites& operator=(const repairsites& other);
    ~repairsites();

public:
    void update_info(const repairsites& other);

public:
    std::string         m_test_unit_number;
    std::string         m_test_site_number;
    std::string         m_repair_site_number;
    std::string         m_rcpu_number;
    std::string         m_ip_address;
    std::string         m_host_name;
    std::string         m_rcb_numbers;
};

// repairsites 管理类
class repairsites_mgr
{
public:
    typedef std::unordered_map<int, std::shared_ptr<repairsites>> repairsites_map_t;
private:
    repairsites_mgr();
    ~repairsites_mgr();

public:
    repairsites_mgr(const repairsites_mgr& other) = delete;
    repairsites_mgr& operator=(const repairsites_mgr& other) = delete;
    static repairsites_mgr* get_instance();

public:
    std::shared_ptr<repairsites> add_repairsites(int board_id, const repairsites& other);
    std::shared_ptr<repairsites> find_repairsites(int board_id);
    repairsites_map_t get_all_repairsites();

private:
    repairsites_map_t   m_repairsites_map;
    boost::shared_mutex m_mux_repairsites;
    static repairsites_mgr* m_instance;
    static std::mutex   m_mtx;
};

// stations 信息
class stations
{
public:
    stations();
    stations(const std::string& physical_station_number, const std::string& test_unit_number);
    stations(const stations& other);
    stations& operator=(const stations& other);
    ~stations();

public:
    void update_info(const stations& other);

public:
    std::string         m_physical_station_number;
    std::string         m_test_unit_number;
};

// stations 管理类
class stations_mgr
{
public:
    typedef std::unordered_map<int, std::shared_ptr<stations>> stations_map_t;
private:
    stations_mgr();
    ~stations_mgr();

public:
    stations_mgr(const stations_mgr& other) = delete;
    stations_mgr& operator=(const stations_mgr& other) = delete;
    static stations_mgr* get_instance();

public:
    std::shared_ptr<stations> add_stations(int board_id, const stations& other);
    std::shared_ptr<stations> find_stations(int board_id);
    stations_map_t get_all_stations();

private:
    stations_map_t      m_stations_map;
    boost::shared_mutex m_mux_stations;
    static stations_mgr* m_instance;
    static std::mutex   m_mtx;
};

//SiteContr会话
class xsitectrl_session : public xtransmitter
{
public:
    xsitectrl_session(const std::string& session_id,const std::string& port_name);
    ~xsitectrl_session();

public:
    //通讯相关接口
    virtual int on_recv(const char *port_name,xsite_package *pack);

public:

protected:
    int get_msgtype(const std::string& msg_type);
    int handle_request(const char *port_name,xsite_package *pack);
    int handle_notify(const char *port_name,xsite_package *pack);
    int handle_response(const char *port_name,xsite_package *pack);
    int handle_broadcast(const char *port_name,xsite_package *pack);

protected:
    //处理device_data命令消息
    int process_device_data(xsite_package *pack,boost::shared_ptr<xcmd> cmd);
    //处理setup_site命令请求
    int process_setup_site(xsite_package *pack,boost::shared_ptr<xcmd>& cmd);
    //执行setup_site
    int exec_setup_site(boost::shared_ptr<xcmd>& cmd);
    int exec_networkif(const std::string& value);
    int exec_sitecnt(const std::string& value);
    int exec_rcpucnt(const std::string& value);
    int exec_optrcpucnt(const std::string& value);
    int exec_nochamber(const std::string& value);
    //获取setup_site指令选项的类型
    int get_setup_site_type(const std::string& option);
    //处理upgrade命令消息
    int process_upgrade(xsite_package *pack,boost::shared_ptr<xcmd> cmd);
    //处理reboot命令消息
    int process_reboot(xsite_package *pack,boost::shared_ptr<xcmd> cmd);
    //处理reboot_confirm确认命令消息通知
    int process_reboot_confirm(xsite_package *pack,boost::shared_ptr<xcmd> cmd);
    std::string get_slotttype_str(int boardtype);

protected:

private:
    //会话消息id
    std::string         m_sess_id;
    //会话端口
    std::string         m_port_name;
};

#endif
