#include "xsitectrl_session.h"
#include "mgr_log.h"
#include "mgr_usb_def.h"
#include "mgr_sitetask.h"

ver_info::ver_info()
{
    type = VER_TYPE_NONE;
}

ver_info::ver_info(const std::string& name, const std::string& ver)
{
    this->type = VER_TYPE_NONE;
    this->m_name = name;
    this->m_ver = ver;
}

ver_info::ver_info(const ver_info& other)
{
    if(this != &other)
    {
        this->type  = other.type;
        this->m_name = other.m_name;
        this->m_ver = other.m_ver;
    }
}

ver_info& ver_info::operator=(const ver_info& other)
{
    if(this != &other)
    {
        this->type  = other.type;
        this->m_name = other.m_name;
        this->m_ver = other.m_ver;
    }
    return *this;
}

ver_info::~ver_info()
{
}

sys_version::sys_version()
{
}

sys_version::sys_version(const sys_version& other)
{
    if(this != &other)
    {
        this->m_sys_ver = other.m_sys_ver;
    }
}

sys_version& sys_version::operator=(const sys_version& other)
{
    if(this != &other)
    {
        this->m_sys_ver = other.m_sys_ver;
    }
    return *this;
}

sys_version::~sys_version()
{
}

std::shared_ptr<ver_info> sys_version::add_version(std::string name, std::string ver)
{
    LOG_MSG(MSG_LOG,"Enter into sys_version::add_version() name=%s",name.c_str());

    std::shared_ptr<ver_info> sys_ver = find_version(name);
    if(sys_ver)
    {
        //已经存在，更新sys_version版本
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_sysver);
        *sys_ver = ver_info(name,ver);
        return sys_ver;
    }

    //不存在，则新建
    LOG_MSG(WRN_LOG,"sys_version::add_version() add name=%s",name.c_str());
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_sysver);
    //新建ver_info对象
    std::shared_ptr<ver_info> new_syscver(new ver_info(name,ver));
    //加入到集合中
    m_sys_ver.insert(std::make_pair(name,new_syscver));

    LOG_MSG(MSG_LOG,"Exited sys_version::add_version");

    return new_syscver;

}

std::shared_ptr<ver_info> sys_version::find_version(const std::string& name)
{
    LOG_MSG(MSG_LOG,"Enter into sys_version::find_version() name=%s",name.c_str());

    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_sysver);
    SysVerMap::iterator iter = m_sys_ver.find(name);
    if(iter != m_sys_ver.end())
    {
        return iter->second;
    }

    LOG_MSG(MSG_LOG,"Exited sys_version::find_logicver()");

    return std::shared_ptr<ver_info>();
}

// 清除 sysver 版本（预留接口，暂未测试）
bool sys_version::clear_sysver()
{
    LOG_MSG(MSG_LOG, "Enter into sys_version::clear_sysver()");
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_sysver);
    m_sys_ver.clear();
    LOG_MSG(MSG_LOG, "Exited sys_version::clear_sysver()");
    return true;
}

sys_version::SysVerMap sys_version::get_sysver()
{
    LOG_MSG(MSG_LOG,"Enter into sys_version::get_sysver()");

    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_sysver);

    LOG_MSG(MSG_LOG,"Exited sys_version::get_sysver()");
    return m_sys_ver;
}

board_version::board_version()
{
}

board_version::board_version(const board_version& other)
{
    if(this != &other)
    {
        this->m_boardtype = other.m_boardtype;
        this->m_boardslot = other.m_boardslot;
        this->m_mcu_ver = other.m_mcu_ver;
        this->m_board_ver = other.m_board_ver;
        this->m_logic_ver = other.m_logic_ver;
    }
}

board_version& board_version::operator=(const board_version& other)
{
    if(this != &other)
    {
        this->m_boardtype = other.m_boardtype;
        this->m_boardslot = other.m_boardslot;
        this->m_mcu_ver = other.m_mcu_ver;
        this->m_board_ver = other.m_board_ver;
        this->m_logic_ver = other.m_logic_ver;
    }
    return *this;
}

board_version::~board_version()
{
}

void board_version::update_info(const board_version &other)
{
    LOG_MSG(MSG_LOG, "Enter into board_version::update_info()");

    if(this != &other)
    {
        this->m_boardtype = other.m_boardtype;
        this->m_boardslot = other.m_boardslot;
        this->m_mcu_ver = other.m_mcu_ver;
        this->m_board_ver = other.m_board_ver;
        for(const auto& it : other.m_logic_ver)
        {
            // 之前没有的逻辑版本会新增
            // 之前已经存在的逻辑版本会修改信息
            this->m_logic_ver[it.first] = it.second;
        }
        // 对于之前存在，但是这一次不存在的信息会保留
    }

    LOG_MSG(MSG_LOG, "Exited board_version::update_info()");
}

std::shared_ptr<ver_info> board_version::add_logicver(const std::string& name, const ver_info& logic_ver)
{
    LOG_MSG(MSG_LOG,"Enter into board_version::add_logicver() name=%s",name.c_str());

    std::shared_ptr<ver_info> lg_ver = find_logicver(name);
    if(lg_ver)
    {
        //已经存在，更新logic版本
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_logicver);
        *lg_ver = logic_ver;
        return lg_ver;
    }

    //不存在，则新建
    LOG_MSG(WRN_LOG,"board_version::add_logicver() add name=%s",name.c_str());
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_logicver);
    //新建ver_info对象
    std::shared_ptr<ver_info> new_logicver(new ver_info(logic_ver));
    //加入到集合中
    m_logic_ver.insert(std::make_pair(name,new_logicver));

    LOG_MSG(MSG_LOG,"Exited board_version::add_logicver()");

    return new_logicver;
}

std::shared_ptr<ver_info> board_version::find_logicver(const std::string& name)
{
    LOG_MSG(MSG_LOG,"Enter into board_version::find_logicver() name=%s",name.c_str());

    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_logicver);
    LogicVerMap::iterator iter = m_logic_ver.find(name);
    if(iter != m_logic_ver.end())
    {
        return iter->second;
    }

    LOG_MSG(MSG_LOG,"Exited board_version::find_logicver()");

    return std::shared_ptr<ver_info>();
}

board_version::LogicVerMap board_version::get_logicver()
{
    LOG_MSG(MSG_LOG,"Enter into board_version::get_logicver()");

    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_logicver);

    LOG_MSG(MSG_LOG,"Exited board_version::find_logicver()");
    return m_logic_ver;
}

site_version::site_version()
{
}

site_version::site_version(const site_version& other)
{
    if(this != &other)
    {
        this->m_board_version = other.m_board_version;
        this->m_sys_version = other.m_sys_version;
    }
}

site_version& site_version::operator=(const site_version& other)
{
    if(this != &other)
    {
        this->m_board_version = other.m_board_version;
        this->m_sys_version = other.m_sys_version;
    }
    return *this;
}

site_version::~site_version()
{
}

std::shared_ptr<board_version> site_version::add_boardver(int boardid, const board_version& board_ver)
{
    LOG_MSG(MSG_LOG,"Enter into site_version::add_boardver() boardid=0x%x",boardid);

    std::shared_ptr<board_version> brd_ver = find_boardver(boardid);
    if(brd_ver)
    {
        //已经存在，更新board版本
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_boardver);
        // 这里不能直接对对象进行赋值，因为如果是 FT SYNC 板上的逻辑走到这里，刷新的 SYNC 板和 PPS 的信息里面是没有逻辑版本的
        // 直接对象赋值，可能造成逻辑版本信息置空
        // *brd_ver = board_ver;
        brd_ver->update_info(board_ver);
        return brd_ver;
    }

    //不存在，则新建
    LOG_MSG(WRN_LOG,"site_version::add_boardver() add boardid:%02x",boardid);
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_boardver);
    //新建board_version对象
    std::shared_ptr<board_version> new_brdver(new board_version(board_ver));
    //加入到集合中
    m_board_version.insert(std::make_pair(boardid,new_brdver));

    LOG_MSG(MSG_LOG,"Exited site_version::add_boardver()");

    return new_brdver;
}

// 删除这个区域上某一块单板的版本信息（预留接口，暂未测试）
bool site_version::clear_oneboardver(int boardid)
{
    LOG_MSG(MSG_LOG, "Enter into site_version::clear_oneboardver(boardid:0x%x)", boardid);
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_boardver);
    int num = m_board_version.erase(boardid);
    if(1 == num)
    {
        LOG_MSG(MSG_LOG, "Exited site_version::clear_oneboardver(boardid:0x%x) clear success", boardid);
        return true;
    }
    LOG_MSG(MSG_LOG, "Exited site_version::clear_oneboardver(boardid:0x%x) clear failed num:%d", boardid, num);
    return false;
}

// 删除这个区域上所有单板的版本信息（预留接口，暂未测试）
bool site_version::clear_allboardver()
{
    LOG_MSG(MSG_LOG, "Enter into site_version::clear_allboardver()");
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_boardver);
    m_board_version.clear();
    LOG_MSG(MSG_LOG, "Exited site_version::clear_allboardver()");
    return true;
}

std::shared_ptr<board_version> site_version::find_boardver(int boardid)
{
    LOG_MSG(MSG_LOG,"Enter into site_version::find_boardver() boardid=0x%x",boardid);

    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_boardver);
    BoardVerMap::iterator iter = m_board_version.find(boardid);
    if(iter != m_board_version.end())
    {
        return iter->second;
    }

    LOG_MSG(MSG_LOG,"Exited site_version::find_boardver()");

    return std::shared_ptr<board_version>();
}

site_version::BoardVerMap site_version::get_boardver()
{
    LOG_MSG(MSG_LOG,"Enter into site_version::get_boardver()");

    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_boardver);

    LOG_MSG(MSG_LOG,"Exited site_version::get_boardver()");
    return m_board_version;
}

xsitever_mgr::xsitever_mgr()
{
}

xsitever_mgr::~xsitever_mgr()
{
}

xsitever_mgr* xsitever_mgr::get_instance()
{
    static xsitever_mgr instance;
    return &instance;
}

std::shared_ptr<site_version> xsitever_mgr::add_sysver(int x86_id, const sys_version& sys_ver)
{
    std::shared_ptr<site_version> site_ver = find_sitever(x86_id);
    if(site_ver)
    {
        //已经存在，更新系统版本
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_sitever);
        site_ver->m_sys_version = sys_ver;
        return site_ver;
    }

    //不存在，则新建
    LOG_MSG(WRN_LOG,"xsitever_mgr::add_sysver() add x86_id:%02x",x86_id);
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_sitever);
    //新建site_version对象
    std::shared_ptr<site_version> new_sitever(new site_version());
    new_sitever->m_sys_version = sys_ver;
    //加入到集合中
    m_sitever_map.insert(std::make_pair(x86_id,new_sitever));

    return new_sitever;
}

std::shared_ptr<site_version> xsitever_mgr::add_boardver(int x86_id, int boardid, const board_version& board_ver)
{
    std::shared_ptr<site_version> site_ver = find_sitever(x86_id);
    if(site_ver)
    {
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_sitever);
        site_ver->add_boardver(boardid,board_ver);
        return site_ver;
    }

    //不存在，则新建
    LOG_MSG(WRN_LOG,"xsitever_mgr::add_sysver() add x86_id:%02x",x86_id);
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_sitever);
    //新建x86 site_version对象
    std::shared_ptr<site_version> new_sitever(new site_version());
    new_sitever->add_boardver(boardid,board_ver);
    //加入到集合中
    m_sitever_map.insert(std::make_pair(x86_id,new_sitever));

    return new_sitever;
}

// 清除一个区域上的 sys ver（预留接口，暂未测试）
bool xsitever_mgr::clear_sysver(int x86_id)
{
    LOG_MSG(MSG_LOG, "Enter into xsitever_mgr::clear_sysver(x86_id:0x%x)", x86_id);
    std::shared_ptr<site_version> site_ver = find_sitever(x86_id);
    if(site_ver)
    {
        //已经存在，清空系统版本
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_sitever);
        site_ver->m_sys_version.clear_sysver();
        LOG_MSG(MSG_LOG, "Exited xsitever_mgr::clear_sysver(x86_id:0x%x) clear sysver success", x86_id);
        return true;
    }
    LOG_MSG(WRN_LOG, "Exited xsitever_mgr::clear_sysver(x86_id:0x%x) clear sysver failed the x86 not exist", x86_id);
    return false;
}

// 清除一个区域上某一块单板的 board ver（预留接口，暂未测试）
bool xsitever_mgr::clear_oneboardver(int x86_id, int boardid)
{
    LOG_MSG(MSG_LOG, "Enter into xsitever_mgr::clear_oneboardver(x86_id:0x%x, boardid:0x%x)", x86_id, boardid);
    bool ret = false;
    std::shared_ptr<site_version> site_ver = find_sitever(x86_id);
    if(site_ver)
    {
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_sitever);
        ret = site_ver->clear_oneboardver(boardid);
    }
    LOG_MSG(MSG_LOG, "Exited xsitever_mgr::clear_oneboardver(x86_id:0x%x, boardid:0x%x) ret:%d", x86_id, boardid, ret);
    return ret;
}

// 清除一个区域上保存的所有 board ver（预留接口，暂未测试）
bool xsitever_mgr::clear_allboardver(int x86_id)
{
    LOG_MSG(MSG_LOG, "Enter into xsitever_mgr::clear_allboardver(x86_id:0x%x)", x86_id);
    std::shared_ptr<site_version> site_ver = find_sitever(x86_id);
    if(site_ver)
    {
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_sitever);
        site_ver->clear_allboardver();
        LOG_MSG(MSG_LOG, "Exited xsitever_mgr::clear_allboardver(x86_id:0x%x) clear board success", x86_id);
        return true;
    }
    LOG_MSG(MSG_LOG, "Exited xsitever_mgr::clear_allboardver(x86_id:0x%x) clear board failed x86 not exist", x86_id);
    return false;
}

// 清除一个区域
bool xsitever_mgr::clear_site(int x86_id)
{
    LOG_MSG(MSG_LOG, "Enter into xsitever_mgr::clear_site(x86_id:0x%x)", x86_id);
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_sitever);
    int num = m_sitever_map.erase(x86_id);
    if(1 == num)
    {
        LOG_MSG(MSG_LOG, "Exited xsitever_mgr::clear_site(x86_id:0x%x) clear site success", x86_id);
        return true;
    }
    LOG_MSG(MSG_LOG, "Exited xsitever_mgr::clear_site(x86_id:0x%x) clear site failed x86 not exist", x86_id);
    return false;
}

std::shared_ptr<site_version> xsitever_mgr::find_sitever(int x86_id)
{
    LOG_MSG(MSG_LOG,"Enter into xsitever_mgr::find_sitever() x86_id=0x%x",x86_id);

    boost::shared_lock<boost::shared_mutex> lock(m_mux_sitever); //读锁
    SiteVerMap::iterator iter = m_sitever_map.find(x86_id);
    if(iter != m_sitever_map.end())
    {
        return iter->second;
    }

    LOG_MSG(MSG_LOG,"Exited xsitever_mgr::find_sitever()");

    return std::shared_ptr<site_version>();
}

xsitever_mgr::SiteVerMap xsitever_mgr::get_allsitever()
{
    boost::shared_lock<boost::shared_mutex> lock(m_mux_sitever); //读锁
    return m_sitever_map;
}

xpkg_interface::xpkg_interface()
{
}

xpkg_interface::~xpkg_interface()
{
}

xpkg_interface* xpkg_interface::get_instance()
{
    static xpkg_interface instance;
    return &instance;
}

std::shared_ptr<xpkg_interface::FileList> xpkg_interface::add(const int type, const std::string filename)
{
    LOG_MSG(MSG_LOG,"Enter into xpkg_interface::add_pkginterface() type=%d filename=%s",type,filename.c_str());
    int ret = 0;

    std::shared_ptr<xpkg_interface::FileList> pkg_filelist = find(type);
    if(pkg_filelist)
    {
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_pkgmap);
        pkg_filelist->push_back(filename);
        return pkg_filelist;
    }

    //不存在，则新建
    LOG_MSG(WRN_LOG,"xpkg_interface::add_pkginterface() type=%d filename=%s",type,filename.c_str());
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_pkgmap);
    //新建x86 site_version对象
    std::shared_ptr<xpkg_interface::FileList> new_filelist(new xpkg_interface::FileList());
    new_filelist->push_back(filename);
    //加入到集合中
    m_pkginterface_map.insert(std::make_pair(type,new_filelist));

    LOG_MSG(MSG_LOG,"Exited xpkg_interface::add_pkginterface()");
    return new_filelist;;
}

xpkg_interface::PkgInterFaceMap xpkg_interface::get_pkgmap()
{
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_pkgmap);
    return m_pkginterface_map;
}

int xpkg_interface::clear()
{
    LOG_MSG(MSG_LOG,"Enter into xpkg_interface::clear()");
    int ret = 0;

    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_pkgmap);
    m_pkginterface_map.clear();
    LOG_MSG(MSG_LOG,"Exited xpkg_interface::clear() ret=%d",ret);
    return ret;
}

std::shared_ptr<xpkg_interface::FileList> xpkg_interface::find(const int type)
{
    LOG_MSG(MSG_LOG,"Enter into xpkg_interface::find() type=%d",type);

    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_pkgmap);
    PkgInterFaceMap::iterator iter = m_pkginterface_map.find(type);
    if(iter != m_pkginterface_map.end())
    {
        return iter->second;
    }

    LOG_MSG(MSG_LOG,"Exited xpkg_interface::find()");
    return std::shared_ptr<xpkg_interface::FileList>();
}

xsitectrl_session::xsitectrl_session(const std::string& session_id,const std::string& port_name) : xtransmitter("<xsitectrl_session>")
{
    m_sess_id    = session_id;
    m_port_name  = port_name;
}

xsitectrl_session::~xsitectrl_session()
{
}

int xsitectrl_session::on_recv(const char *port_name,xsite_package *pack) //消息通知
{
    LOG_MSG(MSG_LOG,"Enter into xsitectrl_session::on_recv()");

    int ret = 0;

    if(m_port_name != port_name)
    {
        m_port_name =port_name; //保存端口
    }

    int msg_type = get_msgtype(pack->m_msg_type);
    switch(msg_type)
    {
        case SITECTRL_MSG_REQUEST:
            handle_request(port_name,pack);
            break;
        case SITECTRL_MSG_NOTIFY:
            handle_notify(port_name,pack);
            break;
        case SITECTRL_MSG_RESPOND:
            handle_response(port_name,pack);
            break;
        case SITECTRL_MSG_BROADCAST:
            handle_broadcast(port_name,pack);
            break;
    }

    ret = xtransmitter::on_recv(port_name,pack);
    LOG_MSG(MSG_LOG,"Exited xsitectrl_session::on_recv() type=%d type_str=%s ret=%d",msg_type,pack->m_msg_type.c_str(),ret);

    return ret;
}

int xsitectrl_session::get_msgtype(const std::string& msg_type)
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

int xsitectrl_session::handle_request(const char *port_name, xsite_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::handle_request()");
    int ret = -1;

    // 返回结果
    xsite_package *xsite_resp = dynamic_cast<xsite_package*>(pack->clone());
    xsite_resp->m_msg_type = SITE_MT_RESPOND;
    int cmd_size = pack->get_cmd_size();

    bool is_reply = true;

    for(int i = cmd_size-1; i >= 0; i--)
    {
        int cmd_ret = 0;

        // 取出命令
        boost::shared_ptr<xcmd> cmd = xsite_resp->find_cmd(i);
        if(cmd->m_cmd_type == SITE_MC_DEVICE_DATA)
        {
            if(cmd->m_cmd_cont == SITE_CONT_SETUP_SITE)
            {
                cmd_ret = process_setup_site(xsite_resp, cmd);
            }
            else if(cmd->m_cmd_cont == STIT_CONT_READATA)
            {
                cmd_ret = process_device_data(xsite_resp, cmd);
            }
        }
        else if(cmd->m_cmd_type == SITE_MC_INTERNAL_CMD)
        {
            if(cmd->m_cmd_cont == SITE_CONT_UPGRADE)
            {
                process_upgrade(pack, cmd);
                is_reply = false;
            }
        }
        else
        {
            // nothing to do ...
            LOG_MSG(MSG_LOG, "xsitectrl_session::handle_request() m_cmd_type=%s m_cmd_cont=%s is not supported", cmd->m_cmd_type.c_str(), cmd->m_cmd_cont.c_str());
        }

        if(cmd_ret != 0)
        {
            // 存储最后一个返回错误码
            xsite_resp->m_err_code = cmd_ret;
        }
    }

    // 设置
    if((is_reply == true) && (xsite_resp->get_cmd_size() > 0))
    {
        ret = send(port_name, xsite_resp);
    }

    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::handle_request() cmd_size=%d ret=%d", cmd_size, ret);
    return ret;
}

int xsitectrl_session::handle_notify(const char *port_name, xsite_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::handle_notify()");
    int ret = 0;
    int cmd_size = pack->get_cmd_size();
    for(int i = cmd_size-1; i >= 0; i--)
    {
        // 取出命令
        boost::shared_ptr<xcmd> cmd = pack->find_cmd(i);
        if(cmd->m_cmd_type == SITE_MC_INTERNAL_CMD)
        {
            if(cmd->m_cmd_cont == SITE_CONT_REBOOTCONFIRM)
            {
                cmd_ret = process_reboot_confirm(pack, cmd);
                LOG_MSG(MSG_LOG, "xsitectrl_session::process_reboot_confirm() cmd_ret=%d", cmd_ret);
            }
        }
        else
        {
            // nothing to do ...
            LOG_MSG(MSG_LOG, "xsitectrl_session::handle_notify() m_cmd_type=%s m_cmd_cont=%s is not supported", cmd->m_cmd_type.c_str(), cmd->m_cmd_cont.c_str());
        }
    }
    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::handle_notify() cmd_size=%d ret=%d", cmd_size, ret);
    return ret;
}

int xsitectrl_session::handle_response(const char *port_name, xsite_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::handle_response()");
    int ret = -1;
    // to do ...

    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::handle_response()");
    return ret;
}

int xsitectrl_session::handle_broadcast(const char *port_name, xsite_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::handle_broadcast()");
    int ret = -1;
    // to do ...

    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::handle_broadcast()");
    return ret;
}

// 处理 device data
int xsitectrl_session::process_device_data(xsite_package *pack, boost::shared_ptr<xcmd> cmd)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::process_device_data()");
    int ret = 0;

    // 1. 清除 cmd 中 param 字段
    int cmd_size = pack->get_cmd_size();
    for(int i = cmd_size-1; i >= 0; i--)
    {
        boost::shared_ptr<xcmd> cmd = pack->find_cmd(i);
        cmd->clear_param();
    }

    cjson_object return_ver_json("{}");
    xsitectrl_mgr* mgr_sitever = xsitectrl_mgr::get_instance();
    siteversion_mgr::SiteVerMap site_map = mgr_sitever->get_allsitever();
    bool sys_ok = false;
    bool brd_ok = false;
    for(auto it : site_map)
    {
        int x86_id = it.first;
        // 2.1 x86 上 Lib 及进程的版本信息
        sys_version sys_x86 = it.second->m_sys_version;
        sys_version::SysVerMap sysver_map = sys_x86.get_sysver();
        cjson_object value_obj1("{}");
        for(auto x86_ver : sysver_map)
        {
            value_obj1.add(x86_ver.second->m_name, x86_ver.second->m_ver);
        }

        if(!value_obj1.is_empty())
        {
            cjson_object sysver_obj("{}");
            sysver_obj.add(SITE_SLOT_TYPE_ITEM, "X86");
            sysver_obj.add(SITE_SLOT_NUM_ITEM, std::to_string(x86_id));
            sysver_obj.add(SITE_VALUE_ITEM, value_obj1);

            return_ver_json.add(sysver_obj);
            sys_ok = true;
        }

        // 2.2 增加 x86 关联的板子上版本信息
        site_version::BoardVerMap brd_map = it.second->get_boardver();
        for(auto it2 : brd_map)
        {
            cjson_object value_obj2("{}");

            // 2.2.1 加入板子 MCU 版本信息
            std::string mcu_name = it2.second->m_mcu_ver.m_name;
            std::string mcu_ver = it2.second->m_mcu_ver.m_ver;
            if(!mcu_name.empty() && !mcu_ver.empty())
            {
                value_obj2.add(mcu_name, mcu_ver);
            }
            LOG_MSG(MSG_LOG, "xsitectrl_session::process_device_data() get board MCU version boardid=0x%x m_mcu_ver.m_name=%s m_mcu_ver.m_ver=%s", it2.first, mcu_name.c_str(), mcu_ver.c_str());

            // 2.2.1 加入板子硬件版本信息
            std::string hw_name = it2.second->m_board_ver.m_name;
            std::string hw_ver = it2.second->m_board_ver.m_ver;
            if(!hw_name.empty() && !hw_ver.empty())
            {
                value_obj2.add(hw_name, hw_ver);
            }
            LOG_MSG(MSG_LOG, "xsitectrl_session::process_device_data() get board hw version boardid=0x%x m_board_ver.m_name=%s m_board_ver.m_ver=%s", it2.first, hw_name.c_str(), hw_ver.c_str());

            // 2.2.2 加入板子的逻辑版本信息
            board_version::LogicVerMap logic_map = it2.second->get_logicver();
            for(auto it3 : logic_map)
            {
                value_obj2.add(it3.second->m_name, it3.second->m_ver);
            }

            if(!value_obj2.is_empty())
            {
                cjson_object brdver_obj("{}");
                brdver_obj.add(SITE_SLOT_TYPE_ITEM, get_slottype_str(it2.second->m_boardtype));
                brdver_obj.add(SITE_SLOT_NUM_ITEM, std::to_string(it2.second->m_boardslot));
                brdver_obj.add(SITE_VALUE_ITEM, value_obj2);
                return_ver_json.add(brdver_obj);
                brd_ok = true;
            }
        }
    }

    if(sys_ok || brd_ok)
    {
        // 加入 software_version 信息
        xreturn_cell return_sw_ver;
        return_sw_ver.add_cell(SITE_NAME_ITEM, "software_version");
        std::string sw_version = return_ver_json.to_string();
        return_sw_ver.add_cell(SITE_VALUE_ITEM, sw_version);
        cmd->add_return(return_sw_ver);

        // 加入 return_info 信息
        xreturn_cell return_info;
        return_info.add_cell(SITE_NAME_ITEM, "return_info");
        return_info.add_cell(SITE_VALUE_ITEM, "success");
        cmd->add_return(return_info);

        // 加入 return_code 信息
        xreturn_cell return_code;
        return_code.add_cell(SITE_NAME_ITEM, "return_code");
        return_code.add_cell(SITE_VALUE_ITEM, "0");
        cmd->add_return(return_code);
    }
    else
    {
        // 加入 software_version 信息
        xreturn_cell return_sw_ver;
        return_sw_ver.add_cell(SITE_NAME_ITEM, "software_version");
        std::string sw_version = return_ver_json.to_string();
        return_sw_ver.add_cell(SITE_VALUE_ITEM, sw_version);
        cmd->add_return(return_sw_ver);

        // 加入 return_info 信息
        xreturn_cell return_info;
        return_info.add_cell(SITE_NAME_ITEM, "return_info");
        return_info.add_cell(SITE_VALUE_ITEM, "failed");
        cmd->add_return(return_info);

        // 加入 return_code 信息
        xreturn_cell return_code;
        return_code.add_cell(SITE_NAME_ITEM, "return_code");
        return_code.add_cell(SITE_VALUE_ITEM, "1");
        cmd->add_return(return_code);

        ret = -1;
        LOG_MSG(MSG_LOG, "xsitectrl_session::process_device_data() failed ret=%d", ret);
    }
    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::process_device_data() ret=%d", ret);
    return ret;
}

int xsitectrl_session::process_setup_site(xsite_package *pack, boost::shared_ptr<xcmd> cmd)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::process_setup_site()");
    int ret = 0;

    // 0. 执行 setup_site 命令
    ret = exec_setup_site(cmd);
    // 1. 清除 cmd 中 param 字段
    cmd->clear_param();

    if(0 == ret)
    {
        // 加入 return_info 信息
        xreturn_cell return_info;
        return_info.add_cell(SITE_NAME_ITEM, "return_info");
        return_info.add_cell(SITE_VALUE_ITEM, "success");
        cmd->add_return(return_info);

        // 加入 return_code 信息
        xreturn_cell return_code;
        return_code.add_cell(SITE_NAME_ITEM, "return_code");
        return_code.add_cell(SITE_VALUE_ITEM, "0");
        cmd->add_return(return_code);
    }
    else
    {
        // 加入 return_info 信息
        xreturn_cell return_info;
        return_info.add_cell(SITE_NAME_ITEM, "return_info");
        return_info.add_cell(SITE_VALUE_ITEM, "failed");
        cmd->add_return(return_info);

        // 加入 return_code 信息
        xreturn_cell return_code;
        return_code.add_cell(SITE_NAME_ITEM, "return_code");
        return_code.add_cell(SITE_VALUE_ITEM, "1");
        cmd->add_return(return_code);
    }
    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::process_setup_site() ret=%d", ret);
    return ret;
}

int xsitectrl_session::exec_setup_site(boost::shared_ptr<xcmd> &cmd)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::exec_setup_site()");
    int ret = 0;
    int param_size = cmd->get_param_size();
    for(int i = 0; i < param_size; ++i)
    {
        boost::shared_ptr<xcell> cell = cmd->get_param(i);
        int option_type = get_setup_site_type(cell->m_name);
        switch(option_type)
        {
            case NETWORKIF:
                ret = exec_networkif(cell->m_value);
                break;
            case SITECNT:
                ret = exec_sitecnt(cell->m_value);
                break;
            case RPCUCNT:
                ret = exec_rpcucnt(cell->m_value);
                break;
            case OPTRPCUCNT:
                ret = exec_optrpcucnt(cell->m_value);
                break;
            case NOCHAMBER:
                ret = exec_nochamber(cell->m_value);
                break;
            default:
                LOG_MSG(ERR_LOG, "error setup_site option:%s, option_type:%d", cell->m_value.c_str(), option_type);
                ret = -1;
        }
        if(ret != 0)
        {
            // 说明有选项执行错误
            break; // 不再执行后续选项
        }
    }
    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::exec_setup_site() ret=%d", ret);
    return ret;
}

int xsitectrl_session::exec_networkif(const std::string &value)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::exec_networkif() value:%s", value.c_str());
    // to do
    int ret = 0;
    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::exec_networkif() ret:%d", ret);
    return ret;
}

int xsitectrl_session::exec_sitecnt(const std::string &value)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::exec_sitecnt() value:%s", value.c_str());
    // to do
    int ret = -1;
    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::exec_sitecnt() ret:%d", ret);
    return ret;
}

int xsitectrl_session::exec_rpcucnt(const std::string &value)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::exec_rpcucnt() value:%s", value.c_str());
    // to do
    int ret = 0;
    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::exec_rpcucnt() ret:%d", ret);
    return ret;
}

int xsitectrl_session::exec_optrpcucnt(const std::string &value)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::exec_optrpcucnt() value:%s", value.c_str());
    // to do
    int ret = 0;
    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::exec_optrpcucnt() ret:%d", ret);
    return ret;
}

int xsitectrl_session::exec_nochamber(const std::string &value)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::exec_nochamber() value:%s", value.c_str());
    // to do
    int ret = 0;
    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::exec_nochamber() ret:%d", ret);
    return ret;
}

int xsitectrl_session::get_setup_site_type(const std::string &option)
{
    if(option == std::string("networkif"))
        return NETWORKIF;
    else if(option == std::string("sitecnt"))
        return SITECNT;
    else if(option == std::string("rpcucnt"))
        return RPCUCNT;
    else if(option == std::string("optrpcucnt"))
        return OPTRPCUCNT;
    else if(option == std::string("nochamber"))
        return NOCHAMBER;
    else
        return -1;
}

// 处理 upgrade 命令消息
int xsitectrl_session::process_upgrade(xsite_package *pack, boost::shared_ptr<xcmd> cmd)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::process_upgrade()");
    int ret = 0;

    // 向 mgr_sitetask 模块任务队列放入任务
    mgr_sitetask* sitetask_mgr = mgr_sitetask::get_instance();
    sitetask_mgr->push_task(m_port_name.c_str(), pack);

    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::process_upgrade() ret=%d", ret);
    return ret;
}

// 处理 reboot 请求命令消息
int xsitectrl_session::process_reboot(xsite_package *pack, boost::shared_ptr<xcmd> cmd)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::process_reboot()");
    int ret = 0;

    std::string reboot_type = cmd->get_param("reboot_type");

    if (reboot_type.compare("1") == 0)
    {
        // 加入 return_info 信息
        cmd->add_return_info("success");
        // 加入 return_code 信息
        cmd->add_return_code("0");

        LOG_MSG(ERR_LOG, "xsitectrl_session::process_reboot() will be reboot");
    }
    else
    {
        LOG_MSG(ERR_LOG, "xsitectrl_session::process_reboot() receive reboot request wrong reboot type");
        // 加入 return_info 信息
        cmd->add_return_info("reboot type not match");
        // 加入 return_code 信息
        cmd->add_return_code("2");
        ret = -1;
    }

    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::process_reboot() ret=%d", ret);
    return ret;
}

// 处理 reboot_confirm 命令消息
int xsitectrl_session::process_reboot_confirm(xsite_package *pack, boost::shared_ptr<xcmd> cmd)
{
    LOG_MSG(MSG_LOG, "Enter into xsitectrl_session::process_reboot_confirm()");
    int ret = 0;

    int cmd_size = pack->get_cmd_size();
    if (pack->get_cmd_size() != 1)
    {
        LOG_MSG(WRN_LOG, "xsitectrl_session::process_reboot_confirm() receive error command number notify packet, command number:%d", cmd_size);
        return -1;
    }

    if (cmd == NULL)
    {
        LOG_MSG(WRN_LOG, "xsitectrl_session::process_reboot_confirm() receive NULL command notify packet");
        return -1;
    }

    if (cmd->m_cmd_type != SITE_MC_INTERNAL_CMD)
    {
        LOG_MSG(WRN_LOG, "xsitectrl_session::process_reboot_confirm() receive error command notify packet, command type:%s", cmd->m_cmd_type.c_str());
        return -1;
    }

    // 收到 reboot 确认报文，立刻 reboot 单板
    if (cmd->m_cmd_cont == SITE_CONT_REBOOTCONFIRM)
    {
        LOG_MSG(WRN_LOG, "xsitectrl_session::process_reboot_confirm() receive reboot confirm packet, and try to reboot system");
#ifndef _WINDOWS
        sync();
#endif
        usleep(500 * 1000);
        std::string reboot_type = cmd->get_param("reboot_type");
        if (reboot_type.compare("1") == 0)
        {
            xbasic::exec_sys_cmd("reboot");
            LOG_MSG(MSG_LOG, "xsitectrl_session::process_reboot_confirm() reboot");
        }
        else
        {
            LOG_MSG(ERR_LOG, "xsitectrl_session::process_reboot_confirm() receive reboot confirm wrong reboot type:%s", reboot_type.c_str());
        }
    }
    else
    {
        LOG_MSG(WRN_LOG, "xsitectrl_session::process_reboot_confirm() receive error command notify packet, command context:%s", cmd->m_cmd_cont.c_str());
    }

    LOG_MSG(MSG_LOG, "Exited xsitectrl_session::process_reboot_confirm() ret=%d", ret);
    return ret;
}

std::string xsitectrl_session::get_slottype_str(int boardtype)
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

site_processors::site_processors()
{
}

site_processors::site_processors(const std::string &number, const std::string &test_unit_number, const std::string &test_site_number,
                                 const std::string &ip_address, const std::string &host_name, const std::string &booted_date_time,
                                 const std::string &os_image_version, const std::string &boot_method)
    : m_number(number), m_test_unit_number(test_unit_number), m_test_site_number(test_site_number),
      m_ip_address(ip_address), m_host_name(host_name), m_booted_date_time(booted_date_time),
      m_os_image_version(os_image_version), m_boot_method(boot_method)
{
}

site_processors::site_processors(const site_processors &sp)
{
    if(this != &sp)
    {
        update_info(sp);
    }
}

site_processors& site_processors::operator=(const site_processors &sp)
{
    // TODO: 在此处插入 return 语句
    if(this != &sp)
    {
        update_info(sp);
    }
    return *this;
}

site_processors::~site_processors()
{
}

void site_processors::update_info(const site_processors &other)
{
    m_number = other.m_number;
    m_test_unit_number = other.m_test_unit_number;
    m_test_site_number = other.m_test_site_number;
    m_ip_address = other.m_ip_address;
    m_host_name = other.m_host_name;
    m_booted_date_time = other.m_booted_date_time;
    m_os_image_version = other.m_os_image_version;
    m_boot_method = other.m_boot_method;
}

site_processors_mgr* site_processors_mgr::m_instance = nullptr;
std::mutex site_processors_mgr::m_mtx;

site_processors_mgr::site_processors_mgr()
{
    // std::cout << std::this_thread::get_id() << "site_processors_mgr::site_processors_mgr()" << std::endl;
}

site_processors_mgr::~site_processors_mgr()
{
    // std::cout << std::this_thread::get_id() << "site_processors_mgr::~site_processors_mgr()" << std::endl;
}

site_processors_mgr* site_processors_mgr::get_instance()
{
    if(m_instance == nullptr) // 第一重检测，如果未初始化
    {
        std::lock_guard<std::mutex> lock(m_mtx); // 上锁，RAII，离开 if() 自动解锁
        if(m_instance == nullptr) // 第二重检测，还未初始化，new
        {
            m_instance = new site_processors_mgr();
        }
    }
    return m_instance;
    // static site_processors_mgr instance; // 这种写法有问题
    // std::cout << "site_processors_mgr instance: " << &instance << std::endl;
    // return &instance;
}

std::shared_ptr<site_processors> site_processors_mgr::add_site_processor(int board_id, const site_processors &site_pro)
{
    std::shared_ptr<site_processors> sp = find_site_processor(board_id);
    if(sp)
    {
        // 已经存在，更新信息
        boost::unique_lock<boost::shared_mutex> lock(m_mux_sitepro);
        sp->update_info(site_pro);
        return sp;
    }

    // 不存在则新建
    LOG_MSG(MSG_LOG, "site_processors_mgr::add_site_processor() add board_id:%02x", board_id);
    // 新建 site_processors 对象
    std::shared_ptr<site_processors> new_site_processors(new site_processors());
    new_site_processors->update_info(site_pro);
    // 加入到集合中
    // 写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_sitepro);
    m_site_processors_map.insert(std::make_pair(board_id, new_site_processors));
    return new_site_processors;
}

site_processors_mgr::site_processors_map_t& site_processors_mgr::get_allsite_processor()
{
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_sitepro);
    return m_site_processors_map;
}

std::shared_ptr<site_processors> site_processors_mgr::find_site_processor(int board_id)
{
    LOG_MSG(MSG_LOG, "Enter into site_processors_mgr::find_site_processor() board_id=0x%02x", board_id);
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_sitepro);
    site_processors_map_t::iterator iter = m_site_processors_map.find(board_id);
    if(iter != m_site_processors_map.end())
    {
        return iter->second;
    }
    LOG_MSG(MSG_LOG, "Exited site_processors_mgr::find_site_processor()");
    return std::shared_ptr<site_processors>();
}

rcpus::rcpus()
{
}

rcpus::rcpus(const std::string &rcpu_number, const std::string &test_unit_number, const std::string &test_site_number, const std::string &repair_site_number,
             const std::string &ip_address, const std::string &host_name, const std::string &booted_date_time, const std::string &os_image_version,
             const std::string &boot_method)
    : m_rcpu_number(rcpu_number), m_test_unit_number(test_unit_number), m_test_site_number(test_site_number), m_repair_site_number(repair_site_number),
      m_ip_address(ip_address), m_host_name(host_name), m_booted_date_time(booted_date_time), m_os_image_version(os_image_version), m_boot_method(boot_method)
{
}

rcpus::rcpus(const rcpus &other)
{
    if (this != &other)
    {
        update_info(other);
    }
}

rcpus &rcpus::operator=(const rcpus &other)
{
    if (this != &other)
    {
        update_info(other);
    }
    return *this;
}

rcpus::~rcpus()
{
}

void rcpus::update_info(const rcpus &other)
{
    m_rcpu_number = other.m_rcpu_number;
    m_test_unit_number = other.m_test_unit_number;
    m_test_site_number = other.m_test_site_number;
    m_repair_site_number = other.m_repair_site_number;
    m_ip_address = other.m_ip_address;
    m_host_name = other.m_host_name;
    m_booted_date_time = other.m_booted_date_time;
    m_os_image_version = other.m_os_image_version;
    m_boot_method = other.m_boot_method;
}

rcpus_mgr *rcpus_mgr::m_instance = nullptr;
std::mutex rcpus_mgr::m_mtx;

rcpus_mgr::rcpus_mgr()
{
    // std::cout << std::this_thread::get_id() << "rcpus_mgr::rcpus_mgr()" << std::endl;
}

rcpus_mgr::~rcpus_mgr()
{
    // std::cout << std::this_thread::get_id() << "rcpus_mgr::~rcpus_mgr()" << std::endl;
}

rcpus_mgr *rcpus_mgr::get_instance()
{
    if (m_instance == nullptr)
    { // 第一重检测，如果未初始化
        std::lock_guard<std::mutex> lck(m_mtx); // 上锁，RAII，离开if()自动解锁
        if (m_instance == nullptr)
        { // 第二重检测，还未初始化，new
            m_instance = new rcpus_mgr();
        }
    }
    return m_instance;
    // static rcpus_mgr *m_instance; // 这种写法有问题
    // std::cout << "rcpus_mgr instance: " << &instance << std::endl;
    // return &instance;
}

std::shared_ptr<rcpus> rcpus_mgr::add_rcpu(int board_id, const rcpus &other)
{
    std::shared_ptr<rcpus> rcpu = find_rcpu(board_id);
    if (rcpu)
    {
        // 已经存在，更新信息
        boost::unique_lock<boost::shared_mutex> lock(m_mux_rcpus);
        rcpu->update_info(other);
        return rcpu;
    }

    // 不存在则新建
    LOG_MSG(MSG_LOG, "rcpus_mgr::add_rcpu() add board_id:0x%02x", board_id);
    // 新建 rcpus 对象
    std::shared_ptr<rcpus> new_rcpu(new rcpus());
    new_rcpu->update_info(other);
    // 加入到集合中
    // 写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_rcpus);
    m_rcpus_map.insert(std::make_pair(board_id, new_rcpu));
    return new_rcpu;
}

std::shared_ptr<rcpus> rcpus_mgr::find_rcpu(int board_id)
{
    LOG_MSG(MSG_LOG, "Enter into rcpus_mgr::find_rcpu() board_id=0x%02x", board_id);
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_rcpus);
    rcpus_map_t::iterator iter = m_rcpus_map.find(board_id);
    if (iter != m_rcpus_map.end())
    {
        return iter->second;
    }
    LOG_MSG(MSG_LOG, "Exited rcpus_mgr::find_rcpu()");
    return std::shared_ptr<rcpus>();
}

rcpus_mgr::rcpus_map_t &rcpus_mgr::get_all_rcpus()
{
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_rcpus);
    return m_rcpus_map;
}

satellite_processors::satellite_processors()
{
}

satellite_processors::satellite_processors(const std::string &number, const std::string &ip_address, const std::string &host_name,
                                           const std::string &booted_date_time, const std::string &os_image_version, const std::string &boot_method)
    : m_number(number), m_ip_address(ip_address), m_host_name(host_name), m_booted_date_time(booted_date_time),
      m_os_image_version(os_image_version), m_boot_method(boot_method)
{
}

satellite_processors::satellite_processors(const satellite_processors &other)
{
    if (this != &other)
    {
        update_info(other);
    }
}

satellite_processors &satellite_processors::operator=(const satellite_processors &other)
{
    if (this != &other)
    {
        update_info(other);
    }
    return *this;
}

satellite_processors::~satellite_processors()
{
}

void satellite_processors::update_info(const satellite_processors &other)
{
    m_number = other.m_number;
    m_ip_address = other.m_ip_address;
    m_host_name = other.m_host_name;
    m_booted_date_time = other.m_booted_date_time;
    m_os_image_version = other.m_os_image_version;
    m_boot_method = other.m_boot_method;
}

satellite_processors_mgr *satellite_processors_mgr::m_instance = nullptr;
std::mutex satellite_processors_mgr::m_mtx;

satellite_processors_mgr::satellite_processors_mgr()
{
}

satellite_processors_mgr::~satellite_processors_mgr()
{
}

satellite_processors_mgr *satellite_processors_mgr::get_instance()
{
    if (m_instance == nullptr)
    { // 第一重检测，如果未初始化
        std::lock_guard<std::mutex> lck(m_mtx); // 上锁，RAII，离开if()自动解锁
        if (m_instance == nullptr)
        { // 第二重检测，还未初始化，new
            m_instance = new satellite_processors_mgr();
        }
    }
    return m_instance;
    // static Satellite_processors_mgr sat_instance; // 这种写法有问题
    // std::cout << "satellite_processors_mgr instance: " << &sat_instance << std::endl;
    // &sat_instance;
}

std::shared_ptr<satellite_processors> satellite_processors_mgr::add_satellite_processors(int board_id, const satellite_processors &other)
{
    std::shared_ptr<satellite_processors> sp = find_satellite_processors(board_id);
    if (sp)
    {
        // 存在，更新值
        // 加写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_satepro);
        sp->update_info(other);
        return sp;
    }

    // 不存在，创建新的
    LOG_MSG(MSG_LOG, "satellite_processors_mgr::add_satellite_processors() add board_id:0x%02x", board_id);
    std::shared_ptr<satellite_processors> new_satepro(new satellite_processors());
    new_satepro->update_info(other);
    // 插入
    boost::unique_lock<boost::shared_mutex> lock(m_mux_satepro);
    m_satellite_processors_map.insert(std::make_pair(board_id, new_satepro));
    return new_satepro;
}

std::shared_ptr<satellite_processors> satellite_processors_mgr::find_satellite_processors(int board_id)
{
    LOG_MSG(MSG_LOG, "Enter into satellite_processors_mgr::find_satellite_processors() board_id=0x%02x", board_id);
    boost::shared_lock<boost::shared_mutex> lock(m_mux_satepro); // 读锁
    satellite_processors_map_t::iterator iter = m_satellite_processors_map.find(board_id);
    if (iter != m_satellite_processors_map.end())
    {
        // 找到了返回
        return iter->second;
    }
    LOG_MSG(MSG_LOG, "Exited satellite_processors_mgr::find_satellite_processors()");
    return std::shared_ptr<satellite_processors>();
}

satellite_processors_mgr::satellite_processors_map_t &satellite_processors_mgr::get_all_satepro()
{
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_satepro);
    return m_satellite_processors_map;
}

option_rcpus::option_rcpus()
{
}

option_rcpus::option_rcpus(const std::string &number, const std::string &ip_address, const std::string &host_name,
                           const std::string &booted_date_time, const std::string &opt_rcpu_base_pkg_revision)
    : m_number(number), m_ip_address(ip_address), m_host_name(host_name), m_booted_date_time(booted_date_time),
      m_opt_rcpu_base_pkg_revision(opt_rcpu_base_pkg_revision)
{
}

option_rcpus::option_rcpus(const option_rcpus &other)
{
    if (this != &other)
    {
        update_info(other);
    }
}

option_rcpus &option_rcpus::operator=(const option_rcpus &other)
{
    if (this != &other)
    {
        update_info(other);
    }
    return *this;
}

option_rcpus::~option_rcpus()
{
}

void option_rcpus::update_info(const option_rcpus &other)
{
    m_number = other.m_number;
    m_ip_address = other.m_ip_address;
    m_host_name = other.m_host_name;
    m_booted_date_time = other.m_booted_date_time;
    m_opt_rcpu_base_pkg_revision = other.m_opt_rcpu_base_pkg_revision;
}

option_rcpus_mgr *option_rcpus_mgr::m_instance = nullptr;
std::mutex option_rcpus_mgr::m_mtx;

option_rcpus_mgr::option_rcpus_mgr()
{
}

option_rcpus_mgr::~option_rcpus_mgr()
{
}

option_rcpus_mgr *option_rcpus_mgr::get_instance()
{
    if (m_instance == nullptr)
    { // 第一重检测，如果未初始化
        std::lock_guard<std::mutex> lck(m_mtx); // 上锁，RAII，离开if()自动解锁
        if (m_instance == nullptr)
        { // 第二重检测，还未初始化，new
            m_instance = new option_rcpus_mgr();
        }
    }
    return m_instance;
    // static option_rcpus_mgr instance;
    // &instance;
}

std::shared_ptr<option_rcpus> option_rcpus_mgr::add_option_rcpus(int board_id, const option_rcpus &other)
{
    // 先查找，看是否存在
    std::shared_ptr<option_rcpus> opt_rcpu = find_option_rcpus(board_id);
    if (opt_rcpu)
    {
        // 说明存在，更新内容
        // 加写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_optioncpu);
        opt_rcpu->update_info(other);
        return opt_rcpu;
    }

    // 不存在则创建新的
    LOG_MSG(MSG_LOG, "option_rcpus_mgr::add_option_rcpus() add board_id:0x%02x", board_id);
    std::shared_ptr<option_rcpus> new_option_rcpus(new option_rcpus());
    new_option_rcpus->update_info(other);
    // 加入到集合中
    // 加写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_optioncpu);
    m_option_rcpu_map.insert(std::make_pair(board_id, new_option_rcpus));
    return new_option_rcpus;
}

std::shared_ptr<option_rcpus> option_rcpus_mgr::find_option_rcpus(int board_id)
{
    LOG_MSG(MSG_LOG, "Enter into option_rcpus_mgr::find_option_rcpus() board_id=0x%02x", board_id);
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_optioncpu);
    option_rcpus_map_t::iterator iter = m_option_rcpu_map.find(board_id);
    if (iter != m_option_rcpu_map.end())
    {
        // 找到了
        return iter->second;
    }
    // 没找到
    LOG_MSG(MSG_LOG, "Exited option_rcpus_mgr::find_option_rcpus()");
    return std::shared_ptr<option_rcpus>();
}

option_rcpus_mgr::option_rcpus_map_t &option_rcpus_mgr::get_all_option_rcpus()
{
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_optioncpu);
    return m_option_rcpu_map;
}

chamber_cpus::chamber_cpus()
{
}

chamber_cpus::chamber_cpus(const std::string &chamber_cpu_number, const std::string &chamber_number, const std::string &ip_address,
                           const std::string &host_name, const std::string &booted_date_time, const std::string &software_revision_booted)
    : m_chamber_cpu_number(chamber_cpu_number), m_chamber_number(chamber_number), m_ip_address(ip_address),
      m_host_name(host_name), m_booted_date_time(booted_date_time), m_software_revision_booted(software_revision_booted)
{
}

chamber_cpus::chamber_cpus(const chamber_cpus &other)
{
    if (this != &other)
    {
        update_info(other);
    }
}

chamber_cpus &chamber_cpus::operator=(const chamber_cpus &other)
{
    if (this != &other)
    {
        update_info(other);
    }
    return *this;
}

chamber_cpus::~chamber_cpus()
{
}

void chamber_cpus::update_info(const chamber_cpus &other)
{
    m_chamber_cpu_number = other.m_chamber_cpu_number;
    m_chamber_number = other.m_chamber_number;
    m_ip_address = other.m_ip_address;
    m_host_name = other.m_host_name;
    m_booted_date_time = other.m_booted_date_time;
    m_software_revision_booted = other.m_software_revision_booted;
}

chamber_cpus_mgr *chamber_cpus_mgr::m_instance = nullptr;
std::mutex chamber_cpus_mgr::m_mtx;

chamber_cpus_mgr::chamber_cpus_mgr()
{
}

chamber_cpus_mgr::~chamber_cpus_mgr()
{
}

chamber_cpus_mgr *chamber_cpus_mgr::get_instance()
{
    if (m_instance == nullptr)
    { // 第一重检测，如果未初始化
        std::lock_guard<std::mutex> lck(m_mtx); // 上锁，RAII，离开if()自动解锁
        if (m_instance == nullptr)
        { // 第二重检测，还未初始化，new
            m_instance = new chamber_cpus_mgr();
        }
    }
    return m_instance;
    // static chamber_cpus_mgr instance;
    // &instance;
}

std::shared_ptr<chamber_cpus> chamber_cpus_mgr::add_chamber_cpus(int board_id, const chamber_cpus &other)
{
    // 先查找，看是否存在
    std::shared_ptr<chamber_cpus> c_cpu = find_chamber_cpus(board_id);
    if (c_cpu)
    {
        // 存在，更新内容
        // 加写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_chamber_cpus);
        c_cpu->update_info(other);
        return c_cpu;
    }

    // 没找到需要重新创建
    LOG_MSG(MSG_LOG, "chamber_cpus_mgr::add_chamber_cpus() add board_id:0x%02x", board_id);
    std::shared_ptr<chamber_cpus> new_chamber_cpu(new chamber_cpus());
    new_chamber_cpu->update_info(other);
    // 插入到 map 中
    // 加写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_chamber_cpus);
    m_chamber_cpus_map.insert(std::make_pair(board_id, new_chamber_cpu));
    return new_chamber_cpu;
}

std::shared_ptr<chamber_cpus> chamber_cpus_mgr::find_chamber_cpus(int board_id)
{
    LOG_MSG(MSG_LOG, "Enter into chamber_cpus_mgr::find_chamber_cpus() board_id=0x%02x", board_id);
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_chamber_cpus);
    chamber_cpus_map_t::iterator iter = m_chamber_cpus_map.find(board_id);
    if (iter != m_chamber_cpus_map.end())
    {
        return iter->second;
    }
    // 没找到
    LOG_MSG(MSG_LOG, "Exited chamber_cpus_mgr::find_chamber_cpus()");
    return std::shared_ptr<chamber_cpus>();
}

chamber_cpus_mgr::chamber_cpus_map_t &chamber_cpus_mgr::get_all_chamber_cpus()
{
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_chamber_cpus);
    return m_chamber_cpus_map;
}

testsites::testsites()
{
}

testsites::testsites(const std::string &test_unit_number, const std::string &test_site_number, const std::string &site_processor_number,
                     const std::string &tester_bus_number, const std::string &ip_address, const std::string &host_name,
                     const std::string &physical_stations, const std::string &chamber_number)
    : m_test_unit_number(test_unit_number), m_test_site_number(test_site_number), m_site_processor_number(site_processor_number),
      m_tester_bus_number(tester_bus_number), m_ip_address(ip_address), m_host_name(host_name), m_physical_stations(physical_stations),
      m_chamber_number(chamber_number)
{
}

testsites::testsites(const testsites &other)
{
    if (this != &other)
    {
        update_info(other);
    }
}

testsites &testsites::operator=(const testsites &other)
{
    if (this != &other)
    {
        update_info(other);
    }
    return *this;
}

testsites::~testsites()
{
}

void testsites::update_info(const testsites &other)
{
    m_test_unit_number = other.m_test_unit_number;
    m_test_site_number = other.m_test_site_number;
    m_site_processor_number = other.m_site_processor_number;
    m_tester_bus_number = other.m_tester_bus_number;
    m_ip_address = other.m_ip_address;
    m_host_name = other.m_host_name;
    m_physical_stations = other.m_physical_stations;
    m_chamber_number = other.m_chamber_number;
}

testsites_mgr *testsites_mgr::m_instance = nullptr;
std::mutex testsites_mgr::m_mtx;

testsites_mgr::testsites_mgr()
{
}

testsites_mgr::~testsites_mgr()
{
}

testsites_mgr *testsites_mgr::get_instance()
{
    if (m_instance == nullptr)
    { // 第一重检测，如果未初始化
        std::lock_guard<std::mutex> lck(m_mtx); // 上锁，RAII，离开if()自动解锁
        if (m_instance == nullptr)
        { // 第二重检测，还未初始化，new
            m_instance = new testsites_mgr();
        }
    }
    return m_instance;
    // static testsites_mgr instance;
    // &instance;
}

std::shared_ptr<testsites> testsites_mgr::add_testsites(int board_id, const testsites &other)
{
    // 先检查是否已经存在
    std::shared_ptr<testsites> testsite = find_testsites(board_id);
    if (testsite)
    {
        // 说明存在，更新内容
        // 加写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_testsites);
        testsite->update_info(other);
        return testsite;
    }

    // 不存在，创建一个新的
    LOG_MSG(MSG_LOG, "testsites_mgr::add_testsites() add board_id:0x%02x", board_id);
    std::shared_ptr<testsites> new_testsite(new testsites());
    new_testsite->update_info(other);
    // 插入，加写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_testsites);
    m_testsites_map.insert(std::make_pair(board_id, new_testsite));
    return new_testsite;
}

std::shared_ptr<testsites> testsites_mgr::find_testsites(int board_id)
{
    LOG_MSG(MSG_LOG, "testsites_mgr::find_testsites() board_id=0x%02x", board_id);
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_testsites);
    testsites_map_t::iterator iter = m_testsites_map.find(board_id);
    if (iter != m_testsites_map.end())
    {
        // 找到了
        return iter->second;
    }
    // 没找到
    LOG_MSG(MSG_LOG, "Exited testsites_mgr::find_testsites()");
    return std::shared_ptr<testsites>();
}

testsites_mgr::testsites_map_t &testsites_mgr::get_all_testsites()
{
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_testsites);
    return m_testsites_map;
}

repairsites::repairsites()
{
}

repairsites::repairsites(const std::string &test_unit_number, const std::string &test_site_number, const std::string &repair_site_number,
                         const std::string &rcpu_number, const std::string &ip_address, const std::string &host_name, const std::string &rcb_numbers)
    : m_test_unit_number(test_unit_number), m_test_site_number(test_site_number), m_repair_site_number(repair_site_number),
      m_rcpu_number(rcpu_number), m_ip_address(ip_address), m_host_name(host_name), m_rcb_numbers(rcb_numbers)
{
}

repairsites::repairsites(const repairsites &other)
{
    if (this != &other)
    {
        update_info(other);
    }
}

repairsites &repairsites::operator=(const repairsites &other)
{
    if (this != &other)
    {
        update_info(other);
    }
    return *this;
}

repairsites::~repairsites()
{
}

void repairsites::update_info(const repairsites &other)
{
    m_test_unit_number = other.m_test_unit_number;
    m_test_site_number = other.m_test_site_number;
    m_repair_site_number = other.m_repair_site_number;
    m_rcpu_number = other.m_rcpu_number;
    m_ip_address = other.m_ip_address;
    m_host_name = other.m_host_name;
    m_rcb_numbers = other.m_rcb_numbers;
}

repairsites_mgr *repairsites_mgr::m_instance = nullptr;
std::mutex repairsites_mgr::m_mtx;

repairsites_mgr::repairsites_mgr()
{
}

repairsites_mgr::~repairsites_mgr()
{
}

repairsites_mgr *repairsites_mgr::get_instance()
{
    if (m_instance == nullptr)
    { // 第一重检测，如果未初始化
        std::lock_guard<std::mutex> lck(m_mtx); // 上锁，RAII，离开if()自动解锁
        if (m_instance == nullptr)
        { // 第二重检测，还未初始化，new
            m_instance = new repairsites_mgr();
        }
    }
    return m_instance;
    // static repairsites_mgr instance;
    // &instance;
}

std::shared_ptr<repairsites> repairsites_mgr::add_repairsites(int board_id, const repairsites &other)
{
    // 先检查是否已经存在
    std::shared_ptr<repairsites> rep = find_repairsites(board_id);
    if (rep)
    {
        // 说明已经存在，更新内容即可
        // 加写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_repairsites);
        rep->update_info(other);
        return rep;
    }

    // 不存在则新建
    LOG_MSG(MSG_LOG, "repairsites_mgr::add_repairsites() add board_id:0x%02x", board_id);
    std::shared_ptr<repairsites> new_rep(new repairsites());
    new_rep->update_info(other);
    // 插入
    boost::unique_lock<boost::shared_mutex> lock(m_mux_repairsites);
    m_repairsites_map.insert(std::make_pair(board_id, new_rep));
    return new_rep;
}

std::shared_ptr<repairsites> repairsites_mgr::find_repairsites(int board_id)
{
    LOG_MSG(MSG_LOG, "Enter into repairsites_mgr::find_repairsites() board_id=0x%02x", board_id);
    boost::shared_lock<boost::shared_mutex> lock(m_mux_repairsites);
    repairsites_map_t::iterator iter = m_repairsites_map.find(board_id);
    if (iter != m_repairsites_map.end())
    {
        // 找到了，直接返回
        return iter->second;
    }
    // 没找到
    LOG_MSG(MSG_LOG, "Exited repairsites_mgr::find_repairsites()");
    return std::shared_ptr<repairsites>();
}

repairsites_mgr::repairsites_map_t &repairsites_mgr::get_all_repairsites()
{
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_repairsites);
    return m_repairsites_map;
}

stations::stations()
{
}

stations::stations(const std::string &physical_station_number, const std::string &test_unit_number)
    : m_physical_station_number(physical_station_number), m_test_unit_number(test_unit_number)
{
}

stations::stations(const stations &other)
{
    if (this != &other)
    {
        update_info(other);
    }
}

stations &stations::operator=(const stations &other)
{
    if (this != &other)
    {
        update_info(other);
    }
    return *this;
}

stations::~stations()
{
}

void stations::update_info(const stations &other)
{
    m_physical_station_number = other.m_physical_station_number;
    m_test_unit_number = other.m_test_unit_number;
}

stations_mgr *stations_mgr::m_instance = nullptr;
std::mutex stations_mgr::m_mtx;

stations_mgr::stations_mgr()
{
}

stations_mgr::~stations_mgr()
{

}

stations_mgr *stations_mgr::get_instance()
{
    if(m_instance == nullptr) {  //第一重检测，如果未初始化
        std::lock_guard<std::mutex> lck(m_mtx);  //上锁，RAII，离开if{}自动解锁
        if(m_instance == nullptr) { //第二重检测，还未初始化，new
            m_instance = new stations_mgr();
        }
    }
    return m_instance;
    // static stations_mgr instance;
    // return &instance;
}

std::shared_ptr<stations> stations_mgr::add_stations(int board_id, const stations &other)
{
    // 先检查是否已经存在
    std::shared_ptr<stations> sta = find_stations(board_id);
    if(sta)
    {
        // 说明已经存在，更新信息
        // 加写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_stations);
        sta->update_info(other);
        return sta;
    }
    // 说明不存在，需要新建
    LOG_MSG(MSG_LOG, "stations_mgr::add_stations() add board_id:0x%02x", board_id);
    std::shared_ptr<stations> new_sta(new stations());
    new_sta->update_info(other);
    // 加写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_stations);
    m_stations_map.insert(std::make_pair(board_id, new_sta));

    return new_sta;
}

std::shared_ptr<stations> stations_mgr::find_stations(int board_id)
{
    LOG_MSG(MSG_LOG, "Enter into stations_mgr::find_stations() board_id=0x%02x", board_id);
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_stations);
    stations_map_t::iterator iter = m_stations_map.find(board_id);
    if(iter != m_stations_map.end())
    {
        // 找到了返回
        return iter->second;
    }
    // 没有找到
    LOG_MSG(MSG_LOG, "Exited stations_mgr::find_stations()");
    return std::shared_ptr<stations>();
}

stations_mgr::stations_map_t stations_mgr::get_all_stations()
{
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_stations);
    return m_stations_map;
}
