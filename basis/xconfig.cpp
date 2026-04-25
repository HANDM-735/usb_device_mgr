#include "xconfig.h"

//xconfig类静态变量初始化
xconfig*    xconfig::m_instance = NULL;
std::mutex  xconfig::m_mtx;

xconfig::xconfig()
{

}

xconfig::~xconfig()
{

}

//单例模式对象获得
xconfig* xconfig::get_instance()
{
    if(m_instance == NULL)
    {
        //第一重检测，如果未初始化
        //上锁，RAII，离开if{}自动解锁
        std::lock_guard<std::mutex> lck(m_mtx);
        if(m_instance == NULL)
        {
            //第二重检测，还未初始化，new
            m_instance = new xconfig();
        }
    }
    return m_instance;
}

void xconfig::release_instance()
{
    std::lock_guard<std::mutex> lck(m_mtx);
    if(m_instance != NULL)
    {
        delete m_instance;
        m_instance = NULL;
    }
}

//获得调试等级
int xconfig::debug()
{
    return xconfig::get_instance()->get_data("debug",0);
}

//获得自身ID
int xconfig::self_id()
{
    return xconfig::get_instance()->get_data("self_id",0);
}

//获得site ID
int xconfig::site_id()
{
    return xconfig::get_instance()->get_data("site_id",0);
}

//设置用户数据
void xconfig::set_data(std::string key,std::string value)
{
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mx_map_data);
    m_map_data[key] =value;
}

//获取用户数据
std::string xconfig::get_data(std::string key)
{
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mx_map_data);
    boost::unordered_map<std::string,std::string>::iterator iter =m_map_data.find(key);
    return (iter==m_map_data.end())?"":(iter->second);
}

//设置用户数据
void xconfig::set_data(std::string key,int value)
{
    //写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mx_map_data);
    m_map_data[key] =boost::str(boost::format("%d")%value);
}

int xconfig::get_data(std::string key,int def_value)
{
    //读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mx_map_data);
    boost::unordered_map<std::string,std::string>::iterator iter =m_map_data.find(key);
    return (iter==m_map_data.end())?def_value:(int)strtol(iter->second.c_str(),NULL,0);
}

xini_config::xini_config()
{

}

xini_config::xini_config(std::string file_path)
{
    m_file_path = file_path;
}

bool xini_config::set_file(std::string file_path)
{
    m_file_path = file_path;
    try
    {
        m_tree_ini.clear();
        boost::property_tree::ini_parser::read_ini(m_file_path,m_tree_ini);
        return true;
    }
    catch (...)
    {
        //读取配置文件错误! 重新生成
        xbasic::debug_output("< cfg > config file: %s read error!\n",m_file_path.c_str());
    }
    return false;
}

std::string xini_config::get_data(std::string key,std::string def_value="")
{
    try
    {
        std::string value =m_tree_ini.get<std::string>(key);
        return value;
    }
    catch (...) {}
    return def_value;
}

int xini_config::get_data(std::string key,int def_value)
{
    try
    {
        std::string value =m_tree_ini.get<std::string>(key);
        return atoi(value.c_str());
    }
    catch (...)
    {

    }
    return def_value;
}

int xini_config::set_data(std::string key,std::string value,bool save=true)
{
    try
    {
        m_tree_ini.put<std::string>(key,value);
        if(save) boost::property_tree::ini_parser::write_ini(m_file_path,m_tree_ini);
    }
    catch (...)
    {
        return -1;
    }
    return 0;
}

int xini_config::set_data(std::string key,int value,bool save=true)
{
    try
    {
        m_tree_ini.put<int>(key,value);
        if(save) boost::property_tree::ini_parser::write_ini(m_file_path,m_tree_ini);
    }
    catch (...)
    {
        return -1;
    }
    return 0;
}

int xini_config::save_data() //保存数据到磁盘文件
{
    try
    {
        boost::property_tree::ini_parser::write_ini(m_file_path,m_tree_ini);
    }
    catch (...)
    {
        return -1;
    }
    return 0;
}

xjson_config::xjson_config()
{

}

xjson_config::xjson_config(std::string file_path)
{
    set_file(file_path);
}

xjson_config xjson_config::from_string(std::string js_string)
{
    xjson_config js_config;
    js_config.set_data(js_string);
    return js_config;
}

//获得文件中的JSON格式的字符串
std::string xjson_config::get_file_json(std::string file_path)
{
    xjson_config js_config(file_path);
    return (js_config.m_js_config.is_empty()?"":js_config.m_js_config.to_string());
}

//设置加载文件
bool xjson_config::set_file(std::string file_path)
{
    if(!m_js_config.is_empty())
    {
        m_js_config.clear();
    }
    m_file_path = file_path;
    FILE *file_fd = fopen(m_file_path.c_str(),"r");
    if(!file_fd)
    {
        return false;
    }
    //定位到文件末尾
    fseek(file_fd,0L,SEEK_END);
    //得到文件大小
    int file_len = ftell(file_fd);
    //定位到文件开头
    fseek(file_fd,0L,SEEK_SET);
    char *buff =new char[file_len+4];
    int read_len =fread(buff,1,file_len,file_fd);
    fclose(file_fd);
    std::string json_str(buff,read_len);
    m_js_config.parse(json_str);
    delete[] buff;
    buff = NULL;

    if(m_js_config.is_empty())
    {
        return false;
    }
    return true;
}

//保存到文件
bool xjson_config::save(std::string file_path)
{
    if(file_path.length() ==0) file_path =m_file_path;
    std::string js_string =m_js_config.to_formatted_string();
    if(js_string.length()<=0) return false;
    FILE *file_fd =fopen(file_path.c_str(),"w+");
    if(!file_fd) return false;
    fwrite(js_string.c_str(),js_string.length(),1,file_fd);
    fclose(file_fd);
    return true;
}

//设置JSON字符串数据
bool xjson_config::set_data(std::string js_data)
{
    return m_js_config.parse(js_data);
}

bool xjson_config::set_value(std::string key,std::string value_s)
{
    return set_value(key,value_s,0,0,V_STRING);
}

bool xjson_config::set_value(std::string key,int value_i)
{
    return set_value(key,"",value_i,0,V_INT);
}

bool xjson_config::set_value(std::string key,float value_f)
{
    return set_value(key,"",0,value_f,V_FLOAT);
}

std::string xjson_config::value(std::string key)
{
    std::string last_key;
    cjson_object &js_node =value_node(key,last_key);
    if(js_node.is_empty() ||last_key.length()==0)
    {
        //父级节点不存在
        return "";
    }
    return js_node(last_key);
}

int xjson_config::value_int(std::string key)
{
    std::string value_s =value(key);
    return atoi(value_s.c_str());
}

float xjson_config::value_float(std::string key)
{
    std::string value_s =value(key);
    return (float)atof(value_s.c_str());
}

//获得数组的长度
int xjson_config::array_size(std::string key)
{
    std::string last_key;
    cjson_object &js_node =value_node(key,last_key);
    if(js_node.is_empty() ||last_key.length()==0)
    {
        //父级节点不存在
        return -1;
    }

    cjson_object &js_last_node =js_node[last_key];
    if(js_last_node.is_empty() ||!js_last_node.is_array())
    {
        return -1;
    }
    return js_last_node.get_array_size();
}

//获得数组指定索引的节点
std::string xjson_config::index_node(std::string key,int index)
{
    std::string last_key;
    cjson_object &js_node =value_node(key,last_key);
    if(js_node.is_empty() ||last_key.length()==0)
    {
        //父级节点不存在
        return "";
    }

    cjson_object &js_last_node =js_node[last_key];
    if(js_last_node.is_empty() ||!js_last_node.is_array())
    {
        return "";
    }

    int arr_size =js_last_node.get_array_size();
    if(index >=arr_size) return "";
    cjson_object &js_item =js_last_node[index];
    std::string js_item_str =(js_item.is_empty()?"":js_item.to_string());
    return js_item_str;
}

//获得数组的值到容器
int xjson_config::array_value(std::string key,std::vector<std::string> &vct_value)
{
    vct_value.clear();
    std::string last_key;
    cjson_object &js_node =value_node(key,last_key);
    if(js_node.is_empty() ||last_key.length()==0)
    {
        //父级节点不存在
        return -1;
    }

    cjson_object &js_last_node =js_node[last_key];
    if(js_last_node.is_empty() ||!js_last_node.is_array())
    {
        return -1;
    }

    int arr_size =js_last_node.get_array_size();
    for(int i=0;i<arr_size;i++)
    {
        cjson_object &js_item =js_last_node[i];
        std::string js_item_str =(js_item.is_empty()?"":js_item.to_string());
        if(js_item_str.length() <=0) continue;
        vct_value.push_back(js_item_str);
    }
    return vct_value.size();
}

//获得key中倒数第二节点
cjson_object& xjson_config::value_node(std::string key,std::string &last_key)
{
    std::vector<std::string> vct_key;
    int key_num =xbasic::split_string(key,".",&vct_key);
    if(key_num==1)
    {
        //只有一级节点
        last_key=vct_key[0];
        return m_js_config;
    }

    cjson_object &js_temp =m_js_config[vct_key[0]];
    if(js_temp.is_empty())
    {
        //父级节点不存在
        return js_temp;
    }

    for(int i=1;i<key_num;i++)
    {
        if(i == key_num-1)
        {
            last_key=vct_key[i];
            return js_temp;
        }

        js_temp =js_temp[vct_key[i]];
        if(js_temp.is_empty())
        {
            return js_temp;
        }
    }
    return js_temp;
}

bool xjson_config::set_value(std::string key,std::string value_s,int value_i,float value_f,int value_type)
{
    std::string js_string =m_js_config.to_string();
    cjson_object js_tmp_cfg(js_string);
    std::vector<std::string> vct_key;
    int key_num =xbasic::split_string(key,".",&vct_key);
    if(key_num==0 || vct_key[0].length()==0)
    {
        return false;
    }
    do
    {
        if(key_num==1)
        {
            //只有一级节点
            js_tmp_cfg.del(vct_key[0]);
            if(value_type ==V_INT)        js_tmp_cfg.add(vct_key[0],value_i);
            else if(value_type ==V_FLOAT) js_tmp_cfg.add(vct_key[0],value_f);
            else                          js_tmp_cfg.add(vct_key[0],value_s);
            break;
        }

        cjson_object &js_temp =js_tmp_cfg[vct_key[0]];
        if(js_temp.is_empty())
        {
            //父级节点不存在
            js_tmp_cfg.add_empty_sub_object(vct_key[0]);
            js_temp =js_tmp_cfg[vct_key[0]];
        }

        for(int i=1;i<key_num;i++)
        {
            if(i==key_num-1) //最后一级节点
            {
                js_temp.del(vct_key[i]);
                if(value_type ==V_INT)        js_temp.add(vct_key[i],value_i);
                else if(value_type ==V_FLOAT) js_temp.add(vct_key[i],value_f);
                else                          js_temp.add(vct_key[i],value_s);
                break;
            }

            cjson_object &js_get_temp = js_temp[vct_key[i]];
            if(js_get_temp.is_empty())
            {
                //中间节点不存在
                js_temp.add_empty_sub_object(vct_key[i]);
                js_temp =js_temp[vct_key[i]];
            }
            else
            {
                js_temp =js_get_temp;
            }
        }

    }while(0);

    js_string =js_tmp_cfg.to_formatted_string();
    m_js_config.parse(js_string);
    return true;
}
