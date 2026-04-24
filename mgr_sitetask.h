#ifndef MGR_SITETASK_H_H
#define MGR_SITETASK_H_H

#include "queue.h"
#include "xbasic.hpp"
#include "xbasicmgr.h"
#include "xbasicasio.hpp"
#include "xconfig.h"
#include "xsite_package.h"
#include "xsitectrl_session.h"
#include "mgr_usb.h"
#include "mgr_device.h"
#include "mgr_serdes.h"
#include "mgr_pcie.h"
#include "mgr_network.h"
#include "mgr_sitectrl_sync.h"
#include <unordered_map>

//升级总超时为180分钟
#define TASKPROGRESS_TIMEOUT        (180*60)
//升级状态检测间隔(S)
#define TASKPROGRESS_CHECK_INTERVAL 30

extern int g_startfork;
class mgr_sitectrl_sync;
class sitetask_msg
{
public:
    sitetask_msg();
    sitetask_msg(const std::string& port_name,boost::shared_ptr<xsite_package> package);
    sitetask_msg(const sitetask_msg& other);
    sitetask_msg(sitetask_msg&& other);
    sitetask_msg& operator=(const sitetask_msg& other);
    sitetask_msg& operator=(sitetask_msg&& other);
    ~sitetask_msg();

public:
    std::string                         m_port_name;
    boost::shared_ptr<xsite_package>    m_packdata;
};

class xtaskprogress
{
public:
    enum TASKSTATUS{TASKSTATUS_NONE,TASKSTATUS_STARTED,TASKSTATUS_COMPLETE};
    enum TASKLOCK{TASKLOCK_IDLE,TASKLOCK_LOCKED,TASKLOCK_UNLOCKED};
    enum TASKRESULT{TASKRESULT_NONE,TASKRESULT_SUCESS,TASKRESULT_FAILED};
private:
    xtaskprogress();
    ~xtaskprogress();
public:
    xtaskprogress(const xtaskprogress&) = delete;
    xtaskprogress& operator=(const xtaskprogress&) = delete;
    static xtaskprogress* get_instance();

public:
    //设置任务状态
    void set_status(int boardid,int status);
    //设置任务开始状态
    void set_startstatus(int boardid);
    //设置任务结束状态
    void set_completestatus(int boardid);
    //设置任务结果
    void set_taskresult(int boardid,int res);
    //设置任务开始时间
    void set_starttime(time_t st_tm);
    //获取任务开始时间
    time_t get_starttime();
    //获取任务状态
    bool get_status(int boardid,int& status);
    //获取任务结果
    bool get_taskresult(int boardid,int& res);
    //绑定任务固定的应答消息
    int bind_sitectrlresp(sitetask_msg& sitectrl_resp);
    //获取任务绑定的应答消息
    boost::shared_ptr<sitetask_msg> get_sitetaskmsg();
    //设置task单板集合数量lock状态
    void set_tasksize_lockstatus(int lkstatus);
    //获取task单板集合数量状态
    int get_tasksize_lockstatus();
    //获取task单板集合数量
    int get_task_size();
    //获取所有单板升级状态集合
    boost::unordered_map<int,int> get_taskprogress_map();
    //清空单板升级状态集合
    void clear();
    //删除单板升级状态集合元素
    void del(int boardid);

private:
    //单板状态集合读写锁
    boost::shared_mutex                   m_mux_progress;
    //即装状态集合,保存每个单板升级状态
    boost::unordered_map<int,int>         m_map_progress;
    //单板结果集合读写锁
    boost::shared_mutex                   m_mux_result;
    //即装结果集合,保存每个单板升级状态
    boost::unordered_map<int,int>         m_map_result;
    //sitectrl的服务端的回复消息
    //回复消息读写锁
    boost::shared_mutex                   m_mux_resp;
    boost::shared_ptr<sitetask_msg>       m_sitectrl_resp;
    //任务开始时间
    time_t                                m_starttime;
    //表示单板集合数量还会发生变化
    int                                   m_tasklocked;
};

class mgr_sitetask : public xmgr_basic<mgr_sitetask>
{
public:
    mgr_sitetask();
    ~mgr_sitetask();

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
    //任务放入任务队列
    int push_task(const char* name,xpacket* packet);

protected:
    //work线程处理函数
    virtual int main_process();
    //处理任务
    int handle_task(const char* port_name,xsite_package* site_pack);
    //处理升级命令消息
    int handle_request(const char* port_name,xsite_package* site_pack);
    //回复应答
    int send_reply(const char* port_name,xsite_package* site_pack);
    //检查任务完成进度
    int check_taskprogress();
    //检查PCIE板/8KISBB板上任务
    int check_nonsync_task();
    //检查sync板上任务
    int check_sync_task();
    //是否延迟回复
    bool isdelay_reply();

private:
    //挂载nfs目录
    bool mount_upgrade_nfs();
    //判断nfs目录是否为一个挂载点
    bool is_mount_point(const char* path);
    //得到消息类型
    int get_msgtype(const std::string& msg_type);
    //处理升级命令消息
    int process_pkg_upgrade(xsite_package *pack,boost::shared_ptr<xcmd> cmd);
    //解包tar.gz包
    int unpacker_pkg(xsite_package *pack,boost::shared_ptr<xcmd> cmd);
    //检查是否转发升级消息
    int check_forward_upgrade();
    //检查x86系统
    int check_x86_forward();
    //检查mcu、fpga进程
    int check_mcu_fpga_forward();
    //进行本地升级处理
    int process_pkg_localupgrade(xsite_package *pack,boost::shared_ptr<xcmd> cmd);
    //分类pkg包中不同的升级接口
    int classify_pkg_interface();
    //加载pkg升级文件前缀
    int load_pkg_prefix(std::vector<std::string>& vect_prefix);
    //读取升级文件列表
    int read_upgrade_filelist(const std::string& path,std::vector<std::string>& vect_file,const std::vector<std::string>& vect_prefix);
    //从文件名中得出升级接口类型
    int get_pkg_interface_type(const std::string& filename);
    //进行升级设备管理升级转发
    int process_pkg_forwardupgrade(xsite_package *pack);
    //向sync板上报升级成功通知
    int notify_taskprogress(int boardid, int status, int result);

private:
    //linux/arm下shell脚本方式升级
    int pkg_shell_upgrade(xsite_package *pack,boost::shared_ptr<xcmd> cmd);
    //USB串口方式升级---(旧) 有同类型单板选择同一个软件包进行升级
    int pkg_usb_upgrade(const xpkg_interface::FileList& filelist);
    //USB串口方式升级---(新) 根据不同的硬件单板选择不同的升级包进行升级
    int pkg_usb_upgrade_ex(const xpkg_interface::FileList& filelist);
    //TCP网络方式升级
    int pkg_tcp_upgrade(const xpkg_interface::FileList& filelist);
    //Serdes驱动方式升级---(旧) 有同类型单板选择同一个软件包进行升级
    int pkg_serdesdrv_upgrade(const xpkg_interface::FileList& filelist);
    //Serdes驱动方式升级---(新) 根据不同的硬件单板选择不同的升级包进行升级
    int pkg_serdesdrv_upgrade_ex(const xpkg_interface::FileList& filelist);
    //PCIE驱动方式升级
    int pkg_pciedrv_upgrade(const xpkg_interface::FileList& filelist);
    //SPI驱动方式升级
    int pkg_spidrv_upgrade(const xpkg_interface::FileList& filelist);
    //初始化文件名前缀与板类型映射表
    int init_boardtype_map();
    //初始化文件名前缀与OTA文件类型映射表
    int init_otatype_map();
    //从boardid中获取板类型
    int get_boardtype(const std::string& filename,int& board_type);
    //从升级文件名中获取OTA文件类型
    bool get_otatype(const std::string& filename,int& ota_type);
    //删除文件
    int removefile(const std::string& path, const std::string& filename);
    //移动文件
    int movefile(const std::string& src, const std::string& dst);
    //添加boardid到任务进度
    int add_taskprogress(const int& boardtype);
    //添加特定单板任务进度
    void add_one_board_taskprogress(const int& board_id);
    //从某一类单板中选取出一块单板添加任务
    void add_one_board_task_progress(const int& board_type, int& board_id);
    //用转发的boardid加入任务进度
    int add_forward_taskprogress();
    //检查是否有需要升的单板
    bool check_board_exist();
    //查找某一个类型单板的数量
    int find_board_num(const int& boardtype);
    //绑定task应答消息
    int bind_taskmsg(const std::string port_name,boost::shared_ptr<xsite_package> siteresp);
    int set_tasksize_status(int status);
    int get_slottype(const std::string& str);
    std::string get_slottype_str(int boardtype);

    //清空目录
    int clear_dir(const std::string& dir_path);
    //检查硬件单板所需软件版本是否存在
    bool check_ver_exist(int pkg_type);
    bool check_board_ver(int ota_type, int board_type);
    //移动文件
    int remove_file(const std::string& path, const std::string& filename);
    //解析升级请求中携带的硬件关系
    void parse_hardware_map(xsite_package *pack,boost::shared_ptr<xcmd> cmd);
    //清理升级进度
    void clear_taskprogress();

protected:
    //系统配置
    xconfig                 *m_sys_config;
    //网络管理
    mgr_network             *m_network_mgr;
    //sitectrl sync同步管理
    mgr_sitectrl_sync       *m_sitectrl_sync_mgr;

private:
    //文件名前缀与板类型映射表
    std::unordered_map<std::string,int>   m_boardtype_map;
    //文件名前缀与OTA类型映射表
    std::unordered_map<std::string,int>   m_otatype_map;
    //任务队列
    SafeQueue<sitetask_msg>               m_task_queue;
    //退出标识
    int                                   m_exited;
    //是否nfs挂载成功
    bool                                  m_mount_nfs;

};

#endif
