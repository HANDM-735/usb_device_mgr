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

    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = TH_CARD_SMOKE_VALUE;
        std::string val =  std::to_string(vect_smk[i].key)+ std::string(":") + std::to_string(vect_smk[i].smk_density);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    // TH-48v线缆腔体出风口烟感阈值
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = TH_48V_SMOKE_VALUE;
        std::string val =  std::to_string(vect_smk[i].key)+ std::string(":") + std::to_string(vect_smk[i].smk_density);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    // TH-HF线缆腔体烟感阈值
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = TH_HFBOX_SMOKE_SENSOR;
        std::string val =  std::to_string(vect_smk[i].key)+ std::string(":") + std::to_string(vect_smk[i].smk_density);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    // MF板卡顶部出风口烟感阈值
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = MF_TOP_OUT_SMOKE_VALUE;
        std::string val =  std::to_string(vect_smk[i].key)+ std::string(":") + std::to_string(vect_smk[i].smk_density);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    // FT-HF-A烟感阈值
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = HF_UP_SMOKE_VALUE;
        std::string val =  std::to_string(vect_smk[i].key)+ std::string(":") + std::to_string(vect_smk[i].smk_density);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    // FT-HF-B烟感阈值
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = HF_DOWN_SMOKE_VALUE;
        std::string val =  std::to_string(vect_smk[i].key)+ std::string(":") + std::to_string(vect_smk[i].smk_density);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    //模拟风扇策略数据
    std::vector<Fan_speed> vect_fan;
    fan_speed fan1;
    fan1.key = 1;
    fan1.sensor_speed = 100.0;
    fan_speed fan2;
    fan2.key = 2;
    fan2.sensor_speed = 200.0;
    fan_speed fan3;
    fan3.key = 3;
    fan3.sensor_speed = 300.0;

    vect_size = vect_fan.size();

    // TH板卡腔体出风口风扇转速
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = TH_BOXFAN_SPEED_SENSOR;
        std::string val =  std::to_string(vect_fan[i].key)+ std::string(":") + std::to_string(vect_fan[i].sensor_speed);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    // TH线缆腔体出风口风扇转速
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = TH_CABLEFAN_SPEED_SENSOR;
        std::string val =  std::to_string(vect_fan[i].key)+ std::string(":") + std::to_string(vect_fan[i].sensor_speed);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    // MF顶部出风口转速阈值
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = MF_TOP_OUT_FAN_VALUE;
        std::string val =  std::to_string(vect_fan[i].key)+ std::string(":") + std::to_string(vect_fan[i].sensor_speed);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    // FT-TH-A腔体转速阈值
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = HF_UP_FAN_VALUE;
        std::string val =  std::to_string(vect_fan[i].key)+ std::string(":") + std::to_string(vect_fan[i].sensor_speed);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    // FT-TH-B腔体转速阈值
    for(int i = 0; i < vect_size; i++)
    {
        unsigned short tid;
        tid = HF_DOWM_FAN_VALUE;
        std::string val =  std::to_string(vect_fan[i].key)+ std::string(":") + std::to_string(vect_fan[i].sensor_speed);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_securtypolicy_test() ret=%d",ret);

    return ret;
}

//解析温度安全策略tid的value值
int xmboard::parse_temppolicy(const std::string& value,std::vector<temp_range>& vct_range)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_temppolicy()");

    int ret = 0;
    //将value格式定义,key(1个字节)+温度区间下限值(4个字节float)+温度区间上限值(4个字节float),9个字节为一组策略
    int len = value.length();

    if(len % 9 != 0)
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xmboard::parse_temppolicy() temp range value len(%d) is wrong",len);
    }
    else
    {
        for(int i =0 ; i < len; )
        {
            temp_range range;
            range.key = (unsigned short)value[i];
            i = i + 1;
            range.lower_range = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(&value[i])),4);
            i = i+ 4;
            range.upper_range = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(&value[i])),4);
            i = i+ 4;
            vct_range.push_back(range);
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_temppolicy() ret=%d",ret);

    return ret;
}

//将温度安全策略的tid转换上位机相关tid
int xmboard::convert_temppolicy(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtvl)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_temppolicy()");

    int ret = 0;

    std::string value = src_xusbtvl->get_data();
    //1、得到对应温度安全策略值
    std::vector<temp_range> vct_range;
    parse_temppolicy(value,vct_range);
    int vect_size = vct_range.size();

    if(vect_size > 0)
    {
        //上位机下限tid和上限tid
        unsigned short lower_tid;
        unsigned short upper_tid;
        swith(src_xusbtvl->m_tid)
        {
            case RMA_BOX_OUTLET_TEMP_SET:
                lower_tid = TH_OUT_LOW_TMP_VALUE;
                upper_tid = TH_OUT_HIGH_TMP_VALUE;
                break;
            case RMA_BOX_48V_TEMP_SET:
                lower_tid = TH_48V_LOW_TMP_VALUE;
                upper_tid = TH_48V_HIGH_TMP_VALUE;
                break;
            case RMA_THCABLE_BOX_TEMP_SET:
                lower_tid = TH_CABLE_BOX_LOW_TMP_VALUE;
                upper_tid = TH_CABLE_BOX_HIGH_TMP_VALUE;
                break;
            case RMA_HF_BOX_TEMP_SET:
                lower_tid = HF_BOX_LOW_TMP_VALUE;
                upper_tid = HF_BOX_HIGH_TMP_VALUE;
                break;
            case RMA_BOX_INLET_TEMP_SET:
                lower_tid = TH_IN_BOX_LOW_TMP_VALUE;
                upper_tid = TH_IN_BOX_HIGH_TMP_VALUE;
                break;
            case RMA_MF_TOP_OUTLET_TEMP_SET:
                lower_tid = MF_TOP_LOW_TMP_VALUE;
                upper_tid = MF_TOP_HIGH_TMP_VALUE;
                break;
            case RMA_48V_OUT_OUTPUT_TEMP_SET:
                lower_tid = POM_48V_OUT_LOW_TMP_LIMIT;
                upper_tid = POM_48V_OUT_HIGH_TMP_LIMIT;
                break;
            case RMA_UPPER_HF_TEMP_SET:
                lower_tid = HF_UP_LOW_TMP_VALUE;
                upper_tid = HF_UP_HIGH_TMP_VALUE;
                break;
            case RMA_LOWER_HF_TEMP_SET:
                lower_tid = HF_DOWM_LOW_TMP_VALUE;
                upper_tid = HF_DOWM_HIGH_TMP_VALUE;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vect_size; i++)
        {
            std::string lower_val =  std::to_string(vect_range[i].key)+ std::string(":") + xbasic::to_string(vect_range[i].lower_range);
            std::string upper_val =  std::to_string(vect_range[i].key)+ std::string(":") + xbasic::to_string(vect_range[i].upper_range);
            boost::shared_ptr<xtvl> new_low_data(new xtv1(lower_tid,lower_val.length(),lower_val.data(),true));
            boost::shared_ptr<xtvl> new_upper_data(new xtv1(upper_tid,upper_val.length(),upper_val.data(),true));

            dst_list.push_back(new_low_data);
            dst_list.push_back(new_upper_data);
        }
    }
    else
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xmboard::convert_temppolicy() parse_temppolicy failed");
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_temppolicy() ret=%d",ret);

    return ret;
}

//解析烟感安全策略tid的value值
int xmboard::parse_smokepolicy(const std::string& value,std::vector<smoke_density>& vct_smk)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_smokepolicy()");

    int ret = 0;
    //将value格式定义,key(4个字节)+烟感浓度(4个字节float),8个字节为一组策略
    int len = value.length();
    if(len % 8 != 0)
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xmboard::parse_smokepolicy() smoke density value len(%d) is wrong",len);
    }
    else
    {
        for(int i =0 ; i < len; )
        {
            smoke_density density;
            density.key = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(&value[i])),4);
            i = i+ 4;
            density.smk_density = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(&value[i])),4);
            i = i+ 4;
            vct_smk.push_back(density);
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_smokepolicy() ret=%d",ret);

    return ret;
}

//将烟感安全策略的tid转换上位机相关tid
int xmboard::convert_smokepolicy(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtvl)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_smokepolicy()");

    int ret = 0;
    std::string value = src_xusbtvl->get_data();

    //1、得到对应烟感安全策略值
    std::vector<smoke_density> vct_smk;
    parse_smokepolicy(value,vct_smk);
    int vect_size = vct_smk.size();

    if(vect_size > 0)
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtvl->m_tid)
        {
            case RMA_BOX_OUTLET_SMOKE_SET:
                tid = TH_CARD_SMOKE_VALUE;
                break;
            case RMA_48V_SMOKE_SET:
                tid = TH_48V_SMOKE_VALUE;
                break;
            case RMA_HFBOX_SMOKE_SET:
                tid = TH_HFBOX_SMOKE_SENSOR;
                break;
            case RMA_TOP_OUTLET_SMOKE_SET:
                tid = MF_TOP_OUT_SMOKE_VALUE;
                break;
            case RMA_UPPER_HF_SMOKE_SET:
                tid = HF_UP_SMOKE_VALUE;
                break;
            case RMA_LOWER_HF_SMOKE_SET:
                tid = HF_DOWM_SMOKE_VALUE;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vect_size; i++)
        {
            std::string val =  std::to_string(vect_smk[i].key)+ std::string(":") + xbasic::to_string(vect_smk[i].smk_density);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xmboard::convert_smokepolicy() parse_smokepolicy failed");
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_smokepolicy() ret=%d",ret);

    return ret;
}

//解析风扇安全策略tid的value值
int xmboard::parse_fanpolicy(const std::string& value,std::vector<fan_speed>& vct_fan)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_fanpolicy()");

    int ret = 0;
    //将value格式定义,key(4个字节)+风扇转速比(4个字节float),8个字节为一组策略
    int len = value.length();
    if(len % 8 != 0)
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xmboard::parse_fanpolicy() fan speed value len(%d) is wrong",len);
    }
    else
    {
        for(int i =0 ; i < len; )
        {
            fan_speed speed;
            speed.key = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(&value[i])),4);
            i = i + 4;
            speed.sensor_speed = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(&value[i])),4);
            i = i+ 4;
            vct_fan.push_back(speed);
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_fanpolicy() ret=%d",ret);

    return ret;
}

//将风扇安全策略的tid转换上位机相关tid
int xmboard::convert_fanpolicy(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtvl)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_fanpolicy()");

    int ret = 0;

    std::string value = src_xusbtvl->get_data();

    //1、得到对应风扇安全策略值
    std::vector<fan_speed> vct_fan;
    parse_fanpolicy(value,vct_fan);
    int vect_size = vct_fan.size();

    if(vect_size > 0)
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtvl->m_tid)
        {
            case RMA_THBOX_FAN_SET:
                tid = TH_BOXFAN_SPEED_SENSOR;
                break;
            case RMA_THCABLE_FAN_SET:
                tid = TH_CABLEFAN_SPEED_SENSOR;
                break;
            case RMA_MFTOP_FAN_SET:
                tid = MF_TOP_OUT_FAN_VALUE;
                break;
            case RMA_UPPER_HF_FAN_SET:
                tid = HF_UP_FAN_VALUE;
                break;
            case RMA_LOWER_HF_FAN_SET:
                tid = HF_DOWM_FAN_VALUE;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vect_size; i++)
        {
            std::string val =  std::to_string(vect_fan[i].key)+ std::string(":") + xbasic::to_string(vect_fan[i].sensor_speed);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xmboard::convert_fanpolicy() parse_fanpolicy failed");
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_fanpolicy() ret=%d",ret);

    return ret;
}

//解析温度告警数值
int xmboard::iparse_temp_alarm(const std::string& value,float& temp_alarm_val)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_temp_alarm()");

    const char* str_data  = value.data();
    size_t length         = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_temp_alarm() value length=%d is invalid",length);
        ret = -1;
    }
    else
    {
        float alarm_tmp = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_temp_alarm() ret=%d",ret);

    return ret;
}

//将温度告警的tid转换上位机相关tid
int xmboard::convert_tempalarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtvl)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_tempalarm()");

    int ret = 0;

    std::string value = src_xusbtvl->get_data();

    //1、得到对应温度告警值
    float tempalarm_val;
    int rt = parse_temp_alarm(value,tempalarm_val);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtvl->m_tid)
        {
            case RMA_BOX_OUTLET_TEMP_ALARM:         // TH板卡腔体出风口1温度报警
                tid = TH_CARD_OUT_TMP1_ALARM;
                break;
            case RMA_BOX_48V_TEMP_ALARM:            // TH-48v汇流排1温度报警
                tid = TH_48_LINE_TMP1_ALARM;
                break;
            case RMA_UPPER_HF_TEMP_ALARM:           // 上HF线缆腔体1温度告警
                tid = HF_UPPER_TMP1_ALARM;
                break;
            case RMA_LOWER_HF_TEMP_ALARM:           // 下HF线缆腔体1温度告警
                tid = HF_DOWM_TMP1_ALARM;
                break;
            case RMA_MF_TOP_OUTLET_TEMP_ALARM:      // MF板卡顶部出风口1温度报警
                tid = MF_TOP_OUT_TMP1_ALARM;
                break;
            case RMA_48V_TEMP_ALARM:                // 动力源48V输出汇流排温度告警
                tid = POM_48_TMP_ALARM;
                break;
            default:
                break;
        }
        std::string val = xbasic::to_string(tempalarm_val);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_tempalarm() parse_temp_alarm failed rt=%d",rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_tempalarm() ret=%d",ret);

    return ret;
}

//解析烟感数值
int xmboard::parse_smoke_alarm(const std::string& value,float& smk_alarm_val)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_smoke_alarm()");

    const char* str_data  = value.data();
    size_t length         = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_smoke_alarm() value length=%d is invalid",length);
        ret = -1;
    }
    else
    {
        float smk_alarm_val = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_smoke_alarm() ret=%d",ret);

    return ret;
}

//将烟感告警的tid转换上位机相关tid
int xmboard::convert_smokealarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtvl)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_smokealarm()");

    int ret = 0;
    std::string value = src_xusbtvl->get_data();

    //1、得到对应烟感告警值
    float smkalam_val;
    int rt = parse_smoke_alarm(value,smkalam_val);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtvl->m_tid)
        {
            case RMA_48V_SMOKE_ALARM:
                tid = TH_48_OUT_SMOKE1_ALARM;         // TH-48v线缆腔体出风口烟感1报警
                break;
            case RMA_BOX_OUTLET_SMOKE_ALARM:
                tid = TH_CARD_OUT_SMOKE1_ALARM;       // TH板卡腔体出风口烟感1报警
                break;
            case RMA_UPPER_HF_SMOKE_ALARM:            // 上HF线缆腔体烟感1报警
                tid = HF_UPPER_SMOKE1_ALARM;
                break;
            case RMA_LOWER_HF_SMOKE_ALARM:            // 下HF线缆腔体烟感1报警
                tid = HF_DOWM_SMOKE1_ALARM;
                break;
            case RMA_TOP_OUTLET_SMOKE_ALARM:          // MF板卡顶部出风口烟感1报警
                tid = MF_TOP_OUT_SMOKE1_ALARM;
                break;
            default:
                break;
        }

        std::string val = xbasic::to_string(smkalam_val);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_smokealarm() parse_smoke_alarm failed rt=%d",rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_smokealarm() ret=%d",ret);

    return ret;
}

//解析CDU告警数值
int xmboard::parse_cdu_alarm(const std::string& value,float& cdu_alarm_val)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_cdu_alarm()");

    const char* str_data  = value.data();
    size_t length         = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_cdu_alarm() value length=%d is invalid",length);
        ret = -1;
    }
    else
    {
        float cdu_alarm_val = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_cdu_alarm() ret=%d",ret);

    return ret;
}

//将CDU告警的tid转换上位机相关tid
int xmboard::convert_cdualarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtvl)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_cdualarm()");

    int ret = 0;

    std::string value = src_xusbtvl->get_data();

    //1、得到对应CDU告警值
    float cdualarm_val;
    int rt = parse_cdu_alarm(value,cdualarm_val);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtvl->m_tid)
        {
            case RMA_CDU_TEMP_ALARM:                  // CDU状态温度报警
                tid = CDU_STATUS_TMP_ALARM;
                break;
            case RMA_CDU_TRAFFIC_ALARM:               // CDU状态流量报警
                tid = CDU_STATUS_STREAM_ALARM;
                break;
            //case RMA_CDU_PUMP_PRESSURE_ALARM:         // CDU状态水泵压力报警
            //    tid = CDU_STATUS_WATER_STRESS_ALARM;
            //    break;
            default:
                break;
        }

        std::string val = xbasic::to_string(cdualarm_val);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_cdualarm() parse_cdu_alarm failed rt=%d",rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_cdualarm() ret=%d",ret);

    return ret;
}

//解析48v动力源告警数值
int xmboard::iparse_48vps_alarm(const std::string& value,float& ps_val)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_48vps_alarm()");

    const char* str_data  = value.data();
    size_t length         = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_48vps_alarm() value length=%d is invalid",length);
        ret = -1;
    }
    else
    {
        float ps_val = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_48vps_alarm() ret=%d",ret);

    return ret;
}

// 将48v动力源告警的tid转换上位机相关tid
int xmboard::convert_48vpsalarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::convert_48vpsalarm()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应48v动力源告警值
    float pslarm_val;
    int rt = parse_48vps_alarm(value, pslarm_val);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_48V_PS_TEMP_ALARM:       // 48V动力源温度报警
                tid = POW_48_TMP_ALARM;
                break;
            case RMA_48V_VOLTAGE_ALARM:       // 48V动力源电压报警
                tid = POW_48_VOL_ALARM;
                break;
            case RMA_48V_CURRENT_ALARM:       // 48V动力源电流报警
                tid = POW_48_CURRENT_ALARM;
                break;
            default:
                break;
        }

        std::string val = xbasic::to_string(pslarm_val);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid, val.length(), val.data(), true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG, "xmboard::convert_48vpsalarm() parse_48vps_alarm failed rt=%d", rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::convert_48vpsalarm() ret=%d", ret);

    return ret;
}

// 解析厂务水告警数值
int xmboard::parse_factorywater_alarm(const std::string& value, float& factorywater_val)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::parse_factorywater_alarm()");

    const char* str_data     = value.data();
    size_t length          = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG, "xmboard::parse_factorywater_alarm() value length=%d is invalid", length);
        ret = -1;
    }
    else
    {
        float factorywater_val = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::parse_factorywater_alarm() ret=%d", ret);

    return ret;
}

// 将厂务水告警的tid转换上位机相关tid
int xmboard::convert_factorywater_alarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::convert_factorywater_alarm()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应厂务水告警值
    float factorywater_larm_val;
    int rt = parse_factorywater_alarm(value, factorywater_larm_val);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_FACTORYWATER_TEMP_ALARM:     // 非动力源厂务水温度报警
                tid = NO_POW_WATER_TMP_ALARM;
                break;
            case RMA_FACTORYWATER_TRAFFIC_ALARM:  // 非动力源厂务水流量报警
                tid = NO_POW_WATER_STREAM_ALARM;
                break;
            case RMA_FACTORYWATER_PRESSURE_ALARM: // 非动力源厂务水压力报警
                tid = NO_POW_WATER_STRESS_ALARM;
                break;
            default:
                break;
        }

        std::string val = xbasic::to_string(factorywater_larm_val);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid, val.length(), val.data(), true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG, "xmboard::convert_factorywater_alarm() parse_factorywater_alarm failed rt=%d", rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::convert_factorywater_alarm() ret=%d", ret);

    return ret;
}

// 解析电压,温度传感器告警数值
int xmboard::parse_sensor_alarm(const std::string& value, std::vector<sensor_alarm>& vect_sensors)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::parse_sensor_alarm()");

    const char* value_ptr = value.data();
    size_t length         = value.length();
    size_t i = 0;

    int ret = 0;
    while(i < length)
    {
        uint8_t sensorId = value_ptr[i++];
        if (i+1 < length)
        {
            uint16_t val = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr + i)), 2);
            i += 2;

            sensor_alarm sensor;
            sensor.sensor_id  = sensorId;
            sensor.sensor_val = val;

            vect_sensors.push_back(sensor);
        }
        else
        {
            LOG_MSG(ERR_LOG, "xmboard::parse_sensor_alarm() i=%d length=%d is Invalid!", i, length);
            ret = -1;
            break;
        }
    }

    return ret;
}

// 将电压,温度传感器告警的tid转换上位机相关tid
int xmboard::convert_sensor_alarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::convert_sensor_alarm()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应电压,温度告警值
    std::vector<sensor_alarm> vct_sensor;
    int rt = parse_sensor_alarm(value, vct_sensor);
    int vct_size = vct_sensor.size();

    if(vct_size > 0)
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_VOLTAGE_ALARM:             // 电压传感器告警
                tid = VOL_SENSOR_ALARM;
                break;
            case RMA_TEMP_ALARM:                // 温度传感器告警
                tid = TMP_SENSOR_ALARM;
                break;
            default:
                break;
        }

        for(int i = 0; i < vct_size; i++)
        {
            std::string val = std::to_string(vct_sensor[i].sensor_id) + std::string(":") + std::to_string(vct_sensor[i].sensor_val);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid, val.length(), val.data(), true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG, "xmboard::convert_sensor_alarm() parse_sensor_alarm failed vct_size=%d rt=%d", vct_size, rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::convert_sensor_alarm() ret=%d", ret);

    return ret;
}

// 解析液冷系统故障,告警相关数据
int xmboard::parse_lcs_fault_alarm(const std::string& value, std::vector<lcs_fault>& vct_fault)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::parse_lcs_fault_alarm()");

    const char* value_ptr = value.data();
    size_t length         = value.length();
    size_t i = 0;

    int ret = 0;
    while(i < length)
    {
        uint16_t sensorId = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr + i)), 2);
        i += 2;

        if (i < length)
        {
            uint8_t code = value_ptr[i];
            i += 1;

            lcs_fault fault_alarm;
            fault_alarm.m_id    = sensorId;
            fault_alarm.m_code  = code;

            vct_fault.push_back(fault_alarm);
        }
        else
        {
            LOG_MSG(ERR_LOG, "xmboard::parse_lcs_fault_alarm() i=%d length=%d is Invalid!", i, length);
            ret = -1;
            break;
        }
    }

    return ret;
}

// 将液冷系统故障,告警的tid转换成上位机相关tid
int xmboard::convert_lcs_fault_alarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::convert_lcs_alarm()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应故障,告警值
    std::vector<lcs_fault> vct_fault;
    int rt = parse_lcs_fault_alarm(value, vct_fault);
    int vct_size = vct_fault.size();

    if(vct_size > 0)
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_PRIM_WATER_TEMPSENSOR_ALARM:             //一次侧供水温度传感器故障
                tid = MF_PRIM_WATER_TEMPSENSOR_ALARM;
                break;
            case RMA_SECD_WATER_TEMP_A_ALARM:                 //二次侧供水温度传感器A故障
                tid = MF_SECD_WATER_TEMP_A_ALARM;
                break;
            case RMA_SECD_WATER_TEMP_B_ALARM:                 //二次侧供水温度传感器B故障
                tid = MF_SECD_WATER_TEMP_B_ALARM;
                break;
            case RMA_SECD_WATER_TEMP_C_ALARM:                 //二次侧供水温度传感器C故障
                tid = MF_SECD_WATER_TEMP_C_ALARM;
                break;
            case RMA_ENV_TEMP_SENSOR_ALARM:                   //环境温度传感器故障
                tid = MF_ENV_TEMP_SENSOR_ALARM;
                break;
            case RMA_SECD_BACKWATER_SENSOR_ALARM:             //二次侧回水温度传感器故障
                tid = MF_SECD_BACKWATER_SENSOR_ALARM;
                break;
            case RMA_PRIM_BACKWATER_SENSOR_ALARM:             //一次侧回水温度传感器故障
                tid = MF_PRIM_BACKWATER_SENSOR_ALARM;
                break;
            case RMA_HUMIDITY_SENSOR_ALARM:                   //机房相对湿度传感故障
                tid = MF_HUMIDITY_SENSOR_ALARM;
                break;
            case RMA_SECD_PS1_SENSOR_ALARM:                   //二次侧回水压力传感器故障
                tid = MF_SECD_PS1_SENSOR_ALARM;
                break;
            case RMA_SECD_PS2_SENSOR_ALARM:                   //二次侧回水压力传感器故障
                tid = MF_SECD_PS2_SENSOR_ALARM;
                break;
            case RMA_SECD_PS3_SENSOR_ALARM:                   //二次侧供水压力传感器故障
                tid = MF_SECD_PS3_SENSOR_ALARM;
                break;
            case RMA_SECD_TRAFFIC_SENSOR_ALARM:               //二次侧流量计故障
                tid = MF_SECD_TRAFFIC_SENSOR_ALARM;
                break;
            case RMA_PRIM_TRAFFIC_SENSOR_ALARM:               //一次侧流量计故障
                tid = MF_PRIM_TRAFFIC_SENSOR_ALARM;
                break;
            case RMA_MEMORYCARD_ALARM:                        //存储卡故障
                tid = MF_MEMORYCARD_ALARM;
                break;
            case RMA_WATERBAG_EMPTY_ALARM:                    //补水袋已空
                tid = MF_WATERBAG_EMPTY_ALARM;
                break;
            case RMA_SYSTEM_LACKWATER_ALARM:                  //系统缺水
                tid = MF_SYSTEM_LACKWATER_ALARM;
                break;
            case RMA_PUMP1_ALARM:                             //水泵1故障
                tid = MF_PUMP1_ALARM;
                break;
            case RMA_PUMP2_ALARM:                             //水泵2故障
                tid = MF_PUMP2_ALARM;
                break;
            case RMA_PUMP_STOP_ALARM:                         //泵故障停机
                tid = MF_PUMP_STOP_ALARM;
                break;
            case RMA_ELEC_VALVE_ALARM:                        //电动阀故障
                tid = MF_ELEC_VALVE_ALARM;
                break;
            case RMA_BOX_MONITORED_LIQUID_ALARM:              //机箱内监测到液体
                tid = MF_BOX_MONITORED_LIQUID_ALARM;
                break;
            case RMA_PRIM_WATERLEAK_ALARM:                    //外部漏水(一次侧)
                tid = MF_PRIM_WATERLEAK_ALARM;
                break;
            case RMA_SECD_WATERLEAK_ALARM:                    //外部漏水(二次侧)
                tid = MF_SECD_WATERLEAK_ALARM;
                break;
            case RMA_WATERBAG_LINE_ALARM:                     //检查补水袋水位
                tid = MF_WATERBAG_LINE_ALARM;
                break;
            case RMA_PRIM_NO_TRAFFIC_ALARM:                   //一次侧无流告警
                tid = MF_PRIM_NO_TRAFFIC_ALARM;
                break;
            case RMA_LIQUIDLEVEL_NOWATER_ALARM:               //液位传感器未检测到水
                tid = MF_LIQUIDLEVEL_NOWATER_ALARM;
                break;
            case RMA_LIQUIDLEVEL_INVALID_ALARM:               //液位传感器失效
                tid = MF_LIQUIDLEVEL_INVALID_ALARM;
                break;
            case RMA_GROUP_CONTROL_NET_ALARM:                 //群控网络故障
                tid = MF_GROUP_CONTROL_NET_ALARM;
                break;
            case RMA_GCN_INSUFFICIENT_ALARM:                  //群控数量不足
                tid = MF_GCN_INSUFFICIENT_ALARM;
                break;
            case RMA_FILTER_PLUGGING_ALARM:                   //二次侧过滤器堵
                tid = MF_FILTER_PLUGGING_ALARM;
                break;
            case RMA_COULOMBMETER_ALARM:                      //电量表故障
                tid = MF_COULOMBMETER_ALARM;
                break;
            case RMA_SECD_WATERTEMP_ADIFF_ALARM:              //二次侧供水温度传感器A差异
                tid = MF_SECD_WATERTEMP_ADIFF_ALARM;
                break;
            case RMA_SECD_WATERTEMP_BDIFF_ALARM:              //二次侧供水温度传感器B差异
                tid = MF_SECD_WATERTEMP_BDIFF_ALARM;
                break;
            case RMA_SECD_WATERTEMP_CDIFF_ALARM:              //二次侧供水温度传感器C差异
                tid = MF_SECD_WATERTEMP_CDIFF_ALARM;
                break;
            case RMA_PUMP1_COMM_ALARM:                        //水泵1通信故障
                tid = MF_PUMP1_COMM_ALARM;
                break;
            case RMA_PUMP2_COMM_ALARM:                        //水泵2通信故障
                tid = MF_PUMP2_COMM_ALARM;
                break;
            case RMA_SUPPLY_WATER_ALARM:                      //补水请求告警
                tid = MF_SUPPLY_WATER_ALARM;
                break;
            case RMA_MEMORYCARD_FULL_ALARM:                   //存储卡满告警
                tid = MF_MEMORYCARD_FULL_ALARM;
                break;
            default:
                break;
        }

        for(int i = 0; i < vct_size; i++)
        {
            std::string val = std::to_string(vct_fault[i].m_id) + std::string(":") + std::to_string(vct_fault[i].m_code);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid, val.length(), val.data(), true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG, "xmboard::convert_lcs_alarm() parse_lcs_fault_alarm failed vct_size=%d rt=%d", vct_size, rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::convert_lcs_alarm() ret=%d", ret);

    return ret;
}

// 解析液冷系统一些数据告警相关数据
int xmboard::parse_lcs_data_alarm(const std::string& value, std::vector<lcs_alarm>& vct_sensor)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::parse_lcs_data_alarm()");

    const char* value_ptr = value.data();
    size_t length         = value.length();
    size_t i = 0;

    int ret = 0;
    while(i < length)
    {
        uint16_t sensorId = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr + i)), 2);
        i += 2;

        if ((i+4) <= length)
        {
            float val = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr + i)), 4);
            i += 4;

            lcs_alarm data_alarm;
            data_alarm.m_sensorid = sensorId;
            data_alarm.m_value = val;

            vct_sensor.push_back(data_alarm);
        }
        else
        {
            LOG_MSG(ERR_LOG, "xmboard::parse_lcs_data_alarm() i=%d length=%d is Invalid!", i, length);
            ret = -1;
            break;
        }
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::parse_lcs_data_alarm()");

    return ret;
}

// 将液冷系统一些数据告警的tid转换成上位机相关tid
int xmboard::convert_lcs_data_alarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::convert_lcs_data_alarm()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应告警值
    std::vector<lcs_alarm> vct_alarm;
    int rt = parse_lcs_data_alarm(value, vct_alarm);
    int vct_size = vct_alarm.size();

    if(vct_size > 0)
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_PRIM_TRAFFIC_LOW_ALARM:            //一次侧低流量告警
                tid = MF_PRIM_TRAFFIC_LOW_ALARM;
                break;
            case RMA_PRIM_WATERTEMP_LOW_ALARM:          //一次侧水温过低告警
                tid = MF_PRIM_WATERTEMP_LOW_ALARM;
                break;
            case RMA_PRIM_WATERTEMP_HIGH_ALARM:         //一次侧水温过高告警
                tid = MF_PRIM_WATERTEMP_HIGH_ALARM;
                break;
            case RMA_SECD_WATERTEMP_LOW_ALARM:          //二次侧水温过低告警
                tid = MF_SECD_WATERTEMP_LOW_ALARM;
                break;
            case RMA_SECD_WATERTEMP_HIGH_ALARM:         //二次侧水温过高告警
                tid = MF_SECD_WATERTEMP_HIGH_ALARM;
                break;
            case RMA_PRIM_STRESS_HIGH_ALARM:            //一次侧压力过高
                tid = MF_PRIM_STRESS_HIGH_ALARM;
                break;
            case RMA_SYS_STRESS_LOW_ALARM:              //系统压力过低告警
                tid = MF_SYS_STRESS_LOW_ALARM;
                break;
            case RMA_SYS_STRESS_HIGH_ALARM:             //系统压力过高告警
                tid = MF_SYS_STRESS_HIGH_ALARM;
                break;
            case RMA_PUMP1_TRAFFIC_LOW_ALARM:           //水泵1流量过低
                tid = MF_PUMP1_TRAFFIC_LOW_ALARM;
                break;
            case RMA_PUMP2_TRAFFIC_LOW_ALARM:           //水泵2流量告警
                tid = MF_PUMP2_TRAFFIC_LOW_ALARM;
                break;
            default:
                break;
        }

        for(int i = 0; i < vct_size; i++)
        {
            std::string val = std::to_string(vct_alarm[i].m_sensorid) + std::string(":") + xbasic::to_string(vct_alarm[i].m_value);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid, val.length(), val.data(), true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG, "xmboard::convert_lcs_data_alarm() parse_sensor_alarm failed vct_size=%d rt=%d", vct_size, rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::convert_lcs_data_alarm() ret=%d", ret);

    return ret;
}

// 解析TH伺服器告警错误代码
int xmboard::parse_th_servo_data_alarm(const std::string& value, servo_err& alarm)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::parse_th_servo_data_alarm()");

    const char* value_ptr = value.data();
    size_t length         = value.length();

    int ret = 0;
    if(length != 8)
    {
        LOG_MSG(ERR_LOG, "xmboard::parse_th_servo_data_alarm() length=%d is Invalid!", length);
        ret = -1;
    }
    else
    {
        alarm.m_servo_id = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        int i = 4;
        alarm.m_errcode = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr+i)), 4);
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::parse_th_servo_data_alarm() ret=%d", ret);

    return ret;
}

// 将TH伺服器告警错误代码转换成上位机相关tid
int xmboard::convert_th_servo_alarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::convert_th_servo_alarm()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、TH伺服器告警
    servo_err alarm;
    int rt = parse_th_servo_data_alarm(value, alarm);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_TH_SERVO_ALARM:        // TH板伺服器告警
                tid = TH_SERVO_ALARM;
                break;
            default:
                break;
        }

        std::string val = std::to_string(alarm.m_servo_id) + std::string(":") + std::to_string(alarm.m_errcode);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid, val.length(), val.data(), true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG, "xmboard::convert_th_servo_alarm() parse_temp_alarm failed rt=%d", rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::convert_th_servo_alarm() ret=%d", ret);

    return ret;
}

// 将告警的tid转换上位机相关tid
int xmboard::convert_alarm(std::list<boost::shared_ptr<xtvl> > & dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG, "Enter into xmboard::convert_alarm()");

    int ret = 0;
    switch(src_xusbtlv->m_tid)
    {
        case RMA_BOX_OUTLET_TEMP_ALARM:
        case RMA_BOX_48V_TEMP_ALARM:
        case RMA_UPPER_HF_TEMP_ALARM:
        case RMA_LOWER_HF_TEMP_ALARM:
        case RMA_HF_TOP_OUTLET_TEMP_ALARM:
        case RMA_48V_TEMP_ALARM:
            ret = convert_tempalarm(dst_list, src_xusbtlv);
            break;
        case RMA_48V_SMOKE_ALARM:
        case RMA_BOX_OUTLET_SMOKE_ALARM:
        case RMA_UPPER_HF_SMOKE_ALARM:
        case RMA_LOWER_HF_SMOKE_ALARM:
        case RMA_TOP_OUTLET_SMOKE_ALARM:
            ret = convert_smokealarm(dst_list, src_xusbtlv);
            break;
        case RMA_CDU_TEMP_ALARM:
        case RMA_CDU_TRAFFIC_ALARM:
            //case RMA_CDU_PUMP_PRESSURE_ALARM:
            ret = convert_cdualarm(dst_list, src_xusbtlv);
            break;
        case RMA_48V_PS_TEMP_ALARM:
        case RMA_48V_VOLTAGE_ALARM:
        case RMA_48V_CURRENT_ALARM:
            ret = convert_48vpsalarm(dst_list, src_xusbtlv);
            break;
        case RMA_FACTORYWATER_TEMP_ALARM:
        case RMA_FACTORYWATER_TRAFFIC_ALARM:
        case RMA_FACTORYWATER_PRESSURE_ALARM:
            ret = convert_factorywater_alarm(dst_list, src_xusbtlv);
            break;
        case RMA_VOLTAGE_ALARM:
        case RMA_TEMP_ALARM:
            ret = convert_sensor_alarm(dst_list, src_xusbtlv);
            break;
        case RMA_PRIM_WATER_TEMPSENSOR_ALARM:
        case RMA_SECD_WATER_TEMP_A_ALARM:
        case RMA_SECD_WATER_TEMP_B_ALARM:
        case RMA_SECD_WATER_TEMP_C_ALARM:
        case RMA_ENV_TEMP_SENSOR_ALARM:
        case RMA_SECD_BACKWATER_SENSOR_ALARM:
        case RMA_PRIM_BACKWATER_SENSOR_ALARM:
        case RMA_HUMIDITY_SENSOR_ALARM:
        case RMA_SECD_PS1_SENSOR_ALARM:
        case RMA_SECD_PS2_SENSOR_ALARM:
        case RMA_SECD_PS3_SENSOR_ALARM:
        case RMA_SECD_TRAFFIC_SENSOR_ALARM:
        case RMA_PRIM_TRAFFIC_SENSOR_ALARM:
        case RMA_MEMORYCARD_ALARM:
        case RMA_WATERBAG_EMPTY_ALARM:
        case RMA_SYSTEM_LACKWATER_ALARM:
        case RMA_PUMP1_ALARM:
        case RMA_PUMP2_ALARM:
        case RMA_PUMP_STOP_ALARM:
        case RMA_ELEC_VALVE_ALARM:
        case RMA_BOX_MONITORED_LIQUID_ALARM:
        case RMA_PRIM_WATERLEAK_ALARM:
        case RMA_SECD_WATERLEAK_ALARM:
        case RMA_WATERBAG_LINE_ALARM:
        case RMA_PRIM_NO_TRAFFIC_ALARM:
        case RMA_LIQUIDLEVEL_NOWATER_ALARM:
        case RMA_LIQUIDLEVEL_INVALID_ALARM:
        case RMA_GROUP_CONTROL_NET_ALARM:
        case RMA_GCN_INSUFFICIENT_ALARM:
        case RMA_FILTER_PLUGGING_ALARM:
        case RMA_COULOMBMETER_ALARM:
        case RMA_SECD_WATERTEMP_ADIFF_ALARM:
        case RMA_SECD_WATERTEMP_BDIFF_ALARM:
        case RMA_SECD_WATERTEMP_CDIFF_ALARM:
        case RMA_PUMP1_COMM_ALARM:
        case RMA_PUMP2_COMM_ALARM:
        case RMA_SUPPLY_WATER_ALARM:
        case RMA_MEMORYCARD_FULL_ALARM:
            ret = convert_lcs_fault_alarm(dst_list, src_xusbtlv);
            break;
        case RMA_PRIM_TRAFFIC_LOW_ALARM:
        case RMA_PRIM_WATERTEMP_LOW_ALARM:
        case RMA_PRIM_WATERTEMP_HIGH_ALARM:
        case RMA_SECD_WATERTEMP_LOW_ALARM:
        case RMA_SECD_WATERTEMP_HIGH_ALARM:
        case RMA_PRIM_STRESS_HIGH_ALARM:
        case RMA_SYS_STRESS_LOW_ALARM:
        case RMA_SYS_STRESS_HIGH_ALARM:
        case RMA_PUMP1_TRAFFIC_LOW_ALARM:
        case RMA_PUMP2_TRAFFIC_LOW_ALARM:
            ret = convert_lcs_data_alarm(dst_list, src_xusbtlv);
            break;
        case RMA_TH_SERVO_ALARM:
            ret = convert_th_servo_alarm(dst_list, src_xusbtlv);
            break;
        default:
            ret = -2;
            break;
    }

    LOG_MSG(MSG_LOG, "Exited xmboard::convert_alarm() ret=%d", ret);

    return ret;
}

//解析读取到温度数值
int xmboard::parse_temp_data(const std::string& value,std::vector<sensor_data>& vct_sensors)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_temp_data()");

    const char* value_ptr = value.data();
    size_t length = value.length();
    int ret = 0;

    if(length == 0)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_temp_data() length is zero");
        ret = -1;
    }
    else
    {
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        LOG_MSG(MSG_LOG,"xmboard::parse_temp_data() num:%d", num);

        if ((num * 4) != (length - 4))
        {
            LOG_MSG(ERR_LOG,"xmboard::parse_temp_data() value is invalild num:%d length:%d",num,length);
            ret = -1;
        }
        else
        {
            int i = 0;
            char* pay_data = (char*)&value_ptr[4];
            while(i < num)
            {
                float data_val = xbasic::readfloat_bigendian((reinterpret_cast<void*>(pay_data+i*4)), 4);

                sensor_data data;
                data.sensor_id = ++i;
                data.sensor_val = data_val;
                vct_sensors.push_back(data);
            }
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_temp_data() ret:%d",ret);

    return ret;
}


//将读取到温度的tid转换上位机相关tid
int xmboard::convert_tempdata(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_tempdata()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应温度数据
    std::vector<sensor_data> vct_sensordata;
    int rt = parse_temp_data(value,vct_sensordata);
    int vct_size = vct_sensordata.size();

    if(vct_size > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_BOX_OUTLET_TEMP:             // TH板卡腔体出风口温度
                tid = TH_CARD_TMP_VALUE;
                break;
            case RMA_48V_TEMP:                    // TH-48V汇流排温度
                tid = TH_48V_TMP_VALUE;
                break;
            case RMA_CABLE_BOX_OUTLET_TEMP:       //FT、CP 线缆腔体温度
                tid = CABLE_BOX_OUTLET_TEMP;
                break;
            case RMA_CPHF_BOX_TEMP:               //CP HF腔体温度
                tid = CPHF_BOX_TEMP;
                break;
            case RMA_FTTH_BOX_INLET_TEMP:         //FT TH板卡腔体进风口温度
                tid = FTTH_BOX_INLET_TEMP;
                break;
            case RMA_UPPER_HF_TEMP:               // 上HF线缆腔体温度
                tid = HF_UP_TMP_VALUE;
                break;
            case RMA_LOWER_HF_TEMP:               // 下HF线缆腔体温度
                tid = HF_DOWN_TMP_VALUE;
                break;
            case RMA_MF_TOP_OUTLET_TEMP:          // MF板卡顶部出风口温度
                tid = MF_TOP_TMP_VALUE;
                break;
            case RMA_48V_TEMP:                    // 动力源-48v输出汇流排温度
                tid = POW_48V_TMP_VALUE;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vct_size; i++)
        {
            std::string val =  std::to_string(vct_sensordata[i].sensor_id)+ std::string(":") + xbasic::to_string(vct_sensordata[i].sensor_val);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_tempdata() parse_temp_data failed vct_size:%d rt:%d",rt,vct_size);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_tempdata() ret:%d",ret);

    return ret;
}

//解析读取到烟感数值
int xmboard::parse_smoke_data(const std::string& value,std::vector<sensor_data>& vct_sensors)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_smoke_data()");

    const char* value_ptr = value.data();
    size_t length = value.length();
    int ret = 0;

    if(length == 0)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_smoke_data() length is zero");
        ret = -1;
    }
    else
    {
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        LOG_MSG(MSG_LOG,"xmboard::parse_smoke_data() num:%d", num);

        if ((num * 4) != (length - 4))
        {
            LOG_MSG(ERR_LOG,"xmboard::parse_smoke_data() value is invalid num:%d length:%d",num,length);
            ret = -1;
        }
        else
        {
            int i = 0;
            char* pay_data = (char*)&value_ptr[4];
            while(i < num)
            {
                float data_val = xbasic::readfloat_bigendian((reinterpret_cast<void*>(pay_data+i*4)), 4);

                sensor_data data;
                data.sensor_id = ++i;
                data.sensor_val = data_val;
                vct_sensors.push_back(data);
            }
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_smoke_data() ret:%d",ret);

    return ret;
}

//将读取到烟感的tid转换上位机相关tid
int xmboard::convert_smokedata(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_smokedata()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应烟感数据
    std::vector<sensor_data> vct_sensordata;
    int rt = parse_smoke_data(value,vct_sensordata);
    int vct_size = vct_sensordata.size();

    if(vct_size > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_48V_SMOKE:                     // TH-48V线缆腔体出风口烟感
                tid = TH_48V_SMOKE_SENSOR;
                break;
            case RMA_BOX_OUTLET_SMOKE:              // TH板卡腔体出风口烟感
                tid = TH_CARD_SMOKE_SENSOR;
                break;
            case RMA_UPPER_HF_SMOKE:                // FT-HF-A腔体烟感
                tid = HF_UP_SMOKE_SENSOR;
                break;
            case RMA_LOWER_HF_SMOKE:                // FT-HF-B腔体烟感
                tid = HF_DOWN_SMOKE_SENSOR;
                break;
            case RMA_CPHF_BOX_SMOKE:                //CP HF腔体烟感
                tid = CPHF_BOX_SMOKE;
                break;
            case RMA_TOP_OUTLET_SMOKE:              // MF板卡顶部出风口烟感
                tid = MF_TOP_SMOKE_SENSOR;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vct_size; i++)
        {
            std::string val =  std::to_string(vct_sensordata[i].sensor_id)+ std::string(":") + xbasic::to_string(vct_sensordata[i].sensor_val);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_smokedata() parse_smoke_data failed vct_size:%d rt:%d",rt,vct_size);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_smokedata() ret:%d",ret);

    return ret;
}

//解析读取到风扇转速数值
int xmboard::parse_fan_data(const std::string& value,std::vector<sensor_data>& vct_sensors)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_fan_data()");

    const char* value_ptr = value.data();
    size_t length = value.length();
    int ret = 0;

    if(length == 0)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_fan_data() length is zero");
        ret = -1;
    }
    else
    {
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        LOG_MSG(MSG_LOG,"xmboard::parse_fan_data() num:%d", num);

        if ((num * 4) != (length - 4))
        {
            LOG_MSG(ERR_LOG,"xmboard::parse_fan_data() value is invalid num:%d length:%d",num,length);
            ret = -1;
        }
        else
        {
            int i = 0;
            char* pay_data = (char*)&value_ptr[4];
            while(i < num)
            {
                float data_val = xbasic::readfloat_bigendian((reinterpret_cast<void*>(pay_data+i*4)), 4);

                sensor_data data;
                data.sensor_id = ++i;
                data.sensor_val = data_val;
                vct_sensors.push_back(data);
            }
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_fan_data() ret:%d",ret);

    return ret;
}

//将读取到风扇转速的tid转换上位机相关tid
int xmboard::convert_fandata(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_fandata()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应风扇转速数据
    std::vector<sensor_data> vct_sensordata;
    int rt = parse_fan_data(value,vct_sensordata);
    int vct_size = vct_sensordata.size();

    if(vct_size > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_TOP_OUTLET_FAN:                // MF板卡顶部出风口风扇转速
                tid = MF_TOP_OUT_FAN_SPEED;
                break;
            case RMA_BOX_OUTLET_FAN:                // TH板卡腔体出风口风扇转速
                tid = TH_CARD_OUT_FAN_SPEED;
                break;
            case RMA_48V_OUTLET_FAN:                // TH-48v线缆腔体出风口风扇转速
                tid = TH_48V_OUT_FAN_SPEED;
                break;
            case RMA_UPPER_HF_FAN:                  // FT-HF-A腔体风扇转速
                tid = FTHF_UPPER_FAN_SPEED;
                break;
            case RMA_LOWER_HF_FAN:                  // FT-HF-B腔体风扇转速
                tid = FTHF_LOWER_FAN_SPEED;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vct_size; i++)
        {
            std::string val =  std::to_string(vct_sensordata[i].sensor_id)+ std::string(":") + xbasic::to_string(vct_sensordata[i].sensor_val);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_fandata() parse_fan_data failed vct_size:%d rt:%d",rt,vct_size);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_fandata() ret:%d",ret);

    return ret;
}

//解析读取到的cdu数值
int xmboard::parse_cdu_data(const std::string& value,float& cdu_alarm_val)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_cdu_data()");

    const char* str_data = value.data();
    size_t length = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_cdu_data() value length:%d is invalid",length);
        ret = -1;
    }
    else
    {
        float cdu_alarm_val = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_cdu_data() ret:%d",ret);

    return ret;
}

//将读取到的cdu数据的tid转换上位机相关tid
int xmboard::convert_cdudata(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_cdudata()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应cdu数据
    float cdu_val;
    int rt = parse_cdu_data(value,cdu_val);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            //case RMA_CDU_PUMP_PRESSURE:         // CDU状态水泵压力
            //    tid = CDU_STATUS_WATER_STRESS;
            //    break;
            //case RMA_CDU_TEMP:                  // CDU状态温度
            //    tid = CDU_STATUS_TMP;
            //    break;
            //case RMA_CDU_TRAFFIC:               // CDU状态流量
            //    tid = CDU_STATUS_STREAM;
            //    break;
            default:
                break;
        }

        std::string val = xbasic::to_string(cdu_val);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_cdudata() parse_cdu_data failed rt:%d",rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_cdudata() ret:%d",ret);

    return ret;
}

//解析读取到的48v动力源数值{电压、电流、功耗}
int xmboard::parse_48vps_data(const std::string& value,std::vector<ps48v_info>& vct_ps)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_48vps_data()");

    const char* str_data = value.data();
    size_t length = value.length();
    int ret = 0;

    //将value格式定义,传感器编号(4个字节)+(电压、电流、功耗)(4个字节float),8个字节为一组策略
    int len = value.length();
    if(len % 8 != 0)
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xmboard::parse_48vps_data() 48v ps value len(%d) is wrong",len);
    }
    else
    {
        for(int i =0 ; i < len; )
        {
            ps48v_info ps;
            ps.m_sersor_id = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(&value[i])),4);
            i = i + 4;
            ps.m_value = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(&value[i])),4);
            i = i+ 4;
            vct_ps.push_back(ps);
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_48vps_data() ret:%d",ret);

    return ret;
}

//将读取到的48v动力源数据的tid转换上位机相关tid
int xmboard::convert_48vps_data(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_48vps_data()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应48V动力源数据
    std::vector<ps48v_info> vct_ps;
    int rt = parse_48vps_data(value,vct_ps);
    int vct_size = vct_ps.size();

    if(vct_size > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_48V_VOLTAGE:                 // 48V动力源电压
                tid = POW_48V_VOL;
                break;
            case RMA_48V_CURRENT:                 // 48V动力源电流
                tid = POW_48V_CURRENT;
                break;
            //case RMA_48V_PS_TEMP:                 // 48V动力源温度
            //    tid = POW_48V_TMP;
            //    break;
            case RMA_48V_TDP:                     // 48V动力源功耗
                tid = POW_48V_W;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vct_size; i++)
        {
            std::string val =  std::to_string(vct_ps[i].m_sersor_id)+ std::string(":") + xbasic::to_string(vct_ps[i].m_value);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_48vps_data() parse_48vps_data failed vct_size:%d rt:%d",vct_size,rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_48vps_data() ret:%d",ret);

    return ret;
}

//解析读取到的厂务水数值
/*
int xmboard::parse_factorywater_data(const std::string& value,float& cdu_alarm_val)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_factorywater_data()");

    const char* str_data = value.data();
    size_t length = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_factorywater_data() value length:%d is invalid",length);
        ret = -1;
    }
    else
    {
        float cdu_alarm_val = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_factorywater_data() ret:%d",ret);

    return ret;
}

//将读取到的厂务水数据的tid转换上位机相关tid
int xmboard::convert_factorywater_data(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_factorywater_data()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应厂务水数据
    float factorywater_val;
    int rt = parse_factorywater_data(value,factorywater_val);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_FACTORYWATER_TEMP:           // 非动力源厂务水温度
                tid = NO_POW_WATER_TMP;
                break;
            case RMA_FACTORYWATER_TRAFFIC:        // 非动力源厂务水流量
                tid = NO_POW_WATER_STREAM;
                break;
            case RMA_FACTORYWATER_PRESSURE:       // 非动力源厂务水压力
                tid = NO_POW_WATER_STRESS;
                break;
            default:
                break;
        }

        std::string val = xbasic::to_string(factorywater_val);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_factorywater_data() parse_factorywater_data failed rt:%d",rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_factorywater_data() ret:%d",ret);

    return ret;
}
*/

//解析读取到的三相电压，电流值
int xmboard::parse_threephase_data(const std::string& value,std::vector<three_phase>& vct_phase)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_threephase_data()");

    const char* value_ptr = value.data();
    size_t length = value.length();
    int ret = 0;

    if(length == 0)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_threephase_data() length is zero");
        ret = -1;
    }
    else
    {
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        LOG_MSG(MSG_LOG,"xmboard::parse_fan_data() num:%d", num);

        if ((num * 4) != (length - 4))
        {
            LOG_MSG(ERR_LOG,"xmboard::parse_threephase_data() value is invalid num:%d length:%d",num,length);
            ret = -1;
        }
        else
        {
            int i = 0;
            char* pay_data = (char*)&value_ptr[4];
            while(i < num)
            {
                float data_val = xbasic::readfloat_bigendian((reinterpret_cast<void*>(pay_data+i*4)), 4);

                three_phase data;
                data.m_id = ++i;
                data.m_value = data_val;
                vct_phase.push_back(data);
            }
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_threephase_data() ret:%d",ret);

    return ret;
}

//将读取到的三相电压，电流的tid转换上位机相关tid
int xmboard::convert_threephase_data(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_threephase_data()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应阀值数据
    std::vector<three_phase> vct_threephase;
    int rt = parse_threephase_data(value,vct_threephase);
    int vct_size = vct_threephase.size();

    if(vct_size > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_THREEPHASE_VOLTAGE:            // 三相电压值
                tid = THREE_PHASE_VOLTAGE;
                break;
            case RMA_THREEPHASE_CURRENT:            // 三相电流值
                tid = THREE_PHASE_CURRENT;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vct_size; i++)
        {
            std::string val =  std::to_string(vct_threephase[i].m_id)+ std::string(":") + xbasic::to_string(vct_threephase[i].m_value);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_threephase_data() parse_threephase_data failed vct_size:%d rt:%d",rt,vct_size);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_threephase_data() ret:%d",ret);

    return ret;
}

//解析读取到的非动力源UPS数值
int xmboard::parse_ups_data(const std::string& value,float& cdu_alarm_val)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_ups_data()");

    const char* str_data = value.data();
    size_t length = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_ups_data() value length:%d is invalid",length);
        ret = -1;
    }
    else
    {
        float cdu_alarm_val = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_ups_data() ret:%d",ret);

    return ret;
}

//将读取到的非动力源UPS数据的tid转换上位机相关tid
int xmboard::convert_ups_data(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_ups_data()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应ups数据
    float ups_val;
    int rt = parse_ups_data(value,ups_val);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_UPS_DATA:                // 非动力源UPS数据
                tid = NO_POW_UPS_DATA;
                break;
            default:
                break;
        }

        std::string val = xbasic::to_string(ups_val);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_ups_data() parse_ups_data failed rt:%d",rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_ups_data() ret:%d",ret);

    return ret;

}


//解析读取到的水浸数值
int xmboard::parse_waterlogging_data(const std::string& value,float& cdu_alam_val)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_waterlogging_data()");

    const char* str_data   = value.data();
    size_t length          = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_waterlogging_data() value length:%d is invalid",length);
        ret = -1;
    }
    else
    {
        float cdu_alam_val = xbasic::readfloat_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_waterlogging_data() ret:%d",ret);

    return ret;

}


//将读取到的水浸数据的tid转换上位机相关tid
int xmboard::convert_waterlogging_data(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_waterlogging_data()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应水浸数据
    float waterlogging_val;
    int rt = parse_waterlogging_data(value,waterlogging_val);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_CDU_WATERLOGGING1:        // 水浸状态
            case RMA_CDU_WATERLOGGING2:
                tid = FLOODING_STATUS;
                break;
            default:
                break;
        }

        std::string val = xbasic::to_string(waterlogging_val);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_waterlogging_data() parse_waterlogging_data failed rt:%d",rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_waterlogging_data() ret:%d",ret);

    return ret;

}

//解析读取到的testerheader hifix数值
int xmboard::parse_hifix_data(const std::string& value,int& hifix_status)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_hifix_data()");

    const char* str_data   = value.data();
    size_t length          = value.length();
    int ret = 0;
    if (4 != length)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_hifix_data() value length:%d is invalid",length);
        ret = -1;
    }
    else
    {
        hifix_status = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(str_data)), 4);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_hifix_data() ret:%d",ret);

    return ret;

}

//将读取到的TesterHeader hifix数据的tid转换上位机相关tid
int xmboard::convert_hifix_data(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_hifix_data()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应testerheader hifix数据
    int hifix_staus;
    int rt = parse_hifix_data(value,hifix_staus);

    if(rt == 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_HIFIX_LOCK:                // TH板卡testheader的hifix锁定状态
                tid = TH_HIFIX_LOCK_STATUS;
                break;
            default:
                break;
        }

        std::string val = std::to_string(hifix_staus);
        boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
        dst_list.push_back(new_data);
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_hifix_data() parse_hifix_data failed rt:%d",rt);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_hifix_data() ret:%d",ret);

    return ret;

}

//将读取到的简单数据的tid转换上位机相关tid
int xmboard::convert_basic_data(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_basic_data()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();
    int sz = value.length();
    std::string val;
    if(sz > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_POWER_STATUS:         // 电源状态
                tid = POWER_STATUS;
                val = std::to_string((int)value[0]);
                break;
            case RMA_POS_STATUS:           // 板在位信息
                tid = BOARD_POS_STATUS;
                val = std::to_string((int)value[0]);
                break;
            case RMA_BOARD_TYPE:           // 单板类型
                tid = BOARD_TYPE;
                val = std::to_string((int)value[0]);
                break;
            case RMA_BOARD_SN:             // 单板SN
                tid = BOARD_SN;
                val.assign(value.c_str(),strlen(value.c_str()));
                break;
            case RMA_HW_VER:               // 单板硬件版本
                tid = BOARD_HW_VER;
                val.assign(value.c_str(),strlen(value.c_str()));
                break;
            case RMA_SW_VER:               // 单板软件版本
                tid = BOARD_SOFT_VER;
                val.assign(value.c_str(),strlen(value.c_str()));
                break;
            case RMA_SLOT:                 // 单板slot号
                tid = BOARD_SLOT;
                val = std::to_string((int)value[0]);
            default:
                break;
        }

        if(!val.empty())
        {
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
        else
        {
            LOG_MSG(ERR_LOG,"xmboard::convert_basic_data() val is empty tid:%d,sz:%d",tid,sz);
            ret = -1;
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_basic_data() failed sz:%d",sz);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_basic_data() ret:%d",ret);

    return ret;

}

//解析读取到液冷系统工作模式
int xmboard::parse_lcs_mode(const std::string& value,std::vector<lcs_mode>& vct_mode)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_lcs_mode()");

    const char* value_ptr  = value.data();
    size_t length          = value.length();
    int ret = 0;

    if(length == 0)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_lcs_mode() length is zero");
        ret = -1;
    }
    else
    {
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        LOG_MSG(MSG_LOG,"xmboard::parse_lcs_mode() num:%d", num);

        if (num != (length - 4))
        {
            LOG_MSG(ERR_LOG,"xmboard::parse_lcs_mode() value is invalid num:%d length:%d",num,length);
            ret = -1;
        }
        else
        {
            int i = 0;
            char* pay_data = (char*)&value_ptr[4];
            while(i < num)
            {
                int data_val = xbasic::read_bigendian((reinterpret_cast<void*>)(pay_data+i), 1);

                lcs_mode mode;
                mode.m_id = ++i;
                mode.m_mode = data_val;

                vct_mode.push_back(mode);
            }
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_lcs_data() ret:%d",ret);

    return ret;

}

//将读取到液冷系统工作模式相关tid转换上位机相关tid
int xmboard::convert_lcs_mode(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_lcs_mode()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应液冷数据
    std::vector<lcs_mode> vct_lcsmode;
    int rt = parse_lcs_mode(value,vct_lcsmode);
    int vct_size = vct_lcsmode.size();

    if(vct_size > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_CDU_WORKMODE:                 //工作模式
                tid = MF_CDU_WORKMODE;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vct_size; i++)
        {
            std::string val = std::to_string(vct_lcsmode[i].m_id)+ std::string(":") + std::to_string(vct_lcsmode[i].m_mode);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_lcs_mode() parse_lcs_mode failed vct_size:%d rt:%d",rt,vct_size);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_lcs_mode() ret:%d",ret);

    return ret;

}

//解析读取到液冷系统CDU相关数据
int xmboard::parse_lcs_data(const std::string& value,std::vector<sensor_data>& vct_sensors)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_lcs_data()");

    const char* value_ptr  = value.data();
    size_t length          = value.length();
    int ret = 0;

    if(length == 0)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_lcs_data() length is zero");
        ret = -1;
    }
    else
    {
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        LOG_MSG(MSG_LOG,"xmboard::parse_lcs_data() num:%d", num);

        if ((num * 4) != (length - 4))
        {
            LOG_MSG(ERR_LOG,"xmboard::parse_lcs_data() value is invalid num:%d length:%d",num,length);
            ret = -1;
        }
        else
        {
            int i = 0;
            char* pay_data = (char*)&value_ptr[4];
            while(i < num)
            {
                float data_val = xbasic::readfloat_bigendian((reinterpret_cast<void*>)(pay_data+i*4), 4);

                sensor_data data;
                data.sensor_id = ++i;
                data.sensor_val = data_val;

                vct_sensors.push_back(data);
            }
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_lcs_data() ret:%d",ret);

    return ret;

}

//将读取到液冷系统CDU相关tid转换上位机相关tid
int xmboard::convert_lcs_data(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_lcs_data()");

    int ret = 0;
    std::string value = src_xusbtlv->get_data();

    //1、得到对应lcs液冷数据
    std::vector<sensor_data> vct_sensordata;
    int rt = parse_lcs_data(value,vct_sensordata);
    int vct_size = vct_sensordata.size();

    if(vct_size > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src_xusbtlv->m_tid)
        {
            case RMA_CDU_PUMP1_SPEED:                //水泵1转速
                tid = MF_CDU_PUMP1_SPEED;
                break;
            case RMA_CDU_PUMP2_SPEED:                //水泵2转速
                tid = MF_CDU_PUMP2_SPEED;
                break;
            case RMA_CDU_ELEC_VALVE_COMMAND:         //电动阀命令
                tid = MF_CDU_ELEC_VALVE_COMMAND;
                break;
            case RMA_CDU_ELEC_VALVE_FEEDBACK:        //电动阀反馈
                tid = MF_CDU_ELEC_VALVE_FEEDBACK;
                break;
            case RMA_CDU_PRIMARY_WATER_TEMP:         //一次侧供水温度
                tid = MF_CDU_PRIMARY_WATER_TEMP;
                break;
            case RMA_CDU_SECONDARY_WATER_T2A:        //二次侧供水温度T2a
                tid = MF_CDU_SECONDARY_WATER_T2A;
                break;
            case RMA_CDU_SECONDARY_WATER_T2B:        //二次侧供水温度T2b
                tid = MF_CDU_SECONDARY_WATER_T2B;
                break;
            case RMA_CDU_SECONDARY_WATER_T2C:        //二次侧供水温度T2c
                tid = MF_CDU_SECONDARY_WATER_T2C;
                break;
            case RMA_CDU_SECONDARY_WATER_TEMP:       //二次侧供水温度
                tid = MF_CDU_SECONDARY_WATER_TEMP;
                break;
            case RMA_CDU_ENV_TEMP:                   //环境温度
                tid = MF_CDU_ENV_TEMP;
                break;
            case RMA_CDU_RELATIVE_HUMIDITY:          //环境相对湿度
                tid = MF_CDU_RELATIVE_HUMIDITY;
                break;
            case RMA_CDU_ENV_DEWPOINT_TEMP:          //环境露点温度
                tid = MF_CDU_ENV_DEWPOINT_TEMP;
                break;
            case RMA_CDU_SECONDARY_BACKWATER_T4:     //二次侧回水温度T4
                tid = MF_CDU_SECONDARY_BACKWATER_T4;
                break;
            case RMA_CDU_SECONDARY_BACKWATER_T5:     //二次侧回水温度T5
                tid = MF_CDU_SECONDARY_BACKWATER_T5;
                break;
            case RMA_CDU_SECONDARY_STRESS_PS1:       //二次侧回水压力PS1
                tid = MF_CDU_SECONDARY_STRESS_PS1;
                break;
            case RMA_CDU_SECONDARY_STRESS_PS2:       //二次侧水泵进口压力PS2
                tid = MF_CDU_SECONDARY_STRESS_PS1;
                break;
            case RMA_CDU_SECONDARY_STRESS_PS3:       //二次侧供水压力PS3
                tid = MF_CDU_SECONDARY_STRESS_PS3;
                break;
            case RMA_CDU_SECONDARY_PS3_PS1:          //二次侧供水压差PS3-PS1
                tid = MF_CDU_SECONDARY_PS3_PS1;
                break;
            case RMA_CDU_SECONDARY_PS1_PS2:          //二次侧过滤器压差PS1-PS2
                tid = MF_CDU_SECONDARY_PS1_PS2;
                break;
            case RMA_CDU_PRIMARY_TRAFFIC:            //一次侧流量
                tid = MF_CDU_PRIMARY_TRAFFIC;
                break;
            case RMA_CDU_SECONDARY_TRAFFIC:          //二次侧流量
                tid = MF_CDU_SECONDARY_TRAFFIC;
                break;
            case RMA_CDU_SECONDARY_LOAD:             //二次侧负载
                tid = MF_CDU_SECONDARY_LOAD;
                break;
            case RMA_CDU_PRIMARY_LOAD:               //一次侧负载
                tid = MF_CDU_PRIMARY_LOAD;
                break;
            case RMA_CDU_TEMP_SET_VALUE:             //温度设定值
                tid = MF_CDU_TEMP_SET_VALUE;
                break;
            default:
                break;
        }

        for(int i = 0 ; i < vct_size; i++)
        {
            std::string val = std::to_string(vct_sensordata[i].sensor_id)+ std::string(":") + xbasic::to_string(vct_sensordata[i].sensor_val);
            boost::shared_ptr<xtvl> new_data(new xtv1(tid,val.length(),val.data(),true));
            dst_list.push_back(new_data);
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::convert_lcs_data() parse_lcs_data failed vct_size:%d rt:%d",rt,vct_size);
        ret = -1;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_lcs_data() ret:%d",ret);

    return ret;

}

//解析读取到TH电机位置
int xmboard::parse_motorpos_data(const std::string& value,std::vector<motor_position>& vct_motorpos)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::parse_motorpos_data()");

    const char* value_ptr  = value.data();
    size_t length          = value.length();
    int ret = 0;

    if(length == 0)
    {
        LOG_MSG(ERR_LOG,"xmboard::parse_motorpos_data() length is zero");
        ret = -1;
    }
    else
    {
        if (length % 9 != 0)
        {
            LOG_MSG(ERR_LOG,"xmboard::parse_motorpos_data() value is invalid,length:%d is not multiple of 9",length);
            ret = -1;
        }
        else
        {
            int i = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(i < length)
            {
                int motor_id = xbasic::read_bigendian((reinterpret_cast<void*>)(pay_data+i), 1);
                i = i + 1;
                int motor_leftpos = xbasic::read_bigendian((reinterpret_cast<void*>)(pay_data+i), 4);
                i = i + 4;
                int motor_rightpos = xbasic::read_bigendian((reinterpret_cast<void*>)(pay_data+i), 4);
                i = i + 4;

                motor_position data;
                data.m_motorid = motor_id;
                data.m_leftpos = motor_leftpos;
                data.m_rightpos = motor_rightpos;

                vct_motorpos.push_back(data);

                LOG_MSG(MSG_LOG," xmboard::parse_motorpos_data() m_motorid:%d m_leftpos:%d,m_rightpos:%d",data.m_motorid,data.m_leftpos,data.m_rightpos);
            }
        }
    }

    int vct_sze = vct_motorpos.size();

    LOG_MSG(MSG_LOG,"Exited xmboard::parse_motorpos_data() size:%d ret:%d",vct_sze,ret);

    return ret;

}

//将read读取的数据的tid转换上位机相关tid
int xmboard::convert_data(std::list<boost::shared_ptr<xtvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::convert_data()");

    int ret = 0;
    switch(src_xusbtlv->m_tid)
    {
        case RMA_BOX_OUTLET_TEMP:
        case RMA_BOX_48V_TEMP:
        case RMA_UPPER_HF_TEMP:
        case RMA_LOWER_HF_TEMP:
        case RMA_HF_TOP_OUTLET_TEMP:
        case RMA_48V_TEMP:
            ret = convert_tempdata(dst_list,src_xusbtlv);
            break;
        case RMA_48V_SMOKE:
        case RMA_BOX_OUTLET_SMOKE:
        case RMA_UPPER_HF_SMOKE:
        case RMA_LOWER_HF_SMOKE:
        case RMA_TOP_OUTLET_SMOKE:
            ret = convert_smokedata(dst_list,src_xusbtlv);
            break;
        case RMA_TOP_OUTLET_FAN:
        case RMA_BOX_OUTLET_FAN:
        case RMA_48V_OUTLET_FAN:
        case RMA_UPPER_HF_FAN:
        case RMA_LOWER_HF_FAN:
            ret = convert_fandata(dst_list,src_xusbtlv);
            break;
        //case RMA_CDU_PUMP_PRESSURE:
        //case RMA_CDU_TEMP:
        //case RMA_CDU_TRAFFIC:
        //    ret = convert_cdudata(dst_list,src_xusbtlv);
        //    break;
        case RMA_48V_VOLTAGE:
        case RMA_48V_CURRENT:
        //case RMA_48V_PS_TEMP:
        case RMA_48V_TDP:
            ret = convert_48vps_data(dst_list,src_xusbtlv);
            break;
        case RMA_THREEPHASE_VOLTAGE:
        case RMA_THREEPHASE_CURRENT:
            ret = convert_threephase_data(dst_list,src_xusbtlv);
            break;
        //case RMA_FACTORYWATER_TEMP:
        //case RMA_FACTORYWATER_TRAFFIC:
        //case RMA_FACTORYWATER_PRESSURE:
        //    ret = convert_factorywater_data(dst_list,src_xusbtlv);
        //    break;
        //case RMA_UPS_DATA:
        //    ret = convert_ups_data(dst_list,src_xusbtlv);
        //    break;
        case RMA_CDU_WATERLOGGING1:
        case RMA_CDU_WATERLOGGING2:
            ret = convert_waterlogging_data(dst_list,src_xusbtlv);
            break;
        case RMA_HIFIX_LOCK:
            ret = convert_hifix_data(dst_list,src_xusbtlv);
            break;
        case RMA_POWER_STATUS:
        case RMA_POS_STATUS:
        case RMA_BOARD_TYPE:
        case RMA_BOARD_SN:
        case RMA_HW_VER:
        case RMA_SW_VER:
        case RMA_SLOT:
            ret = convert_basic_data(dst_list,src_xusbtlv);
            break;
        case RMA_CDU_WORKMODE:
            ret = convert_lcs_mode(dst_list,src_xusbtlv);
            break;
        case RMA_CDU_PUMP1_SPEED:
        case RMA_CDU_PUMP2_SPEED:
        case RMA_CDU_ELEC_VALVE_COMMAND:
        case RMA_CDU_ELEC_VALVE_FEEDBACK:
        case RMA_CDU_PRIMARY_WATER_TEMP:
        case RMA_CDU_SECONDARY_WATER_T2A:
        case RMA_CDU_SECONDARY_WATER_T2B:
        case RMA_CDU_SECONDARY_WATER_T2C:
        case RMA_CDU_SECONDARY_WATER_TEMP:
        case RMA_CDU_ENV_TEMP:
        case RMA_CDU_RELATIVE_HUMIDITY:
        case RMA_CDU_ENV_DEWPOINT_TEMP:
        case RMA_CDU_SECONDARY_BACKWATER_T4:
        case RMA_CDU_SECONDARY_BACKWATER_T5:
        case RMA_CDU_SECONDARY_STRESS_PS1:
        case RMA_CDU_SECONDARY_STRESS_PS2:
        case RMA_CDU_SECONDARY_STRESS_PS3:
        case RMA_CDU_SECONDARY_PS3_PS1:
        case RMA_CDU_SECONDARY_PS1_PS2:
        case RMA_CDU_PRIMARY_TRAFFIC:
        case RMA_CDU_SECONDARY_TRAFFIC:
        case RMA_CDU_SECONDARY_LOAD:
        case RMA_CDU_PRIMARY_LOAD:
        case RMA_CDU_TEMP_SET_VALUE:
            ret = convert_lcs_data(dst_list,src_xusbtlv);
            break;
        default:
            ret = -2;
            break;
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::convert_data() ret:%d",ret);

    return ret;

}

int xmboard::handle_alarm_request_message(xusbpackage *the_pack)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::handle_alarm_request_message()");

    int ret = 0;
    //alarm命令处理
    if((the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_ALARM)
    {
        LOG_MSG(MSG_LOG,"xmboard::handle_alarm_request_message() receive alarm message.");
        //将告警数据发送给平台
        std::list<boost::shared_ptr<xusb_tvl> > list_xusbtlv;
        std::list<boost::shared_ptr<xtvl> > list_xtlv;
        int size = the_pack->get_tlv_data(list_xusbtlv);
        if(size > 0)
        {
            for(auto xusbtlv_tmp : list_xusbtlv)
            {
                //将告警相关xusb_tlv数据转换成平台xtlv数据
                convert_data(list_xtlv,xusbtlv_tmp);

                if((xusbtlv_tmp->m_tid == RMA_TH_SERVO_ALARM) && ((the_pack->m_dstid & BOARDTYPE_MASK) == BOARDTYPE_TH_MONITOR))
                {
                    //将TH伺服器的告警通过http result接口上报给平台
                    send_thservo_alarm(xusbtlv_tmp->m_value);
                }
            }
        }

        xpackage pack(xpackage::MT_NOTIFY,xpackage::MC_ALARM,PROTO_VERSION,0,the_pack->m_srcid); //构造包
        pack.add_tlv_data(list_xtlv);

        int len = send(PORT_PLATFORMFROM,(xpacket*)&pack);
        if(len <= 0 )
        {
            ret = -1;
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::handle_alarm_request_message() ret:%d",ret);

    return ret;

}

//上报TH伺服器告警
int xmboard::send_thservo_alarm(const std::string value)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::send_thservo_alarm()");
    int ret = 0;

    //将TH伺服器的告警通过http result接口上报给平台
    mgr_handler_dev * handler_dev_mgr = mgr_handler_dev::get_instance();
    servo_err alarm;
    int rt = parse_th_servo_data_alarm(value,alarm);
    if(rt != -1)
    {
        handler_dev_mgr->servo_alarm(alarm.m_servo_id,alarm.m_errcode);
    }

    LOG_MSG(MSG_LOG,"exited xmboard::send_thservo_alarm() ret=%d",ret);
    return ret;
}

//南向应答消息处理
int xmboard::handle_south_response(xusbpackage *pack)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::handle_south_response()");
    int ret = -1;
    xusbpackage *the_pack = static_cast<xusbpackage *>(pack);
    if(!the_pack->crc_checksum())
    {
        LOG_MSG(ERR_LOG,"xmboard::handle_south_response(),message crc check failed!");
        return ret;
    }

    //READ命令处理
    if((the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_READ)
    {
        LOG_MSG(MSG_LOG,"xmboard::handle_south_response() receive read message.");
        std::list<boost::shared_ptr<xusb_tvl> > list_xusbtlv;
        std::list<boost::shared_ptr<xtvl> > list_xtlv;
        int size = the_pack->get_tlv_data(list_xusbtlv);
        if(size > 0)
        {
            //保存xusb tlv数据
            save_tlv_data(m_id,list_xusbtlv,xusbpackage::MC_READ);

            //将xusb tlv数据转换成平台xtlv格式数据
            for(auto xusbtlv_tmp : list_xusbtlv)
            {
                //将读取命令字的读取到固有和实时数据相关xusb tlv数据转换成平台xtlv数据
                convert_data(list_xtlv,xusbtlv_tmp);
                //将安全策略相关xusb tlv数据转换成平台xtlv数据
                //convert_securitypolicy(list_xtlv,xusbtlv_tmp);
            }

            //发送给平台
            xpackage pack(xpackage::MT_NOTIFY,xpackage::MC_READ,PROTO_VERSION,0,the_pack->m_srcid); //构造包
            pack.add_tlv_data(list_xtlv);
            int len = send(PORT_PLATFROM,(xpackage*)&pack);
            if(len > 0)
            {
                ret = 0;
            }
        }
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::handle_south_response() ret=%d",ret);
    return ret;
}

//南向通知消息处理
int xmboard::handle_south_notify(xusbpackage *pack)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::handle_south_notify()");
    int ret = -1;
    xusbpackage *the_pack = static_cast<xusbpackage *>(pack);
    if(!the_pack->crc_checksum())
    {
        LOG_MSG(ERR_LOG,"xmboard::handle_south_notify(),message crc check failed!");
        return ret;
    }

    //READ命令处理
    if((the_pack->m_msg_cmd & MSG_CMD_MASK) == xusbpackage::MC_READ)
    {
        LOG_MSG(MSG_LOG,"xmboard::handle_south_notify() receive read message.");
        std::list<boost::shared_ptr<xusb_tvl> > list_xusbtlv;
        int size = the_pack->get_tlv_data(list_xusbtlv);
        if(size > 0)
        {
            //保存xusb tlv数据
            save_tlv_data(m_id,list_xusbtlv,xusbpackage::MC_READ);
            ret = 0;
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"xmboard::handle_south_notify(),m_msg_cmd=%d is invalid",the_pack->m_msg_cmd);
    }

    LOG_MSG(MSG_LOG,"Exited xmboard::handle_south_notify() ret=%d",ret);
    return ret;
}

int xmboard::get_id()
{
    return m_id;
}

void xmboard::set_id(int id)
{
    m_id = id;
}

std::string xmboard::get_port_name()
{
    return m_port_name;
}

int xmboard::save_tlv_data(unsigned short board_id,std::list<boost::shared_ptr<xusb_tvl> >& list_tlv,unsigned char cmd)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::save_tlv_data()");
    boost::shared_ptr<xboard> the_board = m_board_mgr->add_board(board_id);
    if(the_board != NULL)
    {
        the_board->set_tlv_data(list_tlv,cmd); //将TLV数据设置到单板对象
    }
    LOG_MSG(MSG_LOG,"Exited xmboard::save_tlv_data()");
    return 0;
}

boost::shared_ptr<xusb_tvl> xmboard::find_data(unsigned short tid)
{
    int boardid = m_id;
    boost::shared_ptr<xboard> the_board = m_board_mgr->add_board(m_id);
    boost::shared_ptr<xusb_tvl> tvl;
    if(the_board != NULL)
    {
        tvl = the_board->find_data((int)tid);
    }

    return tvl;
}

void xmboard::set_port_name(std::string port_name)
{
    if(m_port_name != port_name)
    {
        m_port_name = port_name;
    }
}

time_t xmboard::get_activetm()
{
    return m_activetm;
}

void xmboard::set_activetm(time_t activetm)
{
    m_activetm = activetm;
}

std::string xmboard::get_ota_fw_name(int ota_type) //获得该单板的OTA升级文件名
{
    switch(ota_type)
    {
        case OTA_TYPE_FTTH_MCU:
            return "FTTH_MCU";
        case OTA_TYPE_FTMF_MCU:
            return "FTMF_MCU";
        case OTA_TYPE_CPTH_MCU:
            return "CPTH_MCU";
        case OTA_TYPE_CPMF_MCU:
            return "CPMF_MCU";
    }
    return "";
}

std::string xmboard::get_board_name(int board_id) //获得单板类型名称
{
    if(board_id == BOARD_X86)        return ("B-X86");
    else if(board_id == BOARD_STV)   return ("B-STV");
    else if(board_id >= BOARD_TST(0))return ("B-TST");
    return ("B-UNK");
}

bool xmboard::is_ota_session_status(int status)
{
    bool ret = false;
    if((status >= OTA_SESSION_STATUS_STARTED) && (status <= OTA_SESSION_STATUS_END))
    {
        ret = true;
    }
    return ret;
}

int xmboard::set_ota_transed_length(unsigned int len)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::set_ota_transed_length()");
    if(m_ota_session == NULL) {
        LOG_MSG(WRN_LOG,"xmboard::set_ota_transed_length() ota session is not exited!");
        return -1;
    }
    //boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    m_ota_session->set_ota_transed_len(len);
    LOG_MSG(MSG_LOG,"Exited xmboard::set_ota_transed_length()");
    return 0;
}

int xmboard::set_ota_session_status(int status)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::set_ota_session() status=%d",status);
    if(m_ota_session == NULL) {
        LOG_MSG(WRN_LOG,"mgr_usb::set_ota_session() ota session is not exited!");
        return -1;
    }
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    int previous_status = m_ota_session->get_session_status();
    if(previous_status == OTA_SESSION_STATUS_CANCLED || (previous_status == OTA_SESSION_STATUS_END) || (previous_status == OTA_SESSION_STATUS_CALFILE_END)) {
        LOG_MSG(WRN_LOG,"mgr_usb::set_ota_session() previous status(%d) is OTA_SESSION_STATUS_CANCLED or OTA_SESSION_STATUS_END,can't change to status(%d)",previous_status,status);
        return -1;
    }
    else
    {
        m_ota_session->set_session_status(status);
        LOG_MSG(MSG_LOG,"mgr_usb::set_ota_session() status from (%d) to (%d)",previous_status,status);
    }
    LOG_MSG(MSG_LOG,"Exited mgr_usb::set_ota_session()");
    return 0;
}

boost::shared_ptr<ota_session> xmboard::add_ota_session(int status,int ota_type,int ota_version,unsigned int total_len)
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::add_ota_session()");
    if(m_ota_session != NULL) {
        LOG_MSG(MSG_LOG,"xmboard::add_ota_session(),ota_type=%d has existed.",ota_type);
        return m_ota_session; //已经存在
    }
    LOG_MSG(WRN_LOG,"chip add new add ota_session:ota_type=0x%02X",ota_type);
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    boost::shared_ptr<ota_session> new_session(new ota_session(status,ota_type,ota_version,total_len));
    m_ota_session = new_session;
    lock.unlock();
    LOG_MSG(MSG_LOG,"Exitt xmboard::add_ota_session()");
    return m_ota_session;
}

boost::shared_ptr<ota_session> xmboard::get_ota_session()
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::del_ota_session()");
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    LOG_MSG(MSG_LOG,"exited xmboard::del_ota_session()");
    return m_ota_session;
}

int xmboard::del_ota_session()
{
    LOG_MSG(MSG_LOG,"Enter into xmboard::del_ota_session()");
    boost::unique_lock<boost::shared_mutex> lock(m_mux_session);    //写锁
    m_ota_session.reset();
    LOG_MSG(MSG_LOG,"exited xmboard::del_ota_session()");
    return 0;
}

#endif
