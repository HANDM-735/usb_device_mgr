#include "xusbadapter_package.h"
#include "xbasicasio.hpp"
#include "mgr_log.h"
#include "mgr_usb_def.h"

xusbadapter_bin_packer::xusbadapter_bin_packer()
{
    //LOG_MSG(WRN_LOG,"xusbadapter_bin_packer::xusbadapter_bin_packer() construct");
}

xusbadapter_bin_packer::~xusbadapter_bin_packer()
{
    //LOG_MSG(WRN_LOG,"xusbadapter_bin_packer::~xusbadapter_bin_packer() deconstruct");
}

boost::shared_ptr<const std::string> xusbadapter_bin_packer::pack_data(const char *pdata,size_t datalen,PACKWAY pack_way)
{
    boost::shared_ptr<const std::string> ppack;
    IF(NULL == pdata || 0 == datalen) return ppack;
    int totalLen =(int)datalen;
    std::string *raw_str =new std::string();
    raw_str->reserve(totalLen); //按原始数据打包
    raw_str->append(pdata,totalLen);
    ppack.reset(raw_str);
    return ppack;
}

xusbadapter_bin_unpacker::xusbadapter_bin_unpacker()
{
    //LOG_MSG(WRN_LOG,"xusbadapter_bin_unpacker::xusbadapter_bin_unpacker() construct");
    m_signed_len =(size_t)-1;
    m_data_len =0;
}

xusbadapter_bin_unpacker::~xusbadapter_bin_unpacker()
{
    //LOG_MSG(WRN_LOG,"xusbadapter_bin_unpacker::~xusbadapter_bin_unpacker() deconstruct");
}

void xusbadapter_bin_unpacker::reset_data()
{
    m_signed_len =(size_t)-1;
    m_data_len = 0;
}

bool xusbadapter_bin_unpacker::unpack_data(size_t bytes_data, boost::container::list<boost::shared_ptr<const std::string> >& list_pack)
{
    m_data_len += bytes_data; //本次收到的字节数
    bool unpack_ok = true;
    char *buff_head = m_raw_buff.begin();
    char *buff_data = buff_head;
    while(unpack_ok)
    {
        if(m_signed_len !=(size_t)-1) //包头解析完，现在解析包体
        {
            if(xbasic::read_bigendian(buff_data,2) != USB_ADAPTER_HEADER) {
                unpack_ok =false;
                xbasic::debug_output("xusbadapter_bin_unpacker::unpack_data header failed!\n");
                break;
            } //包头错误

            size_t pack_len =m_signed_len + USB_ADAPTER_HEADER_LEN;
            if(m_data_len <pack_len) break; //不够一个包长，退出，在现有基础上继续接收
            list_pack.push_back(boost::shared_ptr<std::string>(new std::string(buff_data, pack_len))); //将这个包返回上层
            std::advance(buff_data,pack_len); //增加定位偏移继续分析
            m_data_len -=pack_len;
            m_signed_len =(size_t)-1;
        }
        else if(m_data_len >= USB_ADAPTER_HEADER_LEN) //已经收到了部分数据
        {
            if(xbasic::read_bigendian(buff_data,2) == USB_ADAPTER_HEADER)  m_signed_len =(size_t)xbasic::read_littleendian(buff_data+10,4); //获得长度标识
            else
            {
                xbasic::debug_output("xusbadapter_bin_unpacker::unpack_data() the header is %X! not %X!\n", xbasic::read_bigendian(buff_data,2), USB_ADAPTER_HEADER);
                unpack_ok = false; //包头错误
            }

            if(m_signed_len!=(size_t)-1 &&m_signed_len>(USB_ADAPTER_MAXMSG_LEN-512))
            {
                xbasic::debug_output("xusbadapter_bin_unpacker::unpack_data() the body is %d too long\n", m_signed_len);
                unpack_ok =false; //包头中长度错误
            }
        }
        else
            break;
    } //包都还没有收完，继续收

    if(!unpack_ok) {
        xbasic::debug_bindata("bin unpacker error:",buff_data,m_data_len%512);
        reset_data();
        return unpack_ok;
    } //解包错误，复位解包器

    if(m_data_len>0 && buff_data>buff_head) //拷贝剩余新包到缓冲区头部
    {
        for(unsigned int i=0; i<m_data_len; i++) buff_head[i] =buff_data[i];
    }

    return unpack_ok;
}

boost::asio::mutable_buffer_1 xusbadapter_bin_unpacker::prepare_buff(size_t & min_recv_len) //准备下一个数据接收缓冲区
{
    if(m_data_len >= USB_ADAPTER_MAXMSG_LEN) reset_data(); //接收缓冲区即将溢出

    if(m_data_len >= USB_ADAPTER_HEADER_LEN) //已经收到了部分数据
    {
        char *next_buff = m_raw_buff.begin();
        if(xbasic::read_bigendian(next_buff,2) == USB_ADAPTER_HEADER) //包头正确
            min_recv_len =(m_signed_len==(size_t)-1? USB_ADAPTER_HEADER_LEN-m_data_len:m_signed_len);
    }
    else //还没收到数据
    {
        min_recv_len = USB_ADAPTER_HEADER_LEN - m_data_len;
    }

    if(min_recv_len ==(size_t)-1 || min_recv_len > (USB_ADAPTER_MAXMSG_LEN-1024))
    {
        reset_data();
        min_recv_len = USB_ADAPTER_HEADER_LEN;
    }

    return boost::asio::buffer(boost::asio::buffer(m_raw_buff) +m_data_len); //使用mutable_buffer能防止接收缓冲区溢出
}

xusbadapter_package::xusbadapter_package(MSG_TYPE msg_type,MSG_CMD msg_cmd,unsigned short version,unsigned int session) :
    m_msg_type(msg_type),m_msg_cmd(msg_cmd),m_version(version),m_session(session)
{
}

xusbadapter_package::xusbadapter_package(unsigned char *bin_data,int data_len) : m_msg_type(MT_REQUEST),m_msg_cmd(MC_NONE),m_version(0),m_session(0)
{
    parse_from_bin(bin_data,data_len);
}

xusbadapter_package::xusbadapter_package(const xusbadapter_package& other)
{
    if(this != &other) {
        this->m_msg_type = other.m_msg_type;
        this->m_msg_cmd = other.m_msg_cmd;
        this->m_version = other.m_version;
        this->m_version = other.m_version;

        this->m_ota_type = other.m_ota_type;
        this->m_ota_version = other.m_ota_version;
        this->m_usb_dst = other.m_usb_dst;
        this->m_upgrade_progress = other.m_upgrade_progress;
        this->m_total_len = other.m_total_len;

        this->m_err_code = other.m_err_code;
        this->m_value_type = other.m_value_type;

        this->m_rw_data = other.m_rw_data;
        this->m_usbdst_ids = other.m_usbdst_ids;
        this->m_command_type = other.m_command_type;
        this->m_command_value = other.m_command_value;

        this->m_frequency_type = other.m_frequency_type;
        this->m_frequency_value = other.m_frequency_value;

        this->m_calfilename = other.m_calfilename;
        this->m_utp_chip_id = other.m_utp_chip_id;
    }
}

xusbadapter_package& xusbadapter_package::operator=(const xusbadapter_package& other)
{
    if(this != &other) {
        this->m_msg_type = other.m_msg_type;
        this->m_msg_cmd = other.m_msg_cmd;
        this->m_version = other.m_version;
        this->m_version = other.m_version;

        this->m_ota_type = other.m_ota_type;
        this->m_ota_version = other.m_ota_version;
        this->m_usb_dst = other.m_usb_dst;
        this->m_upgrade_progress = other.m_upgrade_progress;
        this->m_total_len = other.m_total_len;

        this->m_err_code = other.m_err_code;
        this->m_value_type = other.m_value_type;

        this->m_rw_data = other.m_rw_data;
        this->m_usbdst_ids = other.m_usbdst_ids;
        this->m_command_type = other.m_command_type;
        this->m_command_value = other.m_command_value;

        this->m_frequency_type = other.m_frequency_type;
        this->m_frequency_value = other.m_frequency_value;

        this->m_calfilename = other.m_calfilename;
        this->m_utp_chip_id = other.m_utp_chip_id;
    }
    return *this;
}

xpacket* xusbadapter_package::clone() //克隆对象
{
    xusbadapter_package *new_package = new xusbadapter_package(m_msg_type,m_msg_cmd,m_version,m_session);
    return new_package;
}

xusbadapter_package::~xusbadapter_package()
{
}

bool xusbadapter_package::parse_from_bin(unsigned char *pack_data,int pack_len) //从BIN解析数据包
{
    bool ret = false;
    if(parse_msg_header(pack_data,pack_len)) {
        switch(m_msg_type)
        {
            case MT_REQUEST:
                ret = parse_request_body(m_msg_cmd,pack_data,pack_len);
                break;
            case MT_RESPOND:
                ret = parse_response_body(m_msg_cmd,pack_data,pack_len);
                break;
            case MT_NOTIFY:
                ret = parse_notify_body(m_msg_cmd,pack_data,pack_len);
                break;
        }
    }
    return ret;
}

std::string xusbadapter_package::serial_to_bin() //串行化成BIN
{
    LOG_MSG(MSG_LOG,"Enter into xusbadapter_package::serial_to_bin()");
    std::string pack_data;

    //防止多线程，会话id重复，造成异步消息回复处理异常
    boost::unique_lock<boost::shared_mutex> lock(m_mux_sid); //写锁
    static unsigned int s_req_id = 1;
    if((m_msg_type == MT_REQUEST) || (m_msg_type == MT_NOTIFY)) {
        m_session = s_req_id;
        if(++s_req_id >= 65535) s_req_id = 1; //2字节翻转
    }
    lock.unlock();

    switch(m_msg_type)
    {
        case MT_REQUEST:
            pack_data = serail_request(m_msg_cmd);
            break;
        case MT_RESPOND:
            pack_data = serail_response(m_msg_cmd);
            break;
        case MT_NOTIFY:
            pack_data = serail_notify(m_msg_cmd);
            break;
    }

    LOG_MSG(MSG_LOG,"Exited xusbadapter_package::serial_to_bin()");
    return pack_data;
}

bool xusbadapter_package::parse_msg_header(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 10) return false;

    if(xbasic::read_bigendian(pack_data,2) != 0xFFA5) return false;
    m_msg_type = (MSG_TYPE)(pack_data[3]);        //报文类型
    m_msg_cmd = (MSG_CMD)(pack_data[3]);          //报文命令字
    m_version = xbasic::read_littleendian(&pack_data[4],2); //协议版本号
    m_session = xbasic::read_littleendian(&pack_data[6],4); //用于匹配请求和响应
    return true;
}

bool xusbadapter_package::parse_request_body(MSG_CMD msg_cmd,unsigned char *pack_data,int pack_len)
{
    bool ret = false;
    switch(m_msg_cmd)
    {
        case MC_START:
            ret = parse_start_request_body(pack_data,pack_len);
            break;
        case MC_QUERY:
            ret = parse_query_request_body(pack_data,pack_len);
            break;
        case MC_CANCEL:
            ret = parse_cancel_request_body(pack_data,pack_len);
            break;
        case MC_DATA:
            ret = parse_data_request_body(pack_data,pack_len);
            break;
        case MC_HEARTBEAT:
            ret = parse_heartbeat_body(pack_data,pack_len);
            break;
        case MC_CAL_READ:
            ret = parse_cal_read_request_body(pack_data,pack_len);
            break;
        case MC_USBSERIALS_ID:
            ret = parse_usbstds_request_body(pack_data,pack_len);
            break;
        case MC_ASIC_JUNCTION_TEMP:
            ret = parse_junction_temp_request_body(pack_data,pack_len);
            break;
        case MC_GET_VALUE:
            ret = parse_get_data_request_body(pack_data,pack_len);
            break;
        case MC_COMMAND:
            ret = parse_power_switch_request_body(pack_data,pack_len);
            break;
        case MC_FREQUENCY:
            ret = parse_frequency_set_request_body(pack_data,pack_len);
            break;
        case MC_GET_UTP_REGISTER:
            ret = parse_get_utp_register_request_body(pack_data,pack_len);
            break;
        default:
            LOG_MSG(ERR_LOG, "xusbadapter_package::parse_request_body() Unsupport cmd:%d",m_msg_cmd);
            break;
    }
    return ret;
}

bool xusbadapter_package::parse_notify_body(MSG_CMD msg_cmd,unsigned char *pack_data,int pack_len)
{
    bool ret = false;
    switch(m_msg_cmd)
    {
        case MC_COMPLETE:
            ret = parse_complete_notify_body(pack_data,pack_len);
            break;
    }
    return ret;
}

bool xusbadapter_package::parse_response_body(MSG_CMD msg_cmd,unsigned char *pack_data,int pack_len)
{
    bool ret;
    switch(m_msg_cmd)
    {
        case MC_START:
            ret = parse_start_response_body(pack_data,pack_len);
            break;
        case MC_QUERY:
            ret = parse_query_response_body(pack_data,pack_len);
            break;
        case MC_CANCEL:
            ret = parse_cancel_response_body(pack_data,pack_len);
            break;
        case MC_DATA:
            ret = parse_data_response_body(pack_data,pack_len);
            break;
        case MC_HEARTBEAT:
            ret = parse_heartbeat_body(pack_data,pack_len);
            break;
        case MC_CAL_READ:
            ret = parse_cal_read_response_body(pack_data,pack_len);
            break;
        case MC_USBSERIALS_ID:
            ret = parse_usbstds_response_body(pack_data,pack_len);
            break;
        case MC_ASIC_JUNCTION_TEMP:
            ret = parse_junction_temp_response_body(pack_data,pack_len);
            break;
        case MC_GET_VALUE:
            ret = parse_get_value_response_body(pack_data,pack_len);
            break;
        case MC_COMMAND:
            ret = parse_power_switch_response_body(pack_data,pack_len);
            break;
        case MC_FREQUENCY:
            ret = parse_frequency_set_response_body(pack_data,pack_len);
            break;
    }
    return ret;
}

bool xusbadapter_package::parse_start_request_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 30) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_ota_type = xbasic::read_littleendian(&pack_data[14],4);      //ota type
    m_ota_version = xbasic::readlong_long_littleendian(&pack_data[18],8); //ota version
    m_usb_dst = xbasic::read_littleendian(&pack_data[26],4);       //usb dst id

    if(is_ota_caltype(m_ota_type) == true)
    {
        int cal_name_len = load_len - 16;
        m_calfilename.assign((char*)&pack_data[30],cal_name_len); //cal校准文件名
    }
    return true;
}

bool xusbadapter_package::parse_query_request_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 22) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_ota_type = xbasic::read_littleendian(&pack_data[14],4);      //ota type
    m_usb_dst = xbasic::read_littleendian(&pack_data[18],4);       //usb dst id
    return true;
}

bool xusbadapter_package::parse_cancel_request_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 22) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_ota_type = xbasic::read_littleendian(&pack_data[14],4);      //ota type
    m_usb_dst = xbasic::read_littleendian(&pack_data[18],4);       //usb dst id
    return true;
}

bool xusbadapter_package::parse_complete_notify_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 22) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_ota_type = xbasic::read_littleendian(&pack_data[14],4);      //ota type
    m_usb_dst = xbasic::read_littleendian(&pack_data[18],4);       //usb dst id
    return true;
}

bool xusbadapter_package::parse_data_request_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 18) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_usb_dst = xbasic::read_littleendian(&pack_data[14],4);       //usb dst id
    return true;
}

bool xusbadapter_package::parse_heartbeat_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 14) return false;
    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    if(load_len != 0 ) {
        return false;
    }
    return true;
}

bool xusbadapter_package::parse_cal_read_request_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 30) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_ota_type = xbasic::read_littleendian(&pack_data[14],4);      //ota type
    m_ota_version = xbasic::readlong_long_littleendian(&pack_data[18],8); //ota version
    m_usb_dst = xbasic::read_littleendian(&pack_data[26],4);       //usb dst id

    if(is_ota_caltype(m_ota_type) == true)
    {
        int cal_name_len = load_len - 16;
        m_calfilename.assign((char*)&pack_data[30],cal_name_len); //cal校准文件名
    }
    return true;
}

bool xusbadapter_package::parse_usbstds_request_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 14) return false;
    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    if(load_len != 0 ) {
        return false;
    }
    return true;
}

bool xusbadapter_package::parse_junction_temp_request_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 14) return false;
    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    if(load_len != 0 ) {
        return false;
    }
    return true;
}

bool xusbadapter_package::parse_get_data_request_body(unsigned char *pack_data,int pack_len)
{
    xbasic::debug_output("xusbadapter_package::parse_get_data_request_body: pack_len:%d \n", pack_len);
    if(pack_len < 22) return false;
    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    xbasic::debug_output("xusbadapter_package::parse_get_data_request_body: load_len:%d \n", load_len);
    if((load_len != 8) && (load_len != 12))
    {
        return false;
    }

    m_usb_dst = xbasic::read_littleendian(&pack_data[14],4);       //usb dst id
    m_value_type = xbasic::read_littleendian(&pack_data[18],4);

    if(load_len == 12)
    {
        //m_value_type为VALUE_RCAFPGA_TEMP时才有效
        m_r3588_slotid = xbasic::read_littleendian(&pack_data[22],4);
    }
    xbasic::debug_output("xusbadapter_package::parse_get_data_request_body: m_usb_dst:0x%x m_value_type:%d\n", m_usb_dst, m_value_type);
    return true;
}

bool xusbadapter_package::parse_power_switch_request_body(unsigned char *pack_data,int pack_len)
{
    xbasic::debug_output("xusbadapter_package::parse_power_switch_request_body() pack_len:%d \n", pack_len);
    int ret = false;
    if((pack_len == 26) || (pack_len == 30))
    {
        if(pack_len == 30)
        {
            int load_len = xbasic::read_littleendian(&pack_data[10],4);      //4字节负载长度
            if(load_len == 16)
            {
                m_usb_dst = xbasic::read_littleendian(&pack_data[14],4);     //usb dst id
                m_command_type = xbasic::read_littleendian(&pack_data[18],4);
                m_command_value.m_utp40_id = xbasic::read_littleendian(&pack_data[22],4);
                m_command_value.m_switch = xbasic::read_littleendian(&pack_data[26],4);
                ret = true;
            }
        }
        else
        {
            int load_len = xbasic::read_littleendian(&pack_data[10],4);      //4字节负载长度
            if(load_len == 12)
            {
                m_usb_dst = xbasic::read_littleendian(&pack_data[14],4);     //usb dst id
                m_command_type = xbasic::read_littleendian(&pack_data[18],4);
                m_command_value.m_switch = xbasic::read_littleendian(&pack_data[22],4);
                ret = true;
            }
        }
    }
    xbasic::debug_output("xusbadapter_package::parse_power_switch_request_body() m_usb_dst=0x%x m_command_type=%d ret=%d\n", m_usb_dst, m_command_type, ret);
    return ret;
}

bool xusbadapter_package::parse_frequency_set_request_body(unsigned char *pack_data, int pack_len)
{
    xbasic::debug_output("xusbadapter_package::parse_frequency_set_request_body: pack_len:%d \n", pack_len);
    if(pack_len < 28) return false;
    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    xbasic::debug_output("xusbadapter_package::parse_frequency_set_request_body: load_len:%d \n", load_len);
    if(load_len != 14)
    {
        return false;
    }
    m_usb_dst = xbasic::read_littleendian(&pack_data[14],4);       //usb dst id
    m_frequency_type = xbasic::read_littleendian(&pack_data[18],4);
    m_frequency_value.m_frequency = xbasic::read_littleendian(&pack_data[22],4);
    m_frequency_value.m_channel = xbasic::read_littleendian(&pack_data[26],2);
    xbasic::debug_output("xusbadapter_package::parse_frequency_set_request_body: m_usb_dst:0x%x m_frequency_type:%d\n", m_usb_dst, m_frequency_type);
    return true;
}

bool xusbadapter_package::parse_get_utp_register_request_body(unsigned char *pack_data, int pack_len)
{
    //xbasic::debug_output("xusbadapter_package::parse_get_data_request_body: pack_len:%d \n", pack_len);
    LOG_MSG(MSG_LOG, "xusbadapter_package::parse_get_utp_register_request_body: pack_len:%d", pack_len);
    if(pack_len < 22) return false;
    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    //xbasic::debug_output("xusbadapter_package::parse_get_data_request_body: load_len:%d \n", load_len);
    LOG_MSG(MSG_LOG, "xusbadapter_package::parse_get_utp_register_request_body: load_len:%d", load_len);
    if(load_len != 8 )
    {
        return false;
    }
    m_usb_dst = xbasic::read_littleendian(&pack_data[14],4);       //usb dst id
    m_utp_chip_id = xbasic::read_littleendian(&pack_data[18],4);
    // xbasic::debug_output("xusbadapter_package::parse_get_data_request_body: m_usb_dst:0x%x m_value_type:%d\n", m_usb_dst, m_value_type);
    LOG_MSG(MSG_LOG, "xusbadapter_package::parse_get_utp_register_request_body: m_usb_dst:0x%x m_utp_chip_id:%d", m_usb_dst, m_utp_chip_id);
    return true;
}

bool xusbadapter_package::parse_start_response_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 18) return false;
    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_err_code = xbasic::read_littleendian(&pack_data[14],4);      //错误码
    return true;
}

bool xusbadapter_package::parse_query_response_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 26) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_upgrade_progress = xbasic::read_littleendian(&pack_data[14],4); //升级进度
    m_total_len = xbasic::read_littleendian(&pack_data[18],4);     //升级文件总长度
    m_err_code = xbasic::read_littleendian(&pack_data[22],4);      //错误码
    return true;
}

bool xusbadapter_package::parse_cancel_response_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 18) return false;
    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_err_code = xbasic::read_littleendian(&pack_data[14],4);      //错误码
    return true;
}

bool xusbadapter_package::parse_data_response_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 22) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_usb_dst = xbasic::read_littleendian(&pack_data[14],4);       //回传目标地址
    m_err_code = xbasic::read_littleendian(&pack_data[18],4);      //错误码
    //解析data数据
    int i = 0;
    int err_code_len = 4;
    int usb_dst_len = 4;
    int value_len = load_len - err_code_len - usb_dst_len;
    while(i < value_len)
    {
        boost::shared_ptr<xrw_data::data_value> value_data(new xrw_data::data_value());

        value_data->tid = pack_data[22+i];
        i = i + 1;

        value_data->length = xbasic::read_littleendian(&pack_data[22+i],2);
        i = i + 2;

        value_data->value.assign((char*)&pack_data[22+i],static_cast<size_t>(value_data->length));
        i = i + value_data->length;

        if(i <= value_len)
            m_rw_data.m_data_map.insert(std::make_pair(value_data->tid,value_data));
        else {
            //长度不足，发生解析错误
            return false;
        }
    }
    return true;
}

bool xusbadapter_package::parse_cal_read_response_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 18) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);    //4字节负载长度
    m_err_code = xbasic::read_littleendian(&pack_data[14],4);      //错误码
    return true;
}

bool xusbadapter_package::parse_usbstdids_response_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 18) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);     //4字节负载长度
    m_err_code = xbasic::read_littleendian(&pack_data[14],4);       //错误码

    //解析data数据
    int i = 0;
    int err_code_len = 4;
    int value_len = load_len - err_code_len;
    while(i < value_len) {
        int usb_dstid = xbasic::read_littleendian(&pack_data[18+i],2);
        i = i + 2;
        if(i <= value_len) {
            m_usbstd_ids.push_back(usb_dstid);
        }
        else {
            //长度不足,发生解析错误
            return false;
        }
    }

    return true;
}

bool xusbadapter_package::parse_junction_temp_response_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 18) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);     //4字节负载长度
    m_err_code = xbasic::read_littleendian(&pack_data[14],4);       //错误码

    //解析data数据
    int i = 0;
    int err_code_len = 4;
    int value_len = load_len - err_code_len;
    while(i < value_len) {
        int chip_id = xbasic::read_littleendian(&pack_data[18+i],2);
        i = i + 2;
        float temp = xbasic::readfloat_littleendian(&pack_data[18+i],4);
        i = i + 4;
        if(i <= value_len) {
            junction_temp junction;
            junction.m_asicchip_id = chip_id;
            junction.m_temp = temp;
            m_junctions.push_back(junction);
        }
        else {
            //长度不足,发生解析错误
            return false;
        }
    }

    return true;
}

bool xusbadapter_package::parse_get_value_response_body(unsigned char *pack_data,int pack_len)
{
    xbasic::debug_output("xusbadapter_package::parse_get_value_response_body: pack_len:%d \n", pack_len);
    if(pack_len < 18) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);     //4字节负载长度
    m_err_code = xbasic::read_littleendian(&pack_data[14],4);       //错误码
    xbasic::debug_output("xusbadapter_package::parse_get_value_response_body: load_len:%d pack_len:%d \n", load_len, pack_len);

    if (load_len < 0 || (load_len > (pack_len - 14))) {
        xbasic::debug_output("error: xusbadapter_package::parse_get_value_response_body: load_len:%d pack_len:%d \n", load_len, pack_len);
        //长度不足,发生解析错误
        return false;
    } else {
        std::string data(reinterpret_cast<const char*>(pack_data + 18), load_len-4); //负载长度包含了m_err_code长度
        m_value_dev = data;
    }

    return true;
}

bool xusbadapter_package::parse_power_switch_response_body(unsigned char *pack_data,int pack_len)
{
    if(pack_len < 18) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);     //4字节负载长度
    m_err_code = xbasic::read_littleendian(&pack_data[14],4);       //错误码

    return true;
}

bool xusbadapter_package::parse_frequency_set_response_body(unsigned char *pack_data, int pack_len)
{
    if(pack_len < 18) return false;

    int load_len = xbasic::read_littleendian(&pack_data[10],4);     //4字节负载长度
    m_err_code = xbasic::read_littleendian(&pack_data[14],4);       //错误码
    return true;
}

std::string xusbadapter_package::serial_request(MSG_CMD msg_cmd)
{
    std::string pack_data;
    switch(msg_cmd)
    {
        case MC_START:
            serial_start_request(pack_data);
            break;
        case MC_QUERY:
            serial_query_request(pack_data);
            break;
        case MC_CANCEL:
            serial_cancel_request(pack_data);
            break;
        case MC_DATA:
            serial_data_request(pack_data);
            break;
        case MC_HEARTBEAT:
            serial_heartbeat(pack_data);
            break;
        case MC_CAL_READ:
            serial_cal_read_request(pack_data);
            break;
        case MC_USBSERIALS_ID:
            serial_usbstdids_request(pack_data);
            break;
        case MC_ASIC_JUNCTION_TEMP:
            serial_junction_temp_request(pack_data);
            break;
        case MC_GET_VALUE:
            serial_get_value_request(pack_data);
            break;
        case MC_COMMAND:
            serial_power_switch_request(pack_data);
            break;
        case MC_FREQUENCY:
            serial_frequency_set_request(pack_data);
            break;
        default:
            break;
    }

    return pack_data;
}

std::string xusbadapter_package::serial_notify(MSG_CMD msg_cmd)
{
    std::string pack_data;
    switch(msg_cmd)
    {
        case MC_COMPLETE:
            serial_complete_notify(pack_data);
            break;
    }

    return pack_data;
}

std::string xusbadapter_package::serial_response(MSG_CMD msg_cmd)
{
    std::string pack_data;
    switch(msg_cmd)
    {
        case MC_START:
            serial_start_response(pack_data);
            break;
        case MC_QUERY:
            serial_query_response(pack_data);
            break;
        case MC_CANCEL:
            serial_cancel_response(pack_data);
            break;
        case MC_DATA:
            serial_data_response(pack_data);
            break;
        case MC_HEARTBEAT:
            serial_heartbeat(pack_data);
            break;
        case MC_CAL_READ:
            serial_cal_read_response(pack_data);
            break;
        case MC_USBSERIALS_ID:
            serial_usbstdids_response(pack_data);
            break;
        case MC_ASIC_JUNCTION_TEMP:
            serial_junction_temp_response(pack_data);
            break;
        case MC_GET_VALUE:
            serial_get_value_response(pack_data);
            break;
        case MC_COMMAND:
            serial_power_switch_response(pack_data);
            break;
        case MC_FREQUENCY:
            serial_frequency_set_response(pack_data);
            break;
        case MC_GET_UTP_REGISTER:
        case MC_GET_UTP102_REGISTER:
            serial_get_utp_register_response(pack_data);
            break;
    }

    return pack_data;
}

void xusbadapter_package::serial_msg_header(unsigned char* pack_header)
{
    pack_header[0] = 0xFF;
    pack_header[1] = 0xA5;
    pack_header[2] = m_msg_type;
    pack_header[3] = m_msg_cmd;
    xbasic::write_littleendian(&pack_header[4],m_version,2);
    xbasic::write_littleendian(&pack_header[6],m_session,4);
    xbasic::debug_output("xusbadapter_package::serial_msg_header: m_msg_type:%d m_msg_cmd:%d m_version:%d m_session:%d\n", static_cast<int>(m_msg_type), static_cast<int>(m_msg_cmd), m_version, m_session);
}

void xusbadapter_package::serial_heartbeat(std::string& pack_data)
{
    unsigned char pack_head[14] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 0;
    xbasic::write_littleendian(&pack_head[10],load_len,4);

    pack_data.assign((char *)pack_head,14);
}

void xusbadapter_package::serial_start_request(std::string& pack_data)
{
    if(is_ota_caltype(m_ota_type) == false)
    {
        unsigned char pack_head[30] = {0};
        serial_msg_header(&pack_head[0]);

        int load_len = 16;
        xbasic::write_littleendian(&pack_head[10],load_len,4);
        xbasic::write_littleendian(&pack_head[14],m_ota_type,4);
        xbasic::writelong_littleendian(&pack_head[18],m_ota_version,8);
        xbasic::write_littleendian(&pack_head[26],m_usb_dst,4);

        pack_data.assign((char *)pack_head,30);
    }
    else
    {
        unsigned char pack_head[30] = {0};
        serial_msg_header(&pack_head[0]);

        int cal_name_len = m_calfilename.length();
        int load_len = 16 + cal_name_len;
        xbasic::write_littleendian(&pack_head[10],load_len,4);
        xbasic::write_littleendian(&pack_head[14],m_ota_type,4);
        xbasic::writelong_littleendian(&pack_head[18],m_ota_version,8);
        xbasic::write_littleendian(&pack_head[26],m_usb_dst,4);

        pack_data.assign((char *)pack_head,30);
        pack_data.append(m_calfilename);
    }
}

void xusbadapter_package::serial_query_request(std::string& pack_data)
{
    unsigned char pack_head[22] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 8;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_ota_type,4);
    xbasic::write_littleendian(&pack_head[18],m_usb_dst,4);

    pack_data.assign((char *)pack_head,22);
}

void xusbadapter_package::serial_cancel_request(std::string& pack_data)
{
    unsigned char pack_head[22] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 8;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_ota_type,4);
    xbasic::write_littleendian(&pack_head[18],m_usb_dst,4);

    pack_data.assign((char *)pack_head,22);
}

void xusbadapter_package::serial_data_request(std::string& pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 4;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_usb_dst,4);

    pack_data.assign((char *)pack_head,18);
}

void xusbadapter_package::serial_cal_read_request(std::string& pack_data)
{
    if(is_ota_caltype(m_ota_type) == false)
    {
        unsigned char pack_head[30] = {0};
        serial_msg_header(&pack_head[0]);

        int load_len = 16;
        xbasic::write_littleendian(&pack_head[10],load_len,4);
        xbasic::write_littleendian(&pack_head[14],m_ota_type,4);
        xbasic::writelong_littleendian(&pack_head[18],m_ota_version,8);
        xbasic::write_littleendian(&pack_head[26],m_usb_dst,4);

        pack_data.assign((char *)pack_head,30);
    }
    else
    {
        unsigned char pack_head[30] = {0};
        serial_msg_header(&pack_head[0]);

        int cal_name_len = m_calfilename.length();
        int load_len = 16 + cal_name_len;
        xbasic::write_littleendian(&pack_head[10],load_len,4);
        xbasic::write_littleendian(&pack_head[14],m_ota_type,4);
        xbasic::writelong_littleendian(&pack_head[18],m_ota_version,8);
        xbasic::write_littleendian(&pack_head[26],m_usb_dst,4);

        pack_data.assign((char *)pack_head,30);
        pack_data.append(m_calfilename);
    }
}

void xusbadapter_package::serial_usbstdids_request(std::string& pack_data)
{
    unsigned char pack_head[14] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 0;
    xbasic::write_littleendian(&pack_head[10],load_len,4);

    pack_data.assign((char *)pack_head,14);
}

void xusbadapter_package::serial_junction_temp_request(std::string& pack_data)
{
    unsigned char pack_head[14] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 0;
    xbasic::write_littleendian(&pack_head[10],load_len,4);

    pack_data.assign((char *)pack_head,14);
}

void xusbadapter_package::serial_get_value_request(std::string& pack_data)
{
    unsigned char pack_head[26] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 0;

    if(m_value_type == VALUE_RCAPPGA_TEMP)
    {
        load_len = 12;
        xbasic::write_littleendian(&pack_head[10],load_len,4);
        xbasic::write_littleendian(&pack_head[14],m_usb_dst,4);
        xbasic::write_littleendian(&pack_head[18],m_value_type,4);
        xbasic::write_littleendian(&pack_head[22],m_rk3588_slotid,4);

        pack_data.assign((char *)pack_head,26);
    }
    else
    {
        load_len = 8;
        xbasic::write_littleendian(&pack_head[10],load_len,4);
        xbasic::write_littleendian(&pack_head[14],m_usb_dst,4);
        xbasic::write_littleendian(&pack_head[18],m_value_type,4);

        pack_data.assign((char *)pack_head,22);
    }

    xbasic::debug_output("xusbadapter_package::serial_get_value_request() pack_data:%s\n", pack_data.c_str());
}

void xusbadapter_package::serial_power_switch_request(std::string& pack_data)
{
    if((m_command_value.m_utp40_id != 0) && (m_command_value.m_switch != 0))
    {
        unsigned char pack_head[30] = {0};
        serial_msg_header(&pack_head[0]);

        int load_len = 16;
        xbasic::write_littleendian(&pack_head[10],load_len,4);

        xbasic::write_littleendian(&pack_head[14],m_usb_dst,4);
        xbasic::write_littleendian(&pack_head[18],m_command_type,4);
        xbasic::write_littleendian(&pack_head[22],m_command_value.m_utp40_id,4);
        xbasic::write_littleendian(&pack_head[26],m_command_value.m_switch,4);
        pack_data.assign((char *)pack_head,30);
    }
    else if((m_command_value.m_switch != 0))
    {
        unsigned char pack_head[26] = {0};
        serial_msg_header(&pack_head[0]);

        int load_len = 12;
        xbasic::write_littleendian(&pack_head[10],load_len,4);

        xbasic::write_littleendian(&pack_head[14],m_usb_dst,4);
        xbasic::write_littleendian(&pack_head[18],m_command_type,4);
        xbasic::write_littleendian(&pack_head[22],m_command_value.m_switch,4);
        pack_data.assign((char *)pack_head,26);
    }
    else
    {
        //to do nothing
    }
}

void xusbadapter_package::serial_frequency_set_request(std::string &pack_data)
{
    unsigned char pack_head[28] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 14;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_usb_dst,4);
    xbasic::write_littleendian(&pack_head[18],m_frequency_type,4);
    xbasic::writefloat_littleendian(&pack_head[22],m_frequency_value.m_frequency,4);
    xbasic::write_littleendian(&pack_head[26],m_frequency_value.m_channel,2);

    pack_data.assign((char *)pack_head,28);
    xbasic::debug_output("xusbadapter_package::serial_frequency_set_request() pack_data:%s\n", pack_data.c_str());
}

void xusbadapter_package::serial_start_response(std::string& pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 4;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_err_code,4);

    pack_data.assign((char *)pack_head,18);
}

void xusbadapter_package::serial_complete_notify(std::string& pack_data)
{
    unsigned char pack_head[22] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 8;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_ota_type,4);
    xbasic::write_littleendian(&pack_head[18],m_usb_dst,4);

    pack_data.assign((char *)pack_head,22);
}

void xusbadapter_package::serial_query_response(std::string& pack_data)
{
    unsigned char pack_head[26] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 12;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_upgrade_progress,4);
    xbasic::write_littleendian(&pack_head[18],m_total_len,4);
    xbasic::write_littleendian(&pack_head[22],m_err_code,4);

    pack_data.assign((char *)pack_head,26);
}

void xusbadapter_package::serial_cancel_response(std::string& pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 4;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_err_code,4);

    pack_data.assign((char *)pack_head,18);
}

void xusbadapter_package::serial_data_response(std::string& pack_data)
{
    LOG_MSG(MSG_LOG,"Enter into xusbadapter_package::serial_data_response()");
    unsigned char pack_head[22] = {0};
    serial_msg_header(&pack_head[0]);

    std::string value_data;
    int value_len = 0;
    boost::unordered_map<int,boost::shared_ptr<rw_data::data_value> >::iterator it = m_rw_data.m_data_map.begin();
    boost::unordered_map<int,boost::shared_ptr<rw_data::data_value> >::iterator end = m_rw_data.m_data_map.end();
    for(; it != end ; it++) {
        unsigned char buff[3] = {0};
        buff[0] = it->second->tid;
        value_len = value_len + 1;

        xbasic::write_littleendian(&buff[1],it->second->length,2);
        value_len = value_len + 2;

        value_data.append((char*)buff,3);
        value_data.append(it->second->value,0,static_cast<size_t>(it->second->length));
        value_len = value_len + it->second->length;
    }

    int err_code_len = 4;
    int usb_dst_len = 4;
    int load_len = value_len + err_code_len + usb_dst_len;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_usb_dst,4);
    xbasic::write_littleendian(&pack_head[18],m_err_code,4);
    pack_data.assign((char *)pack_head,22);
    if(value_len > 0) {
        pack_data.append(value_data,0,value_len);
    }
    LOG_MSG(MSG_LOG,"Exited xusbadapter_package::serial_data_response()");
}

void xusbadapter_package::serial_cal_read_response(std::string& pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 4;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_err_code,4);

    pack_data.assign((char *)pack_head,18);
}

void xusbadapter_package::serial_usbstdids_response(std::string& pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    std::string value_data;
    int value_len = 0;
    std::vector<int>::iterator it = m_usbstd_ids.begin();
    std::vector<int>::iterator end = m_usbstd_ids.end();

    for(; it != end ; it++) {
        unsigned char buff[2] = {0};

        xbasic::write_littleendian(&buff[0],*it,2);
        value_len = value_len + 2;

        value_data.append((char*)buff,2);
    }

    int err_code_len = 4;
    int load_len = value_len + err_code_len;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_err_code,4);
    pack_data.assign((char *)pack_head,18);
    if(value_len > 0) {
        pack_data.append(value_data,0,value_len);
    }
}

void xusbadapter_package::serial_junction_temp_response(std::string& pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    std::string value_data;
    int value_len = 0;
    std::vector<junction_temp>::iterator it = m_junctions.begin();
    std::vector<junction_temp>::iterator end = m_junctions.end();

    for(; it != end ; it++) {
        unsigned char buff[6] = {0};

        xbasic::write_littleendian(&buff[0],it->m_asicchip_id,2);
        value_len = value_len + 2;
        xbasic::writefloat_littleendian(&buff[2],it->m_temp,4);
        value_len = value_len + 4;

        value_data.append((char*)buff,6);
    }

    int err_code_len = 4;
    int load_len = value_len + err_code_len;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_err_code,4);
    pack_data.assign((char *)pack_head,18);
    if(value_len > 0) {
        pack_data.append(value_data,0,value_len);
    }
}

void xusbadapter_package::serial_get_value_response(std::string& pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    std::string value_data = m_value_dev;
    int value_len = m_value_dev.length();
    int err_code_len = 4;
    int load_len = value_len + err_code_len;
    xbasic::debug_output("xusbadapter_package::serial_get_value_response: load_len:%d value_len:%d err_code_len:%d\n", load_len, value_len, err_code_len);
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_err_code,4);
    pack_data.assign((char *)pack_head,18);
    if(value_len > 0) {
        pack_data.append(value_data,0,value_len);
    }
}

void xusbadapter_package::serial_power_switch_response(std::string& pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 4;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_err_code,4);

    pack_data.assign((char *)pack_head,18);
}

void xusbadapter_package::serial_frequency_set_response(std::string &pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    int load_len = 4;
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_err_code,4);

    pack_data.assign((char *)pack_head,18);
}

void xusbadapter_package::serial_get_utp_register_response(std::string &pack_data)
{
    unsigned char pack_head[18] = {0};
    serial_msg_header(&pack_head[0]);

    std::string value_data = m_value_dev;
    int value_len = m_value_dev.length();
    int err_code_len = 4;
    int load_len = value_len + err_code_len;
    // xbasic::debug_output("xusbadapter_package::serial_get_value_response: load_len:%d value_len:%d err_code_len:%d\n", load_len, value_len, err_code_len);
    LOG_MSG(MSG_LOG, "xusbadapter_package::serial_get_utp_register_response: load_len:%d value_len:%d err_code_len:%d", load_len, value_len, err_code_len);
    xbasic::write_littleendian(&pack_head[10],load_len,4);
    xbasic::write_littleendian(&pack_head[14],m_err_code,4);
    pack_data.assign((char *)pack_head,18);
    if(value_len > 0) {
        pack_data.append(value_data,0,value_len);
    }
}

bool xusbadapter_package::is_ota_caltype(int ota_type)
{
    bool ret = false;
    if((ota_type >= OTA_TYPE_CPPENUTP40_MV_CAL) && (ota_type <= OTA_TYPE_FIFEB_PE_DC_CAL))
    {
        ret = true;
    }

    return ret;
}

void xusbadapter_package::reset()
{
}
