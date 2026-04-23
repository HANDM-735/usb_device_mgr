#include "xsession.h"
#include "mgr_log.h"

xsession::xsession(int session_id, const std::string& port_name) : xtransmitter<xsession>()
{
    m_sess_id = session_id;
    m_mgr_usb = mgr_usb::get_instance();
    m_board_mgr = xboardmgr::get_instance();
    m_port_name = port_name;
    //LOG_MSG(WRN_LOG, "xsession::xsession() construct");
}

xsession::~xsession()
{
    m_mgr_usb = NULL;
    m_board_mgr = NULL;
    //LOG_MSG(WRN_LOG, "xsession::~xsession() deconstruct");
}

std::string xsession::get_port_name()
{
    return m_port_name;
}

int xsession::get_sessionid()
{
    return m_sess_id;
}

int xsession::on_recv(const char *port_name, xusbadapter_package *pack) //消息通知
{
    LOG_MSG(MSG_LOG, "Enter into xsession::on_recv()");
    if (m_port_name != port_name)
    {
        m_port_name = port_name; //保存端口
    }

    if (pack->m_msg_type == xusbadapter_package::MT_REQUEST) {
        switch (pack->m_msg_cmd)
        {
        case xusbadapter_package::MC_START:
            handle_start_request(port_name, pack);
            break;
        case xusbadapter_package::MC_QUERY:
            handle_query_request(port_name, pack);
            break;
        case xusbadapter_package::MC_CANCEL:
            handle_cancel_request(port_name, pack);
            break;
        case xusbadapter_package::MC_DATA:
            handle_data_request(port_name, pack);
            break;
        case xusbadapter_package::MC_CAL_READ:
            handle_cal_read_request(port_name, pack);
            break;
        case xusbadapter_package::MC_HEARTBEAT:
            handle_heartbeat(port_name, pack);
            break;
        case xusbadapter_package::MC_USBSERIALS_ID:
            handle_serialids_request(port_name, pack);
            break;
        case xusbadapter_package::MC_ASIC_JUNCTION_TEMP:
            handle_junction_temp_request(port_name, pack);
            break;
        case xusbadapter_package::MC_GET_VALUE:
            handle_get_value_request(port_name, pack);
            break;
        case xusbadapter_package::MC_COMMAND:
            handle_command_request(port_name, pack);
            break;
        case xusbadapter_package::MC_FREQUENCY:
            handle_frequency_set_request(port_name, pack);
            break;
        case xusbadapter_package::MC_GET_UTP_REGISTER:
            handle_get_utp_register_request(port_name, pack);
            break;
        case xusbadapter_package::MC_GET_UTP102_REGISTER:
            handle_get_utp102_register_request(port_name, pack);
            break;
        }
    } else if (pack->m_msg_type == xusbadapter_package::MT_NOTIFY) {
        switch (pack->m_msg_cmd)
        {
        case xusbadapter_package::ME_COMPLETE:
            handle_complete_notify(port_name, pack);
            break;
        }
    }
    LOG_MSG(MSG_LOG, "Exited xsession::on_recv()");
    return xtransmitter::on_recv(port_name, pack);
}

bool xsession::is_monitor_board(unsigned short boardid)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::is_monitor_board() boardid=%hu", boardid);
    bool ret = false;

    if ((boardid & BOARDTYPE_MASK) == BOARDTYPE_TH_MONITOR || (boardid & BOARDTYPE_MASK) == BOARDTYPE_MF_MONITOR)
    {
        ret = true;
    }

    LOG_MSG(MSG_LOG, "Exited xsession::is_monitor_board() ret=%d", ret);

    return ret;
}

int xsession::handle_start_request(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_start_request()");
    mgr_upgrade *upgrade_mgr = mgr_upgrade::get_instance();

    int ret = 0;

    if (is_ota_caltype(pack->m_ota_type) == false)
    {
        upgrade_mgr->load_otafiles();
        std::string ota_type_name;
        ota_type_name = upgrade_mgr->get_ota_fw_name(pack->m_ota_type);
        if (ota_type_name.empty())
        {
            LOG_MSG(WRN_LOG, "xsession::handle_start_request() ota type(%d) error", pack->m_ota_type);
            ret = -1;
            send_start_response(pack, ret);
            return ret;
        }

        boost::shared_ptr<xxotafile> otafile;
        otafile = upgrade_mgr->find_otafile(ota_type_name);
        if (otafile == NULL)
        {
            LOG_MSG(WRN_LOG, "xsession::handle_start_request() ota file(%s) is not existed", ota_type_name.c_str());
            ret = -1;
            send_start_response(pack, ret);
            return ret;
        }

        if (otafile->m_ver_int != pack->m_ota_version)
        {
            std::string pack_ota_ver = xbasic::version_from_int(pack->m_ota_version);
            LOG_MSG(WRN_LOG, "xsession::handle_start_request() ota type(%d) ota file version(0x%1d:0x%x) is not current available latest version(0x%1d:0x%x)", pack->m_ota_type, pack->m_ota_version, pack->m_ota_version, otafile->m_ver_int, otafile->m_ver_int);
            ret = -1;
            send_start_response(pack, ret);
            return ret;
        }
    }
    else
    {
        // upgrade_mgr->load_calfiles();
        if (!upgrade_mgr->load_file(pack->m_calfilename))
        {
            ret = -1;
            LOG_MSG(WRN_LOG, "xsession::handle_start_request() load cal file failed");
            send_start_response(pack, ret);
            return ret;
        }
    }

    if (is_monitor_board(pack->m_usb_dst) == false)
    {
        mgr_serial *serial_mgr = mgr_serial::get_instance();
        boost::shared_ptr<xdev_serial> the_serial = serial_mgr->find_device_serial_byid(pack->m_usb_dst);
        if (the_serial == NULL)
        {
            LOG_MSG(WRN_LOG, "xsession::handle_start_request() serail(0x%x) is not existed", pack->m_usb_dst);
            ret = -1;
            send_start_response(pack, ret);
            return ret;
        }
    }

    if (is_ota_caltype(pack->m_ota_type) == false)
    {
        ret = m_mgr_usb->send_ota_start(pack->m_usb_dst, pack->m_ota_type);
    }
    else
    {
        ret = m_mgr_usb->send_ota_start(pack->m_usb_dst, pack->m_ota_type, pack->m_calfilename);
    }
    else
    {
#if defined(SYNC_BUILD)
        mgr_device *device_mgr = mgr_device::get_instance();
        ret = device_mgr->send_ota_start(pack->m_usb_dst, pack->m_ota_type);
#endif
    }

    //返回结果
    send_start_response(pack, ret);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_start_request() ret=%d", ret);
    return ret;
}

int xsession::handle_query_request(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_query_request()");
    int ret = -1;
    int progress = 0;
    //初始化一个最大不可能的总长度，防止计算进度时，防止除数为0
    int total_len = 0xFFFFFFFF;
    boost::shared_ptr<ota_session> ota_session;
    if (is_monitor_board(pack->m_usb_dst) == false)
    {
        ota_session = m_mgr_usb->find_ota_session(pack->m_usb_dst);
    }
    else
    {
#if defined(SYNC_BUILD)
        mgr_device *device_mgr = mgr_device::get_instance();
        ota_session = device_mgr->find_ota_session(pack->m_usb_dst);
#endif
    }
    if (ota_session == NULL) {
        LOG_MSG(WRN_LOG, "xsession::handle_query_request() ota session is not existed!");
    } else {
        progress = ota_session->get_ota_transed_len();
        total_len = ota_session->get_ota_total_len();
    }

    ret = (progress > 0) ? 0 : -1;

    LOG_MSG(MSG_LOG, "xsession::handle_query_request() progress:%d total_len:%d", progress, total_len);

    //返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);
    resp.m_upgrade_progress = progress;
    resp.m_total_len = total_len;

    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::handle_query_request() ret=%d", ret);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_query_request()");

    return ret;
}

int xsession::handle_cancel_request(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_cancel_request()");

    int ret = 0;

    if (is_monitor_board(pack->m_usb_dst) == false)
    {
        ret = m_mgr_usb->send_ota_cancel(pack->m_usb_dst, pack->m_ota_type);
    }
    else
    {
#if defined(SYNC_BUILD)
        mgr_device *device_mgr = mgr_device::get_instance();
        ret = device_mgr->send_ota_cancel(pack->m_usb_dst, pack->m_ota_type);
#endif
    }

    //返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);
    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    LOG_MSG(MSG_LOG, "xsession::handle_start_request() receive cancel message");

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_cancel_request()");
    return ret;
}

int xsession::handle_complete_notify(const char *port_name, xusbadapter_package *pack)
{
    int ret = -1;

    boost::shared_ptr<ota_session> ota_session = m_mgr_usb->find_ota_session(pack->m_usb_dst);
    if (ota_session == NULL) {
        LOG_MSG(MSG_LOG, "xsession::handle_complete_notify() ota session is not existed!");
        return ret;
    }

    int current_status = ota_session->get_session_status();

    if (current_status == OTA_SESSION_STATUS_COMPLETED)
    {
        LOG_MSG(WRN_LOG, "xsession::handle_complete_notify() usb dstid=%d ota session(%d) is OTA_SESSION_STATUS_COMPLETED switch to OTA_SESSION_STATUS_END", pack->m_usb_dst, current_status);
        ret = m_mgr_usb->set_ota_session_status(pack->m_usb_dst, OTA_SESSION_STATUS_END);
    }
    else if (current_status == OTA_SESSION_STATUS_CALFILE_COMPLETE)
    {
        LOG_MSG(WRN_LOG, "xsession::handle_complete_notify() usb dstid=%d ota session(%d) is OTA_SESSION_STATUS_CALFILE_COMPLETE, switch to OTA_SESSION_STATUS_END", pack->m_usb_dst, current_status);
        ret = m_mgr_usb->set_ota_session_status(pack->m_usb_dst, OTA_SESSION_STATUS_END);
    }
    else
    {
        LOG_MSG(ERR_LOG, "xsession::handle_complete_notify() usb dstid=%d ota session(%d) is not OTA_SESSION_STATUS_COMPLETED/OTA_SESSION_STATUS_CALFILE_END can't switch to OTA_SESSION_STATUS_END", pack->m_usb_dst, current_status);
    }

    LOG_MSG(MSG_LOG, "Exited xsession::handle_complete_notify() receive complete notify message\n");
    return ret;
}

bool xsession::filter_data_request_tid(unsigned short tid)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::filter_data_request_tid() tid=0x%x", tid);
    bool ret = false;

    switch (tid)
    {
    case RWA_TEMP:
    case RWA_VOLTAGE:
    case RWA_POWER_STATUS:
    case RWA_POS_STATUS:
    case RWA_BOARD_SN:
    case RWA_MANU_INFO:
    case RWA_FW_VER:
    case RWA_SW_VER:
    case RWA_SLOT:
    case RWA_AD9528_LOCKSTATUS:
    case RWA_AD9545_LOCKSTATUS:
    case RWA_PMM_CHECK:
    case RWA_SERDES_STATUS:
        ret = true;
        break;
    }

    LOG_MSG(MSG_LOG, "Exited xsession::filter_data_request_tid() ret=%d", ret);

    return ret;
}

int xsession::handle_data_request(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_data_request() receive data message");
    int ret = -1;
    std::list<boost::shared_ptr<xusb_tlv> > list_tlv;
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);
    resp.m_usb_dst = pack->m_usb_dst;

    int count = 0;
    int size = m_mgr_usb->get_board_data(pack->m_usb_dst, list_tlv);
    if (size != 0)
    {
        std::list<boost::shared_ptr<xusb_tlv> >::iterator iter = list_tlv.begin();
        for (; iter != list_tlv.end(); iter++)
        {
            boost::shared_ptr<xusb_tlv> tlv = *iter;
            if (filter_data_request_tid(tlv->m_tid))
            {
                boost::shared_ptr<xusbadapter_package::rw_data::data_value> value_data(new xusbadapter_package::rw_data::data_value());

                value_data->tid = tlv->m_tid;
                value_data->length = tlv->get_data().length();
                value_data->value.assign(tlv->get_data().c_str(), value_data->length);

                resp.m_rw_data.m_data_map.insert(std::make_pair(value_data->tid, value_data));
                count++;
            }
        }

        if (count > 0)
        {
            ret = 0;
        }
    }

    //返回结果
    if (ret != 0)
    {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
    }
    else
    {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    //LOG_MSG(MSG_LOG, "xsession::handle_data_request() receive data message debug!");
    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_data_request()");

    return ret;
}

int xsession::handle_cal_read_request(const char* port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_cal_read_request() send cal read reponse message");
    mgr_serial *serial_mgr = mgr_serial::get_instance();
    int ret = 0;
    boost::shared_ptr<xdev_serial> the_serial = serial_mgr->find_device_serial_byid(pack->m_usb_dst);
    if (the_serial == NULL) {
        send_cal_read_response(pack, ret);
        LOG_MSG(ERR_LOG, "xsession::handle_cal_read_request() serail(%d) is not existed", pack->m_usb_dst);
        return ret;
    }

    bool file_path_flag = true;

    std::string file_path = pack->m_calfilename;
    LOG_MSG(MSG_LOG, "xsession::handle_cal_read_request() file_path=%s", file_path.c_str());
    if (file_path.empty() || file_path.back() == '/')
    {
        // 传入的路径为空，或者传进来的不是文件路径
        file_path_flag = false;
    }
    else
    {
        // 到这里说明路径不为空，并且不是目录，需要判断该目录是否存在，不存在返回失败

        struct stat path_info;
        // 获取传入的路径信息，获取成功，说明不管是目录还是文件都是存在的
        if (stat(file_path.c_str(), &path_info) == 0)
        {
            // 信息获取成功，查看是否是目录
            if (S_ISDIR(path_info.st_mode))
            {
                // 是目录就返回失败
                file_path_flag = false;
                LOG_MSG(WRN_LOG, "xsession::handle_cal_read_request() file_path=%s is dir", file_path.c_str());
            }
        }
        else
        {
            // 信息获取失败，说明没有当前的路径信息
            // 找到最后一个反斜杠
            size_t last_slash = file_path.find_last_of('/');
            // 如果没找到说明只传入了文件名认为正确
            if (last_slash != std::string::npos)
            {
                // 提取父目录
                std::string parent_dir = file_path.substr(0, last_slash);
                LOG_MSG(MSG_LOG, "xsession::handle_cal_read_request() parent_dir=%s", parent_dir.c_str());
                struct stat dir_info;
                if (stat(parent_dir.c_str(), &dir_info) == 0)
                {
                    // 检查是否是目录
                    if (!S_ISDIR(dir_info.st_mode))
                    {
                        // 不是目录
                        file_path_flag = false;
                        LOG_MSG(WRN_LOG, "xsession::handle_cal_read_request() parent_dir=%s is not dir", parent_dir.c_str());
                    }
                }
                else
                {
                    // 目录不存在
                    file_path_flag = false;
                    LOG_MSG(WRN_LOG, "xsession::handle_cal_read_request() parent_dir=%s is not exist", parent_dir.c_str());
                }
            }
        }
    }

    if (!file_path_flag)
    {
        xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        send(m_port_name.c_str(), &resp);
        ret = -1;
        LOG_MSG(WRN_LOG, "xsession::handle_cal_read_request() send file path error file_path_flag=%d", file_path_flag);
        return ret;
    }

    file_bak(pack->m_calfilename);

    m_version = pack->m_version;
    m_sess_id = pack->m_session;

    ret = m_mgr_usb->send_calfile_read(pack->m_usb_dst, pack->m_ota_type, pack->m_ota_version, m_sess_id, m_port_name, pack->m_calfilename);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_cal_read_request() ret=%d", ret);
    return ret;
}

int xsession::handle_heartbeat(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_heartbeat()");

    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    //LOG_MSG(MSG_LOG, "xsession::handle_heartbeat() debug!");

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_heartbeat()");
    return 0;
}

int xsession::send_start_response(xusbadapter_package *pack, int errcode)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::send_start_response() otatype = %d errcode=%d", pack->m_ota_type, errcode);
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);
    if (errcode != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::send_start_response() otatype = %d errcode=%d", pack->m_ota_type, errcode);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::send_start_response() otatype = %d errcode=%d", pack->m_ota_type, errcode);
    return 0;
}

int xsession::send_cal_read_response(xusbadapter_package *pack, int errcode)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::send_cal_read_response() send cal read reponse message");
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);
    //返回结果
    if (errcode != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::send_cal_read_response() ");
    return 0;
}

int xsession::handle_serialids_request(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_serialids_request() receive serial id message");

    int ret = -1;

    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);
    int size = m_mgr_usb->get_usbserials_id(resp.m_usbids_ids);
    if (size > 0) {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    } else {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_serialids_request() ");

    return 0;
}

int xsession::handle_junction_temp_request(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_junction_temp_request() receive junction temperature message");

    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    //查找ASIC的boardID
    std::vector<int> serial_ids;
    int asic_boardid;
    bool found = false;
    int size = m_mgr_usb->get_usbserials_id(serial_ids);
    for (int i = 0; i < size; i++)
    {
        if ((serial_ids[i] & 0xFF00) == BOARDTYPE_ASIC)
        {
            found = true;
            asic_boardid = serial_ids[i];
            LOG_MSG(WRN_LOG, "xsession::handle_junction_temp_request() asic board id=%d", asic_boardid);
            break;
        }
    }

    if (found)
    {
        int slotid = (asic_boardid & 0xFF00) >> 8;
        int index = slotid * FT_ASIC_CHIP_NUM;
        int ret = -1;
        for (int i = 0; i < FT_ASIC_CHIP_NUM; i++)
        {
            int chipid = i + index;
            double asic_tmp = 0.0;

#if defined(XGB_BUILD)
            ret = DRV_GetAsicTmp(chipid, asic_tmp);
#endif

            if (ret != 0)
            {
                LOG_MSG(ERR_LOG, "mgr_usb::handle_junction_temp_request() DRV_GetAsicTmp chip id %d failed", chipid);
            }
            else
            {
                xusbadapter_package::junction_temp junct;
                junct.m_asicchip_id = chipid;
                junct.m_temp = asic_tmp;
                resp.m_junctions.push_back(junct);
            }
            usleep(10000);
        }

        if (resp.m_junctions.size() > 0)
        {
            resp.m_err_code = xusbadapter_package::ME_SUCCESS;
        }
        else
        {
            resp.m_err_code = xusbadapter_package::ME_FAILED;
            LOG_MSG(ERR_LOG, "xsession::handle_junction_temp_request() not found asic junction temperature size=%d", size);
        }
    }
    else
    {
        //没有找到回复失败
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_junction_temp_request() not found asic boardid");
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_junction_temp_request() ");

    return 0;
}

int xsession::handle_get_value_pcbtemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_pcbtemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);

    std::string temp;
    if(board != NULL)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_TEMP);
        if (tvl != NULL)
        {
            std::vector<pcb_temp> vct_tmp;
            xconvert::parse_pcbtemp_data(tvl->m_value, vct_tmp);
            int vct_size = vct_tmp.size();
            if(vct_size > 0)
            {
                std::vector<std::string> vct_str;
                for(int i = 0; i < vct_size; i++)
                {
                    std::string str;
                    str = std::to_string(vct_tmp[i].m_id) + ":" + std::to_string(vct_tmp[i].m_temp_value);
                    vct_str.push_back(str);
                }
                std::string temp = boost::join(vct_str, ",");
                found = true;
                LOG_MSG(MSG_LOG, "xsession::handle_get_value_pcbtemp() boardid=0x%x pcb temp:%s", boardid, temp.c_str());
            }
            else
            {
                LOG_MSG(ERR_LOG, "xsession::handle_get_value_pcbtemp() boardid=0x%x failed", boardid);
            }
        }
    }

    if(found)
    {
        resp.m_value_dev = temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_pcbtemp() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_pcbtemp() ret=%d", ret);

    return ret;
}

int xsession::handle_get_value_utp40temp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_utp40temp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);

    std::string utp40_temp;
    if(board != NULL)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_UTP40_TEMP);
        if (tvl != NULL)
        {
            utp40_temp = tvl->m_value;
            found = true;
            LOG_MSG(MSG_LOG, "xsession::handle_get_value_utp40temp() VALUE_UTP40_TMP:%s", utp40_temp.c_str());
        }
    }

    if(found)
    {
        resp.m_value_dev = utp40_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = utp40_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_utp40temp() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_utp40temp() ret=%d", ret);

    return ret;
}

int xsession::handle_get_value_rcafpgatemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_rcafpgatemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    int rk3588_slotid = pack->m_rk3588_slotid;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);

    std::string rcafpga_temp;
    if(board != NULL)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_rdata(rk3588_slotid);
        if (tvl != NULL)
        {
            rcafpga_temp = tvl->m_value;
            found = true;
            LOG_MSG(MSG_LOG, "xsession::handle_get_value_rcafpgatemp() VALUE_RCAFPGA_TEMP rk3588_slotid:%d", rk3588_slotid);
        }
    }

    if(found)
    {
        resp.m_value_dev = rcafpga_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = rcafpga_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_rcafpgatemp() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_rcafpgatemp() boardid=0x%x rk3588_slotid=%d ret=%d", boardid, rk3588_slotid, ret);

    return ret;
}

int xsession::handle_get_value_temp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_temp()");

    int ret = 0;

    int boardid = pack->m_usb_dst;
    int boardtype = boardid & 0xFF;
    switch(boardtype)
    {
    case BOARDTYPE_FT_SYNC:
        ret = handle_get_value_ftsynctemp(port_name, pack);
        break;
    case BOARDTYPE_FT_PPS:
        ret = handle_get_value_ftppstemp(port_name, pack);
        break;
    case BOARDTYPE_FT_PGB:
        ret = handle_get_value_ftpgbtemp(port_name, pack);
        break;
    case BOARDTYPE_ASIC:
        ret = handle_get_value_ftasictemp(port_name, pack);
        break;
    case BOARDTYPE_CP_SYNC:
        ret = handle_get_value_cpsynctemp(port_name, pack);
        break;
    case BOARDTYPE_CP_PEM:
        ret = handle_get_value_cppemtemp(port_name, pack);
        break;
    case BOARDTYPE_CP_DPS:
        ret = handle_get_value_cdpstemp(port_name, pack);
        break;
    case BOARDTYPE_CP_RCA:
        ret = handle_get_value_cprcatemp(port_name, pack);
        break;
    default:
        ret = -2;
        break;
    }

    if(ret == -2)
    {
        xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);
        resp.m_value_dev = "";
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        send(m_port_name.c_str(), &resp);
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_temp() boardid=0x%x boardtype=%d port_name=%s", boardid, boardtype, port_name);
    }

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_temp() ret=%d", ret);

    return ret;
}

int xsession::handle_get_value_ftsynctemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_ftsynctemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string ftsync_temp;

    if(board != NULL)
    {
        std::vector<std::string> vct_temp_str;
        // 1、获取X86的温度
        int index_offset = 0;
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftsynctemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> x86temp_tvl = board->find_data(RWA_X86_TEMP);
        if(x86temp_tvl != NULL)
        {
            std::string x86_temp;
            x86_temp = x86temp_tvl->m_value;
            vct_temp_str.push_back(x86_temp);
        }

        // 2、获取FPGA的温度（FPGA的温度是按照二进制的方式存储在内存中的）
        index_offset += FT_SYNC_X86_NUM;
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftsynctemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> fpga_temp_tvl = board->find_data(RWA_FPGA_TEMP);
        std::vector<fpga_temp> vct_fpga_temp;
        if(fpga_temp_tvl != NULL)
        {
            xconvert::parse_fpga_temp(fpga_temp_tvl->m_value, vct_fpga_temp);

            int fpga_temp_size = vct_fpga_temp.size();
            for(int i = 0; i < fpga_temp_size; ++i)
            {
                std::string str;
                str = std::to_string(vct_fpga_temp[i].m_id + index_offset) + ":" + std::to_string(vct_fpga_temp[i].m_temp_value);
                vct_temp_str.push_back(str);
            }

            LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftsynctemp() GET FPGA TEMP SIZE:%d", fpga_temp_size);
        }

        // 3、获取MCU的温度
        index_offset += FT_SYNC_FPGA_KU060_NUM; // 因为FPGA的温度只有一个所以这里是加1
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftsynctemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> temp_tvl = board->find_data(RWA_TEMP);
        std::vector<pcb_temp> vct_tmp;
        if(temp_tvl != NULL)
        {
            xconvert::parse_pcbtemp_data(temp_tvl->m_value, vct_tmp);
        }

        int vct_size = vct_tmp.size();
        for(int i = 0; i < vct_size; i++)
        {
            std::string str;
            str = std::to_string(vct_tmp[i].m_id + index_offset) + ":" + std::to_string(vct_tmp[i].m_temp_value);
            vct_temp_str.push_back(str);
        }

        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftsynctemp() pcb temp vct_size=%d", vct_size);

        int sz = vct_temp_str.size();
        if(sz > 0)
        {
            found = true;
            ftsync_temp = boost::join(vct_temp_str, ",");
        }
    }

    LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftsynctemp() vct_temp_str size=%d", sz);

    if(found)
    {
        resp.m_value_dev = ftsync_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = ftsync_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_ftsynctemp() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_ftsynctemp() ret=%d", ret);

    return ret;
}

int xsession::handle_get_value_ftppstemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_ftppstemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string ftpps_temp;

    if(board != NULL)
    {
        std::vector<std::string> vct_temp_str;

        // 1、获取UTP40的温度
        int index_offset = 0;
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftppstemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> utp40_tvl = board->find_data(RWA_UTP40_TEMP);
        if(utp40_tvl != NULL)
        {
            std::string utp40_temp;
            utp40_temp = utp40_tvl->m_value;
            // 需要注意split_string中有一个清空数组的操作，这里因为是第一个获取utp40的温度，所以不影响
            int utp40_size = xbasic::split_string(utp40_temp, ",", &vct_temp_str);
            LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftppstemp() utp40_size=%d", utp40_size);
        }
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftppstemp() GET UTP40 TEMP SIZE:%d", vct_temp_str.size());

        // 2、获取FPGA的温度（FPGA的温度是按照二进制的方式存储在内存中的）
        index_offset += (UTP40_CHIP_NUM_PPS * 4);
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftppstemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> fpga_temp_tvl = board->find_data(RWA_FPGA_TEMP);
        std::vector<fpga_temp> vct_fpga_temp;
        if(fpga_temp_tvl != NULL)
        {
            xconvert::parse_fpga_temp(fpga_temp_tvl->m_value, vct_fpga_temp);

            int fpga_temp_size = vct_fpga_temp.size();
            for(int i = 0; i < fpga_temp_size; ++i)
            {
                std::string str;
                str = std::to_string(vct_fpga_temp[i].m_id + index_offset) + ":" + std::to_string(vct_fpga_temp[i].m_temp_value);
                vct_temp_str.push_back(str);
            }

            LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftppstemp() GET FPGA TEMP SIZE:%d", fpga_temp_size);
        }

        // 3、获取温度传感器和MCU的温度
        index_offset += FT_PPS_FPGA_7A200T_NUM;
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftppstemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> temp_tvl = board->find_data(RWA_TEMP);
        std::vector<pcb_temp> vct_tmp;
        if(temp_tvl != NULL)
        {
            xconvert::parse_pcbtemp_data(temp_tvl->m_value, vct_tmp);
        }

        int vct_size = vct_tmp.size();
        for(int i = 0; i < vct_size; i++)
        {
            std::string str;
            str = std::to_string(vct_tmp[i].m_id + index_offset) + ":" + std::to_string(vct_tmp[i].m_temp_value);
            vct_temp_str.push_back(str);
        }

        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftppstemp() pcb temp vct_size=%d", vct_size);

        int sz = vct_temp_str.size();
        if(sz > 0)
        {
            found = true;
            ftpps_temp = boost::join(vct_temp_str, ",");
        }
    }

    LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftppstemp() vct_temp_str size=%d", sz);

    if(found)
    {
        resp.m_value_dev = ftpps_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = ftpps_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_ftppstemp() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_ftppstemp() ret=%d", ret);

    return ret;
}

int xsession::handle_get_value_ftpgbtemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_ftpgbtemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string ftpgs_temp;

    if(board != NULL)
    {
        std::vector<std::string> vct_temp_str;

        // 在《FT诊断流程以及LOG说明V0.10》中暂时没有获取PGB上UTP40温度的需求，因此先将下面这段代码注释掉
        // boost::shared_ptr<xusb_tvl> utp40_tvl = board->find_data(RWA_UTP40_TEMP);
        // if(utp40_tvl != NULL)
        // {
        //     std::string utp40_temp;
        //     utp40_temp = utp40_tvl->m_value;
        //     int utp40_size = xbasic::split_string(utp40_temp, ",", &vct_temp_str);
        //     LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftpgbtemp() utp40_size=%d", utp40_size);
        // }
        // int index_offset = UTP40_CHIP_NUM_PGB * 4;

        // 1、获取X86芯片上的温度
        int index_offset = 0;
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftpgbtemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> x86temp_tvl = board->find_data(RWA_X86_TEMP);
        if(x86temp_tvl != NULL)
        {
            std::string x86_temp;
            x86_temp = x86temp_tvl->m_value;
            vct_temp_str.push_back(x86_temp);
        }

        // 2、获取FPGA上的温度
        index_offset += FT_PGB_X86_NUM;
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftpgbtemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> fpga_temp_tvl = board->find_data(RWA_FPGA_TEMP);
        std::vector<fpga_temp> vct_fpga_temp;
        if(fpga_temp_tvl != NULL)
        {
            xconvert::parse_fpga_temp(fpga_temp_tvl->m_value, vct_fpga_temp);

            int fpga_temp_size = vct_fpga_temp.size();
            for(int i = 0; i < fpga_temp_size; ++i)
            {
                std::string str;
                str = std::to_string(vct_fpga_temp[i].m_id + index_offset) + ":" + std::to_string(vct_fpga_temp[i].m_temp_value);
                vct_temp_str.push_back(str);
            }

            LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftpgbtemp() GET FPGA TEMP SIZE:%d", fpga_temp_size);
        }

        // 3、获取温度传感器和MCU上的温度
        index_offset += (FT_PGB_FPGA_KU060_NUM + FT_PGB_FPGA_7A200T_NUM);
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftpgbtemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> temp_tvl = board->find_data(RWA_TEMP);
        std::vector<pcb_temp> vct_tmp;
        if(temp_tvl != NULL)
        {
            xconvert::parse_pcbtemp_data(temp_tvl->m_value, vct_tmp);
        }

        int vct_size = vct_tmp.size();
        for(int i = 0; i < vct_size; i++)
        {
            std::string str;
            str = std::to_string(vct_tmp[i].m_id + index_offset) + ":" + std::to_string(vct_tmp[i].m_temp_value);
            vct_temp_str.push_back(str);
        }

        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftpgbtemp() pcb temp vct_size=%d", vct_size);

        int sz = vct_temp_str.size();
        if(sz > 0)
        {
            found = true;
            ftpgs_temp = boost::join(vct_temp_str, ",");
        }
    }

    LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftpgbtemp() vct_temp_str size=%d", sz);

    if(found)
    {
        resp.m_value_dev = ftpgs_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = ftpgs_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_ftpgbtemp() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_ftpgbtemp() ret=%d", ret);

    return ret;
}

int xsession::handle_get_value_ftasictemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_ftasictemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string ftasic_temp;

    if(board != NULL)
    {
        std::vector<std::string> vct_temp_str;

        // 1、获取FPGA的温度
        int index_offset = 0;
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftasictemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> fpga_temp_tvl = board->find_data(RWA_FPGA_TEMP);
        std::vector<fpga_temp> vct_fpga_temp;
        if(fpga_temp_tvl != NULL)
        {
            xconvert::parse_fpga_temp(fpga_temp_tvl->m_value, vct_fpga_temp);
        }

        int fpga_temp_size = vct_fpga_temp.size();
        for(int i = 0; i < fpga_temp_size; ++i)
        {
            std::string str;
            str = std::to_string(vct_fpga_temp[i].m_id + index_offset) + ":" + std::to_string(vct_fpga_temp[i].m_temp_value);
            vct_temp_str.push_back(str);
        }

        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftasictemp() GET FPGA TEMP SIZE:%d", fpga_temp_size);

        // 2、获取ASIC芯片的温度
        index_offset += (FT_ASIC_FPGA_IST165_NUM + FT_ASIC_FPGA_AGF0810_NUM);
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftasictemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> asic_tvl = board->find_data(RWA_ASIC_TEMP);

        if(asic_tvl != NULL)
        {
            std::string asic_temp;
            asic_temp = asic_tvl->m_value;
            // 需要注意这里的split_string()接口中有一个清空数组的操作，这里不能直接把vct_temp_str传进去，会导致前面的FPGA温度被清除
            std::vector<std::string> vct_asic_temp;
            int asic_size = xbasic::split_string(asic_temp, ",", &vct_asic_temp);
            for(int i = 0; i < asic_size; ++i)
            {
                std::vector<std::string> tmp;
                int tmp_size = xbasic::split_string(vct_asic_temp[i], ":", &tmp);
                if(tmp_size == 2)
                {
                    int id = std::stoi(tmp[0]);
                    std::string str = std::to_string(id + index_offset) + ":" + tmp[1];
                    vct_temp_str.push_back(str);
                }
            }
        }
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftasictemp() asic_size=%d", asic_size);

        // 3、获取传感器的温度
        index_offset += FT_ASIC_CHIP_NUM;
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftasictemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> temp_tvl = board->find_data(RWA_TEMP);
        std::vector<pcb_temp> vct_tmp;
        if(temp_tvl != NULL)
        {
            xconvert::parse_pcbtemp_data(temp_tvl->m_value, vct_tmp);
        }

        int vct_size = vct_tmp.size();
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftasictemp() pcb temp vct_size=%d", vct_size);
        // 记录温度传感器的数量
        int ts_size = vct_size;
        if(vct_size == (FT_ASIC_3_1_TS_NUM + 1) || vct_size == (FT_ASIC_4_0_TS_NUM + 1))
        {
            // 此时说明最后一个是MCU的温度，需要对ts_size减一
            ts_size -= 1;
        }
        LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftasictemp() Temperature Sensor temp size=%d", ts_size);
        // 下面这个遍历拿到的一定是温度传感器的温度
        for(int i = 0; i < ts_size; i++)
        {
            std::string str;
            str = std::to_string(vct_tmp[i].m_id + index_offset) + ":" + std::to_string(vct_tmp[i].m_temp_value);
            vct_temp_str.push_back(str);
        }

        if(vct_size == (FT_ASIC_3_1_TS_NUM + 1) || vct_size == (FT_ASIC_4_0_TS_NUM + 3))
        {
            // 此时说明最后一个是MCU的温度，需要修改偏移量，加上温度传感器的数量最大值
            index_offset += FT_ASIC_4_0_TS_NUM;
            LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftasictemp() index_offset:%d", index_offset);
            // 拿到MCU的温度
            std::string str = std::to_string(index_offset + 1) + ":" + std::to_string(vct_tmp[vct_size-1].m_temp_value);
            vct_temp_str.push_back(str);
        }

        int sz = vct_temp_str.size();
        if(sz > 0)
        {
            found = true;
            ftasic_temp = boost::join(vct_temp_str, ",");
        }
    }

    LOG_MSG(MSG_LOG, "xsession::handle_get_value_ftasictemp() vct_temp_str size=%d", sz);

    if(found)
    {
        resp.m_value_dev = ftasic_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = ftasic_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_ftasictemp() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_ftasictemp() ret=%d", ret);

    return ret;
}

int xsession::handle_get_value_cpsynctemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_cpsynctemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string cpsync_temp;

    if(board != NULL)
    {
        std::vector<comb_temp> vct_temp;

        // CP_SYNC x86温度
        int index_offset = 0;
        int vct_size = 0;
        boost::shared_ptr<xusb_tvl> x86temp_tvl = board->find_data(RWA_X86_TEMP);
        if(x86temp_tvl != NULL)
        {
            std::string x86_temp;
            x86_temp = x86temp_tvl->m_value;
            std::vector<std::string> vct_str;
            int count = xbasic::split_string(x86_temp, ":", &vct_str);
            if(count == 2)
            {
                comb_temp temp;
                temp.m_id = std::stoi(vct_str[0]) + index_offset;
                temp.m_temp_value = std::stof(vct_str[1]);
                vct_temp.push_back(temp);
                LOG_MSG(MSG_LOG, "xsession::handle_get_value_cpsynctemp() x86 id:%d temp:%F", temp.m_id, temp.m_temp_value);
            }
            
        }

        //CP SYNC FPGA温度
        index_offset += CP_SYNC_B1_NUM;
        boost::shared_ptr<xusb_tvl> xfpga_tvl = board->find_data(RWA_FPGA_TEMP);
        std::vector<fpga_temp> vct_fpgatmp;
        if(xfpga_tvl != NULL)
        {
            xconvert::parse_fpga_temp(xfpga_tvl->m_value,vct_fpgatmp);
        }
    
        vct_size = vct_fpgatmp.size();
        if(vct_size == 1)
        {
            comb_temp temp;
            temp.m_id = vct_fpgatmp[0].m_id + index_offset;
            temp.m_temp_value = vct_fpgatmp[0].m_temp_value;
            vect_temp.push_back(temp);
        }
    
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpsynctemp() fpga temp vct_size=%d",vct_size);
    
        //CP SYNC MCU PCB温度
        index_offset += CP_SYNC_FPGA_KU060;
        boost::shared_ptr<xusb_tvl> pcbtemp_tvl = board->find_data(RWA_TEMP);
        std::vector<pcb_temp> vct_pcbtmp;
        if(pcbtemp_tvl != NULL)
        {
            xconvert::parse_pcbtemp_data(pcbtemp_tvl->m_value,vct_pcbtmp);
        }
    
        vct_size = vct_pcbtmp.size();
        for(int i = 0; i < vct_size; i++)
        {
            comb_temp temp;
            temp.m_id = vct_pcbtmp[i].m_id + index_offset;
            temp.m_temp_value = vct_pcbtmp[i].m_temp_value;
            vect_temp.push_back(temp);
        }
    
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpsynctemp() pcb temp vct_size=%d",vct_size);
    
        int sz = vect_temp.size();
        if(sz > 0)
        {
            found = true;
            xconvert::serial_comb_temp(vect_temp,cpsync_temp);
        }
    
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpsynctemp() vct_tmp size=%d",sz);
    }
    
    if(found)
    {
        resp.m_value_dev = cpsync_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = cpsync_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG,"xsession::handle_get_value_cpsynctemp() boardid=0x%x port_name=%s",boardid,port_name);
    }
    
    send(m_port_name.c_str(),&resp);
    
    LOG_MSG(MSG_LOG,"Exited xsession::handle_get_value_cpsynctemp() ret=%d",ret);
    
    return ret;
}

int xsession::handle_get_value_cppemtemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_cppemtemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string cpem_temp;

    if(board != NULL)
    {
        std::vector<comb_temp> vect_temp;
        int index_offset = 0;

        //CP PEM FPGA温度
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> fpgatmp_tvl = board->find_data(RWA_FPGA_TEMP);
        std::vector<fpga_temp> vct_fpgatmp;
        if(fpgatmp_tvl != NULL)
        {
            xconvert::parse_fpga_temp(fpgatmp_tvl->m_value,vct_fpgatmp);
        }

        int vct_size = vct_fpgatmp.size();
        for(int i = 0; i < vct_size; i++)
        {
            comb_temp temp;
            temp.m_id = vct_fpgatmp[i].m_id + index_offset;
            temp.m_temp_value = vct_fpgatmp[i].m_temp_value;
            vect_temp.push_back(temp);
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() temp.m_id:%d", temp.m_id);
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() fpga temp vct_size=%d",vct_size);

        //CP PEM ASIC温度
        index_offset += (FPGA_200T_NUM_PEM + FPGA_165_NUM_PEM + FPGA_B080_NUM_PEM);
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> asictemp_tvl = board->find_data(RWA_ASIC_TEMP);
        std::vector<asic_temp> vct_asictmp;
        if(asictemp_tvl != NULL)
        {
            xconvert::parse_asic_temp(asictemp_tvl->m_value,vct_asictmp);
        }

        vct_size = vct_asictmp.size();
        for(int i = 0; i < vct_size; i++)
        {
            comb_temp temp;
            temp.m_id = vct_asictmp[i].m_id + index_offset;
            temp.m_temp_value = vct_asictmp[i].m_temp_value;
            vect_temp.push_back(temp);
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() temp.m_id:%d", temp.m_id);
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() asic temp vct_size=%d",vct_size);

        //CP PEM UTP40温度
        index_offset += CP_ASIC_CHIP_NUM;
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> utp40temp_tvl = board->find_data(RWA_UTP40_TEMP);
        std::vector<utp40_temp> vct_utp40tmp;
        if(utp40temp_tvl != NULL)
        {
            xconvert::parse_utp40_temp(utp40temp_tvl->m_value,vct_utp40tmp);
        }

        vct_size = vct_utp40tmp.size();
        for(int i = 0; i < vct_size; i++)
        {
            comb_temp temp;
            temp.m_id = vct_utp40tmp[i].m_id + index_offset;
            temp.m_temp_value = vct_utp40tmp[i].m_temp_value;
            vect_temp.push_back(temp);
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() temp.m_id:%d", temp.m_id);
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() utp40 temp vct_size=%d",vct_size);

        //CP PEM MCU PCB温度
        index_offset += (UTP40_CHIP_NUM_DPS*4); //这里的偏移理论上是 603
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> pcbtemp_tvl = board->find_data(RWA_TEMP);
        std::vector<pcb_temp> vct_pcbtmp;
        if(pcbtemp_tvl != NULL)
        {
            xconvert::parse_pcbtemp_data(pcbtemp_tvl->m_value,vct_pcbtmp);
        }

        vct_size = vct_pcbtmp.size();
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() pcb temp vct_size=%d",vct_size);
        // 记录温度传感器的数量
        int ts_size = vct_size;
        if(vct_size == (CP_PEM_2_2_TS_NUM + 1) || vct_size == (CP_PEM_4_0_TS_NUM + 1))
        {
            // 此时说明最后一个是 MCU 的温度，需要对 ts_size 减一
            ts_size -= 1;
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() Temperature Sensor temp size=%d",ts_size);
        // 下面这个遍历拿到的一定是温度传感器的温度
        for(int i = 0; i < ts_size; i++)
        {
            comb_temp temp;
            temp.m_id = vct_pcbtmp[i].m_id + index_offset;
            temp.m_temp_value = vct_pcbtmp[i].m_temp_value;
            vect_temp.push_back(temp);
        }

        if(vct_size == (CP_PEM_2_2_TS_NUM + 1) || vct_size == (CP_PEM_4_0_TS_NUM + 1))
        {
            // 此时说明最后一个是 MCU 的温度，需要修改偏移量，加上温度传感器的数量最大值
            index_offset += CP_PEM_4_0_TS_NUM;
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() index_offset:%d", index_offset);
            // 拿到 MCU 的温度
            comb_temp temp;
            // 因为温度传感器后面接着就是MCU的温度，并且该温度只有一个，所以只需要对偏移量加1就是MCU的温度编号
            temp.m_id = index_offset + 1;
            temp.m_temp_value = vct_pcbtmp[vct_size-1].m_temp_value;
            vect_temp.push_back(temp);
        }

        int sz = vect_temp.size();
        if(sz > 0)
        {
            found = true;
            xconvert::serial_comb_temp(vect_temp,cpem_temp);
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cppemtemp() vct_temp size=%d",sz);
    }

    if(found)
    {
        resp.m_value_dev = cpem_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = cpem_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG,"xsession::handle_get_value_cppemtemp() boardid=0x%x port_name=%s",boardid,port_name);
    }

    send(m_port_name.c_str(),&resp);

    LOG_MSG(MSG_LOG,"Exited xsession::handle_get_value_cppemtemp() ret=%d",ret);

    return ret;
}

int xsession::handle_get_value_cpdpstemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_cpdpstemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string cdpds_temp;

    if(board != NULL)
    {
        std::vector<comb_temp> vect_temp;

        //CP DPS FPGA温度
        int index_offset = 0;
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpdpstemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> fpgatmp_tvl = board->find_data(RWA_FPGA_TEMP);
        std::vector<fpga_temp> vct_fpgatmp;
        if(fpgatmp_tvl != NULL)
        {
            xconvert::parse_fpga_temp(fpgatmp_tvl->m_value,vct_fpgatmp);
        }

        int vct_size = vct_fpgatmp.size();
        for(int i = 0; i < vct_size; i++)
        {
            comb_temp temp;
            temp.m_id = vct_fpgatmp[i].m_id + index_offset;
            temp.m_temp_value = vct_fpgatmp[i].m_temp_value;
            vect_temp.push_back(temp);
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpdpstemp() fpga temp vct_size=%d",vct_size);

        //CP DPS utp40温度
        index_offset += CP_DPS_200T_NUM; // 这里的偏移量是从 11 开始，因为 xconvert::parse_pcbtemp_data() 中的温度编号是从 1 开始的
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpdpstemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> utp40temp_tvl = board->find_data(RWA_UTP40_TEMP);
        std::vector<utp40_temp> vct_utp40tmp;
        if(utp40temp_tvl != NULL)
        {
            xconvert::parse_utp40_temp(utp40temp_tvl->m_value,vct_utp40tmp);
        }

        vct_size = vct_utp40tmp.size();
        for(int i = 0; i < vct_size; i++)
        {
            comb_temp temp;
            temp.m_id = vct_utp40tmp[i].m_id + index_offset;
            temp.m_temp_value = vct_utp40tmp[i].m_temp_value;
            vect_temp.push_back(temp);
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpdpstemp() utp40 temp vct_size=%d",vct_size);

        //cdps MCU PCB温度
        index_offset += (UTP40_CHIP_NUM_DPS*4); //这里的偏移理论上是 603
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpdpstemp() index_offset:%d", index_offset);
        boost::shared_ptr<xusb_tvl> pcbtemp_tvl = board->find_data(RWA_TEMP);
        std::vector<pcb_temp> vct_pcbtmp;
        if(pcbtemp_tvl != NULL)
        {
            xconvert::parse_pcbtemp_data(pcbtemp_tvl->m_value,vct_pcbtmp);
        }

        vct_size = vct_pcbtmp.size();
        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpdpstemp() pcb temp vct_size=%d",vct_size);
        // 记录温度传感器的数量
        int ts_size = vct_size;
        if(vct_size == (CP_DPS_2_0_TS_NUM + 1) || vct_size == (CP_DPS_3_3_TS_NUM + 1))
        {
            // 此时说明最后一个是 MCU 的温度，需要对 ts_size 减一
            ts_size -= 1;
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpdpstemp() Temperature Sensor temp size=%d",ts_size);
        // 下面这个遍历拿到的一定是温度传感器的温度
        for(int i = 0; i < ts_size; i++)
        {
            comb_temp temp;
            temp.m_id = vct_pcbtmp[i].m_id + index_offset;
            temp.m_temp_value = vct_pcbtmp[i].m_temp_value;
            vect_temp.push_back(temp);
        }

        if(vct_size == (CP_DPS_2_0_TS_NUM + 1) || vct_size == (CP_DPS_3_3_TS_NUM + 1))
        {
            // 此时说明最后一个是 MCU 的温度，需要修改偏移量，加上温度传感器的数量最大值
            index_offset += CP_DPS_3_3_TS_NUM;
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpdpstemp() index_offset:%d", index_offset);
            // 拿到 MCU 的温度
            comb_temp temp;
            // 因为温度传感器后面接着就是MCU的温度，并且该温度只有一个，所以只需要对偏移量加1就是MCU的温度编号
            temp.m_id = index_offset + 1;
            temp.m_temp_value = vct_pcbtmp[vct_size-1].m_temp_value;
            vect_temp.push_back(temp);
        }

        int sz = vect_temp.size();
        if(sz > 0)
        {
            found = true;
            xconvert::serial_comb_temp(vect_temp,cdpds_temp);
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cpdpstemp() vct_temp size=%d",sz);
    }

    if(found)
    {
        resp.m_value_dev = cdpds_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = cdpds_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG,"xsession::handle_get_value_cpdpstemp() boardid=0x%x port_name=%s",boardid,port_name);
    }

    send(m_port_name.c_str(),&resp);

    LOG_MSG(MSG_LOG,"Exited xsession::handle_get_value_cpdpstemp() ret=%d",ret);

    return ret;
}

int xsession::handle_get_value_cprcatemp(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_cprcatemp()");

    int ret = 0;
    boost::unordered_map<int, boost::shared_ptr<xboard> > map_board;
    map_board = m_board_mgr->get_all_board();
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string cdpds_temp;

    if(board != NULL)
    {
        std::vector<comb_temp> vect_temp;

        int rca_slot = ((boardid & 0xFF00) >> 8);
        //每一次rca板的id偏移以21*4,
        //因为每个rca板上有4块rk3588，每个rk3588有21个温度
        int index_offset = rca_slot*21*4;
        int i = 0;

        std::map<int, boost::shared_ptr<xusb_tvl> > rcafpga_temp_tvl;
        rcafpga_temp_tvl = board->get_rcafpga_temp();

        for(auto it : rcafpga_temp_tvl)
        {
            //获取每个RK3588槽位的数据
            rk3588_fpgatemp rk3588_fpgatmp;
            xconvert::parse_rcafpga_temp(it.second->m_value, rk3588_fpgatmp);
            rk3588_fpgatemp::iterator it2 = rk3588_fpgatmp.find(it.first);
            if(it2 != rk3588_fpgatmp.end())
            {
                for(auto channel : it2->second)
                {
                    //获取每个通道数据
                    for(auto vec_it : channel)
                    {
                        //获取每个通道内温度集合
                        for(auto rcatemp : vec_it.second)
                        {
                            comb_temp temp;
                            temp.m_id = i + index_offset;
                            temp.m_temp_value = rcatemp.m_temp_value;
                            vect_temp.push_back(temp);
                            i++;
                        }
                    }
                }
            }
        }

        int sz = vect_temp.size();
        if(sz > 0)
        {
            found = true;
            xconvert::serial_comb_temp(vect_temp,cdpds_temp);
        }

        LOG_MSG(MSG_LOG,"xsession::handle_get_value_cprcatemp() vct_temp size=%d",sz);
    }

    if(found)
    {
        resp.m_value_dev = cdpds_temp;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = cdpds_temp;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG,"xsession::handle_get_value_cprcatemp() boardid=0x%x port_name=%s",boardid,port_name);
    }

    send(m_port_name.c_str(),&resp);

    LOG_MSG(MSG_LOG,"Exited xsession::handle_get_value_cprcatemp() ret=%d",ret);

    return ret;
}

int xsession::handle_get_value_9528_status(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_9528_status()");

    int ret = 0;

    // 1. 构造基础响应包
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    // 2. 去查找目标单板
    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);

    // 3. 如果单板查找到了，接下来就去查找单板对应的信息
    std::string status;
    if(board != nullptr)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_AD9528_LOCKSTATUS);
        if(tvl != nullptr)
        {
            // 找到了对应的数据
            status = tvl->m_value;
            found = true;
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_9528_status() boardid=0x%x success",boardid);
        }
        else
        {
            // 没有找到对应的数据
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_9528_status() boardid=0x%x failed",boardid);
        }
    }

    if(found)
    {
        resp.m_value_dev = status;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = status;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG,"xsession::handle_get_value_9528_status() boardid=0x%x port_name=%s",boardid,port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG,"Exited xsession::handle_get_value_9528_status() ret=%d",ret);

    return ret;
}

int xsession::handle_get_value_9545_status(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_9545_status()");

    int ret = 0;

    // 1. 构造基础响应包
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    // 2. 去查找目标单板
    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);

    // 3. 如果单板查找到了，接下来就去查找单板对应的信息
    std::string status;
    if(board != nullptr)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_AD9545_LOCKSTATUS);
        if(tvl != nullptr)
        {
            // 找到了对应的数据
            status = tvl->m_value;
            found = true;
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_9545_status() boardid=0x%x success",boardid);
        }
        else
        {
            // 没有找到对应的数据
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_9545_status() boardid=0x%x failed",boardid);
        }
    }

    if(found)
    {
        resp.m_value_dev = status;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = status;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG,"xsession::handle_get_value_9545_status() boardid=0x%x port_name=%s",boardid,port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG,"Exited xsession::handle_get_value_9545_status() ret=%d",ret);

    return ret;
}

int xsession::handle_get_value_pwm_frequency(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_frequency()");

    int ret = 0;

    // 1. 构造基础响应包
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    // 2. 去查找目标单板
    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);

    // 3. 如果单板查找到了，接下来就去查找单板对应的信息
    std::string pwm_val;
    if(board != nullptr)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_PWM_CHECK);
        if(tvl != nullptr)
        {
            // 找到了对应的数据
            pwm_val = tvl->m_value;
            found = true;
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_frequency() boardid=0x%x success",boardid);
        }
        else
        {
            // 没有找到对应的数据
            LOG_MSG(WRN_LOG,"xsession::handle_get_value_frequency() boardid=0x%x failed",boardid);
        }
    }

    if(found)
    {
        resp.m_value_dev = pwm_val;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = pwm_val;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG,"xsession::handle_get_value_frequency() boardid=0x%x port_name=%s",boardid,port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG,"Exited xsession::handle_get_value_frequency() ret=%d",ret);

    return ret;
}

int xsession::handle_get_value_serdes_status(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_serdes_status()");

    int ret = 0;

    // 1. 构造基础响应包
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    // 2. 去查找目标单板
    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);

    // 3. 如果单板查找到了，接下来就去查找单板对应的信息
    std::string serdes_val;
    if(board != nullptr)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_SERDES_STATUS);
        if(tvl != nullptr)
        {
            // 找到了对应的数据
            serdes_val = tvl->m_value;
            found = true;
            LOG_MSG(MSG_LOG,"xsession::handle_get_value_serdes_status() boardid=0x%x success",boardid);
        }
        else
        {
            // 没有找到对应的数据
            LOG_MSG(ERR_LOG,"xsession::handle_get_value_serdes_status() boardid=0x%x failed",boardid);
        }
    }

    if(found)
    {
        resp.m_value_dev = serdes_val;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        ret = -1;
        resp.m_value_dev = serdes_val;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG,"xsession::handle_get_value_serdes_status() boardid=0x%x port_name=%s",boardid,port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG,"Exited xsession::handle_get_value_serdes_status() ret=%d",ret);

    return ret;
}

// 获取目标单板上所有utp芯片的寄存器信息
int xsession::handle_get_value_utp40_register(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_utp40_register()");

    int ret = 0;

    // 1. 构造基础响应包
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    // 2. 去查找目标单板
    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);

    // 3. 如果单板查找到了，接下来就去查找单板对应的信息
    std::string utp_register_val;
    if (board != nullptr)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_UTP40_REGISTER);
        if (tvl != nullptr)
        {
            // 找到了对应的数据
            utp_register_val = tvl->m_value;
            found = true;
            LOG_MSG(MSG_LOG, "xsession::handle_get_value_utp40_register() boardid=0x%x success", boardid);
        }
        else
        {
            // 没有找到对应的数据
            LOG_MSG(WRN_LOG, "xsession::handle_get_value_utp40_register() boardid=0x%x failed", boardid);
        }
    }

    if (found)
    {
        resp.m_value_dev = utp_register_val;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        // 没找到说明没有从驱动获取到
        ret = -1001;
        resp.m_value_dev = utp_register_val;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_utp40_register() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_utp40_register()");
    return ret;
}

int xsession::handle_get_value_utp102_register(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_value_utp102_register()");

    int ret = 0;

    // 1. 构造基础响应包
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    // 2. 去查找目标单板
    bool found = false;
    int boardid = pack->m_usb_dst;
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);

    // 3. 如果单板查找到了，接下来就去查找单板对应的信息
    std::string utp_register_val;
    if (board != nullptr)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_UTP102_REGISTER);
        if (tvl != nullptr)
        {
            // 找到了对应的数据
            utp_register_val = tvl->m_value;
            found = true;
            LOG_MSG(MSG_LOG, "xsession::handle_get_value_utp102_register() boardid=0x%x success", boardid);
        }
        else
        {
            // 没有找到对应的数据
            LOG_MSG(WRN_LOG, "xsession::handle_get_value_utp102_register() boardid=0x%x failed", boardid);
        }
    }

    if (found)
    {
        resp.m_value_dev = utp_register_val;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        // 没找到说明没有从驱动获取到
        ret = -1001;
        resp.m_value_dev = utp_register_val;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_value_utp102_register() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_value_utp102_register()");
    return ret;
}

// 整板电源控制(不包括MCU本身)
int xsession::handle_command_board_psctrl(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_command_board_psctrl()");

    int ret = 0;

    int boardid = pack->m_usb_dst;
    int sw_val = pack->m_command_value.m_switch;

    ret = m_mgr_usb->exec_board_power_off(boardid, CMD_BOARD_POWER_SWITCH, sw_val);

    // 返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::handle_command_board_psctrl() boardid=0x%x sw_val=%d failed,ret=%d", boardid, sw_val, ret);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_command_board_psctrl() boardid=0x%x sw_val=%d ret=%d", boardid, sw_val, ret);
    return ret;
}

// FPGA电源控制(1S7250/AGFB019)
int xsession::handle_command_fpga_psctrl(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_command_fpga_psctrl()");

    int ret = 0;

    int boardid = pack->m_usb_dst;
    int sw_val = pack->m_command_value.m_switch;

    ret = m_mgr_usb->exec_board_power_off(boardid, CMD_FPGA_POWER_SWITCH, sw_val);

    // 返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::handle_command_fpga_psctrl() boardid=0x%x sw_val=%d failed,ret=%d", boardid, sw_val, ret);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_command_fpga_psctrl() boardid=0x%x sw_val=%d ret=%d", boardid, sw_val, ret);
    return ret;
}

// UTP40单元电源控制
int xsession::handle_command_utp40_psctrl(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_command_utp40_psctrl()");

    int ret = 0;

    int boardid = pack->m_usb_dst;
    int utp40_id = pack->m_command_value.m_utp40_id;
    int sw_val = pack->m_command_value.m_switch;

    ret = m_mgr_usb->exec_board_power_off(boardid, CMD_UTP40_POWER_SWITCH, utp40_id, sw_val);

    // 返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::handle_command_utp40_psctrl() boardid=0x%x utp40_id=%d sw_val=%d failed,ret=%d", boardid, utp40_id, sw_val, ret);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_command_utp40_psctrl() boardid=0x%x utp40_id=%d sw_val=%d ret=%d", boardid, utp40_id, sw_val, ret);
    return ret;
}

// ASIC芯片电源控制
int xsession::handle_command_asic_psctrl(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_command_asic_psctrl()");

    int ret = 0;

    int boardid = pack->m_usb_dst;
    int sw_val = pack->m_command_value.m_switch;

    ret = m_mgr_usb->exec_board_power_off(boardid, CMD_ASIC_POWER_SWITCH, sw_val);

    // 返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::handle_command_asic_psctrl() boardid=0x%x sw_val=%d failed,ret=%d", boardid, sw_val, ret);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_command_asic_psctrl() boardid=0x%x sw_val=%d ret=%d", boardid, sw_val, ret);
    return ret;
}

// FPGA电源控制(7A 200T)
int xsession::handle_command_fpga200t_psctrl(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_command_fpga200t_psctrl()");

    int ret = 0;

    int boardid = pack->m_usb_dst;
    int sw_val = pack->m_command_value.m_switch;

    ret = m_mgr_usb->exec_board_power_off(boardid, CMD_FPGA200T_POWER_SWITCH, sw_val);

    // 返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::handle_command_fpga200t_psctrl() boardid=0x%x sw_val=%d failed,ret=%d", boardid, sw_val, ret);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_command_fpga200t_psctrl() boardid=0x%x sw_val=%d ret=%d", boardid, sw_val, ret);
    return ret;
}

// FPGA电源控制(KU060)
int xsession::handle_command_fpgaku060_psctrl(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_command_fpgaku060_psctrl()");

    int ret = 0;

    int boardid = pack->m_usb_dst;
    int sw_val = pack->m_command_value.m_switch;

    ret = m_mgr_usb->exec_board_power_off(boardid, CMD_KU060_FPGA_POWER_SWITCH, sw_val);

    // 返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::handle_command_fpgaku060_psctrl() boardid=0x%x sw_val=%d failed,ret=%d", boardid, sw_val, ret);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_command_fpgaku060_psctrl() boardid=0x%x sw_val=%d ret=%d", boardid, sw_val, ret);
    return ret;
}

// UTP40总电源控制
int xsession::handle_command_utp40_master_psctrl(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_command_utp40_master_psctrl()");

    int ret = 0;

    int boardid = pack->m_usb_dst;
    int sw_val = pack->m_command_value.m_switch;

    ret = m_mgr_usb->exec_board_power_off(boardid, CMD_UTP40_POWER_MASTERSWITCH, sw_val);

    // 返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::handle_command_utp40_master_psctrl() boardid=0x%x sw_val=%d failed,ret=%d", boardid, sw_val, ret);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_command_utp40_master_psctrl() boardid=0x%x sw_val=%d ret=%d", boardid, sw_val, ret);
    return ret;
}

int xsession::handle_frequency_set_request(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_frequency_set_request()");
    int ret = 0;

    int command = pack->m_frequency_type;
    LOG_MSG(MSG_LOG, "xsession::handle_frequency_set_request() cmd:%d", command);
    switch (command)
    {
    case FREQUENCY_4356_CONTROL:
        ret = handle_frequency_set_4356_ctrl(port_name, pack);
        break;
    }
    return ret;
    LOG_MSG(MSG_LOG, "Exited xsession::handle_frequency_set_request()");
}

int xsession::handle_frequency_set_4356_ctrl(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_frequency_set_4356_ctrl()");

    int ret = 0;

    int boardid = pack->m_usb_dst;
    float fre_val = pack->m_frequency_value.m_frequency;
    int channel = pack->m_frequency_value.m_channel;

    ret = m_mgr_usb->exec_frequency_setting(boardid, CMD_4356_FREQUENCY_SET, fre_val, channel);

    // 返回结果
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    if (ret != 0) {
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(WRN_LOG, "xsession::handle_frequency_set_4356_ctrl() boardid=0x%x fre_val=%f channel=%d failed,ret=%d", boardid, fre_val, channel, ret);
    } else {
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }

    send(port_name, &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_frequency_set_4356_ctrl() boardid=0x%x fre_val=%f channel=%d ret=%d", boardid, fre_val, channel, ret);
    return ret;
}

// 获取目标单板上某一个utp芯片的寄存器信息
int xsession::handle_get_utp_register_request(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_utp_register_request()");

    int ret = 0;

    // 1. 构造基础响应包
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    // 2. 去查找目标单板
    bool found = false;
    int boardid = pack->m_usb_dst;
    int chip_id = pack->m_utp_chip_id;
    LOG_MSG(MSG_LOG, "xsession::handle_get_utp_register_request() boardid=0x%x chip_id:%d", boardid, chip_id);
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string register_val;

    // 3. 如果单板查找到了，接下来就去查找单板对应的信息
    if (board != nullptr)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_UTP40_REGISTER);
        if (tvl != nullptr)
        {
            // 找到了对应的数据
            // 找对应 UTP 芯片的寄存器信息
            xconvert::find_one_utp_register(tvl->m_value, chip_id, register_val);
            if (!register_val.empty())
            {
                found = true;
                LOG_MSG(MSG_LOG, "xsession::handle_get_utp_register_request() boardid=0x%x success", boardid);
            }
        }
        else
        {
            // 没有找到对应的数据
            LOG_MSG(WRN_LOG, "xsession::handle_get_utp_register_request() boardid=0x%x failed", boardid);
        }
    }

    if (found)
    {
        resp.m_value_dev = register_val;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        // 没找到说明没有从驱动获取到
        ret = -1001;
        resp.m_value_dev = register_val;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_utp_register_request() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_utp_register_request()");
    return ret;
}

int xsession::handle_get_utp102_register_request(const char *port_name, xusbadapter_package *pack)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::handle_get_utp102_register_request()");

    int ret = 0;

    // 1. 构造基础响应包
    xusbadapter_package resp(xusbadapter_package::MT_RESPOND, pack->m_msg_cmd, pack->m_version, pack->m_session);

    // 2. 去查找目标单板
    bool found = false;
    int boardid = pack->m_usb_dst;
    int chip_id = pack->m_utp_chip_id;
    LOG_MSG(MSG_LOG, "xsession::handle_get_utp102_register_request() boardid=0x%x chip_id:%d", boardid, chip_id);
    boost::shared_ptr<xboard> board = m_board_mgr->find_board(boardid);
    std::string register_val;

    // 3. 如果单板查找到了，接下来就去查找单板对应的信息
    if (board != nullptr)
    {
        boost::shared_ptr<xusb_tvl> tvl = board->find_data(RWA_UTP102_REGISTER);
        if (tvl != nullptr)
        {
            // 找到了对应的数据
            // 找对应 UTP 芯片的寄存器信息
            xconvert::find_one_utp102_register(tvl->m_value, chip_id, register_val);
            if (!register_val.empty())
            {
                found = true;
                LOG_MSG(MSG_LOG, "xsession::handle_get_utp102_register_request() boardid=0x%x success", boardid);
            }
        }
        else
        {
            // 没有找到对应的数据
            LOG_MSG(WRN_LOG, "xsession::handle_get_utp102_register_request() boardid=0x%x failed", boardid);
        }
    }

    if (found)
    {
        resp.m_value_dev = register_val;
        resp.m_err_code = xusbadapter_package::ME_SUCCESS;
    }
    else
    {
        // 没找到说明没有从驱动获取到
        ret = -1001;
        resp.m_value_dev = register_val;
        resp.m_err_code = xusbadapter_package::ME_FAILED;
        LOG_MSG(ERR_LOG, "xsession::handle_get_utp102_register_request() boardid=0x%x port_name=%s", boardid, port_name);
    }

    send(m_port_name.c_str(), &resp);

    LOG_MSG(MSG_LOG, "Exited xsession::handle_get_utp102_register_request()");
    return ret;
}

bool xsession::is_ota_caltype(int ota_type)
{
    bool ret = false;
    if ((ota_type > OTA_TYPE_CPPDEMUTP40_MW_CAL) && (ota_type <= OTA_TYPE_FTFEB_RE_DC_CAL))
    {
        ret = true;
    }
    return ret;
}

void xsession::file_bak(const std::string &file_path)
{
    LOG_MSG(MSG_LOG, "Enter into xsession::file_bak() file_path:%s", file_path.c_str());

    struct stat buffer;
    if (stat(file_path.c_str(), &buffer) == 0)
    {
        // 得到当前时间戳
        time_t currt;
        struct tm curr_tm;

        time(&currt);
        localtime_r(&currt, &curr_tm);
        char occur_time[30];
        sprintf(occur_time, "%4d%02d%02d%02d%02d%02d",
                curr_tm.tm_year + 1900, curr_tm.tm_mon + 1, curr_tm.tm_mday,
                curr_tm.tm_hour, curr_tm.tm_min, curr_tm.tm_sec);

        // 重命名旧的文件名
        char szNewName[MAX_FILE_NAME_LEN + 1];
        sprintf(szNewName, "%s%s", file_path.c_str(), occur_time);
        rename(file_path.c_str(), szNewName);
    }

    LOG_MSG(MSG_LOG, "Exited xsession::file_bak()");
}
