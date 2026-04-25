#include "xconvert.h"



pcb_temp::pcb_temp()
{
	m_id = 0;
	m_temp_value = 0.0;
}

pcb_temp::~pcb_temp()
{
	
}


pcb_volatage::pcb_volatage()
{
	m_id = 0;
	m_volt_value = 0.0;
}

pcb_volatage::~pcb_volatage()
{
	
}

pwm_check::pwm_check()
{
	m_channel_id = 0;
	m_pwm_value	 = 0.0;
}

pwm_check::~pwm_check()
{
	
}

ad9528_lock::ad9528_lock()
{
	m_lockstatus = 1;
}

ad9528_lock::~ad9528_lock()
{
	
}

ad9545_lock::ad9545_lock()
{
	m_lockstatus = 1;
}


ad9545_lock::~ad9545_lock()
{
	
}

utp40_temp::utp40_temp()
{
	m_id  		 = 0;
	m_temp_value = 0.0;
}

utp40_temp::~utp40_temp()
{
	
}

asic_temp::asic_temp()
{
	m_id  		 = 0;
	m_temp_value = 0.0;
}

asic_temp::~asic_temp()
{
	
}


fpga_temp::fpga_temp()
{
	m_id  		 = 0;
	m_temp_value = 0.0;	
}


fpga_temp::~fpga_temp()
{
	
}


dig_volatage::dig_volatage()
{
	m_id = 0;
	m_volt_value = 0.0;
}

dig_volatage::~dig_volatage()
{
	
}


comb_temp::comb_temp()
{
	m_id  		 = 0;
	m_temp_value = 0.0;	
}

comb_temp::~comb_temp()
{
	
}

rcafpga_temp::rcafpga_temp()
{
	m_id  		 = 0;
	m_temp_value = 0.0;	
}

rcafpga_temp::~rcafpga_temp()
{
	
}

serdes_state::serdes_state()
{
	m_id = 0;
	m_serdes_value = 0;
}

serdes_state::~serdes_state()
{

}

rk3588_ip::rk3588_ip()
{
	m_rk3588_id = 0;
	m_rca_id = 0;
	m_pem_id = 0;
}

rk3588_ip::~rk3588_ip()
{
}


mcu_fpgaversion::mcu_fpgaversion()
{
	m_vertype 	= 0;
	m_ver 		= 0;
}

mcu_fpgaversion::~mcu_fpgaversion()
{
	
}


mcu_fpgaserdes::mcu_fpgaserdes()
{
	m_serdestype 	= 0;
	m_serdes 		= 0;
}

mcu_fpgaserdes::~mcu_fpgaserdes()
{
	
}

utp_register_info::utp_register_info()
{
	m_valid = -1; 		// 默认-1无效
	m_reg_ch = -1;		// 默认通道 -1
	m_reg_addr = -1;	// 默认地址
	m_reg_data = -1;	// 寄存器默认值 
}

utp_register_info::~utp_register_info()
{
}

utp40_register::utp40_register()
{
}

utp40_register::~utp40_register()
{
}


xconvert::xconvert()
{

}

xconvert::~xconvert()
{
	
}

int xconvert::convert(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::convert()");
    int ret = 0;
    unsigned short tid = 0;
    if(src == NULL)
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::convert() src is NULL");
    }
    else
    {
        tid = src->m_tid;
        LOG_MSG(MSG_LOG,"xconvert::convert() tid=0x%x", tid);

        switch(tid)
        {
            case RWA_TEMP:          // pcb温度
                ret = convert_pcbtemp(dst,src);
                break;
            case RWA_VOLTAGE:       // 电压
                ret = convert_pcbvoltage(dst,src);
                break;
            //case RWA_ELE_CURRENT:       // 电流
            //    ret = convert_pcbcurrent(dst,src);
            //    break;
            case RWA_AD9528_LOCKSTATUS:    // AD9528时钟锁定状态
                ret = convert_ad9528_1ckstatus(dst,src);
                break;
            case RWA_AD9545_LOCKSTATUS:   // AD9545时钟锁定状态
                ret = convert_ad9545_1ckstatus(dst,src);
                break;
            case RWA_PWM_CHECK:           // PWM check
                ret = convert_pwm_check(dst,src);
                break;
            case RWA_POWER_STATUS:        // 电源状态
            //case RWA_TEMP_SET:            // 温度告警
            case RWA_BOARD_TYPE:          // 单板类型
            case RWA_BOARD_SN:            // 单板SN
            case RWA_MANU_INFO:           // 单板制造信息
            case RWA_HW_VER:              // 单板硬件版本
            case RWA_SW_VER:              // 单板软件版本
            case RWA_SLOT:                // slot号
            case RWA_POS_STATUS:          // 单板在位状态
            case RWA_DIG_ID:              // DIG板的ID号
            case RWA_DIG_EXIST:           // DIG板在位状态
            case RWA_UTP40_TEMP_ALARM:    // UTP40 tmp alarm
                ret = convert_basic_info(dst,src);
                break;
            case RWA_UTP40_TEMP:          // UTP40 tmp
                ret = convert_utp40_temp(dst,src);
                break;
            default:
                ret = -2;
                break;
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::convert() tid=0x%x ret=%d", tid, ret);
    return ret;
}

int xconvert::parse_pcbtemp_data(const std::string& value,std::vector<pcb_temp>& vct_temp)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_pcbtemp_data()");
    int ret = 0;
    if (value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_pcbtemp_data() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        LOG_MSG(MSG_LOG,"xconvert::parse_pcbtemp_data() num=%d", num);

        if ((num * 4) != (length - 4))
        {
            LOG_MSG(ERR_LOG,"xconvert::parse_pcbtemp_data() value is invaild,length=%d num=%d",length,num);
            ret = -1;
        }
        else
        {
            int i = 0;
            char* pay_data = (char*)&value_ptr[4];
            while(i < num)
            {
                float tmp = xbasic::readfloat_bigendian((reinterpret_cast<void*>(pay_data+i*4)), 4);
                pcb_temp temp;
                temp.m_id = ++i;
                temp.m_temp_value = tmp;
                vct_temp.push_back(temp);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_pcbtemp_data() vct_temp size=%d ret ",vct_temp.size(),ret);
    return ret;
}

int xconvert::convert_pcbtemp(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::convert_pcbtemp()");
    int ret = 0;
    std::vector<pcb_temp> vct_temp;
    parse_pcbtemp_data(src->m_value,vct_temp);
    int vct_size = vct_temp.size();

    if(vct_size > 0)
    {
        unsigned short tid = 0;
        switch(src->m_tid)
        {
            case RWA_TEMP:
                tid = BOARD_TMP_VALUE;
                break;
            default:
                break;
        }

        std::vector<std::string> vect_str;
        for(int i = 0; i < vct_size; i++)
        {
            std::string str;
            str = std::to_string(vct_temp[i].m_id) + ":" + std::to_string(vct_temp[i].m_temp_value);
            vect_str.push_back(str);
        }
        std::string value = boost::join(vect_str,",");
        boost::shared_ptr<xtvl> new_data(new xtvl(tid,value.length(),value.data()));
        dst = new_data;
    }
    else
    {
        ret = -1;
        dst = boost::shared_ptr<xtvl>();
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::convert_pcbtemp() ret=%d",ret);
    return ret;
}

int xconvert::parse_pcbvolt_data(const std::string& value,std::vector<pcb_volatage>& vct_vol)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_pcbvolt_data()");
    int ret = 0;
    if (value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_pcbvolt_data() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        LOG_MSG(MSG_LOG,"xconvert::parse_pcbvolt_data() num=%d",num);

        if ((num * 4) != (length - 4))
        {
            LOG_MSG(ERR_LOG,"xconvert::parse_pcbvolt_data() value is invaild,length=%d num=%d",length,num);
            ret = -1;
        }
        else
        {
            int i = 0;
            char* pay_data = (char*)&value_ptr[4];
            while(i < num)
            {
                float vol = xbasic::readfloat_bigendian((reinterpret_cast<void*>(pay_data+i*4)), 4);
                pcb_volatage volt;
                volt.m_id = ++i;
                volt.m_volt_value = vol;
                vct_vol.push_back(volt);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_pcbvolt_data() vct_vol size=%d ret ",vct_vol.size(),ret);
    return ret;
}

int xconvert::convert_pcbvoltage(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::convert_pcbvoltage()");
    int ret = 0;
    std::vector<pcb_volatage> vct_vol;
    parse_pcbvolt_data(src->m_value,vct_vol);
    int vct_size = vct_vol.size();

    if(vct_size > 0)
    {
        unsigned short tid = 0;
        switch(src->m_tid)
        {
            case RWA_VOLTAGE:
                tid = BOARD_VOLTAGE;
                break;
            default:
                break;
        }
        std::vector<std::string> vect_str;
        for(int i = 0; i < vct_size; i++)
        {
            std::string str;
            str = std::to_string(vct_vol[i].m_id) + ":" + std::to_string(vct_vol[i].m_volt_value);
            vect_str.push_back(str);
        }
        std::string value = boost::join(vect_str,",");
        boost::shared_ptr<xtvl> new_data(new xtvl(tid,value.length(),value.data()));
        dst = new_data;
    }
    else
    {
        ret = -1;
        dst = boost::shared_ptr<xtvl>();
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::convert_pcbvoltage() ret=%d",ret);
    return ret;
}

/*
int xconvert::convert_pcbcurrent(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::convert_pcbcurrent()");
    int ret = 0;
    std::string val;
    std::string value = src->get_data();
    int sz = value.length();

    if(sz > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src->m_tid)
        {
        case RWA_ELE_CURRENT:    // 电流
            tid = PLAT_RWA_ELE_CURRENT;
            val = value;
            break;
        default:
            break;
        }

        if(!val.empty())
        {
            boost::shared_ptr<xtvl> new_data(new xtvl(tid,val.length(),val.data(),true));
            dst = new_data;
        }
        else
        {
            ret = -1;
            dst = boost::shared_ptr<xtvl>();
            LOG_MSG(ERR_LOG,"xconvert::convert_pcbcurrent() val is empty tid=%d,sz=%d",tid,sz);
        }
    }
    else
    {
        ret = -1;
        dst = boost::shared_ptr<xtvl>();
        LOG_MSG(ERR_LOG,"xconvert::convert_pcbcurrent() failed sz=%d",sz);
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::convert_pcbcurrent() ret=%d",ret);
    return ret;
}
*/

int xconvert::parse_ad9528_lckdata(const std::string& value,std::vector<ad9528_lock>& vct_lock)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_ad9528_lckdata()");
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_ad9528_lckdata() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        int length          = value.length();
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 4);
        int i = 0;
        char* pay_data = (char*)&value_ptr[4];
        for(int iread = 0; iread < (length - 4);)
        {
            int ch = xbasic::read_bigendian(&pay_data[iread], 4);
            int lockstatus = xbasic::read_bigendian(&pay_data[iread+ 4], 4);
            ad9528_lock ad9528_lck;
            ad9528_lck.m_lockstatus = lockstatus;
            for(int ich=0; ich < ch; ++ich)
            {
                float pwm_val = xbasic::readfloat_bigendian(&pay_data[iread+8+ich*4], 4);
                ad9528_lck.m_chnpwm_vct.push_back(pwm_val);
            }
            iread += (8 + (ch*4));
            vct_lock.push_back(ad9528_lck);
        }
        LOG_MSG(MSG_LOG,"xconvert::parse_ad9528_lckdata() pwm size=%d ch=%d iread=%d locksize=%d",ad9528_lck.m_chnpwm_vct.size(),ch,iread,vct_lock.size());
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_ad9528_lckdata() ret=%d",ret);
    return ret;
}

int xconvert::convert_ad9528_lckstatus(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::convert_ad9528_lckstatus()");
    int ret = 0;
    std::vector<ad9528_lock> vct_ad9528;
    parse_ad9528_lckdata(src->m_value,vct_ad9528);
    int ad9528_size = vct_ad9528.size();

    if(ad9528_size > 0)
    {
        unsigned short tid = 0;
        switch(src->m_tid)
        {
            case RWA_AD9528_LOCKSTATUS:    // AD9528时钟锁定状态
                tid = BOARD_AD9528_LOCKSTATUS;
                break;
            default:
                break;
        }
        std::vector<std::string> vect_chips;
        for(int i = 0; i < ad9528_size; i++)
        {
            std::string str;
            str = std::to_string(i+1) + std::string(":") + std::to_string(vct_ad9528[i].m_lockstatus);
            for(int j = 0; j < vct_ad9528[i].m_chnpwm_vct.size(); j++)
            {
                str += std::string(" ") + std::to_string(j+1) + std::string("=") + std::to_string(vct_ad9528[i].m_chnpwm_vct[j]);
            }
            vect_chips.push_back(str);
        }
        std::string value = boost::join(vect_chips,",");
        boost::shared_ptr<xtvl> new_data(new xtvl(tid,value.length(),value.data()));
        dst = new_data;
    }
    else
    {
        ret = -1;
        dst = boost::shared_ptr<xtvl>();
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::convert_ad9528_lckstatus() ret=%d",ret);
    return ret;
}

int xconvert::parse_ad9545_lckdata(const std::string& value,ad9545_lock& ad9545_lck)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_ad9545_lckdata()");
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_ad9545_lckdata() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        int length          = value.length();
        char* pay_data = (char*)value_ptr;
        int ch = xbasic::read_bigendian(&pay_data[0], 4);
        if ((8 + ch*4) != length)
        {
            //数据长度错误
            LOG_MSG(ERR_LOG,"xconvert::parse_ad9545_lckdata() value is invaild length=%d channel=%d",length,ch);
            ret = -1;
        }
        else
        {
            int lockstatus = xbasic::read_bigendian(&pay_data[4], 4);
            ad9545_lck.m_lockstatus = lockstatus;
            for(int ich=0; ich < ch; ich++)
            {
                float pwm_val = xbasic::readfloat_bigendian(&pay_data[8+ich*4], 4);
                ad9545_lck.m_chnpwm_vct.push_back(pwm_val);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_ad9545_lckdata() lockstatus=%d channel size=%d ret",ad9545_lck.m_lockstatus,ad9545_lck.m_chnpwm_vct.size(),ret);
    return ret;
}

int xconvert::convert_ad9545_lckstatus(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::convert_ad9545_lckstatus()");
    int ret = 0;
    ad9545_lock ad9545_lck;
    parse_ad9545_lckdata(src->m_value,ad9545_lck);
    int vct_size = ad9545_lck.m_chnpwm_vct.size();

    if(vct_size > 0)
    {
        unsigned short tid = 0;
        switch(src->m_tid)
        {
            case RWA_AD9545_LOCKSTATUS:   // AD9545时钟锁定状态
                tid = BOARD_AD9545_LOCKSTATUS;
                break;
            default:
                break;
        }
        std::vector<std::string> vect_chs;
        for(int i = 0; i < vct_size; i++)
        {
            std::string str;
            str = std::to_string(i+1) + "=" + std::to_string(ad9545_lck.m_chnpwm_vct[i]);
            vect_chs.push_back(str);
        }
        std::string value = std::to_string(ad9545_lck.m_lockstatus) + std::string(" ") + boost::join(vect_chs, ",");
        boost::shared_ptr<xtvl> new_data(new xtvl(tid,value.length(),value.data()));
        dst = new_data;
    }
    else
    {
        ret = -1;
        dst = boost::shared_ptr<xtvl>();
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::convert_ad9545_lckstatus() ret=%d",ret);
    return ret;
}

int xconvert::parse_pwm_check(const std::string& value,std::vector<pwm_check>& vct_pwm)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_pwm_check()");
    int ret = 0;
    if (value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_pwm_check() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        int length          = value.length();
        char* pay_data = (char*)value_ptr;
        int ch = xbasic::read_bigendian(&pay_data[0], 4);
        if ((4 + ch*4) != length)
        {
            //数据长度错误
            LOG_MSG(ERR_LOG,"xconvert::parse_pwm_check() value is invaild length=%d channel=%d",length,ch);
            ret = -1;
        }
        else
        {
            for (int ich=0; ich < ch; )
            {
                int pwm_val = xbasic::read_bigendian(&pay_data[4+ich*4], 4);
                pwm_check pwm;
                pwm.m_channel_id = ++ich;
                pwm.m_pwm_value = pwm_val;
                vct_pwm.push_back(pwm);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_pwm_check() vct_pwm size=%d ret",vct_pwm.size(),ret);
    return ret;
}

//id(2个字节) + 温度(4个字节float)
int xconvert::parse_utp40_temp(const std::string& value,std::vector<utp40_temp>& vct_temp)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_utp40_temp()");
    int ret = 0;
    if (value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_utp40_temp() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        if ((length % 6) != 0)
        {
            LOG_MSG(ERR_LOG,"xconvert::parse_utp40_temp() value is invaild,length=%d",length);
            ret = -1;
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                offset = offset + 2;
                float tmp = xbasic::readfloat_bigendian(&pay_data[offset], 4);
                offset = offset + 4;
                utp40_temp temp;
                temp.m_id = id;
                temp.m_temp_value = tmp;
                vct_temp.push_back(temp);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_utp40_temp() vct_temp size=%d ret ",vct_temp.size(),ret);
    return ret;
}

//id(2个字节) + 温度(4个字节float)
int xconvert::parse_asic_temp(const std::string& value,std::vector<asic_temp>& vct_temp)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_asic_temp()");
    int ret = 0;
    if (value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_asic_temp() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        if ((length % 6) != 0)
        {
            LOG_MSG(ERR_LOG,"xconvert::parse_asic_temp() value is invaild,length=%d",length);
            ret = -1;
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                offset = offset + 2;
                float tmp = xbasic::readfloat_bigendian(&pay_data[offset], 4);
                offset = offset + 4;
                asic_temp temp;
                temp.m_id = id;
                temp.m_temp_value = tmp;
                vct_temp.push_back(temp);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_asic_temp() vct_temp size=%d ret ",vct_temp.size(),ret);
    return ret;
}

//id(2个字节) + 温度(4个字节float)
int xconvert::parse_fpga_temp(const std::string& value,std::vector<fpga_temp>& vct_temp)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_fpga_temp()");
    int ret = 0;
    if (value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_fpga_temp() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        if ((length % 6) != 0)
        {
            LOG_MSG(ERR_LOG,"xconvert::parse_fpga_temp() value is invaild,length=%d",length);
            ret = -1;
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                offset = offset + 2;
                float tmp = xbasic::readfloat_bigendian(&pay_data[offset], 4);
                offset = offset + 4;
                fpga_temp temp;
                temp.m_id = id;
                temp.m_temp_value = tmp;
                vct_temp.push_back(temp);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_fpga_temp() vct_temp size=%d ret ",vct_temp.size(),ret);
    return ret;
}

//id(2个字节) + 电压(4个字节float)
int xconvert::parse_dig_voltage(const std::string& value,std::vector<dig_volatage>& vct_vol)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_dig_voltage()");
    int ret = 0;
    if (value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_dig_voltage() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        if ((length % 6) != 0)
        {
            LOG_MSG(ERR_LOG,"xconvert::parse_dig_voltage() value is invaild,length=%d",length);
            ret = -1;
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                offset = offset + 2;
                float volt = xbasic::readfloat_bigendian(&pay_data[offset], 4);
                offset = offset + 4;
                dig_volatage voltage;
                voltage.m_id = id;
                voltage.m_volt_value = volt;
                vct_vol.push_back(voltage);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_dig_voltage() vct_vol size=%d ret ",vct_vol.size(),ret);
    return ret;
}

//id(2个字节) + 温度(4个字节float)
int xconvert::parse_comb_temp(const std::string& value,std::vector<comb_temp>& vct_temp)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_comb_temp()");
    int ret = 0;
    if (value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_comb_temp() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        if ((length % 6) != 0)
        {
            LOG_MSG(ERR_LOG,"xconvert::parse_comb_temp() value is invaild,length=%d",length);
            ret = -1;
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                offset = offset + 2;
                float tmp = xbasic::readfloat_bigendian(&pay_data[offset], 4);
                offset = offset + 4;
                comb_temp temp;
                temp.m_id = id;
                temp.m_temp_value = tmp;
                vct_temp.push_back(temp);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_comb_temp() vct_temp size=%d ret ",vct_temp.size(),ret);
    return ret;
}

/*
1、第个rk3588槽位号结构数据 + ..... + 第N个rk3588槽位号结构数据;
2、每个rk槽位号结构数据为:
2.1 rk3588槽位号(1个字节) + 通道总数(2个字节)+ 第1个通道结构数据 + ..... + 第N个通道结构数据;
2.2 每个通道结构数据为:
通道号(2个字节) + 通道内温度总数(2个字节) + 第1个温度数据[编号(2个字节)+温度值1(4个字节 float型)] + .... + 第N个温度数据[编号(2个字节)+温度值1(4个字节float)];
*/
int xconvert::parse_rcafpga_temp(const std::string& value, rk3588_fpgatemp& rk3588_temp)
{
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_rcafpga_temp() value is empty");
    }
    else
    {
        int i = 0;
        int len = value.length();
        const char* value_ptr = value.data();
        char* pay_data = (char*)value_ptr;
        while(i < len)
        {
            //获取rk3588槽位号
            int rk3588_slotid = pay_data[i];
            i = i + 1;
            //获取通道总数
            int chann_cnt = xbasic::read_bigendian(&pay_data[i],2);
            i = i + 2;
            //遍历每个通道数据
            std::vector<channel_temp> vect_channel;
            for(int j = 0; j < chann_cnt && i < len; j++)
            {
                //获取通道编号
                int chann_id = xbasic::read_bigendian(&pay_data[i],2);
                i = i + 2;
                //获取每个通道的温度个数
                int temp_cnt = xbasic::read_bigendian(&pay_data[i],2);
                i = i + 2;

                //遍历每个温度数据
                std::vector<rcafpga_temp> vect_temp;
                for(int k = 0 ; k < temp_cnt && i+6 < len; k++)
                {
                    int temp_id = xbasic::read_bigendian(&pay_data[i],2);
                    i = i + 2;
                    float temp_val = xbasic::readfloat_bigendian(&pay_data[i],4);
                    i = i + 4;

                    rcafpga_temp rca_temp;
                    rca_temp.m_id = temp_id;
                    rca_temp.m_temp_value = temp_val;
                    vect_temp.push_back(rca_temp);
                }
                int vect_temp_sz = vect_temp.size();
                if(vect_temp_sz > 0)
                {
                    channel_temp map_channel;
                    map_channel.insert(std::make_pair(chann_id,vect_temp));
                    vect_channel.push_back(map_channel);
                }
            }
            int vect_channel_sz = vect_channel.size();
            if(vect_channel_sz > 0)
            {
                rk3588_temp.insert(std::make_pair(rk588_slotid,vect_channel));
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_rcafpga_temp() rk3588_temp size=%d ret ",rk3588_temp.size(),ret);
    return ret;
}

int xconvert::parse_serdes_state(const std::string& value, std::vector<serdes_state>& vct_serdes)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_serdes_state()");
    int ret = 0;
    if (value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_serdes_state() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        if ((length % 3) != 0)
        {
            LOG_MSG(ERR_LOG,"xconvert::parse_serdes_state() value is invaild,length=%d",length);
            ret = -1;
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                offset = offset + 2;
                char state = xbasic::read_bigendian(&pay_data[offset], 1);
                offset = offset + 1;

                serdes_state serdes_tmp;
                serdes_tmp.m_id = id;
                serdes_tmp.m_serdes_value = state;
                vct_serdes.push_back(serdes_tmp);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_serdes_state() vct_serdes size=%d ret ",vct_serdes.size(),ret);
    return ret;
}

int xconvert::parse_rk3588_ip(const std::string& value, std::vector<rk3588_ip>& vct_rk3588_ip)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_rk3588_ip()");
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_rk3588_ip() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        int length          = value.length();
        // num 代表组数
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 2);
        if(num*8 != length-2)
        {
            // 数据长度对不上
            ret = -1;
            LOG_MSG(WRN_LOG, "xconvert::parse_rk3588_ip() value illegal rk3588 num:%d value_length:%d", num, length);
        }
        else
        {
            // 取出负载数据
            char* pay_data = (char*)&value_ptr[2];
            for(int iread = 0; iread < (length - 2);)
            {
                unsigned char pem_slot = xbasic::read_bigendian(&pay_data[iread+1], 1);
                unsigned char rca_slot = xbasic::read_bigendian(&pay_data[iread+2], 1);
                unsigned char rk3588_slot = xbasic::read_bigendian(&pay_data[iread+3], 1);
                LOG_MSG(MSG_LOG, "xconvert::parse_rk3588_ip() pem_slot:0x%x rca_slot:0x%x rk3588_slot:0x%x", pem_slot, rca_slot, rk3588_slot);
                unsigned int ip = xbasic::read_bigendian(&pay_data[iread+4], 4);
                // 将 IP 地址转化成本地字节序
                // uint32_t host_value = ntohl(ip);
                // 上面的 ip 读出来已经是按照本地字节序进行存储了，无需再转换一次
                std::string ip_str = xbasic::ip_from_int(ip);
                // LOG_MSG(MSG_LOG, "xconvert::parse_rk3588_ip() ip:0x%x host_ip:0x%x ip_str:%s", ip, host_value, ip_str.c_str());
                LOG_MSG(MSG_LOG, "xconvert::parse_rk3588_ip() ip:0x%x ip_str:%s", ip, ip_str.c_str());

                rk3588_ip r_ip;
                r_ip.m_pem_id = pem_slot;
                r_ip.m_rca_id = rca_slot;
                r_ip.m_rk3588_id = rk3588_slot;
                r_ip.m_ip = ip_str;
                vct_rk3588_ip.push_back(r_ip);

                iread += 8;
                LOG_MSG(MSG_LOG,"xconvert::parse_rk3588_ip() iread=%d",iread);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_rk3588_ip()");
    return ret;
}

int xconvert::parse_mcu_fpgaver(const std::string& value, mcu_fpgavermap& map_fpga_ver)
{
    LOG_MSG(MSG_LOG, "Enter into xconvert::parse_mcu_fpgaver()");
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_mcu_fpgaver() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        int length          = value.length();
        // num 代表组数
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 2);
        if((num*6 != length-2) && (num*10 != length-2))
        {
            // 数据长度对不上
            ret = -1;
            LOG_MSG(WRN_LOG, "xconvert::parse_mcu_fpgaver() value mcu check fpga version num=%d value_length=%d not match", num, length);
        }
        else
        {
            // 取出负载数据
            char* pay_data = (char*)&value_ptr[2];
            for(int iread = 0; iread < (length - 2);)
            {
                int ver_offset = 0;
                long long ver = 0;
                unsigned short ver_type = xbasic::read_bigendian(&pay_data[iread], 2);
                ver_offset += 2;
                if(ver_type & 0x80 == 0x80)
                {
                    ver_type = (ver_type & 0x00FF);
                    ver = xbasic::readlong_bigendian(&pay_data[iread+2], 8);
                    ver_offset += 8;
                }
                else
                {
                    ver = xbasic::read_bigendian(&pay_data[iread+2], 4);
                    ver_offset += 4;
                }

                LOG_MSG(MSG_LOG, "xconvert::parse_mcu_fpgaver() ver_type=%d ver=0x%llx", ver_type, ver);
                mcu_fpgavermap::iterator it = map_fpga_ver.find(ver_type);
                if(it == map_fpga_ver.end())
                {
                    mcu_fpgaversion fpga_version;
                    fpga_version.m_vertype = ver_type;
                    fpga_version.m_ver = ver;
                    std::vector<mcu_fpgaversion> vct_fpga_ver;
                    vct_fpga_ver.push_back(fpga_version);
                    map_fpga_ver.insert(std::make_pair(ver_type,vct_fpga_ver));
                }
                else
                {
                    mcu_fpgaversion fpga_version;
                    fpga_version.m_vertype = ver_type;
                    fpga_version.m_ver = ver;
                    it->second.push_back(fpga_version);
                }

                iread += ver_offset;
                LOG_MSG(MSG_LOG,"xconvert::parse_mcu_fpgaver() iread=%d",iread);
            }
            int sz = map_fpga_ver.size();
            LOG_MSG(MSG_LOG,"Exited xconvert::parse_mcu_fpgaver() sz=%d",sz);
        }
    }
    return ret;
}

int xconvert::parse_mcu_fpgaserdes(const std::string& value, mcu_fpgaserdemap& map_fpga_serdes)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_mcu_fpgaserdes()");
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_mcu_fpgaserdes() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        int length          = value.length();
        // num 代表组数
        int num = xbasic::read_bigendian(const_cast<void*>(reinterpret_cast<const void*>(value_ptr)), 2);
        if(num*6 != length-2)
        {
            // 数据长度对不上
            ret = -1;
            LOG_MSG(WRN_LOG, "xconvert::parse_mcu_fpgaserdes() value mcu check fpga version num=%d value_length=%d not match", num, length);
        }
        else
        {
            // 取出负载数据
            char* pay_data = (char*)&value_ptr[2];
            for(int iread = 0; iread < (length - 2);)
            {
                unsigned short serdes_type = xbasic::read_bigendian(&pay_data[iread], 2);
                unsigned int serdes_st = xbasic::read_bigendian(&pay_data[iread+2], 4);
                LOG_MSG(MSG_LOG, "xconvert::parse_mcu_fpgaserdes() serdes_type=%d serdes_st=0x%x", serdes_type, serdes_st);

                mcu_fpgaserdemap::iterator it = map_fpga_serdes.find(serdes_type);
                if(it == map_fpga_serdes.end())
                {
                    mcu_fpgaserdes fpga_serdes;
                    fpga_serdes.m_serdestype = serdes_type;
                    fpga_serdes.m_serdes = serdes_st;
                    std::vector<mcu_fpgaserdes> vct_fpga_serdes;
                    vct_fpga_serdes.push_back(fpga_serdes);
                    map_fpga_serdes.insert(std::make_pair(serdes_type,vct_fpga_serdes));
                }
                else
                {
                    mcu_fpgaserdes fpga_serdes;
                    fpga_serdes.m_serdestype = serdes_type;
                    fpga_serdes.m_serdes = serdes_st;
                    it->second.push_back(fpga_serdes);
                }

                iread += 6;
                LOG_MSG(MSG_LOG,"xconvert::parse_mcu_fpgaserdes() iread=%d",iread);
            }
            int sz = map_fpga_serdes.size();
            LOG_MSG(MSG_LOG,"Exited xconvert::parse_mcu_fpgaserdes() sz=%d",sz);
        }
    }
    return ret;
}

int xconvert::parse_utp40_register(const std::string& value, std::vector<utp40_register>& vct_register)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_utp40_register()");
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_utp40_register() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        // 其中 2 是 chip id 的两字节，6 是一个寄存器信息的六个字节
        if(length % (2+6*UTP_REGISTER_NUM) != 0)
        {
            ret = -1;
            LOG_MSG(ERR_LOG,"xconvert::parse_utp40_register() value is invaild,length=%d",length);
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                utp40_register u_register;
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                u_register.m_utp40_chip_id = id;
                offset = offset + 2;
                for(int i = 0; i < UTP_REGISTER_NUM; i++)
                {
                    // 读该芯片通道上的9个寄存器的信息
                    utp_register_info info;
                    int valid = xbasic::read_bigendian(&pay_data[offset], 1);
                    offset = offset + 1;
                    int reg_ch = xbasic::read_bigendian(&pay_data[offset], 1);
                    offset = offset + 1;
                    int reg_addr = xbasic::read_bigendian(&pay_data[offset], 2);
                    offset = offset + 2;
                    int reg_data = xbasic::read_bigendian(&pay_data[offset], 2);
                    offset = offset + 2;
                    // 填充信息
                    info.m_valid = valid;
                    info.m_reg_ch = reg_ch;
                    info.m_reg_addr = reg_addr;
                    info.m_reg_data = reg_data;
                    u_register.m_register.push_back(info);
                }
                vct_register.push_back(u_register);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_utp40_register()");
    return ret;
}

int xconvert::find_one_utp_register(const std::string& value, int chip_id, std::string& r_value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::find_one_utp_register() chip_id=%d", chip_id);
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::find_one_utp_register() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        // 其中 2 是 chip id 的两字节，6 是一个寄存器信息的六个字节
        if(length % (2+6*UTP_REGISTER_NUM) != 0)
        {
            ret = -1;
            LOG_MSG(ERR_LOG,"xconvert::find_one_utp_register() value is invaild,length=%d",length);
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                if(id == chip_id)
                {
                    // 找到了
                    r_value.assign(&pay_data[offset], 2 + (6*UTP_REGISTER_NUM));
                    LOG_MSG(MSG_LOG, "xconvert::find_one_utp_register() find chip_id:%d", id);
                    break;
                }
                else
                {
                    // 没找到
                    offset = offset + 2 + (6*UTP_REGISTER_NUM);
                }
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::find_one_utp_register() ret=%d",ret);
    return 0;
}

int xconvert::parse_utp102_register(const std::string& value, std::vector<utp40_register>& vct_register)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::parse_utp102_register()");
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::parse_utp102_register() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        // 其中 2 是 chip id 的两字节，6 是一个寄存器信息的六个字节
        if(length % (2+6*UTP102_REGISTER_NUM) != 0)
        {
            ret = -1;
            LOG_MSG(ERR_LOG,"xconvert::parse_utp102_register() value is invaild,length=%d",length);
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                utp40_register u_register;
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                u_register.m_utp40_chip_id = id;
                offset = offset + 2;
                for(int i = 0; i < UTP102_REGISTER_NUM; i++)
                {
                    // 读该芯片通道上的9个寄存器的信息
                    utp_register_info info;
                    int valid = xbasic::read_bigendian(&pay_data[offset], 1);
                    offset = offset + 1;
                    int reg_ch = xbasic::read_bigendian(&pay_data[offset], 1);
                    offset = offset + 1;
                    int reg_addr = xbasic::read_bigendian(&pay_data[offset], 2);
                    offset = offset + 2;
                    int reg_data = xbasic::read_bigendian(&pay_data[offset], 2);
                    offset = offset + 2;
                    // 填充信息
                    info.m_valid = valid;
                    info.m_reg_ch = reg_ch;
                    info.m_reg_addr = reg_addr;
                    info.m_reg_data = reg_data;
                    u_register.m_register.push_back(info);
                }
                vct_register.push_back(u_register);
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::parse_utp102_register()");
    return ret;
}

int xconvert::find_one_utp102_register(const std::string& value, int chip_id, std::string& r_value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::find_one_utp102_register() chip_id=%d", chip_id);
    int ret = 0;
    if(value.empty())
    {
        ret = -1;
        LOG_MSG(ERR_LOG,"xconvert::find_one_utp102_register() value is empty");
    }
    else
    {
        const char* value_ptr  = value.data();
        size_t length          = value.length();
        // 其中 2 是 chip id 的两字节，6 是一个寄存器信息的六个字节
        if(length % (2+6*UTP102_REGISTER_NUM) != 0)
        {
            ret = -1;
            LOG_MSG(ERR_LOG,"xconvert::find_one_utp102_register() value is invaild,length=%d",length);
        }
        else
        {
            int offset = 0;
            char* pay_data = (char*)&value_ptr[0];
            while(offset < length)
            {
                unsigned short id = xbasic::read_bigendian(&pay_data[offset], 2);
                if(id == chip_id)
                {
                    // 找到了
                    r_value.assign(&pay_data[offset], 2 + (6*UTP102_REGISTER_NUM));
                    LOG_MSG(MSG_LOG, "xconvert::find_one_utp102_register() find chip_id:%d", id);
                    break;
                }
                else
                {
                    // 没找到
                    offset = offset + 2 + (6*UTP102_REGISTER_NUM);
                }
            }
        }
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::find_one_utp102_register() ret=%d",ret);
    return 0;
}

int xconvert::serial_utp40_temp(const std::vector<utp40_temp>& vct_temp,std::string& value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::serial_utp40_temp()");
    int ret = 0;
    int sz = vct_temp.size();
    if(sz > 0)
    {
        for(int i = 0; i < sz; i++)
        {
            char buff[6] = {0};
            xbasic::write_bigendian(&buff[0], vct_temp[i].m_id, 2);
            xbasic::writefloat_bigendian(&buff[2], vct_temp[i].m_temp_value, 4);
            value.append(std::string((char*)&buff),sizeof(buff));
        }
    }
    else
    {
        ret = -1;
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::serial_utp40_temp() vct_temp size=%d ret=%d ",vct_temp.size(),ret);
    return ret;
}

int xconvert::serial_asic_temp(const std::vector<asic_temp>& vct_temp,std::string& value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::serial_asic_temp()");
    int ret = 0;
    int sz = vct_temp.size();
    if(sz > 0)
    {
        for(int i = 0; i < sz; i++)
        {
            char buff[6] = {0};
            xbasic::write_bigendian(&buff[0], vct_temp[i].m_id, 2);
            xbasic::writefloat_bigendian(&buff[2], vct_temp[i].m_temp_value, 4);
            value.append(std::string((char*)&buff),sizeof(buff));
        }
    }
    else
    {
        ret = -1;
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::serial_asic_temp() vct_temp size=%d ret=%d ",vct_temp.size(),ret);
    return ret;
}

int xconvert::serial_fpga_temp(const std::vector<fpga_temp>& vct_temp,std::string& value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::serial_fpga_temp()");
    int ret = 0;
    int sz = vct_temp.size();
    if(sz > 0)
    {
        for(int i = 0; i < sz; i++)
        {
            char buff[6] = {0};
            xbasic::write_bigendian(&buff[0], vct_temp[i].m_id, 2);
            xbasic::writefloat_bigendian(&buff[2], vct_temp[i].m_temp_value, 4);
            value.append(std::string((char*)&buff),sizeof(buff));
        }
    }
    else
    {
        ret = -1;
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::serial_fpga_temp() vct_temp size=%d ret ",vct_temp.size(),ret);
    return ret;
}

int xconvert::serial_dig_voltage(const std::vector<dig_volatage>& vct_vol,std::string& value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::serial_dig_voltage()");
    int ret = 0;
    int sz = vct_vol.size();
    if(sz > 0)
    {
        for(int i = 0; i < sz; i++)
        {
            char buff[6] = {0};
            xbasic::write_bigendian(&buff[0], vct_vol[i].m_id, 2);
            xbasic::writefloat_bigendian(&buff[2], vct_vol[i].m_volt_value, 4);
            value.append(std::string((char*)&buff),sizeof(buff));
        }
    }
    else
    {
        ret = -1;
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::serial_dig_voltage() vct_vol size=%d ret ",vct_vol.size(),ret);
    return ret;
}

int xconvert::serial_comb_temp(const std::vector<comb_temp>& vct_temp,std::string& value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::serial_comb_temp()");
    int ret = 0;
    int sz = vct_temp.size();
    if(sz > 0)
    {
        for(int i = 0; i < sz; i++)
        {
            char buff[6] = {0};
            xbasic::write_bigendian(&buff[0], vct_temp[i].m_id, 2);
            xbasic::writefloat_bigendian(&buff[2], vct_temp[i].m_temp_value, 4);
            value.append(std::string((char*)&buff),sizeof(buff));
        }
    }
    else
    {
        ret = -1;
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::serial_comb_temp() vct_temp size=%d ret ",vct_temp.size(),ret);
    return ret;
}

/*
1、第个rk3588槽位号结构数据 + ..... + 第N个rk3588槽位号结构数据;
2、每个rk槽位号结构数据为:
2.1 rk3588槽位号(1个字节) + 通道总数(2个字节)+ 第1个通道结构数据 + ..... + 第N个通道结构数据;
2.2 每个通道结构数据为:
通道号(2个字节) + 通道内温度总数(2个字节) + 第1个温度数据[编号(2个字节)+温度值1(4个字节 float型)] + .... + 第N个温度数据[编号(2个字节)+温度值1(4个字节float)];
*/
int xconvert::serial_rcafpga_temp(const rk3588_fpgatemp& rk3588_temp,std::string& value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::serial_rcafpga_temp()");
    int ret = 0;
    int sz = rk3588_temp.size();
    if(sz > 0)
    {
        //遍历所有RK3588 map元素
        for(auto it1 : rk3588_temp)
        {
            //RK3588槽位号
            char rk3588_slotid = it1.first;
            value.append(1,rk3588_slotid);
            auto channel_vect = it1.second;
            //遍历某个rk3588通道map元素
            std::string channel_vect_str;
            int channel_vect_sz = channel_vect.size();
            if(channel_vect_sz > 0)
            {
                //通道总数
                char channel_cnt[2] = {0};
                xbasic::write_bigendian(&channel_cnt[0],channel_vect_sz,2);
                channel_vect_str.append(&channel_cnt[0],2);

                for(auto channel : channel_vect)
                {
                    //遍历某个rk3588通道的温度元素
                    std::string rca_temp;
                    auto temp_vect = it2.second;
                    int temp_vect_sz = temp_vect.size();
                    if(temp_vect_sz > 0)
                    {
                        //通道内温度总数
                        char temp_cnt[2] = {0};
                        xbasic::write_bigendian(&temp_cnt[0],temp_vect_sz,2);
                        rca_temp.append(&temp_cnt[0],2);

                        //通道内温度结构数据
                        for(auto temp : temp_vect)
                        {
                            char buff[6] = {0};
                            //温度编号
                            xbasic::write_bigendian(&buff[0],temp.m_id,2);
                            //温度数值
                            xbasic::writefloat_bigendian(&buff[2], temp.m_temp_value,4);
                            rca_temp.append(&buff[0],6);
                        }
                    }
                    channel_vect_str.append(rca_temp);
                }
            }
            value.append(channel_vect_str);
        }
    }
    else
    {
        ret = -1;
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::serial_rcafpga_temp() rk3588_temp size=%d ret",sz,ret);
    return ret;
}

int xconvert::serial_serdes_state(const std::vector<serdes_state>& vct_serdes, std::string& value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::serial_serdes_state()");
    int ret = 0;
    int sz = vct_serdes.size();
    if(sz > 0)
    {
        for(int i = 0; i < sz; i++)
        {
            char buff[3] = {0};
            xbasic::write_bigendian(&buff[0], vct_serdes[i].m_id, 2);
            xbasic::write_bigendian(&buff[2], vct_serdes[i].m_serdes_value, 1);
            value.append(std::string((char*)&buff),sizeof(buff));
        }
    }
    else
    {
        ret = -1;
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::serial_serdes_state() vct_serdes size=%d ret ",vct_serdes.size(),ret);
    return ret;
}

int xconvert::serial_utp40_register(const std::vector<utp40_register>& vct_register,std::string& value)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::serial_utp40_register()");
    int ret = 0;
    int sz = vct_register.size();
    if(sz > 0)
    {
        for(const auto& utp40 : vct_register)
        {
            // 写入 ID
            char id_buff[2] = {0};
            xbasic::write_bigendian(&id_buff[0], utp40.m_utp40_chip_id, 2);
            value.append(std::string((char*)&id_buff[0],sizeof(id_buff)));
            for(const auto& register_info : utp40.m_register)
            {
                // 写入寄存器信息
                char buff[6] = {0};
                xbasic::write_bigendian(&buff[0], register_info.m_valid, 1);
                xbasic::write_bigendian(&buff[1], register_info.m_reg_ch, 1);
                xbasic::write_bigendian(&buff[2], register_info.m_reg_addr, 2);
                xbasic::write_bigendian(&buff[4], register_info.m_reg_data, 2);
                value.append(std::string((char*)&buff[0],sizeof(buff)));
            }
        }
    }
    else
    {
        ret = -1;
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::serial_utp40_register() vct_register size=%d ret",vct_register.size(),ret);
    return ret;
}

//ch1=pwm1 ch2=pwm2 chn=pwmwn
int xconvert::convert_pwm_check(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::convert_pwm_check()");
    int ret = 0;
    std::vector<pwm_check> vct_pwm;
    parse_pwm_check(src->m_value,vct_pwm);
    int vct_size = vct_pwm.size();

    if(vct_size > 0)
    {
        unsigned short tid = 0;
        switch(src->m_tid)
        {
            case RWA_PWM_CHECK:    // PWM check
                tid = BOARD_PWM_CHECK;
                break;
            default:
                break;
        }
        std::vector<std::string> vect_str;
        for(int i = 0; i < vct_size; i++)
        {
            std::string str;
            str = std::to_string(vct_pwm[i].m_channel_id) + "=" + std::to_string(vct_pwm[i].m_pwm_value);
            vect_str.push_back(str);
        }
        std::string value = boost::join(vect_str, " ");
        boost::shared_ptr<xtvl> new_data(new xtvl(tid,value.length(),value.data()));
        dst = new_data;
    }
    else
    {
        ret = -1;
        dst = boost::shared_ptr<xtvl>();
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::convert_pwm_check() ret=%d",ret);
    return ret;
}

int xconvert::convert_basic_info(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::convert_basic_info()");
    int ret = 0;
    std::string val;
    std::string value = src->get_data();
    int sz = value.length();
    int power_st = 0;
    int board_type = 0;
    int slot = 0;

    if(sz > 0 )
    {
        //上位机tid
        unsigned short tid;
        switch(src->m_tid)
        {
        case RWA_POWER_STATUS:        // 电源状态
            tid = POWER_STATUS;
            val = val = std::to_string((int)value[0]);
            break;
        case RWA_POS_STATUS:          // 板在位信息
            tid = BOARD_POS_STATUS;
            val = std::to_string((int)value[0]);
            break;
        //case RWA_TEMP_SET:            // 温度告警
        //    tid = PLAT_RWA_TEMP_ALARM;
        //    val = value;
        //    break;
        case RWA_BOARD_TYPE:          // 单板类型
            tid = BOARD_TYPE;
            val = val = std::to_string((int)value[0]);
            break;
        case RWA_BOARD_SN:            // 单板SN
            tid = BOARD_SN;
            val.assign(value.c_str(),strlen(value.c_str()));
            break;
        case RWA_MANU_INFO:           // 单板制造信息
            tid = BOARD_MANU_INFO;
            val.assign(value.c_str(),strlen(value.c_str()));
            break;
        case RWA_HW_VER:              // 单板硬件版本
            tid = BOARD_HW_VER;
            val.assign(value.c_str(),strlen(value.c_str()));
            break;
        case RWA_SW_VER:              // 单板软件版本
            tid = BOARD_SOFT_VER;
            val.assign(value.c_str(),strlen(value.c_str()));
            break;
        case RWA_SLOT:                // slot号
            tid = BOARD_SLOT;
            val = std::to_string((int)value[0]);
            break;
        case RWA_DIG_ID:              // DIG板的ID号
            tid = BOARD_DIG_ID;
            val = std::to_string(xbasic::read_bigendian(&value[0],4));
            break;
        case RWA_DIG_EXIST:           // DIG板在位状态
            tid = BOARD_DIG_EXIST;
            val = std::to_string((int)value[0]);
            break;
        case RWA_UTP40_TEMP_ALARM:    // UTP40 tmp alarm
            tid = PLAT_RWA_UTP40_TEMP_ALARM;
            val = value;
            break;
        default:
            break;
        }

        if(!val.empty())
        {
            boost::shared_ptr<xtvl> new_data(new xtvl(tid,val.length(),val.data(),true));
            dst = new_data;
        }
        else
        {
            ret = -1;
            dst = boost::shared_ptr<xtvl>();
            LOG_MSG(ERR_LOG,"xconvert::convert_basic_info() val is empty tid=%d,sz=%d",tid,sz);
        }
    }
    else
    {
        ret = -1;
        dst = boost::shared_ptr<xtvl>();
        LOG_MSG(ERR_LOG,"xconvert::convert_basic_info() failed sz=%d",sz);
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::convert_basic_info() ret=%d",ret);
    return ret;
}

int xconvert::convert_utp40_temp(boost::shared_ptr<xtvl>& dst, boost::shared_ptr<xusb_tvl>& src)
{
    LOG_MSG(MSG_LOG,"Enter into xconvert::convert_utp40_temp()");
    int ret = 0;
    std::vector<utp40_temp> vct_temp;
    parse_utp40_temp(src->m_value,vct_temp);
    int vct_size = vct_temp.size();

    if(vct_size > 0)
    {
        unsigned short tid = 0;
        switch(src->m_tid)
        {
            case RWA_UTP40_TEMP:          // UTP40 tmp
                tid = PLAT_RWA_UTP40_TEMP;
                break;
            default:
                break;
        }
        std::vector<std::string> vect_str;
        for(int i = 0; i < vct_size; i++)
        {
            std::string str;
            str = std::to_string(vct_temp[i].m_id) + ":" + std::to_string(vct_temp[i].m_temp_value);
            vect_str.push_back(str);
        }
        std::string value = boost::join(vect_str,",");
        boost::shared_ptr<xtvl> new_data(new xtvl(tid,value.length(),value.data()));
        dst = new_data;
    }
    else
    {
        ret = -1;
        dst = boost::shared_ptr<xtvl>();
    }
    LOG_MSG(MSG_LOG,"Exited xconvert::convert_utp40_temp() ret=%d",ret);
    return ret;
}
