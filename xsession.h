#ifndef X_SESSION_H
#define X_SESSION_H
#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <map>
#include <list>
#include <functional>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include "xbasic.hpp"
#include "xbasicmgr.hpp"
#include "xconfig.h"
#include "usbadapter_package.h"
#include "mgr_usb.h"
#include "mgr_device.h"
#include "xboard.h"
#include "xconvert.h"


class xsession : public xtransmitter //会话
{
public:
    xsession(int session_id,const std::string& port_name);
    ~xsession();

public: //通讯相关接口
    virtual int on_recv(const char *port_name,xusbadapter_package *pack); //消息通知

public:
    std::string get_port_name();
    int get_sessionid();


protected:
    int handle_start_request(const char *port_name,xusbadapter_package *pack);
    int handle_query_request(const char *port_name,xusbadapter_package *pack);
    int handle_cancel_request(const char *port_name,xusbadapter_package *pack);
    int handle_complete_notify(const char *port_name,xusbadapter_package *pack);
    int handle_data_request(const char *port_name,xusbadapter_package *pack);
    int handle_cal_read_request(const char *port_name,xusbadapter_package *pack);
    int handle_heartbeat(const char *port_name,xusbadapter_package *pack);
    int handle_seridals_request(const char *port_name,xusbadapter_package *pack);
    int handle_junction_temp_request(const char *port_name,xusbadapter_package *pack);

    int handle_get_value_request(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_allboardid(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_digiexist(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_digid(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_hwver(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_softver(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_digigiver(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_sn(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_boardid(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_voltage(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_boardexist(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_pcbtemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_utp40temp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_rcafpgatemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_utp40temp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_ftsyntemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_ftpspgtemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_ftpgbtemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_ftsasictemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_cpsynctemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_cppmtemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_cpdpstemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_cprcptemp(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_9528_status(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_9545_status(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_pwm_frequency(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_serdes_status(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_utp40_register(const char *port_name,xusbadapter_package *pack);
    int handle_get_value_utp102_register(const char *port_name,xusbadapter_package *pack);

    int handle_command_request(const char *port_name,xusbadapter_package *pack);
    //整板电源控制(不包括MCU本身)
    int handle_command_board_psctrl(const char *port_name,xusbadapter_package *pack);
    //FPGA电源控制(1S250/AGBF019)
    int handle_command_fpga_psctrl(const char *port_name,xusbadapter_package *pack);
    //UTP40单元电源控制
    int handle_command_utp40_psctrl(const char *port_name,xusbadapter_package *pack);
    //ASIC芯片电源控制
    int handle_command_asic_psctrl(const char *port_name,xusbadapter_package *pack);
    //FPGA电源控制(7A 200T)
    int handle_command_fpga200T_psctrl(const char *port_name,xusbadapter_package *pack);
    //FPGA电源控制(KU060)
    int handle_command_fpgaaku060_psctrl(const char *port_name,xusbadapter_package *pack);
    //UTP40总电源控制
    int handle_command_utp40_master_psctrl(const char *port_name,xusbadapter_package *pack);

    //时钟频率配置
    int handle_frequency_set_request(const char *port_name, xusbadapter_package *pack);
    int handle_frequency_set_4356_ctrl(const char *port_name, xusbadapter_package *pack);

    int handle_get_utp_register_request(const char *port_name,xusbadapter_package *pack);
    int handle_get_utp102_register_request(const char *port_name,xusbadapter_package *pack);

    int send_start_response(xusbadapter_package *pack,int errcode);
    int send_cal_read_response(xusbadapter_package *pack,int errcode);

    bool is_monitor_board(unsigned short boardid);
    bool filter_data_request_tid(unsigned short tid);
    bool is_ota_caltype(int ota_type);
    void file_bak(const std::string& file_path);

private:
    int             m_sess_id;       //会话消息id
    std::string     m_port_name;    //会话端口
    unsigned short  m_version;       //协议版本号
protected:
    mgr_usb*        m_mgr_usb;      //usb管理器
    xboardmgr*     m_board_mgr;     //单板管理

#endif
