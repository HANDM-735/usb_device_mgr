#ifndef XCONFIG_H
#define XCONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <mutex>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "xbasic.hpp"
#include "libjson/cjsonobject.h"

#define APP_VERSION    "1.2.0.1" //version

class xconfig //内存配置数据类
{
public:
    xconfig();
    virtual ~xconfig();

    //单例模式对象获得
    static xconfig *get_instance();
    //释放单例模式对象
    static void release_instance();

    static int debug();
    static int self_id();
    static int site_id();

public:
    //设置用户数据
    void set_data(std::string key,std::string value);
    //获取用户数据
    std::string get_data(std::string key);
    //设置用户数据
    void set_data(std::string key,int value);
    //获取用户数据
    int get_data(std::string key,int def_value);

protected:
    //数据
    boost::unordered_map<std::string,std::string>    m_map_data;
    //数据表读写锁
    boost::shared_mutex                               m_mx_map_data;

private:
    //单例实例
    static xconfig*        m_instance;
    //单例多线程安全锁
    static std::mutex      m_mtx;
};

//INI配置类
class xini_config
{
public:
    xini_config();
    xini_config(std::string file_path);

public:
    bool set_file(std::string file_path);
    std::string get_data(std::string key,std::string def_value="");
    int get_data(std::string key,int def_value=0);
    int set_data(std::string key,std::string value,bool save=true);
    int set_data(std::string key,int value,bool save=true);
    //保存数据到磁盘文件
    int save_data();

protected:
    std::string                 m_file_path;
    boost::property_tree::ptree m_tree_ini;
};

//JSON配置文件类
class xjson_config
{
public:
    //数值类型定义
    enum VALUE_TYPE    {V_STRING=0x01,V_INT,V_FLOAT};

public:
    xjson_config();
    xjson_config(std::string file_path);

    static xjson_config from_string(std::string js_string);
    //获得文件中的JSON格式的字符串
    static std::string get_file_json(std::string file_path);

public:
    //设置加载文件
    bool set_file(std::string file_path);
    //保存到文件
    bool save(std::string file_path);
    //设置JSON字符串数据
    bool set_data(std::string js_data);
    bool set_value(std::string key,std::string value_s);
    bool set_value(std::string key,int value_i);
    bool set_value(std::string key,float value_f);

    std::string value(std::string key);
    int value_int(std::string key);
    float value_float(std::string key);

    //获得数组的长度
    int array_size(std::string key);
    //获得数组指定索引的节点
    std::string index_node(std::string key,int index);
    //获得数组的值到容器
    int array_value(std::string key,std::vector<std::string> &vct_value);

    //获得key中倒数第二节点
    cjson_object &value_node(std::string key,std::string &last_key);
    bool set_value(std::string key,std::string value_s,int value_i,float value_f,int value_type=V_STRING);

protected:
    std::string       m_file_path;
    cjson_object      m_js_config;
};

#endif // XCONFIG_H
