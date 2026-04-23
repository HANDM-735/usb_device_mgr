#ifndef X_DEVICE_H
#define X_DEVICE_H
#include <string>
#include <map>
#include <list>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include "xbasic.hpp"
#include "xbasicmgr.hpp"
#include "xpackage.hpp"
#include "xconfig.h"
#include "xusbpackage.h"
#include "xota_session.h"
#include "mgr_network.h"
#include "mgr_usb_def.h"
#include "mgr_platform_tid_def.h"
#include "mgr_upgrade.h"
#include "mgr_device.h"
#include "xboard.h"

#if defined(SYNC_BUILD)

#define BOARD_X86        (0x01)    //x86板ID
#define BOARD_STV        (0x10)    //箱体ID
#define BOARD_TST(slot)  (0x40+slot) //单板ID

class mgr_device;
class temp_range;
class fan_speed;
class smoke_density;
class sensor_alarm;
class sensor_data;
class lcs_mode;
class lcs_fault;
class lcs_alarm;
class three_phase;
class servo_err;
class ps48v_info;
class motor_position;

//TH/MF监控板类
class xmboard : public xtransmitter
{
public:
    xmboard(int id,std::string port_name="");
    ~xmboard();

public:
    int      get_id();
    void     set_id(int id);
    void     set_port_name(std::string port_name);
    void     set_activetm(time_t activetm);
    time_t   get_activetm();

    int      send_ota_start(int dstid,int ota_type);
    int      send_ota_cancel(int dstid,int ota_type);
    void     send_heartbeat();
    void     check_ota_status();

    std::string  get_port_name();
    //获取指定数据
    std::string  get_value(unsigned char tid);
    //获得该单板的OTA升级文件名
    static std::string get_ota_fw_name(int ota_type);
    //获得单板类型名称
    static std::string get_board_name(int board_id);

    boost::shared_ptr<xusb_tvl> find_data(unsigned short tid);
    boost::shared_ptr<ota_session> get_ota_session();

public:
    //通讯相关接口
    //消息通知
    virtual int on_recv(const char *port_name,xusbpackage *pack);

protected:
    //南向消息处理
    int handle_south_recv(xusbpackage *pack);
    //南向请求消息处理
    int handle_south_request(xusbpackage *pack);
    //南向应答消息处理
    int handle_south_response(xusbpackage *pack);
    //南向通知消息处理
    int handle_south_notify(xusbpackage *pack);

    int handle_ota_request_message(xusbpackage *the_pack);
    int handle_alarm_request_message(xusbpackage *the_pack);

private:
    int save_tlv_data(unsigned short board_id,std::list<boost::shared_ptr<xusb_tvl> >& list_tlv,unsigned char cmd);

public:
    //将安全策略的tid转换上位机相关tid
    static int convert_securitypolicy(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);
    //解析温度安全策略tidvalue值
    static int parse_temppolicy(const std::string& value,std::vector<temp_range>& vct_range);
    //将温度安全策略的tid转换上位机相关tid
    static int convert_temppolicy(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析烟感安全策略tidvalue值
    static int parse_smkepolicy(const std::string& value,std::vector<smoke_density>& vct_smk);
    //将烟感安全策略的tid转换上位机相关tid
    static int convert_smkepolicy(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析风扇安全策略tidvalue值
    static int parse_fanpolicy(const std::string& value,std::vector<fan_speed>& vct_fan);
    //将风扇安全策略的tid转换上位机相关tid
    static int convert_fanpolicy(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //将告警的tid转换上位机相关tid
    //解析温度告警数值
    static int parse_temp_alarm(const std::string& value,float& temp_alarm_val);
    //将温度告警的tid转换上位机相关tid
    static int convert_tempalarm(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析烟感数值
    static int parse_smoke_alarm(const std::string& value,float& smoke_alarm_val);
    //将烟感告警的tid转换上位机相关tid
    static int convert_smokealarm(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析cdu告警数值
    static int parse_cdu_alarm(const std::string& value,float& cdu_alarm_val);
    //将cdu告警的tid转换上位机相关tid
    static int convert_cdualarm(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析48v动力源告警数值
    static int parse_48vps_alarm(const std::string& value,float& ps_val);
    //将48v动力源告警的tid转换上位机相关tid
    static int convert_48vpsalarm(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析厂务水告警数值
    static int parse_factorywater_alarm(const std::string& value,float& factorywater_val);
    //将厂务水告警的tid转换上位机相关tid
    static int convert_factorywater_alarm(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析电压,温度传感器告警数值
    static int parse_sensor_alarm(const std::string& value,std::vector<sensor_alarm>& vct_sensors);
    //将电压,温度传感器告警的tid转换上位机相关tid
    static int convert_sensor_alarm(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析液冷系统故障,告警相关数据
    static int parse_lcs_data_alarm(const std::string& value,std::vector<lcs_alarm>& vect_sensor);
    //将液冷系统一些数据告警的tid转换上位机相关tid
    static int convert_lcs_data_alarm(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析TH伺服器告警错误
    static int parse_th_servo_alarm(const std::string& value,float& servo_err_alarm);
    //将TH伺服器告警错误码转换成上位机相关tid
    static int convert_th_servo_alarm(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //read读取的数据的tid转换上位机相关tid
    static int convert_get_device_data(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到温度数值
    static int parse_temp_data(const std::string& value,std::vector<sensor_data>& vct_sensors);
    //将读取到温度的tid转换上位机相关tid
    static int convert_tempdata(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到烟感数值
    static int parse_smoke_data(const std::string& value,std::vector<sensor_data>& vct_sensors);
    //将读取到烟感的tid转换上位机相关tid
    static int convert_smokedata(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到风扇转速数值
    static int parse_fan_data(const std::string& value,std::vector<sensor_data>& vct_sensors);
    //将读取到风扇转速的tid转换上位机相关tid
    static int convert_fandata(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到的cdu数值
    static int parse_cdu_data(const std::string& value,float& cdu_alarm_val);
    static int convert_cdudata(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到的48v动力源数值
    static int parse_48vps_data(const std::string& value,std::vector<ps48v_info>& vect_ps);
    //将读取到的48v动力源数据的tid转换上位机相关tid
    static int convert_48vps_data(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到的厂务水数值
    //static int parse_factorywater_data(const std::string& value,float& alarm_val);
    //static int convert_factorywater_data(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到的三相电压,电流值
    static int parse_three_phase_data(const std::string& value,std::vector<three_phase>& vect_phase);
    //将读取到的三相电压,电流的tid转换上位机相关tid
    static int convert_three_phase_data(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到的非动力源UPS数值
    static int parse_ups_data(const std::string& value,float& cdu_alarm_val);
    //将读取到的非动力源UPS的tid转换上位机相关tid
    //static int convert_ups_data(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到的水浸数值
    static int parse_waterlogging_data(const std::string& value,float& cdu_alarm_val);
    //将读取到的水浸数据的tid转换上位机相关tid
    static int convert_waterlogging_data(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取的TesterHader hifif数值
    static int parse_hifix_data(const std::string& value,int& hifif_status);
    //将读取到的简单数播的tid转换上位机相关tid
    static int convert_hifix_data(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到液冷系统工作模式
    static int parse_lcs_mode(const std::string& value,std::vector<lcs_mode>& vct_mode);
    //将读取到液冷系统工作模式相关tid转换上位机相关tid
    static int convert_lcs_mode(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到液冷系统CDU相关数据
    static int parse_lcs_data(const std::string& value,std::vector<sensor_data>& vct_sensors);
    static int convert_lcs_data(std::list<boost::shared_ptr<xusb_tvl> >& dst_list, boost::shared_ptr<xusb_tvl> &src_xusbtlv);

    //解析读取到TH电机位置
    static int parse_motorpos_data(const std::string& value,std::vector<motor_position>& vct_motorpos);

    //将安全策略的tid转换上位机相关tid模拟测试函数
    static int convert_securitypolicy_test(std::list<boost::shared_ptr<xusb_tvl> >& dst_list);

private:
    //上报TH伺服器告警
    int send_thservo_alarm(const std::string value);

    bool is_ota_session_status(int status);
    int set_ota_transed_length(unsigned int len);
    int set_ota_session_status(int status);
    int read_otadata_from_file(const std::string filename, const unsigned int offset,const unsigned int len,char* read_buff);
    void send_ota_data(int req_sessionid,int dstid,boost::shared_ptr<xusb_tvl>& req_ota_data_tlv);

    int del_ota_session();
    boost::shared_ptr<ota_session> add_ota_session(int status,int ota_type,int ota_version,unsigned int total_len);

protected:
    mgr_upgrade*        m_upgrad_mgr;    //升级管理器
    mgr_device*         m_device_mgr;   //监控板管理器
    xboardmgr*          m_board_mgr;     //单板管理

    int                 m_id;            //单板ID
    std::string         m_port_name;     //所在的端口名称
    time_t              m_activetm;      //最后活跃的时刻

private:
    boost::shared_mutex m_mux_session;   //ota升级会话读写锁
    boost::shared_ptr<ota_session> m_ota_session; //ota升级会话
};

#endif
#endif
