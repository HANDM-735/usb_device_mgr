#ifndef MGR_UPGRADE_H
#define MGR_UPGRADE_H
#include <string>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include "xbasic.hpp"
#include "xbasicmgr.hpp"
#include "xpackage.hpp"

//OTA文件类
class xotafile
{
public:
    xotafile(std::string type,std::string version,std::string path,int fsize,std::string md5);
    virtual ~xotafile();

public:
    //设置OTA文件相关属性
    void set(std::string type,std::string version,std::string path,int fsize,std::string md5);

public:
    //固件类型
    std::string     m_type;
    //固件版本
    std::string     m_version;
    //固件版本整型值
    long long       m_ver_int;
    //固件文件路径
    std::string     m_path;
    //固件文件长度
    int             m_size;
    //固件MD5值
    std::string     m_md5;
};

//CAL校准文件类
//文件命名格式:utp4@cal_<board_type>_<board_sn>_<cal_type>.json
class xcalfile
{
public:
    xcalfile(const std::string& filename,const std::string& version,const std::string& path,int fsize,const std::string& md5);
    virtual ~xcalfile();

public:
    //设置CAL文件的文件属性
    void set_fileinfo(const std::string& filename,const std::string& version,const std::string& path,int fsize,const std::string& md5);
    //设置校准CAL文件的校准属性
    void set_calinfo(const std::string& calprefix,const std::string& boardtype,const std::string& boardsn,const std::string& caltype);

public:
    //校准属性
    //校准文件前缀
    std::string     m_calprefix;
    //CAL校准文件对应板类型
    std::string     m_boardtype;
    //CAL校准文件对应SN号
    std::string     m_boardsn;
    //CAL校准类别
    std::string     m_caltype;

    //文件属性
    //校准文件名
    std::string     m_filename;
    //CAL文件版本
    std::string     m_version;
    //CAL版本整型值
    int             m_ver_int;
    //固件文件路径
    std::string     m_path;
    //固件文件长度
    int             m_size;
    //固件MD5值
    std::string     m_md5;
};

class mgr_board_ver : public xmgr_basic<mgr_board_ver>
{
public:
    // 硬件版本信息与软件版本信息映射表
    typedef boost::unordered_map<std::string, std::vector<std::string> > HwVerMap;
    // 升级固件版本信息映射表
    typedef boost::unordered_map<int, HwVerMap> OtaMap;
private:
    struct VersionInfo {
        std::string filename;
        std::vector<int> versionParts;

        bool operator<(const VersionInfo& other) const {
            size_t minSize = std::min(versionParts.size(), other.versionParts.size());
            for (size_t i = 0; i < minSize; i++) {
                if (versionParts[i] != other.versionParts[i]) {
                    return versionParts[i] < other.versionParts[i];
                }
            }
            return versionParts.size() < other.versionParts.size();
        }
    };

    struct VersionInfoEx {
        std::string filename;
        std::string versionPart;

        bool operator<(const VersionInfoEx& other) const {
            size_t minSize = std::min(versionPart.length(), other.versionPart.length());
            return (strncmp(versionPart.c_str(),other.versionPart.c_str(),minSize) < 0);
        }
    };
    std::string getMaxVersionFileOptimized(const std::vector<std::string>& files) {
        if (files.empty()) return "";

        std::vector<VersionInfo> versionInfos;

        for (const auto& file : files) {
            // 提取版本号部分
            size_t dashPos = file.find('-');
            if (dashPos == std::string::npos) continue;

            std::string versionStr = file.substr(dashPos + 1); // "V0.1.2.8.bin"
            versionStr = versionStr.substr(0, versionStr.find_last_of('.')); // "V0.1.2.8"
            versionStr = versionStr.substr(1); // "0.1.2.8"

            // 解析版本号
            VersionInfo info;
            info.filename = file;

            std::istringstream ss(versionStr);
            std::string part;
            while (std::getline(ss, part, '.')) {
                info.versionParts.push_back(std::stoi(part));
            }

            versionInfos.push_back(info);
        }

        if (versionInfos.empty()) return "";

        // 找到最大版本
        auto maxIt = std::max_element(versionInfos.begin(), versionInfos.end());
        return maxIt->filename;
    }

    std::string getMaxVersionFileOptimizedEx(const std::vector<std::string>& files) {
        if (files.empty()) return "";

        std::vector<VersionInfoEx> versionInfos;

        for (const auto& file : files) {
            // 提取版本号部分
            size_t dashPos = file.find('-');
            if (dashPos == std::string::npos) continue;

            std::string versionStr = file.substr(dashPos + 1); // "2026020614260100.bin"
            versionStr = versionStr.substr(0, versionStr.find_last_of('.')); // "2026020614260100"

            // 解析版本号
            VersionInfoEx info;
            info.filename = file;
            info.versionPart = versionStr;

            versionInfos.push_back(info);
        }

        if (versionInfos.empty()) return "";

        // 找到最大版本
        auto maxIt = std::max_element(versionInfos.begin(), versionInfos.end());
        return maxIt->filename;
    }
public:
    mgr_board_ver();
    ~mgr_board_ver();

public:
    // 获取当前单板硬件对应的软件版本
    std::string get_soft_ver(int ota_type, const std::string& board_ver);
    // 获取当前所有的硬件版本信息
    // void get_hw_ver(std::vector<std::string>& hw_ver);
    // 设置版本映射关系
    void set_hardver_map(int ota_type, const std::string& hw_ver, const std::string& file_name);
    // 清空硬件版本映射表
    void clear_map();

private:
    bool is_fpgaota_type(int ota_type);

private:
    OtaMap                       m_map_ver;
    boost::shared_mutex          m_map_mutex;
};

//升级文件加载管理器
class mgr_upgrade : public xmgr_basic<mgr_upgrade>
{
public:
    mgr_upgrade();
    ~mgr_upgrade();

public:
    //设置OTA目录路径
    void set_ota_path(std::string otafile_path);
    //获取OTA目录路径
    std::string get_ota_path();
    //加载最新OTA固件
    void load_otafiles();
    //加载指定OTA固件
    bool load_otafile(const std::string& file_path);
    //寻找OTA固件
    boost::shared_ptr<xotafile> find_otafile(std::string fw_type);
    boost::shared_ptr<xotafile> find_otafile_ex(const std::string& file_path);
    //检测某个单板OTA升级
    int check_ota();
    //清空otafile map内存数据
    void clear_otafile();

    //设置CAL目录路径
    void set_cal_path(const std::string& calfile_path);
    //获取CAL目录路径
    std::string get_cal_path();
    //加载CAL校准文件
    void load_calfiles();
    //加载文件(根据绝对路径)
    bool load_file(const std::string& file_path);
    //检测文件路径合法性
    bool check_file_path(const std::string& file_path);
    //查找CAL校准文件
    boost::shared_ptr<xcalfile> find_calfile(const std::string& filename);
    //清空calfile map内存数据
    void clear_calfile();

public:
    //初始化
    virtual void init();
    //工作函数
    virtual void work(unsigned long ticket);

protected:
    //OTA文件的路径
    std::string                                                   m_otafile_path;
    //各个最新OTA文件集合
    boost::unordered_map<std::string,boost::shared_ptr<xotafile> > m_map_otafile;
    //OTA文件集合读写锁
    boost::shared_mutex                                           m_mux_otafile;

    // OTA文件集合
    boost::unordered_map<std::string,boost::shared_ptr<xotafile> > m_map_otafile_ex;
    //OTA文件集合读写锁
    boost::shared_mutex                                           m_mux_otafile_ex;

    //CAL路径
    std::string                                                   m_calfile_path;
    //各个最新CAL文件集合
    boost::unordered_map<std::string,boost::shared_ptr<xcalfile> > m_map_calfile;
    //CAL文件集合读写锁
    boost::shared_mutex                                           m_mux_calfile;
};

#endif
