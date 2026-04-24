#include "mgr_sitetask.h"
#include "mgr_usb_def.h"
#include "mgr_upgrade.h"
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

#define TASKCHECK_SUCCESS   0
#define TASKCHECK_FAILED    -1
#define TASKCHECK_TIMEOUT   -2

int g_startfork = 0;

sitetask_msg::sitetask_msg()
{

}

sitetask_msg::sitetask_msg(const std::string& port_name,boost::shared_ptr<xsite_package> package) : \
    m_port_name(port_name),m_packdata(package)
{

}

sitetask_msg::~sitetask_msg()
{

}

sitetask_msg::sitetask_msg(const sitetask_msg& other)
{
    if(this != &other)
    {
        this->m_port_name = other.m_port_name;
        this->m_packdata = other.m_packdata;
    }
}

sitetask_msg& sitetask_msg::operator=(const sitetask_msg& other)
{
    if(this != &other)
    {
        this->m_port_name = other.m_port_name;
        this->m_packdata = other.m_packdata;
    }
    return *this;
}

sitetask_msg::sitetask_msg(sitetask_msg&& other)
{
    if(this != & other)
    {
        m_port_name = std::move(other.m_port_name);
        std::swap(other.m_packdata,this->m_packdata);
    }
}

sitetask_msg::sitetask_msg(const sitetask_msg&& other)
{
    if(this != & other)
    {
        m_port_name = std::move(other.m_port_name);
        this->m_packdata = other.m_packdata;
    }
}

sitetask_msg& sitetask_msg::operator=(sitetask_msg&& other)
{
    if(this != &other)
    {
        m_port_name = std::move(other.m_port_name);
        std::swap(other.m_packdata,this->m_packdata);
    }
    return *this;
}

sitetask_msg& sitetask_msg::operator=(const sitetask_msg&& other)
{
    if(this != &other)
    {
        m_port_name = std::move(other.m_port_name);
        this->m_packdata = other.m_packdata;
    }
    return *this;
}

xtaskprogress::xtaskprogress()
{

}

xtaskprogress::~xtaskprogress()
{

}

xtaskprogress* xtaskprogress::get_instance()
{
    static xtaskprogress instance;
    return &instance;
}

void xtaskprogress::set_status(int boardid,int status)
{
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::set_status() boardid=0x%x,status=%d",boardid,status);

    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_progress);
    boost::unordered_map<int,int>::iterator iter = m_map_progress.find(boardid);
    if(iter != m_map_progress.end())
    {
        //存在，更新状态值
        iter->second = status;
        LOG_MSG(MSG_LOG, "xtaskprogress::set_status() update boardid=0x%x,status=%d",boardid,status);
    }
    else
    {
        //不存在，则新建
        m_map_progress.insert(std::make_pair(boardid,status));
        LOG_MSG(MSG_LOG, "xtaskprogress::set_status() add boardid=0x%x,status=%d",boardid,status);
    }

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::set_status()");
}

//设置任务开始状态
void xtaskprogress::set_startstatus(int boardid)
{
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::set_startstatus() boardid=0x%x",boardid);

    int curr_status = TASKSTATUS_STARTED;
    set_status(boardid,curr_status);

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::set_startstatus() curr_status=%d",curr_status);
}

//设置任务结束状态
void xtaskprogress::set_completestatus(int boardid)
{
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::set_completestatus() boardid=0x%x",boardid);

    bool ret = false;
    int pre_status = TASKSTATUS_NONE;
    ret = get_status(boardid,pre_status);
    if(ret == true)
    {
        int curr_status = TASKSTATUS_COMPLETE;
        set_status(boardid,curr_status);
        LOG_MSG(WRN_LOG, "xtaskprogress::set_completestatus() boardid=0x%x,status(%d --> %d)",boardid,pre_status,curr_status);
    }

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::set_completestatus()");
}

//设置任务开始时间
void xtaskprogress::set_starttime(time_t st_tm)
{
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::set_starttime() st_tm=%lld",st_tm);

    m_starttime = st_tm;

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::set_starttime()");
}

//获取任务开始时间
time_t xtaskprogress::get_starttime()
{
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::get_starttime()");

    return m_starttime;

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::get_starttime()");
}

//获取任务状态
bool xtaskprogress::get_status(int boardid,int& status)
{
    bool ret = false;
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::get_status() boardid=0x%x",boardid);

    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_progress);
    boost::unordered_map<int,int>::iterator iter = m_map_progress.find(boardid);
    if(iter != m_map_progress.end())
    {
        status = iter->second;
        ret = true;
        LOG_MSG(MSG_LOG, "xtaskprogress::get_status() boardid=0x%x status=%d success",boardid,status);
    }
    else
    {
        status = TASKSTATUS_NONE;
        ret = false;
        LOG_MSG(WRN_LOG, "xtaskprogress::get_status() boardid=0x%x status=%d failed",boardid,status);
    }

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::get_status() status=%d,ret=%d",status,ret);
    return ret;
}

//设置任务结果
void xtaskprogress::set_taskresult(int boardid,int res)
{
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::set_taskresult() boardid=0x%x,status=%d",boardid,res);

    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_result);
    boost::unordered_map<int,int>::iterator iter = m_map_result.find(boardid);
    if(iter != m_map_result.end())
    {
        //存在，更新结果值
        iter->second = res;
        LOG_MSG(MSG_LOG, "xtaskprogress::set_taskresult() update boardid=0x%x res=%d",boardid,res);
    }
    else
    {
        //不存在，则新建
        m_map_result.insert(std::make_pair(boardid,res));
        LOG_MSG(WRN_LOG, "xtaskprogress::set_taskresult() add boardid=0x%x res=%d",boardid,res);
    }

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::set_taskresult()");
}

bool xtaskprogress::get_taskresult(int boardid,int& res)
{
    bool ret = false;
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::get_taskresult() boardid=0x%x",boardid);

    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_result);
    boost::unordered_map<int,int>::iterator iter = m_map_result.find(boardid);
    if(iter != m_map_result.end())
    {
        res = iter->second;
        ret = true;
        LOG_MSG(MSG_LOG, "xtaskprogress::get_taskresult() boardid=0x%x res=%d success",boardid,res);
    }
    else
    {
        res = TASKRESULT_NONE;
        ret = false;
        LOG_MSG(WRN_LOG, "xtaskprogress::get_taskresult() boardid=0x%x res=%d failed",boardid,res);
    }

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::get_taskresult() res=%d,ret=%d",res,ret);
    return ret;
}

//绑定任务的应答
int xtaskprogress::bind_sitectrlresp(sitetask_msg& sitectrl_resp)
{
    int ret = 0;
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::bind_sitectrlresp()");

    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_resp);
    boost::shared_ptr<sitetask_msg> the_msg(new sitetask_msg());
    *the_msg = sitectrl_resp;
    m_sitectrl_resp = the_msg;
    m_starttime = time(NULL);

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::bind_sitectrlresp()");

    return ret;
}

//获取任务绑定的应答消息
boost::shared_ptr<sitetask_msg> xtaskprogress::get_sitetaskmsg()
{
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::get_sitetaskmsg()");

    boost::shared_lock<boost::shared_mutex> lock(m_mux_resp);

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::get_sitetaskmsg()");

    return m_sitectrl_resp;
}

//设置task单板集合数量lock状态
void xtaskprogress::set_tasksize_lockstatus(int lkstatus)
{
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::set_tasksize_lockstatus() lkstatus=%d",lkstatus);
    //lkstatus: 0表示集合数量lock状态
    //TASKLOCK_LOCKED: 表示集合数量处于不稳定状态
    //TASKLOCK_UNLOCKED 表示集合数量已处于稳定状态
    m_tasklocked = lkstatus;

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::set_tasksize_lockstatus()");
}

//获取task单板集合数量状态
int xtaskprogress::get_tasksize_lockstatus()
{
    return m_tasklocked;
}

//获取task单板集合数量
int xtaskprogress::get_task_size()
{
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_progress);
    return m_map_progress.size();
}

//获取所有单板升级状态集合
boost::unordered_map<int,int> xtaskprogress::get_taskprogress_map()
{
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_progress);
    int sz = m_map_progress.size();
    LOG_MSG(MSG_LOG, "xtaskprogress::get_taskprogress_map() map_size=%d",sz);
    return m_map_progress;
}

//清空单板升级状态集合
void xtaskprogress::clear()
{
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_progress);
    LOG_MSG(MSG_LOG, "xtaskprogress::clear()");
    m_map_progress.clear();
    m_sitectrl_resp.reset();
}

//删除单板升级状态集合元素
void xtaskprogress::del(int boardid)
{
    LOG_MSG(MSG_LOG, "Enter into xtaskprogress::del()");

    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_progress);
    boost::unordered_map<int,int>::iterator it = m_map_progress.find(boardid);
    if(it != m_map_progress.end())
    {
        m_map_progress.erase(it);
    }

    LOG_MSG(MSG_LOG, "xtaskprogress::del() boardid=0x%x",it->first);

    LOG_MSG(MSG_LOG, "Exited xtaskprogress::del()");
}

mgr_sitetask::mgr_sitetask()
{
    m_sys_config      = xconfig::get_instance();
    m_network_mgr     = mgr_network::get_instance();
    m_sitectrl_sync_mgr = mgr_sitectrl_sync::get_instance();
    m_mount_nfs       = false;
}

mgr_sitetask::~mgr_sitetask()
{
    m_sys_config = NULL;
    m_network_mgr = NULL;
    m_sitectrl_sync_mgr = NULL;
}

//初始化
void mgr_sitetask::init()
{
    init_boardtype_map();
    init_obtype_map();
    m_mount_nfs = mount_upgrade_nfs();
}

//工作函数
void mgr_sitetask::work(unsigned long ticket)
{
    while(m_exited == 0)
    {
        //从任务队列取出任务进行处理
        main_process();
        //定时检查任务进度
        check_taskprogress();
    }
}

//开始工作
int mgr_sitetask::start_work(int work_cycle)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::start_work()");

    m_exited = 0;
    int ret_start = xmgr_basic::start_work(work_cycle);

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::start_work()");
    return ret_start;
}

//停止工作
void mgr_sitetask::stop_work()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::stop_work()");

    m_exited = 1;
    xmgr_basic::stop_work();

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::stop_work()");
    return ;
}

//work线程处理函数
int mgr_sitetask::main_process()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::main_process()");

    sitetask_msg  task;
    bool ret = m_task_queue.Pop_Wait(&task,200);
    if(!ret)
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::main_process() failed to fetch message from task queue len(%d)",m_task_queue.Size());
        return -1;
    }

    if(task.m_packdata == NULL)
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::main_process() packet is null.");
        return -1;
    }

    xsite_package *pack = dynamic_cast<xsite_package *>(task.m_packdata.get());
    std::string port_name = task.m_port_name;
    if(pack != NULL)
    {
        handle_task(port_name.c_str(),pack);
    }
    else
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::main_process() site package is null.");
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::main_process()");

    return 0;
}

//处理任务
int mgr_sitetask::handle_task(const char* port_name,xsite_package* site_pack)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::handle_task()");
    int ret = 0;

    int msg_type = get_msgtype(site_pack->m_msg_type);
    switch(msg_type)
    {
    case SITECTRL_MSG_REQUEST:
        handle_request(port_name,site_pack);
        break;
    default:
        ret = -1;
        break;
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::handle_task() type=%d type_str=%s ret=%d",msg_type,site_pack->m_msg_type.c_str(),ret);
    return ret;
}

//处理请求命令任务
int mgr_sitetask::handle_request(const char* port_name,xsite_package* site_pack)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::handle_request()");
    int ret = -1;

    //返回结果
    xsite_package *xsite_resp = dynamic_cast<xsite_package *>(site_pack->clone());
    xsite_resp->m_msg_type = SITE_MT_RESPOND;
    int cmd_size = site_pack->get_cmd_size();
    boost::shared_ptr<xsite_package> rsp_pack(xsite_resp);

    bool delay_reply = false;
    bool flag = false;

    for(int i = cmd_size-1; i >= 0; i--)
    {
        int cmd_ret = 0;

        //取出命令
        boost::shared_ptr<xcmd> cmd = xsite_resp->find_cmd(i);
        if(cmd->m_cmd_type == SITE_MC_INTERNAL_CMD)
        {
            if(cmd->m_cmd_cont == SITE_CONT_UPGRADE)
            {
                set_tasksize_status(xtaskprogress::TASKLOCK_LOCKED);
                process_pkg_upgrade(site_pack,cmd);
                set_tasksize_status(xtaskprogress::TASKLOCK_UNLOCKED);
                bind_taskmsg(port_name,rsp_pack);
                //是否延迟回复，在检测进度任务定时循环中回复
                delay_reply = isdelay_reply();
            }
        }
        else
        {
            //nothing to do ...
        }

        if(cmd_ret != 0)
        {
            //存储最后一个返回错误码
            xsite_resp->m_err_code = cmd_ret;
        }

        std::string return_code_str = cmd->get_return_value("return_code");
        LOG_MSG(MSG_LOG, "mgr_sitetask::handle_request() return_code_str:%s", return_code_str.c_str());
        if(!return_code_str.empty())
        {
            int return_code = std::stoi(return_code_str);
            if(return_code != 0)
            {
                flag = true;
                LOG_MSG(MSG_LOG, "mgr_sitetask::handle_request() return_code:%d", return_code);
            }
        }
    }

    //设置
    if((delay_reply == false) && (xsite_resp->get_cmd_size() > 0) && flag)
    {
        //发送应答消息
        send_reply(port_name,xsite_resp);
        LOG_MSG(MSG_LOG, "mgr_sitetask::handle_request() send_reply()");
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::handle_request() delay_reply=%d",delay_reply);
    return ret;
}

//将任务放入任务队列
int mgr_sitetask::push_task(const char* port_name, xpacket* packet)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::push_task()");

    int ret = 0;

    sitetask_msg  task_msg;
    task_msg.m_port_name = port_name;
    task_msg.m_packdata = NULL;
    if(packet != NULL)
    {
        xsite_package *pack = dynamic_cast<xsite_package *>(packet);
        boost::shared_ptr<xsite_package> packagdata(dynamic_cast<xsite_package*>(pack->clone()));
        task_msg.m_packdata = packagdata;
    }

    LOG_MSG(MSG_LOG,"mgr_sitetask::push_task() push site task msg task_msg.m_port_name=%s",task_msg.m_port_name.c_str());

    if(!m_task_queue.Push(task_msg))
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::push_task() push site task msg into queue failed!");
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::push_task() ret=%d",ret);
    return ret ;
}

//挂载nfs目录
bool mgr_sitetask::mount_upgrade_nfs()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::mount_upgrade_nfs()");

    std::string nfs_addr;
    std::string sitectrl_addr = m_sys_config->get_data("sitectrl_addr");
    std::vector< std::string > vct_addr;
    if (xbasic::split_string(sitectrl_addr, ":", &vct_addr) < 2)
    {
        //不是地址格式
        LOG_MSG(WRN_LOG,"mgr_sitetask::mount_upgrade_nfs() site control server address(%s) is error",sitectrl_addr.c_str());
        return false;
    }

    nfs_addr = vct_addr[0];

    int site_id = m_sys_config->get_data("site_id", 65);
    int slot_id = site_id - 64;
    char slot_str[4];
    sprintf(slot_str, "%d", slot_id);
    slot_str[3] = '\0';

    std::string nfs_server_path = nfs_addr + ":/opt/compile_output/" + slot_str;
    std::string upgrade_nfs_path = m_sys_config->get_data("upgrade_nfs");
    LOG_MSG(MSG_LOG,"mgr_sitetask::mount_upgrade_nfs() nfs_server_path:%s",nfs_server_path.c_str());
    LOG_MSG(MSG_LOG,"mgr_sitetask::mount_upgrade_nfs() upgrade_nfs_path:%s",upgrade_nfs_path.c_str());

#ifndef _WINDOWS //LINUX平台
    if(access(upgrade_nfs_path.c_str(), F_OK) != 0)
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::mount_upgrade_nfs() the folder (%s) is not exist",upgrade_nfs_path.c_str());
        xbasic::exe_sys_cmd("mkdir -p " + upgrade_nfs_path);
        if(access(upgrade_nfs_path.c_str(), F_OK) != 0)
        {
            LOG_MSG(ERR_LOG,"mgr_sitetask::mount_upgrade_nfs() the folder (%s) is not exist",upgrade_nfs_path.c_str());
            return false;
        }
    }

#endif

    if(is_mount_point(upgrade_nfs_path.c_str()) == true)
    {
        xbasic::exe_sys_cmd("busybox umount -f " + upgrade_nfs_path);
    }

    int ret_val = xbasic::exe_sys_cmd("busybox mount -t nfs -o nolock,nfsvers=3 " + nfs_server_path + " " + upgrade_nfs_path);

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::mount_upgrade_nfs() ret_val=%d",ret_val);
    return ret_val == 0 ? true : false;
}

bool mgr_sitetask::is_mount_point(const char* path)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::is_mount_point()");

    bool ret = false;
    if(path == NULL)
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::is_mount_point() path is NULL");
        return ret;
    }

    char line[1024];
    char mount_point[1024];
    memset(&line[0],0,sizeof(line));
    memset(&mount_point[0],0,sizeof(mount_point));
    int len = strlen(path);
    if(*(path + len -1) == '/')
    {
        strncpy(&mount_point[0],path,len-1);
    }

    FILE *fp = fopen("/proc/mounts", "r");
    while(fgets(line, sizeof(line), fp))
    {
        if(strstr(line, mount_point))
        {
            fclose(fp);
            ret = true;
            LOG_MSG(WRN_LOG,"mgr_sitetask::is_mount_point() path:%s is mount point",path);
            break;
        }
        memset(&line[0],0,sizeof(line));
    }

    fclose(fp);
    fp = NULL;

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::is_mount_point() path:%s ret=%d",path,ret);
    return ret;
}

//得到消息类型
int mgr_sitetask::get_msgtype(const std::string& msg_type)
{
    int type = SITECTRL_MSG_NONE;
    if(msg_type == std::string(SITE_MT_REQUEST))
    {
        type = SITECTRL_MSG_REQUEST;
    }
    else if(msg_type == std::string(SITE_MT_NOTIFY))
    {
        type = SITECTRL_MSG_NOTIFY;
    }
    else if(msg_type == std::string(SITE_MT_RESPOND))
    {
        type = SITECTRL_MSG_RESPOND;
    }
    else if(msg_type == std::string(SITE_MT_BROADCAST))
    {
        type = SITECTRL_MSG_BROADCAST;
    }
    else
    {
        type = SITECTRL_MSG_NONE;
    }

    return type;
}

//回复应答
int mgr_sitetask::send_reply(const char* port_name,xsite_package site_pack)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::send_reply() port_name=%s",port_name);
    int ret = 0;
    if(port_name == NULL)
    {
        LOG_MSG(ERR_LOG,"mgr_sitetask::send_reply() port_name is NULL");
        return ret;
    }

    if(strlen(port_name) == 0)
    {
        LOG_MSG(ERR_LOG,"mgr_sitetask::send_reply() port_name len is zero");
        return ret;
    }

    if(site_pack == NULL)
    {
        LOG_MSG(ERR_LOG,"mgr_sitetask::send_reply() site_pack is NULL");
        return ret;
    }


#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
    //发送消息到sitectrl sync服务端
    if(strncmp(port_name,PORT_SITECTRL_CLI,strlen(PORT_SITECTRL_CLI)) == 0)
    {
        //发送数据到网络透传
        ret = m_network_mgr->send_to_sitectrl_syncsvr(site_pack);
    }
    else
    {
        LOG_MSG(ERR_LOG,"mgr_sitetask::send_reply() port_name(%s) is unsupported",port_name);;
    }

#elif defined(SYNC_BUILD)
    //发送消息到sitecontrol平台
    if(strncmp(port_name,PORT_SITECONTROL,strlen(PORT_SITECONTROL)) == 0)
    {
        //发送数据到网络透传
        ret = m_network_mgr->send_to_sitectrl(site_pack);
    }
    else
    {
        LOG_MSG(ERR_LOG,"mgr_sitetask::send_reply() port_name(%s) is unsupported",port_name);
    }
#endif

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::send_reply() ret=%d",ret);
    return ret;
}

//检查任务完成进度
int mgr_sitetask::check_taskprogress()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::check_taskprogress()");
    int ret = 0;

#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)

    //1. 检查任务
    ret = check_nonsync_task();
    LOG_MSG(MSG_LOG,"mgr_sitetask::check_taskprogress() check nonsync task ret=%d",ret);

#elif defined(SYNC_BUILD)

    //1. 检查任务
    ret = check_sync_task();
    LOG_MSG(MSG_LOG,"mgr_sitetask::check_taskprogress() check sync task ret=%d",ret);

#endif

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::check_taskprogress() ret=%d",ret);
    return ret;
}

//检查PGB板/RK3588板上任务
int mgr_sitetask::check_nonsync_task()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::check_nonsync_task()");
    int ret = 0;

    //1. 检查检测间隔
    static time_t last_tm = 0;
    time_t curr_tm = time(NULL);
    if(curr_tm - last_tm > TASKPROGRESS_CHECK_INTERVAL)
    {
        //2. 保存本次时间点
        last_tm = curr_tm;

        //3. 检查板子升级进度
        xtaskprogress* taskprogress = xtaskprogress::get_instance();
        if(taskprogress->get_tasksize_lockstatus() == xtaskprogress::TASKLOCK_UNLOCKED)
        {
            //3.1. 遍历单板升级状态集合
            boost::unordered_map<int,int> task_map;
            task_map = taskprogress->get_taskprogress_map();
            for(auto it : task_map)
            {
                int boardid = it.first;
                int task_st = it.second;
                if(task_st == xtaskprogress::TASKSTATUS_COMPLETE)
                {
                    int result = xtaskprogress::TASKRESULT_NONE;
                    taskprogress->get_taskresult(boardid,result);
                    notify_taskprogress(boardid,task_st,result);
                    //3.2. 删除升级状态集合元素
                    taskprogress->del(boardid);
                    LOG_MSG(MSG_LOG,"mgr_sitetask::check_nonsync_task() boardid=0x%x status=%d result=%d",boardid,task_st,result);
                }
            }
        }
        else
        {
            ret = -1;
        }
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::check_nonsync_task() ret=%d",ret);
    return ret;
}

//检查sync板上任务
int mgr_sitetask::check_sync_task()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::check_sync_task()");
    int ret = 0;

    //1. 检查检测间隔
    static time_t last_tm = 0;
    time_t curr_tm = time(NULL);
    if(curr_tm - last_tm > TASKPROGRESS_CHECK_INTERVAL)
    {
        //2. 保存本次时间点
        last_tm = curr_tm;

        //3. 检查板子升级进度
        //4. @sitectrl平台服务端返回升级应答消息
        time_t curr_tm = time(NULL);
        xtaskprogress* taskprogress = xtaskprogress::get_instance();

        if(taskprogress->get_tasksize_lockstatus() == xtaskprogress::TASKLOCK_UNLOCKED)
        {
            //4.1. 遍历单板升级状态集合
            boost::unordered_map<int,int> task_map;
            task_map = taskprogress->get_taskprogress_map();
            int task_sz = task_map.size();

            LOG_MSG(MSG_LOG,"mgr_sitetask::check_sync_task() task_sz=%d",task_sz);

            if(task_sz > 0)
            {
                std::vector<int> sum_result;
                for(auto it : task_map)
                {
                    int check_st = TASKCHECK_TIMEOUT;
                    int boardid = it.first;
                    int task_st = it.second;

                    if(task_st != xtaskprogress::TASKSTATUS_COMPLETE)
                    {
                        check_st = TASKCHECK_TIMEOUT;
                        sum_result.push_back(check_st);
                        LOG_MSG(MSG_LOG,"mgr_sitetask::check_sync_task() upgrade boardid=0x%x check_st=%d task_st=%d uncomplete",boardid,check_st,task_st);
                    }
                    else
                    {
                        int task_res = 0;
                        taskprogress->get_taskresult(boardid,task_res);
                        if(task_res == xtaskprogress::TASKRESULT_FAILED)
                        {
                            //说明有一个board升级失败了
                            check_st = TASKCHECK_FAILED;
                            sum_result.push_back(check_st);
                            LOG_MSG(MSG_LOG,"mgr_sitetask::check_sync_task() upgrade boardid=0x%x check_st=%d task_st=%d failed",boardid,check_st,task_st);
                        }
                        else if(task_res == xtaskprogress::TASKRESULT_SUCESS)
                        {
                            //说明有一个board升级成功了
                            check_st = TASKCHECK_SUCCESS;
                            sum_result.push_back(check_st);
                            LOG_MSG(MSG_LOG,"mgr_sitetask::check_sync_task() upgrade boardid=0x%x check_st=%d task_st=%d success",boardid,check_st,task_st);
                        }
                        else if(task_res == xtaskprogress::TASKRESULT_NONE)
                        {
                            //说明有一个board可能还在升级中
                            check_st = TASKCHECK_TIMEOUT;
                            sum_result.push_back(check_st);
                            LOG_MSG(MSG_LOG,"mgr_sitetask::check_sync_task() upgrade boardid=0x%x task_res=%d task_st=%d upgrading",boardid,task_res,task_st);
                        }
                        else
                        {
                            LOG_MSG(WRN_LOG,"mgr_sitetask::check_sync_task() upgrade boardid=0x%x task_res=%d task_st=%d is invalid",boardid,task_res,task_st);
                        }
                    }
                }

                int vect_sum_sz = sum_result.size();
                LOG_MSG(MSG_LOG,"mgr_sitetask::check_sync_task() vect_sum_sz=%d",vect_sum_sz);
                if(vect_sum_sz > 0)
                {
                    int check_res = TASKCHECK_SUCCESS;
                    for(auto it : sum_result)
                    {
                        if(it != TASKCHECK_SUCCESS)
                        {
                            check_res = it;
                            LOG_MSG(MSG_LOG,"mgr_sitetask::check_sync_task() vect_sum_sz=%d check_res=%d",vect_sum_sz,check_res);
                            if(check_res == TASKCHECK_FAILED)
                            {
                                //只要有一个升级失败，就认为整体失败
                                break;
                            }
                        }
                    }
                }

                //4.2 回复升级结果给sitectrl平台服务端
                if(check_res == TASKCHECK_SUCCESS)
                {
                    //4.3 发送升级成功回复应答
                    boost::shared_ptr<sitetask_msg> task = taskprogress->get_sitetaskmsg();
                    xsite_package *xsite_resp = dynamic_cast<xsite_package *>(task->m_packdata.get());
                    std::string port_name = task->m_port_name;

                    boost::shared_ptr<xcmd> cmd = xsite_resp->find_cmd(0);
                    //加入return_info信息
                    cmd->add_return_info("upgrade success");
                    //return_code信息
                    cmd->add_return_code("0");

                    int len = send_reply(port_name.c_str(), xsite_resp);
                    //4.4 清空taskprogress所有单板升级状态集合
                    taskprogress->clear();
                    LOG_MSG(WRN_LOG,"mgr_sitetask::check_sync_task() upgrade success len=%d check_res=%d",len,check_res);
                }
                else if(check_res == TASKCHECK_FAILED)
                {
                    //4.3 发送升级失败回复应答
                    boost::shared_ptr<sitetask_msg> task = taskprogress->get_sitetaskmsg();
                    xsite_package *xsite_resp = dynamic_cast<xsite_package *>(task->m_packdata.get());
                    std::string port_name = task->m_port_name;

                    boost::shared_ptr<xcmd> cmd = xsite_resp->find_cmd(0);
                    //加入return_info信息
                    cmd->add_return_info("upgrade failed");
                    //return_code信息
                    cmd->add_return_code("25");

                    int len = send_reply(port_name.c_str(), xsite_resp);
                    //4.4 清空taskprogress所有单板升级状态集合
                    taskprogress->clear();
                    LOG_MSG(WRN_LOG,"mgr_sitetask::check_sync_task() upgrade failed len=%d check_res=%d",len,check_res);
                }
                else
                {
                    LOG_MSG(WRN_LOG,"mgr_sitetask::check_sync_task() check_res=%d",check_res);
                    if((curr_tm - taskprogress->get_starttime()) > TASKPROGRESS_TIMEOUT)
                    {
                        //当时升级所有单板升级超时，还存在未完成时升级任务，则认为整体失败
                        //4.5 发送升级失败回复应答
                        boost::shared_ptr<sitetask_msg> task = taskprogress->get_sitetaskmsg();
                        xsite_package *xsite_resp = dynamic_cast<xsite_package *>(task->m_packdata.get());
                        std::string port_name = task->m_port_name;

                        boost::shared_ptr<xcmd> cmd = xsite_resp->find_cmd(0);
                        //加入return_info信息
                        cmd->add_return_info("upgrade failed timeout");
                        //return_code信息
                        cmd->add_return_code("25");

                        int len = send_reply(port_name.c_str(), xsite_resp);

                        //2.6 清空taskprogress所有单板升级状态集合
                        taskprogress->clear();
                        LOG_MSG(WRN_LOG,"mgr_sitetask::check_sync_task() upgrade failed len=%d check_res=%d",len,check_res);
                    }
                }
            }
        }
        else
        {
            ret = -1;
        }
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::check_sync_task() ret=%d",ret);
    return ret;
}

//是否延迟回复
bool mgr_sitetask::isdelay_reply()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::isdelay_reply()");

    int ret = true;
    xtaskprogress* xtask_progress = xtaskprogress::get_instance();
    int task_sz = xtask_progress->get_task_size();
    if(task_sz == 0)
    {
        //不需要延迟回复
        ret = false;
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::isdelay_reply() ret=%d",ret);
    return ret;
}

//处理升级命令消息
int mgr_sitetask::process_pkg_upgrade(xsite_package *pack,boost::shared_ptr<xcmd> cmd)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::process_pkg_upgrade()");
    int ret = 0;

    //0. 清除之前的任务进度
    clear_taskprogress();

    //1. 解压缩pkg升级包
    ret = unpacker_pkg(pack,cmd);
    //1.1. 解析硬件关系映射表
    parse_hardware_map(pack, cmd);
    if(ret == 0)
    {
        //2. 检查是否需要转发到下级设备管理
        classify_pkg_interface();
        int is_forward = check_forward_upgrade();

        if((is_forward & PKG_FORWARD_TYPE_MASK) != PKG_FORWARD_TYPE_NONE)
        {
            if(is_forward & PKG_FORWARD_TYPE_MASK == (PKG_FORWARD_TYPE_LOCAL | PKG_FORWARD_TYPE_REMOTE))
            {
                //注：这个分支只有FT的shell升级才会进入
                //将升级消息转发到sync板下级x86设备管理上
                ret = process_pkg_forwardupgrade(pack);
                LOG_MSG(MSG_LOG,"mgr_sitetask::process_pkg_upgrade() process_pkg_forwardupgrade is_forward=0x%x,ret=%d",is_forward,ret);
            }

            //这里消息转发可能失败，如果 x86 升级消息转发失败就不进行本地 x86 升级，直接向上位机返回失败
            if(ret == 0)
            {
                //进行本地升级
                ret = process_pkg_localupgrade(pack,cmd);
                LOG_MSG(MSG_LOG,"mgr_sitetask::process_pkg_upgrade() process_pkg_localupgrade is_forward=0x%x,ret=%d",is_forward,ret);
            }

            if(ret == -1)
            {
                //加入return_info信息
                cmd->add_return_info("upgrade failed");
                //加入return_code信息
                cmd->add_return_code("25");
            }
        }
        else if((is_forward & PKG_FORWARD_TYPE_LOCAL) == PKG_FORWARD_TYPE_LOCAL)
        {
            //3. 进行本地升级
            ret = process_pkg_localupgrade(pack,cmd);
            if(ret == -1)
            {
                //加入return_info信息
                cmd->add_return_info("upgrade failed");
                //加入return_code信息
                cmd->add_return_code("25");
            }

            LOG_MSG(MSG_LOG,"mgr_sitetask::process_pkg_upgrade() process_pkg_localupgrade is_forward=0x%x,ret=%d",is_forward,ret);
        }
        else if((is_forward & PKG_FORWARD_TYPE_REMOTE) == PKG_FORWARD_TYPE_REMOTE)
        {
            // FT MCU 逻辑 升级会走到这个分支
            // 监测是否存在要升级的类型单板
            bool flag = check_board_exist();
            if(!flag)
            {
                // 不存在要升级类型的单板
                ret = -1;
                LOG_MSG(WRN_LOG,"mgr_sitetask::process_pkg_upgrade() board not exited,ret=%d",ret);
            }
            else
            {
                bool usb_flag = check_ver_exist(PKG_TYPE_USB);
                bool serdes_flag = check_ver_exist(PKG_TYPE_SERDES_DRV);
                LOG_MSG(MSG_LOG,"mgr_sitetask::process_pkg_upgrade() usb_flag=%d serdes_flag=%d",usb_flag,serdes_flag);
                if(usb_flag || serdes_flag)
                {
                    //4. 将升级消息转发到SYNC板下级X86设备管理上
                    ret = process_pkg_forwardupgrade(pack);
                    LOG_MSG(MSG_LOG,"mgr_sitetask::process_pkg_upgrade() process pkg forwardupgrade ret=%d",ret);
                }
                else
                {
                    //到这里说明本次升级包中缺少当前环境下硬件单板所需要的版本包
                    ret = -1;
                    LOG_MSG(WRN_LOG,"mgr_sitetask::process_pkg_upgrade() check ver exited error,ret=%d",ret);
                }
            }

            if(ret == -1)
            {
                //加入return_info信息
                cmd->add_return_info("upgrade forward failed");
                //加入return_code信息
                cmd->add_return_code("25");
            }
        }

        LOG_MSG(MSG_LOG,"mgr_sitetask::process_pkg_upgrade() process_pkg_forwardupgrade is_forward=0x%x,ret=%d",is_forward,ret);
    }
    else
    {
        ret = -1;
        //加入return_info信息
        cmd->add_return_info("upgrade failed");
        //加入return_code信息
        cmd->add_return_code("25");
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::process_pkg_upgrade() is_forward=0x%x ret=%d",is_forward,ret);
    return ret;
}

//解压pkg包
int mgr_sitetask::unpacker_pkg(xsite_package *pack,boost::shared_ptr<xcmd> cmd)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::unpacker_pkg()");
    int ret = 0;

    do
    {
        if(!m_mount_nfs)
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::unpacker_pkg() nfs not mount, try mount again");
            m_mount_nfs = mount_upgrade_nfs();
            if(m_mount_nfs != true)
            {
                LOG_MSG(WRN_LOG,"mgr_sitetask::unpacker_pkg() mount nfs failed, can not upgrade");
                cmd->add_return_info("mount nfs failed, can not upgrade");
                cmd->add_return_code("2");
                ret = -1;
                break;
            }
        }

        //1. 得到升级包名
        std::string package_name = cmd->get_param("upgrade_package");

        //2.2. 拷贝升级包文件到本地
        //2.1 检查NFS目录是否存在
        std::string upgrade_nfs_path = m_sys_config->get_data("upgrade_nfs");
        if(upgrade_nfs_path.back() != '/')
        {
            upgrade_nfs_path += std::string("/");
        }
        std::string upgrade_package_pathname = upgrade_nfs_path + package_name;
        LOG_MSG(MSG_LOG,"mgr_sitetask::unpacker_pkg() upgrade_package_pathname=%s",upgrade_package_pathname.c_str());

        if(xbasic::file_exist(upgrade_package_pathname) == false)
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::unpacker_pkg() upgrade package(%s) is not exist",upgrade_package_pathname.c_str());
            cmd->add_return_info("upgrade package is not exist");
            cmd->add_return_code("2");
            ret = -1;
            break;
        }

        //2.2 建立本地目录
        std::string upgrade_local_path = m_sys_config->get_data("upgrade_local");
        if(upgrade_local_path.back() != '/')
        {
            upgrade_local_path += std::string("/");
        }
        if(access(upgrade_local_path.c_str(), F_OK) != 0)
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::unpacker_pkg() the local upgrade folder(%s) is not exist",upgrade_local_path.c_str());
            xbasic::exe_sys_cmd("mkdir -p " + upgrade_local_path);
            if(access(upgrade_local_path.c_str(), F_OK) != 0)
            {
                LOG_MSG(WRN_LOG,"mgr_sitetask::unpacker_pkg() create local upgrade folder(%s) failed.",upgrade_local_path.c_str());
                cmd->add_return_info("create local upgrade folder failed");
                cmd->add_return_code("3");
                ret = -1;
                break;
            }
        }

        //2.3 先清空本地目录中内容
        xbasic::exe_sys_cmd("rm -rf " + upgrade_local_path + "*");

        //2.4 拷贝升级包文件
        int exe_ret = 0;
        exe_ret = xbasic::exe_sys_cmd("cp -rf " + upgrade_package_pathname + " " + upgrade_local_path);
        if(exe_ret != 0)
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::unpacker_pkg() copy package file %s failed.",upgrade_package_pathname.c_str());
            cmd->add_return_info("copy upgrade package file failed");
            cmd->add_return_code("4");
            ret = -1;
            break;
        }

        //3. 提取升级tar包文件
        exe_ret = xbasic::exe_sys_cmd("tail -c +1025 " + upgrade_local_path + package_name + " > " + upgrade_local_path + "target.tar");
        if(exe_ret != 0)
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::unpacker_pkg() get tar file failed.");
            cmd->add_return_info("get tar file from upgrade package failed");
            cmd->add_return_code("5");
            ret = -1;
            break;
        }

        //4. 解压升级tar包文件
        exe_ret = xbasic::exe_sys_cmd("tar -xvf " + upgrade_local_path + "target.tar -C " + upgrade_local_path);
        if(exe_ret != 0)
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::unpacker_pkg() untar tar file failed.");
            cmd->add_return_info("untar tar file failed");
            cmd->add_return_code("6");
            ret = -1;
            break;
        }
    } while(0);

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::unpacker_pkg() ret=%d",ret);
    return ret;
}

//检查是否转发升级消息
int mgr_sitetask::check_forward_upgrade()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::check_forward_upgrade()");
    int is_forward = PKG_FORWARD_TYPE_NONE;

    //1. 需要转发情形如下:
    //CP
    //1.1 CP的ARM版PCIe的升级包
    //1.1.1 CP的ARM版PCIe的升级包不需要转发,因为PCIe驱动库放在sync板上 2025/08/25
    //1.2 CP的ARM系统上的包,需要转发

    //FT
    //1.1. 存在PGB上x86系统上的包，则FT需要转发
    //1.2. 存在下级设备管理所管理的MCU、FPGA包，则需要转发
    is_forward = check_x86_forward();
    if(is_forward != PKG_FORWARD_TYPE_NONE)
    {
        LOG_MSG(MSG_LOG,"mgr_sitetask::check_forward_upgrade() check_x86_forward is_forward=0x%x",is_forward);
    }
    else
    {
        is_forward = check_mcu_fpga_forward();
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::check_forward_upgrade() is_forward=0x%x",is_forward);
    return is_forward;
}

//检查x86系统
int mgr_sitetask::check_x86_forward()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::check_x86_forward()");
    int ret = PKG_FORWARD_TYPE_NONE;

#if defined(SYNC_BUILD)

#if defined(DEV_TYPE_CP)

    xpkg_interface* pkg_instance = xpkg_interface::get_instance();
    //CP现网x86上程序不需要转发
    std::shared_ptr<xpkg_interface::FileList> x86_filelist = pkg_instance->find(PKG_TYPE_X86);
    if(x86_filelist != NULL)
    {
        ret = PKG_FORWARD_TYPE_LOCAL;
    }
    else
    {
        //CP ARM上程序需要转发
        std::shared_ptr<xpkg_interface::FileList> arm_filelist = pkg_instance->find(PKG_TYPE_ARM);
        if(arm_filelist != NULL)
        {
            ret = PKG_FORWARD_TYPE_REMOTE;
        }
    }

    LOG_MSG(MSG_LOG,"mgr_sitetask::check_x86_forward() CP sync ret=0x%x",ret);

#elif defined(DEV_TYPE_FT)

    xpkg_interface* pkg_instance = xpkg_interface::get_instance();
    std::shared_ptr<xpkg_interface::FileList> x86_filelist = pkg_instance->find(PKG_TYPE_X86);
    if(x86_filelist != NULL)
    {
        ret = PKG_FORWARD_TYPE_LOCAL | PKG_FORWARD_TYPE_REMOTE;
    }

    LOG_MSG(MSG_LOG,"mgr_sitetask::check_x86_forward() FT sync ret=0x%x",ret);

#elif defined(PGB_BUILD) && defined(DEV_TYPE_FT)

    xpkg_interface* pkg_instance = xpkg_interface::get_instance();
    std::shared_ptr<xpkg_interface::FileList> x86_filelist = pkg_instance->find(PKG_TYPE_X86);
    if(x86_filelist != NULL)
    {
        ret = PKG_FORWARD_TYPE_LOCAL;
    }

    LOG_MSG(MSG_LOG,"mgr_sitetask::check_x86_forward() FT PGB ret=0x%x",ret);

#elif defined(CP_RK3588_BUILD) && defined(DEV_TYPE_CP)

    xpkg_interface* pkg_instance = xpkg_interface::get_instance();
    std::shared_ptr<xpkg_interface::FileList> arm_filelist = pkg_instance->find(PKG_TYPE_ARM);
    if(arm_filelist != NULL)
    {
        ret = PKG_FORWARD_TYPE_LOCAL;
    }

    LOG_MSG(MSG_LOG,"mgr_sitetask::check_x86_forward() CP rk3588 ret=0x%x",ret);

#else

#endif

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::check_x86_forward() ret=0x%x",ret);
    return ret;
}

//检查mcu、fpga逻辑
int mgr_sitetask::check_mcu_fpga_forward()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::check_mcu_fpga_forward()");
    int ret = PKG_FORWARD_TYPE_NONE;
    xpkg_interface* pkg_instance = xpkg_interface::get_instance();

#if defined(SYNC_BUILD)

#if defined(DEV_TYPE_CP)

    std::shared_ptr<xpkg_interface::FileList> arm_pcie_filelist = pkg_instance->find(PKG_TYPE_PCIE_DRV);
    std::shared_ptr<xpkg_interface::FileList> mcu_fpga_filelist = pkg_instance->find(PKG_TYPE_USB);
    std::shared_ptr<xpkg_interface::FileList> serdes_filelist = pkg_instance->find(PKG_TYPE_SERDES_DRV);

    if((arm_pcie_filelist != NULL) || (mcu_fpga_filelist != NULL) || (serdes_filelist != NULL))
    {
        //CP的ARM版PCIe的升级包不需要转发,因为PCIe驱动库放在sync板上 2025/08/25
        ret = PKG_FORWARD_TYPE_LOCAL;
    }
    else
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::check_mcu_fpga_forward() cp sync PKG_TYPE_PCIE_DRV arm_pcie_filelist is NULL");
    }

#elif defined(DEV_TYPE_FT)
    std::shared_ptr<xpkg_interFace::filelist> mcu_fpga_filelist  = pkg_instance->find(PKG_TYPE_USB);
    std::shared_ptr<xpkg_interFace::filelist> serdes_filelist  = pkg_instance->find(PKG_TYPE_SERDES_DRV);

    if((mcu_fpga_filelist != NULL) || (serdes_filelist != NULL))
    {
        //检查是否存在需要转PKG_TYPE_USB的mcu、fpga
        xpkg_interFace::Filelist filelist;
        if(mcu_fpga_filelist)
        {
            filelist = *mcu_fpga_filelist;
            LOG_MSG(MSG_LOG,"mgr_sitetask::check_mcu_fpga_forward() ft sync PKG_TYPE_USB mcu fpga");
        }
        else
        {
            filelist = *serdes_filelist;
            LOG_MSG(MSG_LOG,"mgr_sitetask::check_mcu_fpga_forward() ft sync PKG_TYPE_SERDES_DRV fpga");
        }

        for(auto it : filelist)
        {
            std::string filename = it;
            int boardtype = 0;
            bool flag = get_boardtype(filename,boardtype);
            if((flag == true) && ((boardtype == BOARDTYPE_FT_PGB) || (boardtype == BOARDTYPE_ASIC) || (boardtype == BOARDTYPE_FT_DIG)))
            {
                ret = PKG_FORWARD_TYPE_REMOTE;
                break;
            }
            else if((flag == true) && ((boardtype == BOARDTYPE_FT_SYNC) || (boardtype == BOARDTYPE_FT_PPS)))
            {
                ret = PKG_FORWARD_TYPE_LOCAL;
                break;
            }
            else
            {
                LOG_MSG(WRN_LOG,"mgr_sitetask::check_mcu_fpga_forward() ft sync PKG_TYPE_USB mcu_fpga filename:%s flag:%d boardtype:0x%x",filename.c_str(),flag,boardtype);
            }
        }
    }
    else
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::check_mcu_fpga_forward() ft sync PKG_TYPE_USB mcu_fpga_filelist is NULL");
    }

#else
#endif

#elif defined(PGB_BUILD) && defined(DEV_TYPE_FT)

    std::shared_ptr<xpkg_interFace::filelist> mcu_fpga_filelist  = pkg_instance->find(PKG_TYPE_USB);
    std::shared_ptr<xpkg_interFace::filelist> serdes_filelist  = pkg_instance->find(PKG_TYPE_SERDES_DRV);

    if((mcu_fpga_filelist != NULL) || (serdes_filelist != NULL))
    {
        xpkg_interFace::Filelist filelist;
        if(mcu_fpga_filelist)
        {
            filelist = *mcu_fpga_filelist;
            LOG_MSG(MSG_LOG,"mgr_sitetask::check_mcu_fpga_forward() ft pgb PKG_TYPE_USB mcu fpga");
        }
        else
        {
            filelist = *serdes_filelist;
            LOG_MSG(MSG_LOG,"mgr_sitetask::check_mcu_fpga_forward() ft pgb PKG_TYPE_SERDES_DRV fpga");
        }

        for(auto it : filelist)
        {
            std::string filename = it;
            int boardtype = 0;
            bool flag = get_boardtype(filename,boardtype);
            if((flag == true) && ((boardtype == BOARDTYPE_FT_PGB) || (boardtype == BOARDTYPE_ASIC) || (boardtype == BOARDTYPE_FT_DIG)))
            {
                ret = PKG_FORWARD_TYPE_LOCAL;
                break;
            }
            else
            {
                LOG_MSG(WRN_LOG,"mgr_sitetask::check_mcu_fpga_forward() ft pgb PKG_TYPE_USB mcu_fpga filename:%s flag:%d boardtype:0x%x",filename.c_str(),flag,boardtype);
            }
        }
    }
    else
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::check_mcu_fpga_forward() ft pgb PKG_TYPE_USB mcu_fpga_filelist is NULL");
    }

#else
#endif

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::check_mcu_fpga_forward() ret:0x%x",ret);
    return ret;
}


//进行本地升级处理
int mgr_sitetask::process_pkg_localupgrade(xsite_package *pack,boost::shared_ptr<cxmd> cmd)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::process_pkg_localupgrade()");
    int ret = 0;

    std::string ota_dir = m_sys_config->get_data("ota_path");
    if(access(ota_dir.c_str(), F_OK) != 0)
    {
        LOG_MSG(WRN_LOG,"mgr_sitetask::process_pkg_localupgrade() ota_path(%s) is not exist,will create directory",ota_dir.c_str());
        xbasic::exe_sys_cmd("mkdir -p " + ota_dir);
    }

    xpkg_interFace* pkg_instance = xpkg_interFace::get_instance();
    xpkg_interFace::PkgInterFaceMap pkg_interfacemap = pkg_instance->get_pkgmap();
    for(auto it : pkg_interfacemap)
    {
        if(it.first == PKG_TYPE_X86)
        {
            pkg_shell_upgrade(pack,cmd);
        }
        else if(it.first == PKG_TYPE_USB)
        {
            ret = pkg_usb_upgrade_ex(*it.second);
        }
        else if(it.first == PKG_TYPE_TCP)
        {
            pkg_tcp_upgrade(*it.second);
        }
        else if(it.first == PKG_TYPE_SERDES_DRV)
        {
            ret = pkg_serdesdrv_upgrade_ex(*it.second);
        }
        else if(it.first == PKG_TYPE_PCIE_DRV)
        {
            pkg_pciedrv_upgrade(*it.second);
        }
        else if(it.first == PKG_TYPE_SPI_DRV)
        {
            pkg_spidrv_upgrade(*it.second);
        }
        else
        {
            //nothing to do
        }
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::process_pkg_localupgrade() ret:%d",ret);
    return ret;
}


//加载pkg升级文件的前缀
int mgr_sitetask::load_pkg_prefix(std::vector<std::string>& vect_prefix)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::load_pkg_prefix()");

    int ret = 0;
    //1、读取pkg_type.json
    const char* prog_path = xbasic::get_module_path();
    std::string json_file = std::string(prog_path)+"pkg_type.json";
    std::string json_str = xbasic::read_data_from_file(json_file);

    cJSON_object js_root(json_str);
    cJSON_object pkg = js_root["pkg"];
    int pkg_size = pkg.get_array_size();

    for(int i = 0; i < pkg_size; i++)
    {
        bool found = false;
        cJSON_object pkg_obj = pkg[i];
        std::string pkg_type = pkg_obj["pkg_type"];
        cJSON_object pkg_prefix = pkg_obj["pkg_prefix"];
        int pkg_prefix_sz = pkg_prefix.get_array_size();
        for(int j = 0; j < pkg_prefix_sz; j++)
        {
            cJSON_object prefix_obj = pkg_prefix[j];
            std::string prefix = prefix_obj["prefix"];
            if(!prefix.empty())
            {
                vect_prefix.push_back(prefix);
                LOG_MSG(MSG_LOG,"mgr_sitetask::load_pkg_prefix() pkg_type:%s prefix:%s",pkg_type.c_str(),prefix.c_str());
            }
        }
    }

    ret = vect_prefix.size();

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::load_pkg_prefix() ret:%d",ret);
    return ret;
}


//分类pkg包中不同的升级接口
int mgr_sitetask::classify_pkg_interFace()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::classify_pkg_interFace()");
    int ret = -1;

    std::string upgrade_local_path = m_sys_config->get_data("upgrade_local");
    if(upgrade_local_path.empty())
    {
        LOG_MSG(ERR_LOG,"mgr_sitetask::classify_pkg_interFace() upgrade_local_path is empty");
        return ret;
    }

    if(upgrade_local_path.back() != '/')
    {
        upgrade_local_path += std::string("/");
    }


    //1、读取升级本地目录的升级文件
    std::string target_dir = upgrade_local_path + std::string("target/");

    //读取pkg升级文件前缀信息
    std::vector<std::string> vect_prefix;
    int prefix_sz = load_pkg_prefix(vect_prefix);

    std::vector<std::string> file_list;
    int sz = read_upgrade_filelist(target_dir,file_list,vect_prefix);

    LOG_MSG(MSG_LOG,"mgr_sitetask::classify_pkg_interFace() prefix_sz:%d sz:%d",prefix_sz,sz);

    //2、根据目录下升级文件的名称的模块字段确定升级接口类型
    //大致分成以下几类:
    //2.1、本地X86系统上Lib库和进程的升级文件
    //2.2、本地通过USB接口的MCU和FPGA升级文件
    //2.3、本地通过TCP网络接口的MCU和FPGA升级文件
    //2.4、本地通过Serdes驱动接口的FPGA升级文件
    //2.5、本地通过PCIE驱动接口中的FPGA升级文件

    xpkg_interFace* pkg_instance = xpkg_interFace::get_instance();
    pkg_instance->clear();
    for(int i = 0; i < sz; i++)
    {
        std::string file_name = file_list[i];
        int type = get_pkg_interFace_type(file_name);
        pkg_instance->add(type, file_name);
        LOG_MSG(MSG_LOG,"mgr_sitetask::classify_pkg_interFace() type:%d filename:%s",type,file_name.c_str());
    }

    ret = sz > 0 ? 0 : -1;

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::classify_pkg_interFace() ret:%d",ret);
    return ret;
}


//读取升级文件列表
int mgr_sitetask::read_upgrade_filelist(const std::string& path,std::vector<std::string>& vect_file,const std::vector<std::string>& vect_prefix)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::read_upgrade_filelist() path:%s",path.c_str());

    int ret = 0;

    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    if(path.empty())
    {
        LOG_MSG(ERR_LOG,"mgr_sitetask::read_upgrade_filelist() path is NULL");
        return ret;
    }

    bool flag = true;
    if(path.back() != '/')
    {
        //path路径没有'/'结尾
        flag = false;
    }

    // 打开目录
    dir = opendir(path.c_str());
    if(dir == NULL)
    {
        LOG_MSG(ERR_LOG,"mgr_sitetask::read_upgrade_filelist() open dir(%s) failed",path.c_str());
        return ret;
    }

    // 读取目录中的每个文件
    while((entry = readdir(dir)) != NULL)
    {
        // 构建文件的完整路径
        char file_path[1024];
        memset(file_path,0,sizeof(file_path));
        if(flag)
        {
            snprintf(file_path, sizeof(file_path), "%s%s", path.c_str(), entry->d_name);
        }
        else
        {
            snprintf(file_path, sizeof(file_path), "%s/%s", path.c_str(), entry->d_name);
        }

        // 获取文件的信息
        if(lstat(file_path, &file_stat) < 0)
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::read_upgrade_filelist() lstat file(%s) failed",file_path);
            continue;
        }

        // 判断文件类型
        if (S_ISDIR(file_stat.st_mode))
        {
            // 如果是目录，则跳过
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            LOG_MSG(WRN_LOG,"mgr_sitetask::read_upgrade_filelist() file_path(%s) is directory type",file_path);
            read_upgrade_filelist(file_path,vect_file,vect_prefix);
        }
        else
        {
            // 如果是文件，则保存文件名
            std::string filename = entry->d_name;
            for(auto it : vect_prefix)
            {
                std::string prefix = it;
                if((!prefix.empty()) && filename.compare(0,prefix.length(),prefix) == 0)
                {
                    vect_file.push_back(filename);
                    LOG_MSG(WRN_LOG,"mgr_sitetask::read_upgrade_filelist() filename(%s)",filename.c_str());
                }
            }
        }
    }

    // 关闭目录
    closedir(dir);
    dir = NULL;

    ret = vect_file.size();

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::read_upgrade_filelist() ret:%d",ret);
    return ret;
}


//从文件名中得到升级接口类型
int mgr_sitetask::get_pkg_interFace_type(const std::string& filename)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::get_pkg_interFace_type() filename:%s",filename.c_str());
    int type = PKG_TYPE_NONE;

    //通过文件名的中前缀字段区分不同的升级接口类型

    //1、读取pkg_type.json
    const char* prog_path = xbasic::get_module_path();
    std::string json_file = std::string(prog_path)+"pkg_type.json";
    std::string json_str = xbasic::read_data_from_file(json_file);

    cJSON_object js_root(json_str);
    cJSON_object pkg = js_root["pkg"];
    int pkg_size = pkg.get_array_size();

    for(int i = 0; i < pkg_size; i++)
    {
        bool found = false;
        cJSON_object pkg_obj = pkg[i];
        std::string pkg_type = pkg_obj["pkg_type"];
        cJSON_object pkg_prefix = pkg_obj["pkg_prefix"];
        int pkg_prefix_sz = pkg_prefix.get_array_size();
        for(int j = 0; j < pkg_prefix_sz; j++)
        {
            cJSON_object prefix_obj = pkg_prefix[j];
            std::string prefix = prefix_obj["prefix"];
            if((!prefix.empty()) && filename.compare(0,prefix.length(),prefix) == 0)
            {
                found = true;
                break;
            }
        }

        if(found == true)
        {
            if(pkg_type == std::string("pkg_x86"))
            {
                type = PKG_TYPE_X86;
            }
            else if(pkg_type == std::string("pkg_usb"))
            {
                type = PKG_TYPE_USB;
            }
            else if(pkg_type == std::string("pkg_tcp"))
            {
                type = PKG_TYPE_TCP;
            }
            else if(pkg_type == std::string("pkg_serdes"))
            {
                type = PKG_TYPE_SERDES_DRV;
            }
            else if(pkg_type == std::string("pkg_pcie"))
            {
                type = PKG_TYPE_PCIE_DRV;
            }
            else if(pkg_type == std::string("pkg_spi"))
            {
                type = PKG_TYPE_SPI_DRV;
            }
            else if(pkg_type == std::string("pkg_arm"))
            {
                type = PKG_TYPE_ARM;
            }
            else
            {
                type = PKG_TYPE_NONE;
            }

            break;
        }
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::get_pkg_interFace_type() type:%d",type);
    return type;
}


//进行下级设备管理升级转发
int mgr_sitetask::process_pkg_forwardupgrade(xsite_package *pack)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::process_pkg_forwardupgrade()");
    int ret = 0;
    if(pack == NULL)
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"mgr_sitetask::process_pkg_forwardupgrade() pack is NULL,ret:%d",ret);
        return ret;
    }

#if defined(SYNC_BUILD)

    //1、保存需要转发的board任务集合
    add_forward_taskprogress();
    //2、通过mgr_sitectrl_sync模块中sitectrl sync服务端接口，将消息转发下级设备管理
    int len = m_sitectrl_sync_mgr->send_to_allsynccli(pack);
    //这里要在消息转发成功的前提下再去添加任务，避免出现任务添加了，但是消息转发失败
    if(len > 0)
    {
        //1、保存需要转发的board任务集合
        add_forward_taskprogress();
        //需要注意这里还可能出现消息转发成功了，但是没有要升级的单板，这里就不必添加任务
        //这里的解决方案是在上一层调用的地方，process_pkg_upgrade() 中判断是否存在要升级的单板，存在的话才会调用process_pkg_forwardupgrade
        ret = 0;
        LOG_MSG(MSG_LOG,"mgr_sitetask::process_pkg_forwardupgrade() len:%d,ret:%d",len,ret);
    }
    else
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"mgr_sitetask::process_pkg_forwardupgrade() len:%d,ret:%d",len,ret);
    }

#endif

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::process_pkg_forwardupgrade() ret:%d",ret);
    return ret;
}


//linux/arm下shell脚本方式升级
int mgr_sitetask::pkg_shell_upgrade(xsite_package *pack,boost::shared_ptr<cxmd> cmd)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::pkg_shell_upgrade()");

    int ret = 0;
    //获取upgrade_local目录
    std::string upgrade_local_path = m_sys_config->get_data("upgrade_local");
    if(upgrade_local_path.back() != '/')
    {
        upgrade_local_path += std::string("/");
    }
    //获取upgrade_script执行脚本文件名
    std::string script_name = m_sys_config->get_data("upgrade_script");

    do
    {
        //1、检查升级脚本
        if(xbasic::file_exist(upgrade_local_path + "target/" + script_name) == false)
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::pkg_shell_upgrade() upgrade script file(%s) is not exist",script_name.c_str());
            cmd->add_return_info("upgrade script file is not exist");
            cmd->add_return_code("7");

            ret = -1;
            break;
        }

        //将target目录下执行权限改为755
        std::string target_dir = upgrade_local_path + std::string("target");
        int chmod_ret = xbasic::exe_sys_cmd("chmod -R 755 " + target_dir);
        if(chmod_ret < 0)
        {
            LOG_MSG(ERR_LOG,"mgr_sitetask::pkg_shell_upgrade() change directory(%s) permission failed chmod_ret:%d",target_dir.c_str(),chmod_ret);

            //加入return_info信息
            cmd->add_return_info("upgrade failed");
            cmd->add_return_code("25");

            ret = -1;
            break;
        }
        LOG_MSG(WRN_LOG,"mgr_sitetask::pkg_shell_upgrade() change directory(%s) permission chmod_ret:%d",target_dir.c_str(),chmod_ret);

    }while(0);

    if(ret == 0)
    {
        // 保存请求消息的json数据到shell_request.json
        std::string filename = "shell_request.json";
        std::string file_path = std::string(xbasic::get_module_path()) + "/" + filename;
        std::string json_req = pack->serial_to_json();
        xbasic::save_to_file(file_path.c_str(),(const_cast<char*>)(json_req.c_str()), json_req.length());
        LOG_MSG(WRN_LOG,"mgr_sitetask::pkg_shell_upgrade() file_path:%s json_req:%s",file_path.c_str(),json_req.c_str());

        sync();
        sleep(15);

        //设置start_shellupgrade-fork标识让main函数执行fork操作
        g_startfork = 1;
        LOG_MSG(MSG_LOG,"mgr_sitetask::pkg_shell_upgrade() set g_startfork:%d",g_startfork);
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::pkg_shell_upgrade() ret:%d",ret);
    return ret;
}


//USB串口方式升级
int mgr_sitetask::pkg_usb_upgrade(const xpkg_interFace::FileList& filelist)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::pkg_usb_upgrade()");
    int ret = 0;

    //获取OTA目录
    std::string ota_dir = m_sys_config->get_data("ota_path");

    for(auto it : filelist)
    {
        int boardtype = 0;
        int ota_type = 0;
        std::string filename = it;

        //1、从文件名的名称前缀获取到文件对应的BoardType
        bool bflag1 = get_boardtype(filename,boardtype);
        //2、从文件名的名称前缀获取到文件对应的OTAtype
        bool bflag2 = get_otatype(filename,ota_type);

        if(bflag1 && bflag2)
        {
            //3、删除OTA目录下同类型除filename外其它文件
            remove_file(ota_dir,filename);

            //4、保存每个单板升级进度状态
            add_taskprogress(boardtype);
            //5、获取本地usb串口地址协调这个BoardType的所有地址信息
            //向这些串口地址发送升级消息
            mgr_usb* usb_mgr = mgr_usb::get_instance();
            int is_ok = usb_mgr->send_all_start(boardtype, ota_type);
        }
        if(is_ok != 0)
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::pkg_usb_upgrade() filename:%s boardtype:0x%x ota_type:%d upgrade failed",filename.c_str(),boardtype,ota_type);
        }
        else
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::pkg_usb_upgrade() filename:%s boardtype:0x%x ota_type:%d gettype failed",filename.c_str(),boardtype,ota_type);
        }
    }

    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::pkg_usb_upgrade() ret:%d",ret);
    return ret;
}

int mgr_sitetask::pkg_usb_upgrade_ex(const xpkg_interFace::FileList& filelist)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::pkg_usb_upgrade_ex()");
    int ret = 0;

    //获取OTA目录
    std::string ota_dir = m_sys_config->get_data("ota_path");

    //检查升级文件列表中的升级固件是否齐全
    bool c_flag = check_ver_exist(PKG_TYPE_USB);
    if(c_flag == false)
    {
        ret = -1;
        LOG_MSG(WRN_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() check ver exited failed ret:%d", ret);
        return ret;
    }

    // 升级文件类型和板类型的映射表
    std::unordered_map<int, int> ota_boardtype_map;
    for(auto it : filelist)
    {
        int boardtype = 0;
        int ota_type = 0;
        std::string filename = it;

        //1、从文件名的名称前缀获取到文件对应的BoardType
        bool bflag1 = get_boardtype(filename,boardtype);
        //2、从文件名的名称前缀获取到文件对应的OTAtype
        bool bflag2 = get_otatype(filename,ota_type);

        if(bflag1 && bflag2)
        {
            //3、删除OTA目录下同类型除filename外其它文件
            ret = remove_file(ota_dir,filename);
            if(ret != 0)
            {
                LOG_MSG(WRN_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() remove file ota_dir:%s filename:%s failed", ota_dir.c_str(), filename.c_str());
                return ret;
            }

            ota_boardtype_map[ota_type] = boardtype;

            LOG_MSG(MSG_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() find ota_type:%d boardtype:0x%x", ota_type, boardtype);
        }
        else
        {
            LOG_MSG(WRN_LOG,"mgr_sitetask::pkg_usb_upgrade_ex() filename:%s boardtype:0x%x ota_type:%d get type failed",filename.c_str(),boardtype,ota_type);
        }
    }

    // board type ota type
    for(auto ota_board_it : ota_boardtype_map)
    {
        int o_type = ota_board_it.first;
        int b_type = ota_board_it.second;
        LOG_MSG(MSG_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() o_type:%d b_type:0x%x", o_type, b_type);
        // 遍历该类型的单板
        boost::unordered_map<int,boost::shared_ptr<cxboard> > all_map;
        xboardmgr* mgrboard = xboardmgr::get_instance();
        all_map = mgrboard->get_all_board();
        for(auto board_it : all_map)
        {
            int board_id = board_it.first;
            boost::shared_ptr<cxboard> board_ptr = board_it.second;
            if((board_id & BOARDTYPE_MASK) == b_type)
            {
                // 拿到该单板的硬件版本信息（MCU）
                boost::shared_ptr<xusb_tvl> tvl = board_ptr->find_data(RNA_HW_VER);
                if(tvl != nullptr)
                {
                    int str_len = strlen(tvl->m_value.c_str());
                    std::string hw_ver_str(tvl->m_value.c_str(), str_len);
                    LOG_MSG(MSG_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() board_id:0x%x find board hw version:%s size:%d", board_id, hw_ver_str.c_str(), hw_ver_str.size());
                    //找到了单板硬件版本信息
                    //到版本信息映射表中查找该硬件版本对应的软件包
                    mgr_board_ver* board_ver_mgr = mgr_board_ver::get_instance();
                    std::string soft_ver = board_ver_mgr->get_soft_ver(o_type, hw_ver_str);
                    if(!soft_ver.empty())
                    {
                        LOG_MSG(MSG_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() board_id:0x%x ota_type:%d hw version:%s find soft version:%s", board_id, o_type, hw_ver_str.c_str(), soft_ver.c_str());
                        std::string dir = ota_dir;
                        if(dir.back() != '/')
                        {
                            dir += std::string("/");
                        }

                        std::string file_path = dir + soft_ver;
                        LOG_MSG(MSG_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() file_path:%s", file_path.c_str());

                        // 添加任务检测
                        add_one_board_taskprogress(board_id);

                        // 向单板发送开始升级请求
                        mgr_usb* usb_mgr = mgr_usb::get_instance();
                        int is_ok = usb_mgr->send_ota_start(board_id, o_type, file_path);

                        if(is_ok != 0)
                        {
                            // 当前板子升级失败
                            xtaskprogress* xtask_progress = xtaskprogress::get_instance();
                            xtask_progress->set_completeStatus(board_id);
                            xtask_progress->set_taskResult(board_id,xtaskprogress::TASKRESULT_FAILED);
                            LOG_MSG(WRN_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() send ota start failed board_id:0x%x ota_type:%d file_path:%s", board_id, o_type, file_path.c_str());
                        }
                        else
                        {
                            //没有找到当前硬件版本所需的软件包，返回失败---理论上不会走到这里，因为在 check_ver_exited 中已经判断，如果出现这种情况会直接返回失败
                            ret = -1;
                            LOG_MSG(WRN_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() board_id:0x%x ota_type:%d hw version:%s not find soft version! ret:%d", board_id, o_type, hw_ver_str.c_str(), ret);
                            return ret;
                        }
                    }
                    else
                    {
                        // 没找到该单板的硬件版本信息---理论上不会走到这里，因为在 check_ver_exited 中已经判断，如果出现这种情况会直接返回失败
                        ret = -1;
                        LOG_MSG(WRN_LOG, "mgr_sitetask::pkg_usb_upgrade_ex() board_id:0x%x not find hw version! ret:%d", board_id, ret);
                        return ret;
                    }
                }
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited mgr_sitetask::pkg_usb_upgrade_ex() ret=%d",ret);
    return ret;
}

    {
        if(fileStat.st_mode & S_IFDIR)
        {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            LOG_MSG(MSG_LOG, "mgr_sitetask::read_upgrade_filelist() file_path(%s) is directory type", file_path);
            read_upgrade_filelist(file_path, vec_file, vec_profile);
        }
        else
        {
            // 如果是文件，获取文件名
            std::string filename = entry->d_name;
            if(file_type == "")
            {
                // 直接保存文件名
                vec_file.push_back(filename);
                LOG_MSG(MSG_LOG, "mgr_sitetask::read_upgrade_filelist() filename=%s", filename.c_str());
            }
            else
            {
                // 判断文件名前缀是否和传入的前缀相同，相同才保存
                if(filename.compare(0, prefix.length(), prefix) == 0)
                {
                    vec_file.push_back(filename);
                    LOG_MSG(MSG_LOG, "mgr_sitetask::read_upgrade_filelist() filename=%s", filename.c_str());
                }
            }
        }
    }

    // 关闭目录
    closedir(dir);
    dir = NULL;

    ret = vec_file.size();
    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::read_upgrade_filelist() ret=%d", ret);
    return ret;
}

// 从文件名中解析出升级包类型
int mgr_sitetask::get_pkg_interface_type(const std::string& filename)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::get_pkg_interface_type() filename=%s", filename.c_str());
    int type = PKG_TYPE_NONE;

    // 通过文件名前缀判断是那个不同的升级包类型
    std::string pkg_type;
    const char* pkg_path = xbasic::get_module_path();
    std::string json_file = std::string(pkg_path) + "PKG_type.json";
    std::string json_str = xbasic::read_data_from_file(json_file);

    cJSON_object js_root(json_str);
    cJSON_object pkg = js_root[filename];
    if(pkg != NULL)
    {
        pkg_type = pkg["type"];
        if(pkg_type == "USB")
        {
            type = PKG_TYPE_USB;
        }
        else if(pkg_type == "SERDES")
        {
            type = PKG_TYPE_SERDES_DRV;
        }
        else if(pkg_type == "PCIE")
        {
            type = PKG_TYPE_PCIE_DRV;
        }
        else if(pkg_type == "SPI")
        {
            type = PKG_TYPE_SPI_DRV;
        }
        else if(pkg_type == "TCP")
        {
            type = PKG_TYPE_TCP;
        }
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::get_pkg_interface_type() type=%d", type);
    return type;
}

// 检查升级文件列表中的升级固件是否齐全
bool mgr_sitetask::check_ver_exist(int pkg_type)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::check_ver_exist() pkg_type=%d", pkg_type);
    bool ret = true;
    std::string upgrade_local_path = m_sys_config->get_data("upgrade_local");
    if(upgrade_local_path.empty())
    {
        LOG_MSG(ERR_LOG, "mgr_sitetask::check_ver_exist() upgrade_local_path is empty");
        return false;
    }
    if(upgrade_local_path.back() != '/')
    {
        upgrade_local_path += "/";
    }
    upgrade_local_path += "target/";

    // 读取升级文件列表文件
    std::vector<std::string> vec_file;
    std::vector<std::string> vec_prefix;
    int file_num = read_upgrade_filelist(upgrade_local_path, vec_file, vec_prefix);
    if(file_num <= 0)
    {
        LOG_MSG(ERR_LOG, "mgr_sitetask::check_ver_exist() read_upgrade_filelist failed");
        return false;
    }

    // 获取升级包类型对应的板卡类型映射表
    std::unordered_map<int, int> ota_boardtype_map;
    std::unordered_map<std::string, int>::iterator it;
    if(pkg_type == PKG_TYPE_USB)
    {
        for(it = m_boardtype_map.begin(); it != m_boardtype_map.end(); it++)
        {
            ota_boardtype_map[it->second] = 0;
        }
    }
    else if(pkg_type == PKG_TYPE_SERDES_DRV)
    {
        for(it = m_otatype_map.begin(); it != m_otatype_map.end(); it++)
        {
            if(it->second == OTA_TYPE_FTTUSB_FPGM_FPGA || it->second == OTA_TYPE_FTTUSB_FPOS_FPGA)
            {
                ota_boardtype_map[BOARDTYPE_FT_PGB] = 0;
            }
            else
            {
                ota_boardtype_map[it->second] = 0;
            }
        }
    }
    else if(pkg_type == PKG_TYPE_PCIE_DRV)
    {
        for(it = m_otatype_map.begin(); it != m_otatype_map.end(); it++)
        {
            ota_boardtype_map[it->second] = 0;
        }
    }
    else if(pkg_type == PKG_TYPE_SPI_DRV)
    {
        for(it = m_otatype_map.begin(); it != m_otatype_map.end(); it++)
        {
            ota_boardtype_map[it->second] = 0;
        }
    }
    else if(pkg_type == PKG_TYPE_TCP)
    {
        for(it = m_otatype_map.begin(); it != m_otatype_map.end(); it++)
        {
            ota_boardtype_map[it->second] = 0;
        }
    }

    // 遍历升级文件列表，检查升级固件是否齐全
    for(auto it : vec_file)
    {
        int boardtype = 0;
        int ota_type = 0;
        std::string filename = it;

        // 1、从文件名的名称前缀获取到文件对应的Boardtype
        bool bflag1 = get_boardtype(filename, boardtype);
        // 2、从文件名的名称前缀获取到文件对应的OTAtype
        bool bflag2 = get_otatype(filename, ota_type);

        if(bflag1 && bflag2)
        {
            if(pkg_type == PKG_TYPE_USB)
            {
                ota_boardtype_map[boardtype] = 1;
            }
            else
            {
                ota_boardtype_map[ota_type] = 1;
            }
        }
    }

    // 检查升级固件是否齐全
    for(auto it : ota_boardtype_map)
    {
        if(it.second == 0)
        {
            ret = false;
            LOG_MSG(ERR_LOG, "mgr_sitetask::check_ver_exist() boardtype=0x%x ota_type=%d not exist", it.first, it.first);
            break;
        }
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::check_ver_exist() ret=%d", ret);
    return ret;
}

// 获取板卡类型
bool mgr_sitetask::get_boardtype(const std::string& filename, int& board_type)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::get_boardtype() filename=%s", filename.c_str());
    bool found = false;

    for(auto it : m_boardtype_map)
    {
        const std::string& prefix = it.first;
        if(filename.compare(0, prefix.length(), prefix) == 0)
        {
            board_type = it.second;
            found = true;
            break;
        }
    }

    if(!found)
    {
        LOG_MSG(WRN_LOG, "mgr_sitetask::get_boardtype() filename=%s board_type=%d found=%d", filename.c_str(), board_type, found);
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::get_boardtype() board_type=%d found=%d", board_type, found);
    return found;
}

// 从升级文件名中获取OTA文件类型
bool mgr_sitetask::get_otatype(const std::string& filename, int& ota_type)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::get_otatype() filename=%s", filename.c_str());

    bool found = false;

    for(auto it : m_otatype_map)
    {
        const std::string& prefix = it.first;
        if(filename.compare(0, prefix.length(), prefix) == 0)
        {
            ota_type = it.second;
            found = true;
            break;
        }
    }

    if(!found)
    {
        LOG_MSG(WRN_LOG, "mgr_sitetask::get_otatype() filename=%s ota_type=%d found=%d", filename.c_str(), ota_type, found);
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::get_otatype() ota_type=%d found=%d", ota_type, found);
    return found;
}

// 删除文件
int mgr_sitetask::removefile(const std::string& path, const std::string& filename)
{
    int ret = 0;
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::removefile() path=%s filename=%s", path.c_str(), filename.c_str());
    if(path.empty())
    {
        LOG_MSG(ERR_LOG, "mgr_sitetask::removefile() filename=%s path is empty", filename.c_str());
        return -1;
    }

    if(filename.empty())
    {
        LOG_MSG(ERR_LOG, "mgr_sitetask::removefile() path=%s filename is empty", path.c_str());
        return -1;
    }

    std::string dir = path;
    if(dir.back() != '/')
    {
        dir += std::string("/");
    }

    std::string filetype;
    std::string::size_type found = filename.find(".");
    if(found != std::string::npos)
    {
        filetype = filename.substr(0, found);
    }
    else
    {
        filetype = filename;
    }
    LOG_MSG(WRN_LOG, "mgr_sitetask::removefile() dir=%s filename=%s filetype=%s", dir.c_str(), filename.c_str(), filetype.c_str());

    // 将文件从upgrade_local目录下移到ota目录下
    std::string upgrade_local_path = m_sys_config->get_data("upgrade_local");
    if(upgrade_local_path.back() != '/')
    {
        upgrade_local_path += std::string("/");
    }
    upgrade_local_path += std::string("/");

    std::string src_filename = upgrade_local_path + std::string("target/") + filename;
    std::string dst_filename = dir + filename;
    int mv_ret = movefile(src_filename, dst_filename);
    if(mv_ret != 0)
    {
        LOG_MSG(WRN_LOG, "mgr_sitetask::removefile() move src_filename=%s to dst_filename=%s failed", src_filename.c_str(), dst_filename.c_str());
    }
    else
    {
        LOG_MSG(MSG_LOG, "mgr_sitetask::removefile() move src_filename=%s to dst_filename=%s success", src_filename.c_str(), dst_filename.c_str());
        // target目录
        // 将下面的注释掉，单板 MCU 的升级文件可能会存放在同一个 PKG 包中，并且在 unpacker_pkg 会清空 upgrade_local_path 目录
        // /xbasic::exe_sys_cmd("rm -rf " + upgrade_local_path + "target/");

        // 删除同类型文件(filename文件除外)
        // find . -maxdepth 1 -name 'CPSYNC_MCU*' ! -name 'CPSYNC_MCU-V0.0.0.1.bin' | xargs rm -f
        char rm_cmd[1024] = {0};
        sprintf(rm_cmd, "find %s -maxdepth 1 -name '%s*' ! -name '%s' | xargs rm -f", dir.c_str(), filetype.c_str(), filename.c_str());
        LOG_MSG(MSG_LOG, "mgr_sitetask::removefile() rm_cmd=%s", rm_cmd);

        ret = xbasic::exe_sys_cmd(rm_cmd);

        // 重新刷新OTA目录
        mgr_upgrade* upgrade_mgr = mgr_upgrade::get_instance();
        upgrade_mgr->load_otafiles();
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::removefile() ret=%d", ret);
    return ret;
}

// 移动文件
int mgr_sitetask::movefile(const std::string& src, const std::string& dst)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::movefile() src=%s dst=%s", src.c_str(), dst.c_str());
    int ret = 0;
    if(src.empty())
    {
        LOG_MSG(ERR_LOG, "mgr_sitetask::movefile() src is empty");
        ret = -1;
    }
    else if(dst.empty())
    {
        LOG_MSG(ERR_LOG, "mgr_sitetask::movefile() dst is empty");
        ret = -1;
    }
    else
    {
        if(rename(src.c_str(), dst.c_str()) != 0)
        {
            LOG_MSG(ERR_LOG, "mgr_sitetask::movefile() src=%s move to dst=%s failed", src.c_str(), dst.c_str());
            ret = -1;
        }
        else
        {
            LOG_MSG(MSG_LOG, "mgr_sitetask::movefile() src=%s move to dst=%s success", src.c_str(), dst.c_str());
        }
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::movefile() ret=%d", ret);
    return ret;
}

int mgr_sitetask::clear_dir(const std::string& dir_path)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::clear_dir() dir_path=%s", dir_path.c_str());

    int ret = 0;

    if(dir_path.empty())
    {
        LOG_MSG(ERR_LOG, "mgr_sitetask::clear_dir() dir_path=%s is empty", dir_path.c_str());
        ret = -1;
        return ret;
    }

    std::string dir = dir_path;
    if(dir.back() != '/')
    {
        dir += std::string("/");
    }

    // rm -rf /home/gh/USB-Device-Mgr/OTA/*
    char rm_cmd[1024] = {0};
    sprintf(rm_cmd, "rm -rf %s*", dir.c_str());
    ret = xbasic::exe_sys_cmd(rm_cmd);

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::clear_dir() ret=%d", ret);
    return ret;
}

bool mgr_sitetask::check_ver_exist(int pkg_type)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::check_ver_exist() pkg_type=%d", pkg_type);
    bool ret = false;

    // 单板类型集合
    std::set<int> boardtype_set;
    // ota 类型集合
    std::unordered_map<int, int> ota_boardtype_map;

    xpkg_interface* pkg_instance = xpkg_interface::get_instance();
    int sz = 0;

    std::shared_ptr<xpkg_interface::FileList> filelist = pkg_instance->find(pkg_type);
    if(filelist != nullptr)
    {
        for(auto it : *filelist)
        {
            std::string filename = it;
            int boardtype = 0;
            int ota_type = 0;
            bool bflag1 = get_boardtype(filename,boardtype);
            bool bflag2 = get_otatype(filename, ota_type);
            if(bflag1 == true && bflag2 == true)
            {
                ota_boardtype_map[ota_type] = boardtype;
                LOG_MSG(MSG_LOG, "mgr_sitetask::check_ver_exist() insert ota_type=%d boardtype=0x%x", ota_type, boardtype);
            }
        }
    }

    sz = ota_boardtype_map.size();
    for(auto it : ota_boardtype_map)
    {
        // 拿到了单板类型
        ret = check_board_ver(it.first, it.second);
        if(ret == false)
        {
            LOG_MSG(WRN_LOG, "mgr_sitetask::check_ver_exist() ota_type=%d board_type=0x%x check board ver failed!", it.first, it.second);
            break;
        }
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::check_ver_exist() ret=%d", ret);
    return ret;
}

bool mgr_sitetask::check_board_ver(int ota_type, int board_type)
{
    // 该函数的作用是: 判断当前环境上的所有 board_type 单板对应的 ota_type 升级类型的文件是否在下发的升级包中包含
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::check_board_ver() ota_type=%d board_type=0x%x", ota_type, board_type);
    bool ret = false;

    boost::unordered_map<int,boost::shared_ptr<xboard>> all_map;
    xboardmgr* mgrboard = xboardmgr::get_instance();
    all_map = mgrboard->get_all_board();

    for(auto it : all_map)
    {
        int board_id = it.first;
        boost::shared_ptr<xboard> board_ptr = it.second;
        if((board_id & BOARDTYPE_MASK) == board_type)
        {
            // 获取单板的硬件版本信息
            boost::shared_ptr<xusb_tvl> tvl = board_ptr->find_data(RWA_HW_VER);
            if(tvl != nullptr)
            {
                int str_len = strlen(tvl->m_value.c_str());
                std::string hw_ver_str(tvl->m_value.c_str(), str_len);
                LOG_MSG(MSG_LOG, "mgr_sitetask::check_board_ver() board_id=0x%x find board hw version:%s size=%d", board_id, hw_ver_str.c_str(), hw_ver_str.size());

                // 到版本信息列表中查找是否存在该硬件版本对应的软件包
                mgr_board_ver* board_ver_mgr = mgr_board_ver::get_instance();
                std::string soft_ver = board_ver_mgr->get_soft_ver(ota_type, hw_ver_str);
                if(soft_ver.empty())
                {
                    // 没有找到当前硬件版本所需的软件包, 返回失败
                    ret = false;
                    LOG_MSG(WRN_LOG, "mgr_sitetask::check_board_ver() board_id=0x%x ota_type=%d hw version:%s not find soft version!", board_id, ota_type, hw_ver_str.c_str());
                    break;
                }
                else
                {
                    // 找到了当前硬件版本所需的软件, 设置成功
                    ret = true;
                    LOG_MSG(MSG_LOG, "mgr_sitetask::check_board_ver() board_id=0x%x ota_type=%d hw version:%s find soft version:%s", board_id, ota_type, hw_ver_str.c_str(), soft_ver.c_str());
                }
            }
            else
            {
                // 没找到该单板的硬件版本信息
                ret = false;
                LOG_MSG(WRN_LOG, "mgr_sitetask::check_board_ver() board_id=0x%x not find hw version!", board_id);
                break;
            }
        }
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::check_board_ver() ret=%d", ret);
    return ret;
}

int mgr_sitetask::remove_file(const std::string& path, const std::string& filename)
{
    int ret = 0;
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::remove_file() path:%s filename:%s",path.c_str(),filename.c_str());
    if(path.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG, "mgr_sitetask::remove_file() filename:%s, path is empty",filename.c_str());
        return ret;
    }

    if(filename.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG, "mgr_sitetask::remove_file() path:%s filename is empty",path.c_str());
        return ret;
    }

    std::string dir = path;
    if(dir.back() != '/')
    {
        dir += std::string("/");
    }

    std::string filetype;
    std::string::size_type found = filename.find("-");
    if(found != std::string::npos)
    {
        filetype = filename.substr(0,found);
    }
    else
    {
        filetype = filename;
    }
    LOG_MSG(WRN_LOG, "mgr_sitetask::remove_file() dir:%s filename:%s filetype:%s",dir.c_str(),filename.c_str(),filetype.c_str());

    // 将文件从upgrade_local目录下移到ota目录下
    std::string upgrade_local_path = m_sys_config->get_data("upgrade_local");
    if(upgrade_local_path.back() != '/')
    {
        upgrade_local_path += std::string("/");
    }
    std::string src_filename = upgrade_local_path + std::string("target/")+filename;
    std::string dst_filename = dir + filename;

    int mv_ret = movefile(src_filename,dst_filename);
    if(mv_ret != 0)
    {
        ret = -1;
        LOG_MSG(WRN_LOG, "mgr_sitetask::remove_file() move src_filename:%s to dst_filename:%s failed",src_filename.c_str(),dst_filename.c_str());
        return ret;
    }
    else
    {
        LOG_MSG(MSG_LOG, "mgr_sitetask::remove_file() move src_filename:%s to dst_filename:%s success",src_filename.c_str(),dst_filename.c_str());
        //target目录
        // 将下面的注释掉, 单板MCU 的升级的升级文件可能会存放在同一个 PKG 包中, 并且在 unpacker_pkg 会清空 upgrade_local_path 目录
        // xbasic::exe_cmd("rm -rf " + upgrade_local_path + "target/");
    }

    //重新刷新OTA目录
    mgr_upgrade *upgrade_mgr = mgr_upgrade::get_instance();
    // 将该文件添加到映射
    bool l_flag = upgrade_mgr->load_otafile(dst_filename);
    if(l_flag == false)
    {
        ret = -1;
        LOG_MSG(WRN_LOG, "mgr_sitetask::remove_file() load ota file:%s failed", dst_filename.c_str());
        return ret;
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::remove_file() ret=%d",ret);
    return ret;
}

// 解析上位机发送过来的硬件版本信息映射表
void mgr_sitetask::parse_hardware_map(xsite_package *pack, boost::shared_ptr<xcmd> cmd)
{
    int ret = 0;
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::parse_hardware_map()");

    // 到这里说明升级包中有该升级文件
    mgr_board_ver* board_ver_mgr = mgr_board_ver::get_instance();
    // 清空硬件版本信息映射表
    board_ver_mgr->clear_map();

    std::string hardware_map_str = cmd->get_param("hardware_map");
    cjson_object hardware_map_json(hardware_map_str);
    int hardware_size = hardware_map_json.get_array_size();
    LOG_MSG(MSG_LOG, "mgr_sitetask::parse_hardware_map() hardware_size:%d", hardware_size);

    for(int i = 0; i < hardware_size; i++)
    {
        cjson_object hardware_obj_json = hardware_map_json[i];
        std::string hardware_ver_str = hardware_obj_json["hardware_ver"];
        cjson_object components_obj = hardware_obj_json["components"];
        int components_size = components_obj.get_array_size();
        LOG_MSG(MSG_LOG, "mgr_sitetask::parse_hardware_map() hardware_ver_str:%s components_size:%d", hardware_ver_str.c_str(), components_size);

        for(int j = 0; j < components_size; ++j)
        {
            std::string file_name = components_obj[j].to_string();
            if(file_name[0] == '\"' && file_name.length() >= 2)
            {
                // 截取掉文件名中携带的冒号
                file_name = file_name.substr(1, file_name.length()-2);
                LOG_MSG(MSG_LOG, "mgr_sitetask::parse_hardware_map() file_name:%s", file_name.c_str());

                // 判断升级包中是否存在这个文件
                std::string upgrade_local_path = m_sys_config->get_data("upgrade_local");
                if(upgrade_local_path.back() != '/')
                {
                    upgrade_local_path += std::string("/");
                }
                std::string file_path = upgrade_local_path + std::string("target/")+file_name;

                if(access(file_path.c_str(), F_OK) != 0)
                {
                    // 文件不存在
                    LOG_MSG(WRN_LOG, "mgr_sitetask::parse_hardware_map() file_path:%s not exist", file_path.c_str());
                    continue;
                }

                int ota_type = 0;
                bool flag = get_otatype(file_name, ota_type);
                if(flag == false)
                {
                    // 获取升级类型失败
                    LOG_MSG(WRN_LOG, "mgr_sitetask::parse_hardware_map() get ota failed fiel_name:%s", file_name.c_str());
                    continue;
                }

                // 向映射表中添加映射关系
                board_ver_mgr->set_hardware_map(ota_type, hardware_ver_str, file_name);
                LOG_MSG(MSG_LOG, "mgr_sitetask::parse_hardware_map() set hardware map ota_type:%d hw_ver:%s file_name:%s", ota_type, hardware_ver_str.c_str(), file_name.c_str());
            }
        }
    }

    // 解析完了, 将版本信息移除
    cmd->clear_param("hardware_map");

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::parse_hardware_map() ret:%d", ret);
}

void mgr_sitetask::clear_taskprogress()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::clear_taskprogress()");
    xtaskprogress* taskprogress = xtaskprogress::get_instance();
    taskprogress->clear();
    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::clear_taskprogress()");
}

int mgr_sitetask::add_taskprogress(const int& boardtype)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::add_taskprogress() boardtype=0x%x",boardtype);
    int ret = 0;

    boost::unordered_map<int,boost::shared_ptr<xboard>> all_map;
    xboardmgr* mgrboard = xboardmgr::get_instance();
    all_map = mgrboard->get_all_board();
    for(auto it : all_map)
    {
        if((it.first & BOARDTYPE_MASK) == boardtype)
        {
            int boardid = it.first;
            xtaskprogress* xtask_progress = xtaskprogress::get_instance();
            xtask_progress->set_startstatus(boardid);
            xtask_progress->set_taskresult(boardid, xtaskprogress::TASKRESULT_NONE);
            ret++;
        }
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::add_taskprogress() ret=%d",ret);
    return ret ;
}

void mgr_sitetask::add_one_board_taskprogress(const int &board_id)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::add_one_board_taskprogress() board_id=0x%d", board_id);
    xtaskprogress* xtask_progress = xtaskprogress::get_instance();
    xtask_progress->set_startstatus(board_id);
    xtask_progress->set_taskresult(board_id, xtaskprogress::TASKRESULT_NONE);
    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::add_one_board_taskprogress()");
}

bool mgr_sitetask::add_one_board_task_progress(const int &board_type, int &board_id)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::add_one_board_task_progress() board_type:0x%x", board_type);
    bool ret = false;

    boost::unordered_map<int,boost::shared_ptr<xboard>> all_map;
    xboardmgr* mgrboard = xboardmgr::get_instance();
    all_map = mgrboard->get_all_board();
    for(auto it : all_map)
    {
        if((it.first & BOARDTYPE_MASK) == board_type)
        {
            int boardid = it.first;
            xtaskprogress* xtask_progress = xtaskprogress::get_instance();
            xtask_progress->set_startstatus(boardid);
            xtask_progress->set_taskresult(boardid, xtaskprogress::TASKRESULT_NONE);
            board_id = boardid;
            ret = true;
            LOG_MSG(MSG_LOG, "mgr_sitetask::add_one_board_task_progress() find board_id:0x%x", board_id);
            break;
        }
    }

    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::add_one_board_task_progress() ret:%d board_id:0x%x", ret, board_id);
    return ret;
}

// 将转发的boardid加入任务进度
int mgr_sitetask::add_forward_taskprogress()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::add_forward_taskprogress()");

    // 1、保存需要转发的boardid任务集合
    std::set<int> boardtype_set;
    xpkg_interface* pkg_instance = xpkg_interface::get_instance();
    int sz = 0;

#if defined(SYNC_BUILD)
#if defined(DEV_TYPE_CP)
    std::shared_ptr<xpkg_interface::FileList> arm_pcie_filesit = pkg_instance->find(PKG_TYPE_PCIE_DRV);
    if(arm_pcie_filesit != NULL)
    {
        xpkg_interface::FileList filelist = *arm_pcie_filesit;
        for(auto it : filelist)
        {
            std::string filename = it;
            int boardtype = 0;
            bool flag = get_boardtype(filename,boardtype);
            if(flag == true && (boardtype == BOARDTYPE_CP_RCA))
            {
                boardtype_set.insert(boardtype);
            }
        }
    }
#elif defined(DEV_TYPE_FT)
    std::shared_ptr<xpkg_interface::FileList> mcu_fpga_filelist = pkg_instance->find(PKG_TYPE_USB);
    std::shared_ptr<xpkg_interface::FileList> serdes_filelist = pkg_instance->find(PKG_TYPE_SERDES_DRV);
    if((mcu_fpga_filelist != NULL) || (serdes_filelist != NULL))
    {
        // 检查是否存在需要转PKG_TYPE_USB的mcu、fpga对应boardid
        xpkg_interface::FileList filelist;
        if(mcu_fpga_filelist)
        {
            filelist = *mcu_fpga_filelist;
            LOG_MSG(MSG_LOG, "mgr_sitetask::add_forward_taskprogress() ft sync PKG_TYPE_USB mcu_fpga");
        }
        else
        {
            filelist = *serdes_filelist;
            LOG_MSG(MSG_LOG, "mgr_sitetask::add_forward_taskprogress() ft sync PKG_TYPE_SERDES_DRV fpga");
        }
        for(auto it : filelist)
        {
            std::string filename = it;
            int boardtype = 0;
            bool flag = get_boardtype(filename,boardtype);
            if((flag == true) && ((boardtype == BOARDTYPE_FT_PGB) || (boardtype == BOARDTYPE_ASIC) || (boardtype == BOARDTYPE_FT_DIG)))
            {
                boardtype_set.insert(boardtype);
            }
        }
    }
#endif
#endif

    sz = boardtype_set.size();
    for(auto it : boardtype_set)
    {
        add_taskprogress(it);
    }
    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::add_forward_taskprogress() sz:%d",sz);
    return sz ;
}

bool mgr_sitetask::check_board_exist()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::check_board_exist()");
    bool ret = true;

    // 1、保存需要转发的boardid任务集合
    std::set<int> boardtype_set;
    xpkg_interface* pkg_instance = xpkg_interface::get_instance();
    int sz = 0;

#if defined(SYNC_BUILD)
#if defined(DEV_TYPE_CP)
    std::shared_ptr<xpkg_interface::FileList> arm_pcie_filesit = pkg_instance->find(PKG_TYPE_PCIE_DRV);
    if(arm_pcie_filesit != NULL)
    {
        xpkg_interface::FileList filelist = *arm_pcie_filesit;
        for(auto it : filelist)
        {
            std::string filename = it;
            int boardtype = 0;
            bool flag = get_boardtype(filename,boardtype);
            if(flag == true && (boardtype == BOARDTYPE_CP_RCA))
            {
                boardtype_set.insert(boardtype);
            }
        }
    }
#elif defined(DEV_TYPE_FT)
    std::shared_ptr<xpkg_interface::FileList> mcu_fpga_filelist = pkg_instance->find(PKG_TYPE_USB);
    std::shared_ptr<xpkg_interface::FileList> serdes_filelist = pkg_instance->find(PKG_TYPE_SERDES_DRV);
    if((mcu_fpga_filelist != NULL) || (serdes_filelist != NULL))
    {
        // 检查是否存在需要转PKG_TYPE_USB的mcu、fpga对应boardid
        xpkg_interface::FileList filelist;
        if(mcu_fpga_filelist)
        {
            filelist = *mcu_fpga_filelist;
            LOG_MSG(MSG_LOG, "mgr_sitetask::check_board_exist() ft sync PKG_TYPE_USB mcu_fpga");
        }
        else
        {
            filelist = *serdes_filelist;
            LOG_MSG(MSG_LOG, "mgr_sitetask::check_board_exist() ft sync PKG_TYPE_SERDES_DRV fpga");
        }
        for(auto it : filelist)
        {
            std::string filename = it;
            int boardtype = 0;
            bool flag = get_boardtype(filename,boardtype);
            if((flag == true) && ((boardtype == BOARDTYPE_FT_PGB) || (boardtype == BOARDTYPE_ASIC) || (boardtype == BOARDTYPE_FT_DIG)))
            {
                boardtype_set.insert(boardtype);
            }
        }
    }
#endif
#endif

    sz = boardtype_set.size();
    LOG_MSG(MSG_LOG, "mgr_sitetask::check_board_exist() board type size:%d", sz);
    if(sz > 0)
    {
        for(auto it : boardtype_set)
        {
            int num = find_board_num(it);
            LOG_MSG(MSG_LOG, "mgr_sitetask::check_board_exist() find board_type:0x%x board num:%d", it, num);
            if(num == 0)
            {
                // 说明当前没有该类型单板, 那就不应该添加任务进行升级
                ret = false;
                LOG_MSG(WRN_LOG, "mgr_sitetask::check_board_exist() not find board_type:0x%x board", it);
                break;
            }
        }
    }
    else
    {
        ret = false;
        LOG_MSG(WRN_LOG, "mgr_sitetask::check_board_exist() not find any board type!");
    }
#endif
    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::check_board_exist() ret:%d", ret);
    return ret;
}

int mgr_sitetask::find_board_num(const int &boardtype)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::find_board_num() boardtype=0x%x",boardtype);
    int ret = 0;

    boost::unordered_map<int,boost::shared_ptr<xboard>> all_map;
    xboardmgr* mgrboard = xboardmgr::get_instance();
    all_map = mgrboard->get_all_board();
    for(auto it : all_map)
    {
        if((it.first & BOARDTYPE_MASK) == boardtype)
        {
            ret++;
        }
    }
    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::find_board_num() ret=%d",ret);
    return ret;
}

//绑定task应答消息
int mgr_sitetask::bind_taskmsg(const std::string port_name,boost::shared_ptr<xsite_package> siteresp)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::bind_taskmsg() port_name:%s",port_name.c_str());
    int ret = 0;
    sitetask_msg msg;
    msg.m_port_name = port_name;
    msg.m_packdata = siteresp;
    xtaskprogress* xtask_progress = xtaskprogress::get_instance();
    int task_sz = xtask_progress->get_task_size();
    if(task_sz > 0)
    {
        xtask_progress->bind_sitectrlresp(msg);
    }
    else
    {
        ret = -1;
        LOG_MSG(WRN_LOG, "mgr_sitetask::bind_taskmsg() task_sz=%d,failed",task_sz);
    }
    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::bind_taskmsg() ret=%d",ret);
    return ret;
}

//设置tasksize状态
int mgr_sitetask::set_tasksize_status(int status)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_sitetask::set_tasksize_status() status=%d",status);
    int ret = 0;
    xtaskprogress* xtask_progress = xtaskprogress::get_instance();
    taskprogress->set_tasksize_lockstatus(status);
    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::set_tasksize_status() ret=%d",ret);
    return ret;
}

//向sync板上报升级成功通知
int mgr_sitetask::notify_taskprogresss(int boardid, int status, int result)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_sitetask::notify_taskprogresss() boardid=0x%x status=%d result=%d",boardid,status,result);
    int ret = 0;
#if defined(PGB_BUILD) || defined(CP_RK3588_BUILD)
    std::string site_id = std::to_string(xconfig::self_id());
    boost::shared_ptr<xsite_package> pack(new xsite_package(SITE_MT_NOTIFY,"",site_id));
    boost::shared_ptr<xcmd> cmd(new xcmd(SITE_MC_INTERNAL_CMD,SITE_CONT_UPGRADE));
    cjson_object parm_upgrade_json("[]");
    cjson_object brdver_obj("{}");
    int boardtype = boardid & BOARDTYPE_MASK;
    int slot_num = (boardid & SLOT_MASK) >> 8;
    brdver_obj.add(SITE_SLOT_TYPE_ITEM,get_slottype_str(boardtype));
    brdver_obj.add(SITE_SLOT_NUM_ITEM,std::to_string(slot_num));
    brdver_obj.add(SITE_UPGRADE_STATUS, status);
    brdver_obj.add(SITE_UPGRADE_RESULT, result);
    parm_upgrade_json.add(brdver_obj);

    std::string upgrade_progress = parm_upgrade_json.to_string();
    cmd->add_param("upgrade_progress",upgrade_progress);
    pack->add_cmd(cmd);

    ret = m_sitectrl_sync_mgr->send_to_sitectrl_syncsvr(pack.get());
#endif
    LOG_MSG(MSG_LOG, "Exited mgr_sitetask::notify_taskprogresss() ret=%d",ret);
    return ret;
}

int mgr_sitetask::get_slottype(const std::string str)
{
    int type = 0;
    if(str == std::string("CP_SYNC"))
    {
        type = BOARDTYPE_CP_SYNC;
    }
    else if(str == std::string("CP_PGB"))
    {
        type = BOARDTYPE_CP_PGB;
    }
    else if(str == std::string("CP_DPS"))
    {
        type = BOARDTYPE_CP_DPS;
    }
    else if(str == std::string("CP_PEM"))
    {
        type = BOARDTYPE_CP_PEM;
    }
    else if(str == std::string("CP_RCA"))
    {
        type = BOARDTYPE_CP_RCA;
    }
    else if(str == std::string("CP_DIG"))
    {
        type = BOARDTYPE_CP_DIG;
    }
    else if(str == std::string("FT_ASIC"))
    {
        type = BOARDTYPE_ASIC;
    }
    else if(str == std::string("FT_SYNC"))
    {
        type = BOARDTYPE_FT_SYNC;
    }
    else if(str == std::string("FT_PGB"))
    {
        type = BOARDTYPE_FT_PGB;
    }
    else if(str == std::string("FT_PPS"))
    {
        type = BOARDTYPE_FT_PPS;
    }
    else if(str == std::string("FT_DIG"))
    {
        type = BOARDTYPE_FT_DIG;
    }
    else if(str == std::string("TH_MONITOR"))
    {
        type = BOARDTYPE_TH_MONITOR;
    }
    else if(str == std::string("MF_MONITOR"))
    {
        type = BOARDTYPE_MF_MONITOR;
    }
    return type;
}

std::string mgr_sitetask::get_slottype_str(int boardtype)
{
    std::string str;
    switch(boardtype)
    {
    case BOARDTYPE_CP_SYNC:
        str = "CP_SYNC";
        break;
    case BOARDTYPE_CP_PGB:
        str = "CP_PGB";
        break;
    case BOARDTYPE_CP_DPS:
        str = "CP_DPS";
        break;
    case BOARDTYPE_CP_PEM:
        str = "CP_PEM";
        break;
    case BOARDTYPE_CP_RCA:
        str = "CP_RCA";
        break;
    case BOARDTYPE_CP_DIG:
        str = "CP_DIG";
        break;
    case BOARDTYPE_ASIC:
        str = "FT_ASIC";
        break;
    case BOARDTYPE_FT_SYNC:
        str = "FT_SYNC";
        break;
    case BOARDTYPE_FT_PGB:
        str = "FT_PGB";
        break;
    case BOARDTYPE_FT_PPS:
        str = "FT_PPS";
        break;
    case BOARDTYPE_FT_DIG:
        str = "FT_DIG";
        break;
    case BOARDTYPE_TH_MONITOR:
        str = "TH_MONITOR";
        break;
    case BOARDTYPE_MF_MONITOR:
        str = "MF_MONITOR";
        break;
    }
    return str;
}
