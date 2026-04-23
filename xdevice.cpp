#include "xdevice.h"
#include "mgr_lib_driver.h"
#include "mgr_sitetask.h"
#include <boost/shared_array.hpp>

#if defined(SYNC_BUILD)

xmboard::xmboard(int id,std::string port_name) : xtransmitter("<board>")
{
    m_id = id;
    m_port_name = port_name;
    m_activetm = time(NULL);
    m_upgrad_mgr = mgr_upgrade::get_instance();
    m_device_mgr = mgr_device::get_instance();
    m_board_mgr = xboardmgr::get_instance();
}

xmboard::~xmboard()
{
    m_upgrad_mgr = NULL;
    m_device_mgr = NULL;
    m_board_mgr = NULL;
}

//消息通知
int xmboard::on_recv(const char *port_name,xusbpackage *pack)
{
    if(strcmp(port_name,PORT_TH_MONITOR) == 0 || strcmp(port_name,PORT_MF_MONITOR) == 0 )
    {
        set_activetm(time(NULL));
        //南向消息处理
        handle_south_recv(pack);
    }
    return xtransmitter::on_recv(port_name,pack);
}

//南向消息处理
int xmboard::handle_south_recv(xusbpackage *pack)
{
    switch(pack->m_msg_cmd & USB_MESSAGE_MASK)
    {
        case USB_REQUEST_MESSAGE:
            handle_south_request(pack);
            break;
        case USB_RESPONE_MESSAGE:
            handle_south_response(pack);
            break;
        case USB_NOTIFY_MESSAGE:
            handle_south_notify(pack);
            break;
    }

    return 0;
}

//南向请求消息处理
int xmboard::handle_south_request(xusbpackage *pack)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::handle_south_request()");
    xusbpackage *the_pack = static_cast<xusbpackage *>(pack);
    if(!the_pack->crc_check())
    {
        LOG_MSG(WRN_LOG,"xmboard::handle_south_request(),message msg_cmd=%d crc check failed!",the_pack->m_msg_cmd);
        return -1;
    }

    if ((the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_OTA)
    {
        //OTA命令处理
        int ret = handle_ota_request_message(the_pack);
        if(ret != 0) {
            LOG_MSG(WRN_LOG,"xmboard::handle_south_request(),handle_ota_request_message msg_cmd=%d failed!",the_pack->m_msg_cmd);
            return -1;
        }
    }

    if ((the_pack->m_msg_cmd & MSG_CMD_MASK)== xusbpackage::MC_ALARM)
    {
        //ALarm命令处理
        int ret = handle_alarm_request_message(the_pack);
        if(ret != 0) {
            LOG_MSG(WRN_LOG,"xmboard::handle_south_request(),handle_alarm_request_message msg_cmd=%d failed!",the_pack->m_msg_cmd);
            return -1;
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::handle_south_request()");

    return 0;
}

int xmboard::handle_ota_request_message(xusbpackage *the_pack)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::handle_ota_request_message()");
    if(m_ota_session == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::handle_ota_request_message(),m_srcid = %d ota session not existed!",the_pack->m_srcid);
        return -1;
    }
    //判断当前状态机是否在ota升级逻辑的状机中,为了控制ota升级与cal校准文件读取流程是互斥进行的
    int status = m_ota_session->get_session_status();
    if(is_ota_session_status(status) == false)
    {
        LOG_MSG(WRN_LOG,"xmboard::handle_ota_request_message(),current status(%d) is not in ota session status range",status);
        return -1;
    }

    //OTA命令处理
    if((the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_OTA)
    {
        LOG_MSG(MSG_LOG,"xmboard::handle_ota_request_message( receive OTA message)");
        boost::shared_ptr<xusb_tvl> ota_data_tlv = the_pack->find_data(FIRMWARE_DATA);
        if(ota_data_tlv.get() != NULL)
        {
            //发送固件数据
            LOG_MSG(MSG_LOG,"xmboard::handle_ota_request_message() send OTA firmware data message");
            send_ota_data(the_pack->m_session,the_pack->m_srcid,ota_data_tlv);
        }
        //LOG_MSG(MSG_LOG,"mgr_usb::handle_ota_request_message( debug:");
        boost::shared_ptr<xusb_tvl> ota_completed_tlv = the_pack->find_data(OTA_COMPLETE);
        if(ota_completed_tlv.get() != NULL)
        {
            //是否需要发送应答给usb
            //暂时不需要响应这个请求的应答
            LOG_MSG(MSG_LOG,"xmboard::handle_ota_request_message( m_srcid=%d receive OTA firmware data complete message",the_pack->m_srcid);
            int ret = set_ota_transed_length(m_ota_session->get_ota_total_len());
            if(ret != 0) {
                LOG_MSG(WRN_LOG,"xmboard::send_ota_data() set ota transed length failed!");
            }
            set_ota_session_status(OTA_SESSION_STATUS_COMPLETED);
            xtaskprogress* xtask_progress = xtaskprogress::get_instance();
            xtask_progress->set_completestatus(the_pack->m_srcid);
            xtask_progress->set_taskresult(the_pack->m_srcid,xtaskprogress::TASKRESUL_SUCCESS);
        }
    }
    LOG_MSG(MSG_LOG,"Exited xmboard::handle_ota_request_message()");
    return 0;
}

int xmboard::send_ota_start(int dstid,int ota_type)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::send_ota_start() dstid=0x%X ota_type=%d",dstid,ota_type);
    int status = m_device_mgr->get_serial_keepalived(dstid);
    if(status != HEARTBEAT_ALIVED) {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_start(),dstid = %d usb heartbeat not alived!",dstid);
        return -1;
    }

    if(m_ota_session != NULL)
    {
        //再次检查ota会话的状态,如果会话已经结束删除旧的会话
        int status = m_ota_session->get_session_status();
        if( (status == OTA_SESSION_STATUS_COMPLETED) || (status == OTA_SESSION_STATUS_END) || (status == OTA_SESSION_STATUS_CANCLED) || (status == OTA_SESSION_STATUS_CALFILE_END))
        {
            LOG_MSG(WRN_LOG,"xmboard::send_ota_start(),dstid=0x%X ota_type=%d ota session/cal session have exited,status=%d",dstid,ota_type,status);
            del_ota_session();
        }
        else
        {
            LOG_MSG(ERR_LOG,"xmboard::send_ota_start(),dstid=0x%X ota_type=%d ota session/cal session have exited,status=%d",dstid,ota_type,status);
            return -1;
        }
    }

    std::string fw_typestr = xmboard::get_ota_fw_name(ota_type);
    boost::shared_ptr<xotafile> otafile = m_upgrad_mgr->find_otafile(fw_typestr);
    if(otafile.get() == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_start() find %s type ota file failure",fw_typestr.c_str());
        return -1;
    }

    xusbpackage req_pack(xusbpackage::MC_OTA,PROTO_VERSION,0,xconfig::self_id(),dstid);
    unsigned char value[7] = {0};
    //固件类型
    unsigned char fw_type = ota_type;
    //固件版本
    unsigned short fw_version = otafile->m_ver_int;
    //固件总长度
    unsigned int fw_length = otafile->m_size;
    //文件检验md5
    unsigned char file_md5[16] = {0};
    xbasic::str_to_hex(&otafile->m_md5[0],&file_md5[0],otafile->m_md5.length());

    value[0] = fw_type;
    xbasic::write_bigendian(&value[1], fw_version, 2);
    xbasic::write_bigendian(&value[3], fw_length, 4);
    std::string type_value((char*)&value[0],7);
    type_value.append(std::string((char*)file_md5,16));
    req_pack.add_data(OTA_START,type_value,xusbpackage::MC_OTA);
    int len = send(m_port_name.c_str(),(xpacket*)&req_pack);
    add_ota_session(OTA_SESSION_STATUS_STARTED,ota_type,fw_version,fw_length);
    if(len > 0)
    {
        //std::string ota_verstr = xbasic::version_from_int(otafile->m_ver_int);
        LOG_MSG(WRN_LOG,"xmboard::send_ota_start() dstid=0x%X ota_type=%d version(%lld:0x%X)",dstid,ota_type,otafile->m_ver_int,otafile->m_ver_int);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::send_ota_start()");

    return 0;
}

int xmboard::send_ota_cancle(int dstid,int ota_type)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::send_ota_cancle() dstid=%d ota_type = %d",dstid,ota_type);
    int serial_status = m_device_mgr->get_serial_keepalived(dstid);
    if(serial_status != HEARTBEAT_ALIVED) {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_cancle(),dstid = %d usb heartbeat not alived!",dstid);
        return -1;
    }

    if(m_ota_session == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_cancle(),dstid = %d ota session not existed!",dstid);
        return -1;
    }
    //判断当前状态机是否在ota升级逻辑的状机中,为了控制ota升级与cal校准文件读取流程是互斥进行的
    int ota_status = m_ota_session->get_session_status();
    if(is_ota_session_status(ota_status) == false)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_cancle(),current status(%d) is not in ota session status range",ota_status);
        return -1;
    }

    std::string fw_typestr = xmboard::get_ota_fw_name(ota_type);
    boost::shared_ptr<xotafile> otafile = m_upgrad_mgr->find_otafile(fw_typestr);
    if(otafile.get() == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_start() find %s type ota file failure",fw_typestr.c_str());
        return -1;
    }

    xusbpackage req_pack(xusbpackage::MC_OTA,PROTO_VERSION,0,xconfig::self_id(),dstid);
    unsigned char value[3] = {0};
    //固件类型
    unsigned char fw_type = ota_type;
    //固件版本
    unsigned short fw_version = otafile->m_ver_int;

    value[0] = fw_type;
    xbasic::write_bigendian(&value[1], fw_version, 2);
    std::string type_value((char*)&value[0],3);
    req_pack.add_data(OTA_CANCEl,type_value,xusbpackage::MC_OTA);
    send(m_port_name.c_str(),(xpacket*)&req_pack);
    set_ota_session_status(OTA_SESSION_STATUS_CANCLED);
    LOG_MSG(MSG_LOG,"Exited xmboard::send_ota_cancle()");
    return 0;
}

void xmboard::send_ota_data(int req_sessionid,int dstid,boost::shared_ptr<xusb_tvl>& req_ota_data_tlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::send_ota_data() dstid=%d",dstid);
    std::string tlv_value = req_ota_data_tlv->get_data();
    unsigned char ota_type = tlv_value[0];
    unsigned int req_data_offset = 0;
    req_data_offset=xbasic::read_bigendian(&tlv_value[1], 4);
    unsigned short req_data_len = 0;
    req_data_len=xbasic::read_bigendian(&tlv_value[5], 2);

    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() req_data_offset=%u request_data_len=%hu",req_data_offset,req_data_len);

    if(req_data_len > MAX_FILE_TRANSFER_LEN)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() request data len(%u) > max file transfer len(%d)",req_data_len,MAX_FILE_TRANSFER_LEN);
        req_data_len = MAX_FILE_TRANSFER_LEN;
    }

    //从OTA文件中在指定的offset开始读取req_data_len长度
    boost::shared_array<char> pdata(new char[req_data_len]);
    if(pdata.get() == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() allocate ota buffer memory failed!");
        return ;
    }

    std::string fw_typestr=xmboard::get_ota_fw_name(ota_type);
    boost::shared_ptr<xotafile> otafile = m_upgrad_mgr->find_otafile(fw_typestr);
    //otafile类成员m_path保存是ota文件完整路径
    if(otafile == NULL)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() not found ota type=%d,ota_file=%s",ota_type,fw_typestr.c_str());
        return;
    }
    //LOG_MSG(MSG_LOG,"mgr_usb::send_ota_data() debug0");
    std::string ota_filename = otafile->m_path;
    int ret = read_otadata_from_file(ota_filename,req_data_offset,req_data_len,pdata.get());
    if(ret != 0)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() read ota data req_data_offset=%u request_data_len=%hu from %s file failure",req_data_offset,req_data_len,ota_filename.c_str());
        return ;
    }

    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() debug1");
    //封装OTA_DATA请求的回复包
    xusbpackage resp_pack(xusbpackage::MC_OTA|0x80,PROTO_VERSION,req_sessionid,xconfig::self_id(),dstid);
    unsigned char value[7] = {0};
    //固件类型
    unsigned char fw_type = ota_type;
    //数据偏移
    unsigned int data_offset = req_data_offset;
    //本次传输的长度
    unsigned short transfer_len = req_data_len;

    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() debug2");
    value[0] = fw_type;
    xbasic::write_bigendian(&value[1], data_offset, 4);
    xbasic::write_bigendian(&value[5], transfer_len, 2);
    std::string type_valum((char*)&value[0],7);
    type_valum.append(std::string((char*)pdata.get(),(transfer_len)));
    resp_pack.add_data(FIRMWARE_DATA,type_valum,xusbpackage::MC_OTA);
    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() debug3");
    ret = set_ota_session_status(OTA_SESSION_STATUS_UPGRADING);
    if(ret != 0)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() set ota session status failed!");
        return ;
    }
    //以请求偏移长度为传输成功的长度更准确
    ret = set_ota_transed_length(req_data_offset);
    if(ret != 0)
    {
        LOG_MSG(WRN_LOG,"xmboard::send_ota_data() set ota transed length failed!");
        return ;
    }
    LOG_MSG(MSG_LOG,"xmboard::send_ota_data() debug4");
    send(m_port_name.c_str(),(xpacket*)&resp_pack);

    LOG_MSG(MSG_LOG,"Exited xmboard::send_ota_data()");
}

void xmboard::send_heartbeat()
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::send_heartbeat()");

    xusbpackage pack(xusbpackage::MC_HEARTBEAT,PROTO_VERSION,0,xconfig::self_id(),0); //构造心跳请求包

    int len = send(m_port_name.c_str(),(xpacket*)&pack);

    LOG_MSG(MSG_LOG,"Exited xmboard::send_heartbeat(len=%d)",len);
}

void xmboard::check_ota_status()
{
    if(m_ota_session == NULL)
    {
        LOG_MSG(ERR_LOG,"xmboard::check_ota_status() ota session is not existed boardid=%d",m_id);
        return;
    }

    int status = m_ota_session->get_session_status();
    if(status == OTA_SESSION_STATUS_COMPLETED)
    {
        set_ota_session_status(OTA_SESSION_STATUS_END);
    }
}

int xmboard::read_otadata_from_file(const std::string filename, const unsigned int offset,const unsigned int len,char* read_buff)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::read_otadata_from_file() filename=%s offset=%d len=%d",filename.c_str(),offset,len);

    FILE *fp = fopen(filename.c_str(),"rb");
    if(!fp)
    {
        LOG_MSG(WRN_LOG,"xmboard::read_otadata_from_file() open %s file failure",filename.c_str());
        return -1;
    }

    if(len == 0)
    {
        LOG_MSG(MSG_LOG,"xmboard::read_otadata_from_file() filename=%s offset=%d len=%d,len is invalid!",filename.c_str(),offset,len);
        return -1;
    }

    fseek(fp,0L,SEEK_END);             //定位到文件末尾
    unsigned int file_len = ftell(fp);  //得到文件大小
    fseek(fp,offset,SEEK_SET);          //定位到文件开头

    if(offset+len > file_len)
    {
        LOG_MSG(WRN_LOG,"xmboard::read_otadata_from_file() read len(%u) byte from file offset(%u),exceed file total length(%u)",len,offset,file_len);
        return -1;
    }

    unsigned int read_len = 0;
    unsigned int index = len/1024;
    unsigned int reminder = len%1024;

    //以1024字节长度读取
    if(index > 0)
    {
        do
        {
            int ret = 0;
            ret = fread(read_buff+read_len,1,1024,fp);
            read_len += ret;
        } while(read_len < (index*1024));
    }

    //不足1024长度读取
    if(reminder > 0)
    {
        int pos = index*1024;
        fread(read_buff+pos,1,reminder,fp);
    }

    fclose(fp);
    fp = NULL;

    LOG_MSG(MSG_LOG,"xmboard::read_otadata_from_file() debug2 index=%d read_len=%d reminder=%d",index, read_len, reminder);
    LOG_MSG(MSG_LOG,"Exited xmboard::read_otadata_from_file()");

    return 0;
}

//将安全策略的tid转换上位机相关tid
int xmboard::convert_securitypolicy(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_securitypolicy()");
    int ret = 0;

#ifndef TEST

    switch(src_xusbtlv->m_tid)
    {
        case RMA_BOX_OUTLET_TEMP_SET:
        case RMA_BOX_48V_TEMP_SET:
        case RMA THCABLE_BOX_TEMP_SET:
        case RMA_HF_BOX_TEMP_SET:
        case RMA_BOX_INLET_TEMP_SET:
        case RMA_MF_TOP_OUTLET_TEMP_SET:
        case RMA_48V_PS_OUTPUT_TEMP_SET:
        case RMA_UPPER_HF_TEMP_SET:
        case RMA_LOWER_HF_TEMP_SET:
            ret = convert_temppolicy(dst_list,src_xusbtlv);
            break;
        case RMA_BOX_OUTLET_SMOKE_SET:
        case RMA_48V_SMOKE_SET:
        case RMA_HFBOX_SMOKE_SET:
        case RMA_TOP_OUTLET_SMOKE_SET:
        case RMA_UPPER_HF_SMOKE_SET:
        case RMA_LOWER_HF_SMOKE_SET:
            ret = convert_smokepolicy(dst_list,src_xusbtlv);
            break;
        case RMA_THBOX_FAN_SET:
        case RMA THCABLE_FAN_SET:
        case RMA_MF_TOP_FAN_SET:
        case RMA_UPPER_HF_FAN_SET:
        case RMA_LOWER_HF_FAN_SET:
            ret = convert_fanpolicy(dst_list,src_xusbtlv);
            break;
        default:
            ret = -2;
            break;
    }

#else

    //ret = convert_securitypolicy_test(dst_list);

#endif

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_securitypolicy() ret=%d",ret);
    return ret;
}

//将安全策略的tid转换上位机相关tid模拟测试函数
int xmboard::convert_securitypolicy_test(std::list<boost::shared_ptr<xtvl> > & dst_list)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_securitypolicy_test()");

    int ret = 0;
    //模拟温度策略数据
    std::vector<temp_range> vect_range;
    temp_range temp1;
    temp1.key = 1;
    temp1.lower_range = 1.0;
    temp1.upper_range = 2.0;

    temp_range temp2;
    temp2.key = 2;
    temp2.lower_range = 3.0;
    temp2.upper_range = 4.0;

    temp_range temp3;
    temp3.key = 3;
    temp3.lower_range = 5.0;
    temp3.upper_range = 6.0;

    vect_range.push_back(temp1);
    vect_range.push_back(temp2);
    vect_range.push_back(temp3);
    int vect_size = vect_range.size();

    // TH板卡腔体出风口温度范围下限值
    // TH板卡腔体出风口温度范围上限值
    for(int i = 0 ; i < vect_size; i++)
    {
        unsigned short lower_tid;
        unsigned short upper_tid;

        lower_tid = TH_OUT_LOW_TMP_VALUE;
        upper_tid = TH_OUT_HIGH_TMP_VALUE;

        std::string lower_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].lower_range);
        std::string upper_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].upper_range);

        boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
        boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

        dst_list.push_back(new_low_data);
        dst_list.push_back(new_upper_data);
    }

    // TH-48V汇流排温度范围下限值
    // TH-48V汇流排温度范围上限值
    for(int i = 0 ; i < vect_size; i++)
    {
        unsigned short lower_tid;
        unsigned short upper_tid;

        lower_tid = TH_48V_LOW_TMP_VALUE;
        upper_tid = TH_48V_HIGH_TMP_VALUE;

        std::string lower_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].lower_range);
        std::string upper_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].upper_range);

        boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
        boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

        dst_list.push_back(new_low_data);
        dst_list.push_back(new_upper_data);
    }

    for(int i = 0 ; i < vect_size; i++)
    {
        unsigned short lower_tid;
        unsigned short upper_tid;

        lower_tid = TH_CABLE_BOX_LOW_TMP_VALUE;
        upper_tid = TH_CABLE_BOX_HIGH_TMP_VALUE;

        std::string lower_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].lower_range);
        std::string upper_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].upper_range);

        boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
        boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

        dst_list.push_back(new_low_data);
        dst_list.push_back(new_upper_data);
    }

    // HF腔体温度范围下限
    // HF腔体温度范围上限
    for(int i = 0 ; i < vect_size; i++)
    {
        unsigned short lower_tid;
        unsigned short upper_tid;

        lower_tid = HF_BOX_LOW_TMP_VALUE;
        upper_tid = HF_BOX_HIGH_TMP_VALUE;

        std::string lower_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].lower_range);
        std::string upper_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].upper_range);

        boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
        boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

        dst_list.push_back(new_low_data);
        dst_list.push_back(new_upper_data);
    }

    // TH板卡腔体进风口温度下限值
    // TH板卡腔体进风口温度上限值
    for(int i = 0 ; i < vect_size; i++)
    {
        unsigned short lower_tid;
        unsigned short upper_tid;

        lower_tid = TH_IN_BOX_LOW_TMP_VALUE;
        upper_tid = TH_IN_BOX_HIGH_TMP_VALUE;

        std::string lower_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].lower_range);
        std::string upper_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].upper_range);

        boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
        boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

        dst_list.push_back(new_low_data);
        dst_list.push_back(new_upper_data);
    }

    // MF板卡顶部出风口温度范围下限值
    // MF板卡顶部出风口温度范围上限值
    for(int i = 0 ; i < vect_size; i++)
    {
        unsigned short lower_tid;
        unsigned short upper_tid;

        lower_tid = MF_TOP_LOW_TMP_VALUE;
        upper_tid = MF_TOP_HIGH_TMP_VALUE;

        std::string lower_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].lower_range);
        std::string upper_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].upper_range);

        boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
        boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

        dst_list.push_back(new_low_data);
        dst_list.push_back(new_upper_data);
    }

    // 动力源48V输出汇流排温度告警下限值
    // 动力源48V输出汇流排温度告警上限值
    for(int i = 0 ; i < vect_size; i++)
    {
        unsigned short lower_tid;
        unsigned short upper_tid;

        lower_tid = P0M_48V_OUT_LOW_TMP_LIMIT;
        upper_tid = POM_48V_OUT_HIGH_TMP_LIMIT;

        std::string lower_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].lower_range);
        std::string upper_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].upper_range);

        boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
        boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

        dst_list.push_back(new_low_data);
        dst_list.push_back(new_upper_data);
    }

    // FT-HF-A腔体温度告警下限值
    // FT-HF-A腔体温度告警上限值
    for(int i = 0 ; i < vect_size; i++)
    {
        unsigned short lower_tid;
        unsigned short upper_tid;

        lower_tid = HF_UP_LOW_TMP_VALUE;
        upper_tid = HF_UP_HIGH_TMP_VALUE;

        std::string lower_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].lower_range);
        std::string upper_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].upper_range);

        boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
        boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

        dst_list.push_back(new_low_data);
        dst_list.push_back(new_upper_data);
    }

    // FT-HF-B腔体温度告警下限值
    // FT-HF-B腔体温度告警上限值
    for(int i = 0 ; i < vect_size; i++)
    {
        unsigned short lower_tid;
        unsigned short upper_tid;

        lower_tid = HF_DOWN_LOW_TMP_VALUE;
        upper_tid = HF_DOWN_HIGH_TMP_VALUE;

        std::string lower_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].lower_range);
        std::string upper_val = std::to_string(vect_range[i].key) + std::string(":") + std::to_string(vect_range[i].upper_range);

        boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
        boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

        dst_list.push_back(new_low_data);
        dst_list.push_back(new_upper_data);
    }

    //模拟烟感策略数据
    std::vector<smoke_density> vect_smk;
    smoke_density smk1;
    smk1.key = 1;
    smk1.smk_density = 10.0;
    smoke_density smk2;
    smk2.key = 2;
    smk2.smk_density = 20.0;
    smoke_density smk3;
    smk3.key = 3;
    smk3.smk_density = 30.0;
    vect_smk.push_back(smk1);
    vect_smk.push_back(smk2);
    vect_smk.push_back(smk3);
    vect_size = vect_smk.size();

    // TH板卡腔体出风口烟感阈值
    
