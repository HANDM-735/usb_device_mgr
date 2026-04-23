#ifndef XUSBPACKAGE_H_
#define XUSBPACKAGE_H_

#include "xbasic.hpp"
#include "xpackage.hpp"
#include "xbasicasio.hpp"
#include <boost/shared_ptr.hpp>
#include <boost/array.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/thread/mutex.hpp>
#include <string.h>
#include <list>
#include "mgr_log.h"

#define USB_HEADER 0xFE55  // USB包头标识
#define USB_HEADER_LEN 11  // USB包头长度
#define USB_MAXMSG_LEN 40960  // USB包最大包长度
#define USB_MSG_CRC 2  // USB的消息包的CRC16校验
#define PORT_USB "<USB>"  // USB端口名称定义

#define USB_MESSAGE_MASK 0xF0
#define USB_REQUEST_MESSAGE 0x00
#define USB_RESPONE_MESSAGE 0x80
#define USB_NOTIFY_MESSAGE 0x90
#define USB_CMD_MASK 0x0F


class usb_bin_packer : public xpacker  // BIN消息打包器类
{
public:
    usb_bin_packer();
    virtual ~usb_bin_packer();

    virtual boost::shared_ptr<const std::string> pack_data(const char *pdata, size_t datalen, PACKWAY pack_way);
};

class usb_bin_unpacker : public xunpacker  // BIN消息解包器类
{
public:
    usb_bin_unpacker();
    virtual ~usb_bin_unpacker();

public:
    virtual void reset_data();
    virtual bool unpack_data(size_t bytes_data, boost::container::list<boost::shared_ptr<const std::string> > &list_pack);
    virtual boost::asio::mutable_buffers_1 prepare_buff(size_t &min_recv_len);  // 准备下一个数据接收缓冲区
};


class xusbpackage : public xpacket  // usb报文
{
public:
    enum MSG_CMD { MC_NONE = 0x00, MC_READ = 0x01, MC_WRITE, MC_COMMAND, MC_HEARTBEAT, MC_ALARM, MC_OTA, MC_NOTIFY };  // 报文命令字定义

    xusbpackage(unsigned char msg_cmd = MC_NONE, unsigned char version = 0, unsigned char session = 0, unsigned short srcid = 0, unsigned short dstid = 0, unsigned short crc16 = 0) :
        m_msg_cmd(msg_cmd), m_version(version), m_session(session), m_srcid(srcid), m_dstid(dstid), m_crc16(crc16), m_crc_checksum(0)
    {}
    xusbpackage(unsigned char *bin_data, int data_len) : m_msg_cmd(MC_NONE), m_version(0), m_session(0), m_srcid(0), m_dstid(0), m_crc16(0)
    {
        parse_from_bin(bin_data, data_len);
    }

    virtual ~xusbpackage() {}

    virtual xpacket* clone();  // 克隆对象

public:
    virtual bool parse_from_bin(unsigned char *pack_data, int pack_len);  // 从BIN解析数据包
    virtual std::string serial_to_bin();  // 串行化成BIN

    int add_tlv_data(std::list<boost::shared_ptr<xusb_tlv> > &list_tlv, unsigned char cmd);  // 将TLV对象数组加入到包
    int get_tlv_data(std::list<boost::shared_ptr<xusb_tlv> > &list_tlv);  // 获取TLV对象数组
    void add_data(unsigned short tid, std::string value, unsigned char cmd);  // 加入tlv数据单元到package
    void add_data(boost::shared_ptr<xusb_tlv> tvl_data, unsigned char cmd);  // 加入tlv数据单元到package
    boost::shared_ptr<xusb_tlv> find_data(unsigned short tid);  // 从package寻找数据单元
    void del_data(unsigned short tid);  // 从package删除数据单元

    virtual void reset();

    virtual bool is_empty() { return (m_msg_cmd == MC_NONE); }
    virtual bool need_confirm() { return (m_msg_cmd & USB_MESSAGE_MASK == USB_REQUEST_MESSAGE); }  // 需要确认并重送的包
    virtual bool type_confirm() { return (m_msg_cmd & USB_MESSAGE_MASK == USB_RESPONE_MESSAGE); }  // 是确认包
    virtual int flag_confirm() { return m_session; }  // 获取确认报文的确认标志

    bool crc_checksum() { return m_crc_checksum == 0; }

public:
    unsigned char m_msg_cmd;  // 报文命令字
    unsigned char m_version;  // 协议版本号
    unsigned char m_session;  // 用于匹配请求和响应
    unsigned short m_srcid;  // 源设备地址
    unsigned short m_dstid;  // 目的设备地址
    unsigned short m_crc16;  // crc16校验码

private:
    unsigned short m_crc_checksum;  // 校验接收到的请求和应答消息是否crc校验正确，//当为0，表示校验正确，非0表示不正确

protected:
    std::list<boost::shared_ptr<xusb_tlv> > m_list_tlv;  // TLV数据链表
    boost::mutex m_mux_tlv;  // TLV数据锁
};

#endif
