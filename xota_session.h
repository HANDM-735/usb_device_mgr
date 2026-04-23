#ifndef XOTA_SESSION__H_H_
#define XOTA_SESSION__H_H_

#include <string>

#define MAX_FILE_TRANSFER_LEN    256
#define MAX_CAL_FILE_READ_LEN    200

enum ota_type
{
    //PGB板主FPGA
    OTA_TYPE_PGB_M_FPGA          = 0,
    OTA_TYPE_FTPGB_FPCU_FPGA     = 0,
    //PGB板从FPGA
    OTA_TYPE_PGB_D_FPGA          = 1,
    OTA_TYPE_FTPGB_FPMU_FPGA     = 1,
    //PGB板单片机
    OTA_TYPE_PGB_MCU             = 2,
    OTA_TYPE_FTPGB_MCU           = 2,

    //PPS板主FPGA
    OTA_TYPE_PPS_M_FPGA          = 3,
    OTA_TYPE_FTPPS_FCCU_FPGA     = 3,
    //PPS板从FPGA
    OTA_TYPE_PPS_D_FPGA          = 4,
    OTA_TYPE_FTPPS_FPSU_FPGA     = 4,
    //PPS板单片机
    OTA_TYPE_PPS_MCU             = 5,
    OTA_TYPE_FTPPS_MCU           = 5,

    //SYNC板FPGA
    OTA_TYPE_SYNC_FPGA           = 6,
    OTA_TYPE_FTSYNC_FXBU_FPGA    = 6,
    //SYNC板单片机
    OTA_TYPE_SYNC_MCU            = 7,
    OTA_TYPE_FTSYNC_MCU          = 7,

    //ASIC板主FPGA
    OTA_TYPE_ASIC_M_FPGA         = 8,
    //ASIC板从FPGA
    OTA_TYPE_ASIC_D_FPGA         = 9,
    //ASIC板单片机
    OTA_TYPE_ASIC_MCU            = 10,
    OTA_TYPE_FTASIC_MCU          = 10,

    //CAL校准文件
    OTA_TYPE_CAL_FILE            = 11,

    //CP设备类型
    OTA_TYPE_CPPGB_ZU11_M_FPGA   = 12,
    OTA_TYPE_CPPGB_S_FPGA       = 13,
    OTA_TYPE_CPPGB_MCU           = 14,

    //CPDPS板主FPGA
    OTA_TYPE_CPDPS_M_FPGA        = 15,
    OTA_TYPE_CPDPS_CCCU_FPGA     = 15,
    //CPDPS板从FPGA
    OTA_TYPE_CPDPS_S_FPGA        = 16,
    OTA_TYPE_CPDPS_CPSU_FPGA     = 16,
    OTA_TYPE_CPDPS_MCU           = 17,

    //CPSYNC板的FPGA
    OTA_TYPE_CPSYNC_FPGA         = 18,
    OTA_TYPE_CPSYNC_CXBU_FPGA    = 18,

    OTA_TYPE_CPSYNC_MCU          = 19,

    //CPPEM板200T的FPGA
    OTA_TYPE_CPPEM_FPGA          = 20,
    OTA_TYPE_CPPEM_CPMU_FPGA     = 20,
    OTA_TYPE_CPPEM_MCU           = 21,
    OTA_TYPE_CPRCA_MCU          = 22,

    //FT TH监控板MCU
    OTA_TYPE_FTTH_MCU            = 23,
    //FT MF监控板MCU
    OTA_TYPE_FTMF_MCU            = 24,
    //CP TH监控板MCU
    OTA_TYPE_CPTH_MCU            = 25,
    //CP MF监控板MCU
    OTA_TYPE_CPMF_MCU             = 26,

    //CPRCA板FPGA
    OTA_TYPE_CPRCA_CAFU_FPGA     = 27,
    //CPDIG板FPGA
    OTA_TYPE_CPDIG_CDCU_FPGA     = 28,
    //CPPEM板1ST的FPGA
    OTA_TYPE_CPPEM_CPDS_FPGA     = 29,
    //CPPEM板AGFB的FPGA
    OTA_TYPE_CPPEM_CPGM_FPGA     = 30,

    //FTFEB_FPGM的FPGA
    OTA_TYPE_FTFEB_FPGM_FPGA     = 31,
    //FTFEB_FPDS的FPGA
    OTA_TYPE_FTFEB_FPDS_FPGA     = 32,
    //FTDIG_FDCU的FPGA
    OTA_TYPE_FTDIG_FDCU_FPGA     = 33,

    //校准文件类型定义
    //CP校准文件类型
    // pem 板UTP40校准文件
    OTA_TYPE_CPPEMUTP40_MV_CAL  = 40,
    OTA_TYPE_CPPEMUTP40_MI_CAL  = 41,
    OTA_TYPE_CPPEMUTP40_FV_CAL  = 42,
    OTA_TYPE_CPPEMUTP40_FI_CAL  = 43,
    OTA_TYPE_CPPEMUTP40_ADC_CAL = 44,
    OTA_TYPE_CPPEMUTP40_IC_CAL  = 45,

    // pem 板UTP102校准文件
    OTA_TYPE_CPPEMUTP102_MV_CAL  = 50,
    OTA_TYPE_CPPEMUTP102_MI_CAL  = 51,
    OTA_TYPE_CPPEMUTP102_FV_CAL  = 52,
    OTA_TYPE_CPPEMUTP102_FI_CAL  = 53,

    // dps 板UTP40校准文件
    OTA_TYPE_CPDPSUTP40_MV_CAL  = 60,
    OTA_TYPE_CPDPSUTP40_MI_CAL  = 61,
    OTA_TYPE_CPDPSUTP40_FV_CAL  = 62,
    OTA_TYPE_CPDPSUTP40_FI_CAL  = 63,
    OTA_TYPE_CPDPSUTP40_ADC_CAL = 64,
    OTA_TYPE_CPDPSUTP40_IC_CAL  = 65,

    //FT校准文件类型
    // pgb 板UTP40校准文件
    OTA_TYPE_FTPGBUTP40_MV_CAL  = 70,
    OTA_TYPE_FTPGBUTP40_MI_CAL  = 71,
    OTA_TYPE_FTPGBUTP40_FV_CAL  = 72,
    OTA_TYPE_FTPGBUTP40_FI_CAL  = 73,
    OTA_TYPE_FTPGBUTP40_ADC_CAL = 74,
    OTA_TYPE_FTPGBUTP40_IC_CAL  = 75,

    // pps 板UTP40校准文件
    OTA_TYPE_FTPPSUTP40_MV_CAL  = 80,
    OTA_TYPE_FTPPSUTP40_MI_CAL  = 81,
    OTA_TYPE_FTPPSUTP40_FV_CAL  = 82,
    OTA_TYPE_FTPPSUTP40_FI_CAL  = 83,
    OTA_TYPE_FTPPSUTP40_ADC_CAL = 84,
    OTA_TYPE_FTPPSUTP40_IC_CAL  = 85,

    // CP PE 校准文件
    //PEM板上PE幅值校准文件
    OTA_TYPE_CPPEM_PE_AC_CAL   = 90,
    //PEM板上PE相位校准文件
    OTA_TYPE_CPPEM_PE_DC_CAL   = 91,

    // FT PE 校准文件
    //FEB板上PE幅值校准文件
    OTA_TYPE_FTFEB_PE_AC_CAL    = 100,
    //FEB板上PE相位校准文件
    OTA_TYPE_FTFEB_PE_DC_CAL    = 101,

    OTA_TYPE_MAX
};

enum ota_session_status
{
    OTA_SESSION_STATUS_IDLE        = 0,
    OTA_SESSION_STATUS_STARTED,
    OTA_SESSION_STATUS_UPGRADING,
    OTA_SESSION_STATUS_CANCLED,
    OTA_SESSION_STATUS_COMPLETED,
    OTA_SESSION_STATUS_END,

    OTA_SESSION_STATUS_CALFILE_READ,
    OTA_SESSION_STATUS_CALFILE_DATA,
    OTA_SESSION_STATUS_CALFILE_COMPLETE,
    OTA_SESSION_STATUS_CALFILE_END,

    OTA_SESSION_MAX
};

class ota_session
{
public:
    ota_session();
    ota_session(int staus,int type,int version,unsigned int total_len = 0);
    virtual ~ota_session();

public:
    void set_session_status(int status);
    void set_ota_type(int type);
    void set_ota_version(int version);
    void set_ota_transed_len(unsigned int len);
    void set_ota_total_len(unsigned int len);
    void set_ota_md5(const unsigned char* md5);
    void set_xsession_id(int sessid);
    void set_xsession_portname(const std::string& port_name);
    void set_ota_filename(const std::string& filename);
    void reset_last_active_tm(time_t value);

    int get_session_status();
    int get_ota_type();
    int get_ota_version();
    unsigned int get_ota_transed_len();
    unsigned int get_ota_total_len();
    void get_ota_md5(unsigned char* md5);
    int get_xsession_id();
    std::string get_xsession_portname();
    std::string get_ota_filename();
    time_t get_last_active_tm();

private:
    //升级ota会话状态
    int             m_status;
    //ota固件类型
    int             m_ota_type;
    //ota固件版本
    int             m_ota_version;
    //已经传输的长度
    unsigned int    m_trans_len;
    //ota固件总长度
    unsigned int    m_total_len;
    //文件md5校验
    unsigned char   m_md5[16];
    //文件名称
    std::string     m_filename;
    //关联xsession对象id
    int             m_usbadapter_session_id;
    //关联xsession对象portname
    std::string     m_session_portname;
    //最后活动时间戳
    time_t          m_last_active_time;
};

#endif
