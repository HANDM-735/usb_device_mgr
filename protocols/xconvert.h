#ifndef X_CONVERT__H_H_
#define X_CONVERT__H_H_

#include <string>
#include <boost/shared_ptr.hpp>
#include "xusbpackage.h"
#include "xboard.h"
#include "mgr_usb_def.h"
#include "mgr_platform_tid_def.h"

#define UTP_REGISTER_NUM    11  // UTP40 寄存器数量
#define UTP102_REGISTER_NUM 11  // UTP102 寄存器数量

class pcb_temp
{
public:
    pcb_temp();
    ~pcb_temp();

public:
    int     m_id;
    float   m_temp_value;
};

class pcb_volatage
{
public:
    pcb_volatage();
    ~pcb_volatage();

public:
    int     m_id;
    float   m_volt_value;
};

class pwm_check
{
public:
    pwm_check();
    ~pwm_check();

public:
    int m_channel_id;
    int m_pwm_value;
};

class ad9528_lock
{
public:
    ad9528_lock();
    ~ad9528_lock();

public:
    int                 m_lockstatus;
    std::vector<float>  m_chnpwm_vct;
};

class ad9545_lock
{
public:
    ad9545_lock();
    ~ad9545_lock();

public:
    int                 m_lockstatus;
    std::vector<float>  m_chnpwm_vct;
};

class utp40_temp
{
public:
    utp40_temp();
    ~utp40_temp();

public:
    short   m_id;
    float   m_temp_value;
};

class asic_temp
{
public:
    asic_temp();
    ~asic_temp();

public:
    short   m_id;
    float   m_temp_value;
};

class fpga_temp
{
public:
    fpga_temp();
    ~fpga_temp();

public:
    short   m_id;
    float   m_temp_value;
};

class dig_volatage
{
public:
    dig_volatage();
    ~dig_volatage();

public:
    int     m_id;
    float   m_volt_value;
};

class comb_temp
{
public:
    comb_temp();
    ~comb_temp();

public:
    short   m_id;
    float   m_temp_value;
};

class rcafpga_temp
{
public:
    rcafpga_temp();
    ~rcafpga_temp();

public:
    short   m_id;
    float   m_temp_value;
};

class serdes_state
{
public:
    serdes_state();
    ~serdes_state();

public:
    short       m_id;
    unsigned char m_serdes_value;
};

class rk3588_ip
{
public:
    rk3588_ip();
    ~rk3588_ip();

public:
    short       m_rk3588_id;
    short       m_rca_id;
    short       m_pem_id;
    std::string m_ip;
};

class mcu_fpgaversion
{
public:
    mcu_fpgaversion();
    ~mcu_fpgaversion();

public:
    int     m_vertype;
    long long m_ver;
};

class mcu_fpgaserdes
{
public:
    mcu_fpgaserdes();
    ~mcu_fpgaserdes();

public:
    int m_serdestype;
    int m_serdes;
};

class utp_register_info
{
public:
    utp_register_info();
    ~utp_register_info();

public:
    int8_t      m_valid;    // 寄存器是否有效
    uint8_t     m_reg_ch;   // 通道号
    uint16_t    m_reg_addr; // 寄存器地址
    uint16_t    m_reg_data; // 寄存器值
};

class utp40_register
{
public:
    utp40_register();
    ~utp40_register();

public:
    short                                   m_utp40_chip_id; // utp40 芯片是连续编号
    std::vector<utp_register_info>          m_register;       // 寄存器信息数组
};

//key: 通道编号
typedef std::map<int,std::vector<rcafpga_temp> > channel_temp;
//key: rk3588槽位号
typedef std::map<int,std::vector<channel_temp> > rk3588_fpgatemp;
//key: fpga版本类型
typedef std::map<int,std::vector<mcu_fpgaversion> > mcu_fpgavermap;
//key: fpga的serdes类型
typedef std::map<int,std::vector<mcu_fpgaserdes> > mcu_fpgaserdemap;

class xconvert
{
public:
    xconvert();
    ~xconvert();

public:
    static int parse_pcbtemp_data(const std::string& value,std::vector<pcb_temp>& vct_temp);
    static int parse_pcbvolt_data(const std::string& value,std::vector<pcb_volatage>& vct_vol);
    static int parse_ad9528_lockdata(const std::string& value,std::vector<ad9528_lock>& vct_lock);
    static int parse_ad9545_lockdata(const std::string& value,ad9545_lock& ad9545_lck);
    static int parse_pwm_check(const std::string& value,std::vector<pwm_check>& vct_pwm);
    static int parse_utp40_temp(const std::string& value,std::vector<utp40_temp>& vct_temp);
    static int parse_asic_temp(const std::string& value,std::vector<asic_temp>& vct_temp);
    static int parse_fpga_temp(const std::string& value,std::vector<fpga_temp>& vct_temp);
    static int parse_dig_volatage(const std::string& value,std::vector<dig_volatage>& vct_vol);
    static int parse_comb_temp(const std::string& value,std::vector<comb_temp>& vct_temp);
    static int parse_rcafpga_temp(const std::string& value, rk3588_fpgatemp& rk3588_temp);
    static int parse_serdes_state(const std::string& value, std::vector<serdes_state>& vct_serdes);
    static int parse_rk3588_ip(const std::string& value, std::vector<rk3588_ip>& vct_rk3588_ip);
    static int parse_mcu_fpgaver(const std::string& value, mcu_fpgavermap& map_fpga_ver);
    static int parse_mcu_fpgaserdes(const std::string& value, mcu_fpgaserdemap& map_fpga_serdes);
    static int parse_utp40_register(const std::string& value,std::vector<utp40_register>& vct_register);
    static int find_one_utp_register(const std::string& value, int chip_id, std::string& r_value);
    static int find_one_utp102_register(const std::string& value, int chip_id, std::string& r_value);

    static int serial_utp40_temp(const std::vector<utp40_temp>& vct_temp,std::string& value);
    static int serial_asic_temp(const std::vector<asic_temp>& vct_temp,std::string& value);
    static int serial_fpga_temp(const std::vector<fpga_temp>& vct_temp,std::string& value);
    static int serial_dig_volatage(const std::vector<dig_volatage>& vct_vol,std::string& value);
    static int serial_comb_temp(const std::vector<comb_temp>& vct_temp,std::string& value);
    static int serial_rcafpga_temp(const rk3588_fpgatemp& rk3588_temp,std::string& value);
    static int serial_serdes_state(const std::vector<serdes_state>& vct_serdes,std::string& value);
    static int serial_utp40_register(const std::vector<utp40_register>& vct_register,std::string& value);

    static int convert(boost::shared_ptr<xtvl> dst, boost::shared_ptr<xusb_tvl>& src);
    static int convert_pcbtemp(boost::shared_ptr<xtvl> dst, boost::shared_ptr<xusb_tvl>& src);
    static int convert_pcbvoltage(boost::shared_ptr<xtvl> dst, boost::shared_ptr<xusb_tvl>& src);
    //static int convert_pcbcurrent(boost::shared_ptr<xtvl> dst, boost::shared_ptr<xusb_tvl>& src);
    static int convert_ad9528_lockstatus(boost::shared_ptr<xtvl> dst, boost::shared_ptr<xusb_tvl>& src);
    static int convert_ad9545_lockstatus(boost::shared_ptr<xtvl> dst, boost::shared_ptr<xusb_tvl>& src);
    static int convert_pwm_check(boost::shared_ptr<xtvl> dst, boost::shared_ptr<xusb_tvl>& src);
    static int convert_basic_info(boost::shared_ptr<xtvl> dst, boost::shared_ptr<xusb_tvl>& src);
    static int convert_utp40_temp(boost::shared_ptr<xtvl> dst, boost::shared_ptr<xusb_tvl>& src);
};

#endif
