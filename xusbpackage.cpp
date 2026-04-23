#include "xusbpackage.h"
#include "mgr_log.h"

usb_bin_packer::usb_bin_packer()
{
    //LOG_MSG(WRN_LOG,"usb_bin_packer::usb_bin_packer() construct");
}

usb_bin_packer::~usb_bin_packer()
{
    //LOG_MSG(WRN_LOG,"usb_bin_packer::~usb_bin_packer() deconstruct");
}

boost::shared_ptr<const std::string> usb_bin_packer::pack_data(const char *pdata,size_t datalen,PACKWAY pack_way)
{
    LOG_MSG(MSG_LOG,"Enter into usb_bin_packer::pack_data() -- datalen=%u",datalen);
    boost::shared_ptr<const std::string> ppack;
    if(NULL==pdata || 0==datalen) return ppack;
    int totallen =(int)datalen;
    std::string *raw_str =new std::string();
    raw_str->reserve(totallen); //按照原始数据打包
    raw_str->append(pdata,totallen);
    ppack.reset(raw_str);
    LOG_MSG(MSG_LOG,"Exited usb_bin_packer::pack_data()");
    return ppack;
}

usb_bin_unpacker::usb_bin_unpacker()
{
    m_signed_len =(size_t)-1;
    m_data_len =0;
    //LOG_MSG(WRN_LOG,"usb_bin_unpacker::usb_bin_unpacker() construct");
}

usb_bin_unpacker::~usb_bin_unpacker()
{
    //LOG_MSG(WRN_LOG,"usb_bin_unpacker::~usb_bin_unpacker() deconstruct");
}

void usb_bin_unpacker::reset_data()
{
    m_signed_len =(size_t)-1;
    m_data_len = 0;
}

bool usb_bin_unpacker::unpack_data(size_t bytes_data, boost::container::list<boost::shared_ptr<const std::string> > & list_pack)
{
    LOG_MSG(MSG_LOG,"Enter into usb_bin_unpacker::unpack_data() bytes_data=%d",bytes_data);
    m_data_len += bytes_data; //本次收到的字节数
    bool unpack_ok = true;
    char *buff_head = m_raw_buff.begin();
    char *buff_data = buff_head;
    while(unpack_ok)
    {
        if(m_signed_len !=(size_t)-1) //包头解析完，现在解析包体
        {
            if(xbasic::read_bigendian(buff_data,2) !=USB_HEADER)
            {
                //包头错误
                LOG_MSG(ERR_LOG,"usb_bin_unpacker::unpack_data() usb header error buff_data[0]=0x%x buff_data[1]=0x%x",buff_data[0],buff_data[1]);
                unpack_ok = false;
                break;
            }
            size_t pack_len = m_signed_len + USB_HEADER_LEN;
            LOG_MSG(MSG_LOG,"usb_bin_unpacker::unpack_data() pack_len=%d",pack_len);
            if(m_data_len < pack_len)
            {
                LOG_MSG(WRN_LOG,"usb_bin_unpacker::unpack_data() m_data_len=%d < pack_len=%d",m_data_len,pack_len);
                break; //不够一个包长，退出，在现有基础上继续接收
            }
            list_pack.push_back(boost::shared_ptr<std::string>(new std::string(buff_data, pack_len))); //将这个包返回上层
            std::advance(buff_data,pack_len); //增加指定偏移继续分析
            m_data_len -= pack_len;
            m_signed_len = (size_t)-1;
        }
        else if(m_data_len >= USB_HEADER_LEN) //已经收到了部分数据
        {
            if(xbasic::read_bigendian(buff_data,2) == USB_HEADER)
            {
                m_signed_len = (size_t)xbasic::read_bigendian(buff_data+9,2); //获得长度标识
                m_signed_len += USB_MSG_CRC;  //加上CRC校验长度
            }
            else
            {
                unpack_ok = false; //包头错误
                LOG_MSG(ERR_LOG,"usb_bin_unpacker::unpack_data() usb header error buff_data[0]=0x%x buff_data[1]=0x%x m_data_len=%d",buff_data[0],buff_data[1],m_data_len);
            }
            if(m_signed_len != (size_t)-1 && m_signed_len > (USB_MAXMSG_LEN - 512)) //这里是不是限制了包体中data数据域长度不能大于USB_MAXMSG_LEN-512-USB_MSG_CRC???
            {
                unpack_ok = false; //包体中长度错误
                LOG_MSG(ERR_LOG,"usb_bin_unpacker::unpack_data() m_signed_len=%d  m_data_len=%d",m_signed_len,m_data_len);
            }
        }
        else
        {
            break; //包头都还没有收完，继续收
        }
    }

    if(!unpack_ok)
    {
        LOG_MSG(ERR_LOG,"usb_bin_unpacker::unpack_data() bin unpacker error");
        reset_data();
        return unpack_ok;
    } //解包错误，复位解包器

    if(m_data_len > 0 && buff_data > buff_head) //拷贝剩余断包到缓冲区头部
    {
        for(unsigned int i=0; i<m_data_len; i++) buff_head[i] =buff_data[i];
    }
    LOG_MSG(MSG_LOG,"Exited usb_bin_unpacker::unpack_data()");
    return unpack_ok;
}

boost::asio::mutable_buffer usb_bin_unpacker::prepare_buffer(size_t min_recv_len) //准备下一个数据接收缓冲区
{
    LOG_MSG(MSG_LOG,"Enter into usb_bin_unpacker::prepare_buffer()");
    if(m_data_len >=USB_MAXMSG_LEN) reset_data(); //接收缓冲区即将溢出
    if(m_data_len >=USB_HEADER_LEN) //已经收到了部分数据
    {
        char *next_buff = m_raw_buff.begin();
        if(xbasic::read_bigendian(next_buff,2) ==USB_HEADER) //包头正确
            min_recv_len =(m_signed_len==(size_t)-1? USB_HEADER_LEN-m_data_len:m_signed_len);
    }
    else //还没有收到数据
    {
        min_recv_len =USB_HEADER_LEN-m_data_len;
    }

    if(min_recv_len ==(size_t)-1 || min_recv_len >(USB_MAXMSG_LEN-1024))
    {
        reset_data();
        min_recv_len =USB_HEADER_LEN;
    }
    LOG_MSG(MSG_LOG,"Exited usb_bin_unpacker::prepare_buffer()");
    return boost::asio::buffer(boost::asio::buffer(m_raw_buff) +m_data_len); //使用mutable_buffer能防止接受缓冲区溢出
}

xpacket* xusbpackage::clone() //克隆对象
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::clone()");
    xusbpackage *new_package =new xusbpackage(m_msg_cmd,m_version,m_session,m_srcid,m_dstid,m_crc16);
    get_tlv_data(new_package->m_list_tlv);
    LOG_MSG(MSG_LOG,"Exited xusbpackage::clone()");
    return new_package;
}

bool xusbpackage::parse_from_bin(unsigned char *pack_data,int pack_len) //从BIN解析数据包
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::parse_from_bin()");
    if((xbasic::read_bigendian(pack_data,2))!= 0xFE55)
    {
        LOG_MSG(MSG_LOG,"xusbpackage::parse_from_bin(),usb message header field is not 0xFE 55");
        return false;
    }
    m_msg_cmd = (pack_data[2]);  //报文命令字
    m_version = (unsigned char)pack_data[3]; //协议版本号
    m_session = (unsigned char)pack_data[4]; //用于匹配请求和响应
    m_srcid = xbasic::read_bigendian(&pack_data[5],2); //源设备地址
    m_dstid = xbasic::read_bigendian(&pack_data[7],2); //目的设备地址
    int load_len = xbasic::read_bigendian(&pack_data[9],2); //2字节负载长度
    unsigned char *load_data=(unsigned char *)&pack_data[11]; //负载TLV格式数据
    boost::mutex::scoped_lock lock(m_mux_tlv);
    xusb_tvl::parse(load_dat,load_len,m_list_tlv,m_msg_cmd);
    m_crc16 = xbasic::read_bigendian((&pack_data[0]+USB_HEADER_LEN+load_len),2);

    LOG_MSG(MSG_LOG,"xusbpackage::parse_from_bin() crc=%hu",m_crc16);

    //校验crc
    std::string check_buff((char *)pack_data,pack_len-2);
    unsigned char crc16_buff[3] = {0};
    memcpy(crc16_buff,&m_crc16,sizeof(m_crc16));
    check_buff.append(std::string((char *)crc16_buff,2));

    boost::crc_16_type crc16;
    crc16.process_bytes(check_buff.data(),check_buff.length());
    unsigned short crc_checksum = crc16.checksum();
    m_crc_checksum = crc_checksum;

    lock.unlock();
    LOG_MSG(MSG_LOG,"Exited xusbpackage::parse_from_bin()");
    return true;
}

std::string xusbpackage::serial_to_bin() //串行化成BIN
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::serial_to_bin()");
    static unsigned char s_req_id =1;
    unsigned char pack_head[11] ={0xFE, 0x55};
    pack_head[2] =m_msg_cmd;
    pack_head[3] =m_version;
    //打包request消息时，才s_req_id才自增生成session id字段
    //打包response消息时，应将request消息中session 字段
    if((m_msg_cmd & USB_MESSAGE_MASK) == USB_REQUEST_MESSAGE) {
        m_session = s_req_id;
    }

    pack_head[4] =m_session;
    xbasic::write_bigendian(&pack_head[5],m_srcid,2);
    xbasic::write_bigendian(&pack_head[7],m_dstid,2);
    if((m_msg_cmd & USB_MESSAGE_MASK) == USB_REQUEST_MESSAGE) {
        if(++s_req_id >255) s_req_id = 1; //几字节翻转
    }
    boost::mutex::scoped_lock lock(m_mux_tlv);
    std::string tlv_data =xusb_tlv::serial(m_list_tlv);
    lock.unlock();
    xbasic::write_bigendian(&pack_head[9],tlv_data.length(),2);
    std::string pack_data((char *)pack_head,11);
    pack_data.append(tlv_data);

    //计算crc的值
    boost::crc_16_type crc16;
    crc16.process_bytes(pack_data.data(),pack_data.length());
    unsigned short crc_checksum = crc16.checksum();
    m_crc16 = crc_checksum;
    LOG_MSG(MSG_LOG,"xusbpackage::serial_to_bin() crc=%hu",m_crc16);
    unsigned char crc16_buff[3] ={0};
    memcpy(crc16_buff,&m_crc16,2);
    xbasic::write_bigendian(&crc16_buff[0],m_crc16,2);
    pack_data.append(std::string((char *)crc16_buff,2));
    LOG_MSG(MSG_LOG,"Exited xusbpackage::serial_to_bin()");
    return pack_data;
}

int xusbpackage::add_tlv_data(std::list<boost::shared_ptr<xusb_tlv> > &llist_tlv,unsigned char cmd) //将TLV对象数组加入到包
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::add_tlv_data()");
    for(std::list<boost::shared_ptr<xusb_tlv> >::iterator iter=llist_tlv.begin(); iter!=llist_tlv.end(); iter++)
    {
        add_data(*iter,cmd);
    }
    LOG_MSG(MSG_LOG,"Exited xusbpackage::add_tlv_data()");
    return llist_tlv.size();
}

int xusbpackage::get_tlv_data(std::list<boost::shared_ptr<xusb_tlv> > &llist_tlv) //获取TLV对象数组
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::get_tlv_data()");
    boost::mutex::scoped_lock lock(m_mux_tlv);
    for(std::list<boost::shared_ptr<xusb_tlv> >::iterator iter=m_list_tlv.begin(); iter!=m_list_tlv.end(); iter++)
    {
        boost::shared_ptr<xusb_tlv> tlv =*iter;
        boost::shared_ptr<xusb_tlv> new_data(tlv->clone());
        llist_tlv.push_back(new_data);
    }
    LOG_MSG(MSG_LOG,"Exited xusbpackage::get_tlv_data()");
    return llist_tlv.size();
}

void xusbpackage::add_data(unsigned short tid,std::string value,unsigned char cmd) //加入tlv数据单元到package
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::add_data()");
    boost::mutex::scoped_lock lock(m_mux_tlv);
    for(std::list<boost::shared_ptr<xusb_tlv> >::iterator iter=m_list_tlv.begin(); iter!=m_list_tlv.end(); iter++) //寻找对应的tlv数据
    {
        boost::shared_ptr<xusb_tlv> tlv =*iter;
        if(tlv->m_tid ==tid) {tlv->update_data(value); return;} //是需要的数据
    }
    boost::shared_ptr<xusb_tlv> new_data(new xusb_tlv(tid,value.length(),value.data(),cmd)); //没有找到就新增
    LOG_MSG(MSG_LOG,"Exited xusbpackage::add_data()");
    m_list_tlv.push_back(new_data);
}

void xusbpackage::add_data(boost::shared_ptr<xusb_tlv> tlv_data,unsigned char cmd) //加入tlv数据单元到package
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::add_data()");
    add_data(tlv_data->m_tid,tlv_data->m_value,cmd);
    LOG_MSG(MSG_LOG,"Exited xusbpackage::add_data()");
}

boost::shared_ptr<xusb_tlv> xusbpackage::find_data(unsigned short tid) //从package寻找数据单元
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::find_data()");
    boost::mutex::scoped_lock lock(m_mux_tlv);
    for(std::list<boost::shared_ptr<xusb_tlv> >::iterator iter=m_list_tlv.begin(); iter!=m_list_tlv.end(); iter++)
    {
        boost::shared_ptr<xusb_tlv> tlv =*iter;
        // LOG_MSG(MSG_LOG, "xusbpackage::find_data() tid:0x%x tlv_id:0x%x", tid, tlv->m_tid);
        if(tlv->m_tid ==tid) return tlv;
    }
    LOG_MSG(MSG_LOG,"Exited xusbpackage::find_data()");
    return boost::shared_ptr<xusb_tlv>();
}

void xusbpackage::del_data(unsigned short tid) //从package删除数据单元
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::del_data()");
    boost::mutex::scoped_lock lock(m_mux_tlv);
    for(std::list<boost::shared_ptr<xusb_tlv> >::iterator iter=m_list_tlv.begin(); iter!=m_list_tlv.end();)
    {
        boost::shared_ptr<xusb_tlv> tlv =*iter;
        if(tlv->m_tid ==tid) m_list_tlv.erase(iter++);
        else iter++;
    }
    LOG_MSG(MSG_LOG,"Exited xusbpackage::del_data()");
}

void xusbpackage::reset()
{
    LOG_MSG(MSG_LOG,"Enter into xusbpackage::reset()");
    m_msg_cmd = NC_NONE;
    m_version = 0;
    m_session = 0;
    m_srcid = 0;
    m_dstid = 0;
    m_crc16 = 0;
    boost::mutex::scoped_lock lock(m_mux_tlv);
    m_list_tlv.clear();
    LOG_MSG(MSG_LOG,"Exited xusbpackage::reset()");
}
