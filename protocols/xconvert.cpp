#include "xconvert.h"
#include "mgr_log.h"

#define UTP_REGISTER_NUM	11	// UTP40 寄存器数量
#define UTP102_REGISTER_NUM	11	// UTP102 寄存器数量

servo_err::servo_err(const servo_err& other)
{
    if (&other != this) {
        this->m_servo_id = other.m_servo_id;
        this->m_errcode  = other.m_errcode;
    }
}

servo_err& servo_err::operator=(const servo_err& other)
{
    if (&other != this) {
        this->m_servo_id = other.m_servo_id;
        this->m_errcode  = other.m_errcode;
    }
    return *this;
}

int xconvert::convert(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(DBG_LOG, "Enter into xconvert::convert()");
    int ret = 0;
    unsigned short tid = 0;
    if (src == NULL) {
        LOG_MSG(ERR_LOG, "xconvert::convert() src is NULL");
        return -1;
    }
    tid = src->m_tid;
    LOG_MSG(DBG_LOG, "xconvert::convert() tid=0x%x", tid);
    switch (tid) {
        case RWA_TEMP: // pcb温度
            ret = convert_pcbtemp(dst, src);
            break;
        case RWA_VOLTAGE: // 电压
            ret = convert_pcbvoltage(dst, src);
            break;
        case RWA_AD9528_LOCKSTATUS: // AD9528时钟锁定状态
            ret = convert_ad9528_lckstatus(dst, src);
            break;
        case RWA_AD9545_LOCKSTATUS: // AD9545时钟锁定状态
            ret = convert_ad9545_lckstatus(dst, src);
            break;
        case RWA_PWM_CHECK: // PWM check
            ret = convert_pwm_check(dst, src);
            break;
        case RWA_POWER_STATUS:      // 电源状态
        case RWA_BOARD_TYPE:        // 单板类型
        case RWA_BOARD_SN:          // 单板SN
        case RWA_MANU_INFO:         // 单板制造信息
        case RWA_HW_VER:  	        // 单板硬件版本
        case RWA_SW_VER:  	        // 单板软件版本
        case RWA_SLOT:  	        // slot号
        case RWA_POS_STATUS:        // 单板在位状态
        case RWA_DIG_ID:	        // DIG板的ID号
       case RWA_DIG_EXIST:	        // DIG板在位状态
        case RWA_UTP40_TEMP_ALARM:  // UTP40 tmp alarm
            ret = convert_basic_info(dst, src);
            break;
        case RWA_UTP40_TEMP:        // UTP40 tmp
            ret = convert_utp40_temp(dst, src);
            break;
        default: 
            ret = -2;
            break;
    }
    LOG_MSG(DBG_LOG, "Exited xconvert::convert() tid=0x%x ret=%d", tid, ret);
    return ret;
}

int xconvert::parse_pcbtemp_data(const std::string& value, std::vector<pcb_temp>& vct_temp)
{
    LOG_MSG(MSG_LOG, "Enter into xconvert::parse_pcbtemp_data()");
    int ret = 0;
    if (value.empty()) {
        LOG_MSG(ERR_LOG, "xconvert::parse_pcbtemp_data() value is empty");
        return -1;
    }

    const char* value_ptr = value.data();
    size_t length = value.length();
    int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
    LOG_MSG(MSG_LOG, "xconvert::parse_pcbtemp_data() num=%d", num);
    if ((num * 4) != (length - 4)) {
        LOG_MSG(ERR_LOG, "xconvert::parse_pcbtemp_data() value is invaild,length=%d num=%d", length, num);
        return -1;
    }
    int i = 0;
    char* pay_data = (char*)&value_ptr[4];
    while (i < num) {
        if ((i * 4) > (length - 4)) {
            LOG_MSG(ERR_LOG, "xconvert::parse_pcbtemp_data() index=%d was greater than length=%d", (i * 4), (length - 4));
            return -1;
        }
        float tmp = xbasic::readfloat_bigendian((reinterpret_cast<void*>(pay_data + i * 4)), 4);
        pcb_temp temp;
        temp.m_id = ++i;
        temp.m_temp_value = tmp;
        vct_temp.push_back(temp);
    }
    LOG_MSG(MSG_LOG, "Exited xconvert::parse_pcbtemp_data() vct_temp size=%d ret=%d ", vct_temp.size(), ret);
    return ret;
}

int xconvert::convert_pcbtemp(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG, "Enter into xconvert::convert_pcbtemp()");
    int ret = 0;
    std::vector<pcb_temp> vct_tmp;

    // 先校验解析结果，异常直接返回
    int parse_ret = parse_pcbtemp_data(src->m_value, vct_tmp);
    if (parse_ret != 0) {
        dst = boost::shared_ptr<xtvl>();
        LOG_MSG(ERR_LOG, "xconvert::convert_pcbtemp() parse pcbtemp data failed, ret=%d", parse_ret);
        return -1;
    }

    int vct_size = vct_tmp.size();
    // 空数据异常，提前返回
    if (vct_size <= 0) {
        dst = boost::shared_ptr<xtvl>();
        LOG_MSG(WRN_LOG, "xconvert::convert_pcbtemp() temp data size=%d", vct_size);
        return -1;
    }
    // 核心业务
    unsigned short tid = 0;
    switch (src->m_tid) {
        case RWA_TEMP:
            tid = BOARD_TMP_VALUE;
            break;
        default:
            break;
    }
    std::vector<std::string> vect_str;
    for (int i = 0; i < vct_size; i++) {
        std::string str;
        str = std::to_string(vct_tmp[i].m_id) + ":" + std::to_string(vct_tmp[i].m_temp_value);
        vect_str.push_back(str);
    }
    std::string value = boost::join(vect_str, ",");
    boost::shared_ptr<xtvl> new_data(new xtvl(tid, value.length(), value.data()));
    dst = new_data;
    LOG_MSG(MSG_LOG, "Exited xconvert::convert_pcbtemp() ret=%d", ret);
    return ret;
}

int xconvert::parse_pcbvolt_data(const std::string& value,std::vector<pcb_volatage>& vct_vol)
{
    LOG_MSG(MSG_LOG, "Enter into xconvert::parse_pcbvolt_data()");
    int ret = 0;
    if (value.empty()) {
        LOG_MSG(ERR_LOG, "xconvert::parse_pcbvolt_data() value is empty");
        return -1;
    }

    const char* value_ptr = value.data();
    size_t length = value.length();
    int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
    LOG_MSG(MSG_LOG, "xconvert::parse_pcbvolt_data() num=%d", num);
    if ((num * 4) != (length - 4)) {
        LOG_MSG(ERR_LOG, "xconvert::parse_pcbvolt_data() value is invaild,length=%d num=%d", length, num);
        return -1;
    }
    int i = 0;
    char* pay_data = (char*)&value_ptr[4];
    while (i < num) {
        if ((i * 4) > (length - 4)) {
            LOG_MSG(ERR_LOG, "xconvert::parse_pcbvolt_data() index=%d was greater than length=%d", (i * 4), (length - 4));
            return -1;
        }
        float vol = xbasic::readfloat_bigendian((reinterpret_cast<void*>(pay_data + i * 4)), 4);
        pcb_volatage volt;
        volt.m_id = ++i;
        volt.m_volt_value = vol;
        vct_vol.push_back(volt);
    }
    LOG_MSG(MSG_LOG, "Exited xconvert::parse_pcbvolt_data() vct_vol size=%d ret=%d ", vct_vol.size(), ret);
    return ret;
}

int xconvert::convert_pcbvoltage(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG, "Enter into xconvert::convert_pcbvoltage()");
    int ret = 0;
    std::vector<pcb_volatage> vct_vol;

    // 先校验解析结果，异常直接返回
    int parse_ret = parse_pcbvolt_data(src->m_value, vct_vol);
    if (parse_ret != 0) {
        dst = boost::shared_ptr<xtvl>();
        LOG_MSG(ERR_LOG, "xconvert::convert_pcbvoltage() parse pcbvolt data failed, ret=%d", parse_ret);
        return -1;
    }

    int vct_size = vct_vol.size();
    // 空数据异常，提前返回
    if (vct_size <= 0) {
        dst = boost::shared_ptr<xtvl>();
        LOG_MSG(WRN_LOG, "xconvert::convert_pcbvoltage() volt data size=%d", vct_size);
        return -1;
    }
    // 核心业务逻辑，无嵌套，扁平化
    unsigned short tid = 0;
    switch (src->m_tid) {
        case RWA_VOLTAGE:
            tid = BOARD_VOLTAGE;
            break;
        default:
            break;
    }
    std::vector<std::string> vect_str;
    for (int i = 0; i < vct_size; i++) {
        std::string str;
        str = std::to_string(vct_vol[i].m_id) + ":" + std::to_string(vct_vol[i].m_volt_value);
        vect_str.push_back(str);
    }
    std::string value = boost::join(vect_str, ",");
    boost::shared_ptr<xtvl> new_data(new xtvl(tid, value.length(), value.data()));
    dst = new_data;
    LOG_MSG(MSG_LOG, "Exited xconvert::convert_pcbvoltage() ret=%d", ret);
    return ret;
}

int xconvert::parse_ad9528_lckdata(const std::string& value, std::vector<ad9528_lock>& vct_lock)
{
    LOG_MSG(MSG_LOG, "Enter into xconvert::parse_ad9528_lckdata()");
    int ret = 0;
    if (value.empty()) {
        LOG_MSG(ERR_LOG, "xconvert::parse_ad9528_lckdata() value is empty");
        return -1;
    }

    const char* value_ptr = value.data();
    size_t length = value.length();
    int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
    LOG_MSG(MSG_LOG, "xconvert::parse_ad9528_lckdata() num=%d", num);

    // 变长结构报文最小长度校验（报文至少能存下4个字节的协议头）
    if (length < 4) {
        LOG_MSG(ERR_LOG, "xconvert::parse_ad9528_lckdata() value length is too short, length=%d", length);
        return -1;
    }

    int iread = 0;
    char* pay_data = (char*)&value_ptr[4];
    size_t pay_length = length - 4;
    int i = 0;

    while (i < num) {
        // 基础字段读取不越界
        if ((size_t)iread + 8 > pay_length) {
            LOG_MSG(ERR_LOG, "xconvert::parse_ad9528_lckdata() read out of bounds, iread=%d pay_length=%zu", iread, pay_length);
            return -1;
        }
        int ch = xbasic::read_bigendian(reinterpret_cast<void*>(pay_data + iread), 4);
        int lockstatus = xbasic::read_bigendian(reinterpret_cast<void*>(pay_data + iread + 4), 4);

        // 通道数合法性
        if (ch < 0 || ch > 1024) { 
            LOG_MSG(ERR_LOG, "xconvert::parse_ad9528_lckdata() invalid channel num=%d", ch);
            return -1;
        }

        // pwm数据读取不越界
        size_t need_length = 8 + (size_t)ch * 4;
        if ((size_t)iread + need_length > pay_length) {
            LOG_MSG(ERR_LOG, "xconvert::parse_ad9528_lckdata() pwm data out of bounds, iread=%d need=%d pay_length=%d", iread, need_length, pay_length);
            return -1;
        }
        ad9528_lock ad9528_lck;
        ad9528_lck.m_lockstatus = lockstatus;
        // 读取pwm数据
        for (int ich = 0; ich < ch; ++ich) {
            float pwm_val = xbasic::readfloat_bigendian(reinterpret_cast<void*>(pay_data + iread + 8 + ich * 4), 4);
            ad9528_lck.m_chnpwm_vct.push_back(pwm_val);
        }
        iread += need_length;
        vct_lock.push_back(ad9528_lck);
        i++;
        LOG_MSG(MSG_LOG, "xconvert::parse_ad9528_lckdata() pwm size=%d ch=%d iread=%d locksize=%d",
                ad9528_lck.m_chnpwm_vct.size(), ch, iread, vct_lock.size());
    }
    LOG_MSG(MSG_LOG, "Exited xconvert::parse_ad9528_lckdata() vct_lock size=%d ret=%d", vct_lock.size(), ret);
    return ret;
}