#include "mgr_upgrade.h"
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include "xcrypto.hpp"
#include "xpackage.hpp"
#include "xconfig.h"
#include "xota_session.h"
#include "mgr_log.h"

xotafile::xotafile(std::string type,std::string version,std::string path,int fsize,std::string md5) : m_type(type),m_version(version),m_path(path),m_size(fsize),m_md5(md5)
{
    if(version.find(".") != std::string::npos)
    {
        m_ver_int = xbasic::version_to_int(m_version);
    }
    else
    {
        m_ver_int = std::stoll(m_version);
    }

}

xotafile::~xotafile()
{

}

//设置OTA文件相关属性
void xotafile::set(std::string type,std::string version,std::string path,int fsize,std::string md5)
{
    m_type      = type;
    m_version   = version;
    m_path      = path;
    m_size      = fsize;
    m_md5       = md5;
    if(version.find(".") != std::string::npos)
    {
        m_ver_int = xbasic::version_to_int(m_version);
    }
    else
    {
        m_ver_int = std::stoll(m_version);
    }
}

xcalfile::xcalfile(const std::string& filename,const std::string& version,const std::string& path,int fsize,const std::string& md5)
    : m_filename(filename),m_version(version),m_path(path),m_size(fsize),m_md5(md5)
{
    m_ver_int = xbasic::version_to_int(m_version);
}

xcalfile::~xcalfile()
{

}

//设置CAL文件的文件属性
void xcalfile::set_fileinfo(const std::string& filename,const std::string& version,const std::string& path,int fsize,const std::string& md5)
{
    //校准文件名
    m_filename = filename;
    //CAL文件版本
    m_version = version;
    //CAL版本整型值
    m_ver_int = xbasic::version_to_int(m_version);
    //固件文件路径
    m_path = path;
    //固件文件长度
    m_size = fsize;
    //固件MD5值
    m_md5 = md5;

    return ;
}

//设置校准CAL文件的校准属性
void xcalfile::set_calinfo(const std::string& calprefix,const std::string& boardtype,const std::string& boardsn,const std::string& caltype)
{
    //校准文件前缀
    m_calprefix = calprefix;
    //CAL校准文件对应板类型
    m_boardtype = boardtype;
    //CAL校准文件对应SN号
    m_boardsn = boardsn;
    //CAL校准类别
    m_caltype = caltype;

    return ;
}

mgr_upgrade::mgr_upgrade()
{

}

mgr_upgrade::~mgr_upgrade()
{

}

//初始化
void mgr_upgrade::init()
{

}

//工作函数
void mgr_upgrade::work(unsigned long ticket)
{

}

//设置OTA目录路径
void mgr_upgrade::set_ota_path(std::string otafile_path)
{
    m_otafile_path = otafile_path;
    return ;
}

//获取OTA目录路径
std::string mgr_upgrade::get_ota_path()
{
    return m_otafile_path;
}

//加载最新OTA固件
void mgr_upgrade::load_otafiles()
{
    if(m_otafile_path.length() <= 0)
    {
        m_otafile_path = std::string(xbasic::get_module_path())+"otafiles";
    }

    std::vector<std::string> vct_files;
    xbasic::get_files_in_dir(m_otafile_path,&vct_files);

    for(unsigned int i = 0; i < vct_files.size(); i++)
    {
        //文件完整路径
        std::string file_path = vct_files[i];
        LOG_MSG(MSG_LOG,"mgr_upgrade::load_otafiles() file path=%s", file_path.c_str());
        const char *split = strrchr(file_path.c_str(),'/');
        if(!split)
        {
            split = strrchr(file_path.c_str(),'\\');
        }

        //文件名
        std::string file_name = std::string(split?(split+1):file_path.c_str());
        const char *exten = strrchr(file_path.c_str(),'.');
        //文件扩展名
        std::string file_extn = (exten?exten:"");
        if((file_extn != ".bin") && (file_extn != ".bit") && (file_extn != ".mcs") && (file_extn != ".rpd"))
        {
            //不是OTA文件
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafiles() file_path=%s file suffix is unsupported",file_path.c_str());
            continue;
        }

        //得到去掉扩展名的文件名
        file_name = file_name.substr(0,file_name.length() - 4);
        std::vector< std::string > vct_name;
        //将文件名分成文件类型、文件版本两个字段
        int count = xbasic::split_string(file_name,"-",&vct_name);
        if(count != 2 || vct_name[0].length() <= 0)
        {
            //文件命名不正确
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafiles() file_path=%s file name format is unsupported",file_path.c_str());
            continue;
        }

        int fw_size = 0;
        boost::shared_ptr<xotafile> otafile_find = find_otafile(vct_name[0]);
        if(otafile_find != NULL)
        {
            //已经存在该类型的固件
            bool sm_flag = false;
            if(vct_name[1].find(".") != std::string::npos)
            {
                if(xbasic::version_to_int(vct_name[1]) <= xbasic::version_to_int(otafile_find->m_version))
                {
                    sm_flag = true;
                }
            }
            else
            {
                if(strcmp(vct_name[1].c_str(),otafile_find->m_version.c_str()) < 0)
                {
                    sm_flag = true;
                }
            }

            if(sm_flag)
            {
                //判断之前版本文件是否存在,如果存在忽略,否则将当前文件更新到map内存为当前最新版本
                bool old_existed = false;
                for(auto it : vct_files)
                {
                    if(it == otafile_find->m_path)
                    {
                        old_existed = true;
                        break;
                    }
                }
                if(old_existed == true)
                {
                    //该当前固件的版本比之前版本的低,忽略
                    LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafiles() current file_path=%s version(%s) lower latest file_path=%s version(%s)",file_path.c_str(),vct_name[1].c_str(),otafile_find->m_path.c_str(),otafile_find->m_version.c_str());
                    continue;
                }
                else
                {
                    LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafiles() latest file_path=%s version(%s) is not existed,current file_path=%s version(%s) will be updated",otafile_find->m_path.c_str(),otafile_find->m_version.c_str(),file_path.c_str(),vct_name[1].c_str());
                }
            }
        }

        //计算文件的MD5同时获得文件长度
        std::string fw_md5 = xcrypto::iget_file_md5(file_path,&fw_size);
        if(fw_size <= 0 || fw_md5.length() != 32)
        {
            //MD5计算错误
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafiles() update ota map file_path=%s md5 calculate error",file_path.c_str());
            continue;
        }

        //更新最新固件文件属性
        otafile_find->set(vct_name[0],vct_name[1],file_path,fw_size,fw_md5);
    }
    else
    {
        //计算文件的MD5同时获得文件长度
        std::string fw_md5 = xcrypto::iget_file_md5(file_path,&fw_size);

        if(fw_size <= 0 || fw_md5.length() != 32)
        {
            //MD5计算错误
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafiles() insert ota map file_path=%s md5 calculate error",file_path.c_str());
            continue;
        }
        boost::shared_ptr<xotafile> otafile(new xotafile(vct_name[0],vct_name[1],file_path,fw_size,fw_md5));
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_otafile);
        //加入到集合中
        m_map_otafile.insert(std::make_pair(vct_name[0],otafile));
        lock.unlock();
    }

    if(xconfig::debug() < 1)
    {
        //不打印
        return;
    }

    LOG_MSG(MSG_LOG,"mgr_upgrade::load_otafiles() < ota > otafiles list:");
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_otafile);
    for(boost::unordered_map<std::string,boost::shared_ptr<xotafile> >::iterator iter = m_map_otafile.begin(); iter != m_map_otafile.end(); iter++)
    {
        boost::shared_ptr<xotafile> otafile_find = iter->second;
        LOG_MSG(MSG_LOG,"mgr_upgrade::load_otafiles() < ota > type:%s \tversion:%s \tsize:%d",otafile_find->m_type.c_str(),otafile_find->m_version.c_str(),otafile_find->m_size);
    }
    LOG_MSG(MSG_LOG,"\n");
}

bool mgr_upgrade::load_otafile(const std::string &file_path)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_upgrade::load_otafile() file_path=%s", file_path.c_str());
    bool ret = true;
    if(!check_file_path(file_path))
    {
        // 文件路径不合法
        ret = false;
        LOG_MSG(WRN_LOG, "Exited mgr_upgrade::load_otafile() file_path error ret=%d", ret);
        return ret;
    }

    // 找到字符串中的最后一个 /
    const char *split = strrchr(file_path.c_str(),'/');
    if(!split)
    {
        split = strrchr(file_path.c_str(),'\\');
    }

    //文件名---CPPEN_MCU_V0.2.1.9.bin
    std::string file_name = std::string(split?(split+1):file_path.c_str());
    const char *exten = strrchr(file_path.c_str(),'.');
    //文件扩展名---.bin
    std::string file_extn = (exten?exten:"");
    if((file_extn != ".bin") && (file_extn != ".bit") && (file_extn != ".mcs") && (file_extn != ".rpd"))
    {
        //不是OTA文件
        ret = false;
        LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafile() file_path=%s file suffix is unsupported ret=%d",file_path.c_str(), ret);
        return ret;
    }

    //得到去掉扩展名的文件名---CPPEN_MCU_V0.2.1.9
    file_name = file_name.substr(0,file_name.length() - 4);
    std::vector< std::string > vct_name;
    //将文件名分成文件类型、文件版本两个字段
    int count = xbasic::split_string(file_name,"-",&vct_name);
    if(count != 2 || vct_name[0].length() <= 0)
    {
        //文件命名不正确
        ret = false;
        LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafile() file_path=%s file name format is unsupported ret=%d",file_path.c_str(), ret);
        return ret;
    }

    int fw_size = 0;
    boost::shared_ptr<xotafile> otafile_find = find_otafile_ex(file_path);
    if(otafile_find != nullptr)
    {
        //已经存在该文件对象---更新相关属性
        std::string fw_md5 = xcrypto::iget_file_md5(file_path,&fw_size);
        if(fw_size <= 0 || fw_md5.length() != 32)
        {
            //MD5计算错误
            ret = false;
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafile() update ota map file_path=%s md5 calculate error ret=%d",file_path.c_str(), ret);
            return ret;
        }

        otafile_find->set(vct_name[0],vct_name[1],file_path,fw_size,fw_md5);
    }
    else
    {
        //不存在该文件对象
        std::string fw_md5 = xcrypto::iget_file_md5(file_path,&fw_size);
        if(fw_size <= 0 || fw_md5.length() != 32)
        {
            //MD5计算错误
            ret = false;
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_otafile() update ota map file_path=%s md5 calculate error ret=%d",file_path.c_str(), ret);
            return ret;
        }

        boost::shared_ptr<xotafile> otafile(new xotafile(vct_name[0],vct_name[1],file_path,fw_size,fw_md5));
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_otafile_ex);
        //加入到集合中
        m_map_otafile_ex.insert(std::make_pair(file_path,otafile));
        lock.unlock();
    }

    LOG_MSG(MSG_LOG,"mgr_upgrade::load_otafile() < ota > otafiles list:");
    LOG_MSG(MSG_LOG,"\n");
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_otafile_ex);
    for(boost::unordered_map<std::string,boost::shared_ptr<xotafile> >::iterator iter = m_map_otafile_ex.begin(); iter != m_map_otafile_ex.end(); iter++)
    {
        boost::shared_ptr<xotafile> otafile_find = iter->second;
        LOG_MSG(MSG_LOG,"mgr_upgrade::load_otafile() < ota > type:%s \tversion:%s \tsize:%d",otafile_find->m_type.c_str(),otafile_find->m_version.c_str(),otafile_find->m_size);
    }
    LOG_MSG(MSG_LOG,"\n");

    LOG_MSG(MSG_LOG, "Exited mgr_upgrade::load_otafile() ret=%d", ret);
    return ret;
}

//寻找OTA固件
boost::shared_ptr<xotafile> mgr_upgrade::find_otafile(std::string fw_type)
{
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_otafile);
    boost::unordered_map<std::string,boost::shared_ptr<xotafile> >::iterator iter = m_map_otafile.find(fw_type);
    if(iter != m_map_otafile.end())
    {
        //已经找到
        return iter->second;
    }
    return boost::shared_ptr<xotafile>();
}

boost::shared_ptr<xotafile> mgr_upgrade::find_otafile_ex(const std::string &file_path)
{
    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_otafile_ex);
    boost::unordered_map<std::string,boost::shared_ptr<xotafile> >::iterator iter = m_map_otafile_ex.find(file_path);
    if(iter != m_map_otafile_ex.end())
    {
        return iter->second;
    }
    return boost::shared_ptr<xotafile>();
}

//检测OTA升级
int mgr_upgrade::check_ota()
{
    return 0;
}

//清空otafile map内存数据
void mgr_upgrade::clear_otafile()
{
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_otafile);
    m_map_otafile.clear();

    return ;
}

//设置CAL目录路径
void mgr_upgrade::set_cal_path(const std::string& calfile_path)
{
    m_calfile_path = calfile_path;
    return ;
}

//获取CAL目录路径
std::string mgr_upgrade::get_cal_path()
{
    return m_calfile_path;
}

//加载CAL校准文件
void mgr_upgrade::load_calfiles()
{
    if(m_calfile_path.length() <= 0)
    {
        m_calfile_path = std::string(xbasic::get_module_path())+"calfiles";
    }

    std::vector<std::string> vct_files;
    xbasic::get_files_in_dir(m_calfile_path,&vct_files);

    for(unsigned int i = 0; i < vct_files.size(); i++)
    {
        //文件完整路径
        std::string file_path = vct_files[i];
        const char *split = strrchr(file_path.c_str(),'/');
        if(!split)
        {
            split = strrchr(file_path.c_str(),'\\');
        }

        //文件名
        std::string file_name = std::string(split?(split+1):file_path.c_str());
        const char *exten = strrchr(file_path.c_str(),'.');
        //文件扩展名
        std::string file_extn = (exten?exten:"");
        if(file_extn != ".json")
        {
            //不是CAL文件
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_calfiles() file_path=%s file suffix is unsupported",file_path.c_str());
            continue;
        }

        //得到去掉扩展名的文件名
        std::string temp = file_name.substr(0,file_name.length() - 5);
        std::vector< std::string > vct_name;
        //将文件名分成文件前缀、boardtype、boardsn、cal_type四个字段
        int count = xbasic::split_string(temp,"_",&vct_name);
        if(count != 4 || vct_name[0].length() <= 0)
        {
            //文件命名不正确
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_calfiles() file_path=%s file name format is unsupported",file_path.c_str());
            continue;
        }

        int fw_size = 0;
        boost::shared_ptr<xcalfile> calfile_find = find_calfile(file_name);
        //版本号,由于没有版本号字段,统一使用固定版本号
        std::string ver_str = "0.0.0.1";
        if(calfile_find != NULL)
        {
            //已经存在该类型的固件,更新相关属性
            //计算文件的MD5同时获得文件长度
            std::string fw_md5 = xcrypto::iget_file_md5(file_path,&fw_size);
            if(fw_size <= 0 || fw_md5.length() != 32)
            {
                //MD5计算错误
                LOG_MSG(WRN_LOG,"mgr_upgrade::load_calfiles() update cal map file_path=%s md5 calculate error",file_path.c_str());
                continue;
            }

            //更新CAL文件的文件属性
            calfile_find->set_fileinfo(file_name,ver_str,file_path,fw_size,fw_md5);
            //更新CAL文件的校准属性
            calfile_find->set_calinfo(vct_name[0],vct_name[1],vct_name[2],vct_name[3]);
        }
        else
        {
            //计算文件的MD5同时获得文件长度
            std::string fw_md5 = xcrypto::iget_file_md5(file_path,&fw_size);
            if(fw_size <= 0 || fw_md5.length() != 32)
            {
                //MD5计算错误
                LOG_MSG(WRN_LOG,"mgr_upgrade::load_calfiles() insert cal map file_path=%s md5 calculate error",file_path.c_str());
                continue;
            }

            boost::shared_ptr<xcalfile> calfile(new xcalfile(file_name,ver_str,file_path,fw_size,fw_md5));
            //更新CAL文件的校准属性
            calfile->set_calinfo(vct_name[0],vct_name[1],vct_name[2],vct_name[3]);
            //写锁
            boost::unique_lock<boost::shared_mutex> lock(m_mux_calfile);
            //加入到集合中
            m_map_calfile.insert(std::make_pair(file_name,calfile));
            lock.unlock();
        }

    }

    if(xconfig::debug() < 1)
    {
        //不打印
        return;
    }

    LOG_MSG(MSG_LOG,"mgr_upgrade::load_calfiles() < ota > otafiles list:");
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_calfile);
    for(boost::unordered_map<std::string,boost::shared_ptr<xcalfile> >::iterator iter = m_map_calfile.begin(); iter != m_map_calfile.end(); iter++)
    {
        boost::shared_ptr<xcalfile> calfile_find = iter->second;
        LOG_MSG(MSG_LOG,"mgr_upgrade::load_calfiles() < cal > m_calprefix:%s m_boardtype:%s m_boardsn:%s m_caltype:%s version:%s size:%d",\
            calfile_find->m_calprefix.c_str(),calfile_find->m_boardtype.c_str(),calfile_find->m_boardsn.c_str(),calfile_find->m_caltype.c_str(),calfile_find->m_version.c_str(),calfile_find->m_size);
    }
    LOG_MSG(MSG_LOG,"\n");

    return ;
}

bool mgr_upgrade::load_file(const std::string &file_path)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_upgrade::load_file() file_path=%s", file_path.c_str());
    bool ret = true;
    if(!check_file_path(file_path))
    {
        // 文件路径不合法
        ret = false;
        LOG_MSG(WRN_LOG, "Exited mgr_upgrade::load_file() file_path error");
        return ret;
    }

    int fw_size = 0;
    boost::shared_ptr<xcalfile> calfile_find = find_calfile(file_path);
    //版本号,由于没有版本号字段,统一使用固定版本号
    std::string ver_str = "0.0.0.1";
    if(calfile_find != NULL)
    {
        //已经存在该类型的固件,更新相关属性
        //计算文件的MD5同时获得文件长度
        std::string fw_md5 = xcrypto::iget_file_md5(file_path,&fw_size);
        if(fw_size <= 0 || fw_md5.length() != 32)
        {
            //MD5计算错误
            ret = false;
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_file() update cal map file_path=%s md5 calculate error",file_path.c_str());
            return ret;
        }

        //更新CAL文件的文件属性
        calfile_find->set_fileinfo(file_path,ver_str,file_path,fw_size,fw_md5);
        //更新CAL文件的校准属性
        // calfile_find->set_calinfo("", "", "", "");
    }
    else
    {
        //计算文件的MD5同时获得文件长度
        std::string fw_md5 = xcrypto::iget_file_md5(file_path,&fw_size);
        if(fw_size <= 0 || fw_md5.length() != 32)
        {
            //MD5计算错误
            ret = false;
            LOG_MSG(WRN_LOG,"mgr_upgrade::load_file() insert cal map file_path=%s md5 calculate error",file_path.c_str());
            return ret;
        }

        boost::shared_ptr<xcalfile> calfile(new xcalfile(file_path,ver_str,file_path,fw_size,fw_md5));
        //更新CAL文件的校准信息
        // calfile->set_calinfo(vct_name[0],vct_name[1],vct_name[2],vct_name[3]);
        //写锁
        boost::unique_lock<boost::shared_mutex> lock(m_mux_calfile);
        //加入到集合中
        m_map_calfile.insert(std::make_pair(file_path,calfile));
        lock.unlock();
    }

    LOG_MSG(MSG_LOG,"mgr_upgrade::load_file() < cal > file list:");
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_calfile);
    for(boost::unordered_map<std::string,boost::shared_ptr<xcalfile> >::iterator iter = m_map_calfile.begin(); iter != m_map_calfile.end(); iter++)
    {
        boost::shared_ptr<xcalfile> calfile_find = iter->second;
        LOG_MSG(MSG_LOG,"mgr_upgrade::load_file() < cal > m_calprefix:%s m_boardtype:%s m_boardsn:%s m_caltype:%s version:%s size:%d",\
            ,calfile_find->m_calprefix.c_str(),calfile_find->m_boardtype.c_str(),calfile_find->m_boardsn.c_str(),calfile_find->m_caltype.c_str(),calfile_find->m_version.c_str(),calfile_find->m_size);
    }
    LOG_MSG(MSG_LOG,"\n");

    LOG_MSG(MSG_LOG, "Exited mgr_upgrade::load_file()");
    return true;
}

bool mgr_upgrade::check_file_path(const std::string &file_path)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_upgrade::check_file_path() file_path=%s", file_path.c_str());
    bool ret = true;
    if(file_path.empty())
    {
        ret = false;
        LOG_MSG(WRN_LOG, "mgr_upgrade::check_file_path() file_path is empty");
    }

    // 2. 检查文件是否存在
    if(access(file_path.c_str(), F_OK) != 0)
    {
        ret = false;
        LOG_MSG(WRN_LOG, "mgr_upgrade::check_file_path() file_path is not exist");
    }

    // 3. 检查是否是普通文件（不是目录、符号链接等）
    struct stat buffer;
    if (stat(file_path.c_str(), &buffer) != 0)
    {
        ret = false;
        LOG_MSG(WRN_LOG, "mgr_upgrade::check_file_path() get file_path=%s stat filed", file_path.c_str());
    }
    else
    {
        if(!S_ISREG(buffer.st_mode))
        {
            ret = false;
            LOG_MSG(WRN_LOG, "mgr_upgrade::check_file_path() file_path=%s is not normal file", file_path.c_str());
        }
    }
    LOG_MSG(MSG_LOG, "Exited mgr_upgrade::check_file_path() ret=%d", ret);
    return ret;
}

//查找CAL校准文件
boost::shared_ptr<xcalfile> mgr_upgrade::find_calfile(const std::string& filename)
{
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_calfile);
    boost::unordered_map<std::string,boost::shared_ptr<xcalfile> >::iterator iter = m_map_calfile.find(filename);
    if(iter != m_map_calfile.end())
    {
        //已经找到
        return iter->second;
    }
    return boost::shared_ptr<xcalfile>();
}

//清空calfile map内存数据
void mgr_upgrade::clear_calfile()
{
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_calfile);
    m_map_calfile.clear();

    return ;
}

mgr_board_ver::mgr_board_ver()
{

}

mgr_board_ver::~mgr_board_ver()
{

}

std::string mgr_board_ver::get_soft_ver(int ota_type, const std::string &board_ver)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_board_ver::get_soft_ver() ota_type=%d board_ver:%s board_ver_size:%d", ota_type, board_ver.c_str(), board_ver.size());

    std::string ret_str;

    // 加读锁
    boost::shared_lock<boost::shared_mutex> lock(m_map_mutex);
    OtaMap::iterator it = m_map_ver.find(ota_type);
    if (it != m_map_ver.end())
    {
        // 下面的代码是调试用的
        // LOG_MSG(MSG_LOG, "mgr_board_ver::get_soft_ver() find ota_type=%d", it->first);
        // for(auto hw_it : it->second)
        // {
        //     LOG_MSG(MSG_LOG, "mgr_board_ver::get_soft_ver() hw_ver=%s hw_ver_size=%d", hw_it.first.c_str(), hw_it.first.size());
        //     if(board_ver == hw_it.first)
        //     {
        //         LOG_MSG(MSG_LOG, "mgr_board_ver::get_soft_ver() find!!!");
        //     }
        //     for(auto file : hw_it.second)
        //     {
        //         LOG_MSG(MSG_LOG, "mgr_board_ver::get_soft_ver() file=%s", file.c_str());
        //     }
        // }

        // 找到了对应的 ota_type
        HwVerMap::iterator hw_it = it->second.find(board_ver);

        if (hw_it != it->second.end())
        {
            // 找到了对应的硬件版本
            // if(is_fpgaota_type(ota_type) == true)
            // {
            //     ret_str = getMaxVersionFileOptimizedEx(hw_it->second);
            // }
            // else
            {
                ret_str = getMaxVersionFileOptimized(hw_it->second);
            }
            LOG_MSG(MSG_LOG, "mgr_board_ver::get_soft_ver() find latest soft_ver:%s", ret_str.c_str());
        }
    }

    LOG_MSG(MSG_LOG, "Exited mgr_board_ver::get_soft_ver() ret_str:%s", ret_str.c_str());
    return ret_str;
}

void mgr_board_ver::set_hardware_map(int ota_type, const std::string &hw_ver, const std::string &file_name)
{
    LOG_MSG(MSG_LOG, "Enter into mgr_board_ver::set_hardware_map() ota_type=%d, hw_ver=%s, file_name=%s", ota_type, hw_ver.c_str(), file_name.c_str());

    // 加写锁
    boost::unique_lock<boost::shared_mutex> lock(m_map_mutex);
    OtaMap::iterator it = m_map_ver.find(ota_type);
    if (it != m_map_ver.end())
    {
        // 找到了对应的 ota_type
        HwVerMap::iterator hw_it = it->second.find(hw_ver);
        if (hw_it != it->second.end())
        {
            // 找到了对应的硬件版本
            hw_it->second.push_back(file_name);
            LOG_MSG(MSG_LOG, "mgr_board_ver::set_hardware_map() hw_ver:%s push_back file_name:%s", hw_it->first.c_str(), file_name.c_str());
        }
        else
        {
            // 没找到对应的硬件版本
            std::vector<std::string> file_name_vct;
            file_name_vct.push_back(file_name);
            HwVerMap ver_map;
            ver_map.insert(std::make_pair(hw_ver, file_name_vct));
            m_map_ver.insert(std::make_pair(ota_type, ver_map));
            LOG_MSG(MSG_LOG, "mgr_board_ver::set_hardware_map() make new pair ota_type:%d hw_ver:%s push_back file_name:%s", ota_type, hw_ver.c_str(), file_name.c_str());
        }
    }
    else
    {
        // 没找到对应的 ota_type
        std::vector<std::string> file_name_vct;
        file_name_vct.push_back(file_name);
        HwVerMap ver_map;
        ver_map.insert(std::make_pair(hw_ver, file_name_vct));
        m_map_ver.insert(std::make_pair(ota_type, ver_map));
        LOG_MSG(MSG_LOG, "mgr_board_ver::set_hardware_map() make new pair ota_type:%d hw_ver:%s push_back file_name:%s", ota_type, hw_ver.c_str(), file_name.c_str());
    }

    LOG_MSG(MSG_LOG, "mgr_board_ver::set_hardware_map() <HARDWARE MAP> list:");
    for (auto it : m_map_ver)
    {
        LOG_MSG(MSG_LOG, "mgr_board_ver::set_hardware_map() ota_type=%d", it.first);
        for (auto hw_it : it.second)
        {
            LOG_MSG(MSG_LOG, "mgr_board_ver::set_hardware_map() hw_ver=%s", hw_it.first.c_str());
            for (auto file : hw_it.second)
            {
                LOG_MSG(MSG_LOG, "mgr_board_ver::set_hardware_map() file name=%s", file.c_str());
            }
        }
    }

    LOG_MSG(MSG_LOG, "Exited mgr_board_ver::set_hardware_map()");
}

void mgr_board_ver::clear_map()
{
    LOG_MSG(MSG_LOG, "Enter into mgr_board_ver::clear_map()");

    // 加写锁
    boost::unique_lock<boost::shared_mutex> lock(m_map_mutex);
    m_map_ver.clear();

    LOG_MSG(MSG_LOG, "Exited mgr_board_ver::clear_map()");
}

bool mgr_board_ver::is_fpgaota_type(int ota_type)
{
    bool ret = false;

    static int fpga_otatype[16] = {
        OTA_TYPE_CPPEM_CPMU_FPGA, OTA_TYPE_CPSYNC_CXBU_FPGA, OTA_TYPE_CROPS_CCCU_FPGA, OTA_TYPE_CPDPS_CPSU_FPGA,
        OTA_TYPE_CPRCA_CAPU_FPGA, OTA_TYPE_CPDIG_CDCU_FPGA, OTA_TYPE_CPPEM_CPDS_FPGA, OTA_TYPE_CPPEM_CPGM_FPGA,
        OTA_TYPE_FTISNC_FXBU_FPGA, OTA_TYPE_FTRCB_FPCU_FPGA, OTA_TYPE_FTRGB_FPMU_FPGA, OTA_TYPE_FTPMS_FCCU_FPGA,
        OTA_TYPE_FTIPIS_FPSU_FPGA, OTA_TYPE_FITEB_FPGM_FPGA, OTA_TYPE_FTDIG_FDCU_FPGA, OTA_TYPE_FPIEB_FPOS_FPGA
    };

    int sz = sizeof(fpga_otatype) / sizeof(fpga_otatype[0]);
    for (int i = 0; i < sz; i++)
    {
        if (fpga_otatype[i] == ota_type)
        {
            ret = true;
            break;
        }
    }

    return ret;
}

// void mgr_board_ver::get_hw_ver(std::vector<std::string> &hw_ver)
// {
//     LOG_MSG(MSG_LOG, "Enter into mgr_board_ver::get_hw_ver()");
//     // 加读锁
//     boost::shared_lock<boost::shared_mutex> lock(m_map_mutex);
//     for (auto it : m_map_ver)
//     {
//         hw_ver.push_back(it.first);
//         LOG_MSG(MSG_LOG, "mgr_board_ver::get_hw_ver() find hw version:%s", it.first.c_str());
//     }
//     LOG_MSG(MSG_LOG, "Exited mgr_board_ver::get_hw_ver()");
// }
