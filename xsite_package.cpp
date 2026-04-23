#include "xsite_package.h"

boost::shared_ptr<const std::string> sitejson_packer::pack_data(const char *pdata, size_t datalen, PACKWAY pack_way)
{
    LOG_MSG(MSG_LOG, "Enter into sitejson_packer::pack_data() data:%s, datalen:%d, pack_way:%d", pdata, datalen, pack_way);
    boost::shared_ptr<const std::string> ppack;
    if (NULL == pdata || 0 == datalen)
    {
        return ppack;
    }

    int totallen = (int)datalen;
    LOG_MSG(MSG_LOG, "sitejson_packer::pack_data() totallen:%d", totallen);
    std::string *raw_str = new std::string();

    if (pack_way == PACK_BASIC)
    {
        char header[SITE_HEADER_LEN] = {0};
        xbasic::write_bigendian(header, SITE_HEADER, 2);
        xbasic::write_bigendian(header + 2, totallen, 4);
        // 申请指定长度空间的字符串
        raw_str->reserve(totallen + SITE_HEADER_LEN);
        raw_str->append(header, SITE_HEADER_LEN);
        raw_str->append(pdata, totallen);
    }
    else
    {
        // 按原始数据打包
        // 申请指定长度空间的字符串
        raw_str->reserve(totallen);
        raw_str->append(pdata, totallen);
    }

    ppack.reset(raw_str);

    return ppack;
}

sitejson_unpacker::sitejson_unpacker() : xunpacker()
{
}

sitejson_unpacker::~sitejson_unpacker()
{
}

void sitejson_unpacker::reset_data()
{
    m_signed_len = (size_t)-1;
    m_data_len = 0;
}

bool sitejson_unpacker::unpack_data(size_t bytes_data, boost::container::list<boost::shared_ptr<const std::string>> &list_pack)
{
    // 本次收到的字节数
    m_data_len += bytes_data;
    bool unpack_ok = true;
    char *buff_head = m_raw_buff.begin();
    char *buff_data = buff_head;
    while (unpack_ok)
    {
        if (m_signed_len != (size_t)-1) // 包头解析完，现在解析包体
        {
            if (xbasic::read_bigendian(buff_data, 2) != SITE_HEADER)
            {
                // 包头错误
                unpack_ok = false;
                break;
            }

            size_t pack_len = m_signed_len + SITE_HEADER_LEN;
            if (m_data_len < pack_len)
            {
                // 不够一个包长，退出，在现有基础上继续接收
                break;
            }

            // 将这个包返回上层
            list_pack.push_back(boost::shared_ptr<std::string>(new std::string(buff_data, pack_len)));
            // 增加指定的长度做分析
            std::advance(buff_data, pack_len);
            m_data_len -= pack_len;
            m_signed_len = (size_t)-1;
        }
        else if (m_data_len >= SITE_HEADER_LEN)
        {
            // 已经收到了部分数据
            if (xbasic::read_bigendian(buff_data, 2) == SITE_HEADER)
            {
                // 获得长度标识
                m_signed_len = (size_t)xbasic::read_bigendian(buff_data + 2, 4);
            }
            else
            {
                // 包头错误
                unpack_ok = false;
            }
            if (m_signed_len != (size_t)-1 && m_signed_len > (MAX_MSGLEN - 1024))
            {
                // 包头中长度错误
                unpack_ok = false;
            }
        }
        else
        {
            // 包头都还没有收完，继续收
            break;
        }

        if (!unpack_ok)
        {
            // 解包错误，复位解包器
            xbasic::debug_output("network> json unpacker error!\n");
            reset_data();
            return unpack_ok;
        }

        if (m_data_len > 0 && buff_data > buff_head)
        {
            // 拷贝剩余断包到缓冲区头部
            for (unsigned int i = 0; i < m_data_len; i++)
            {
                buff_head[i] = buff_data[i];
            }
        }
    }
    return unpack_ok;
}

boost::asio::mutable_buffers_1 sitejson_unpacker::prepare_buff(size_t &min_recv_len)
{
    if (m_data_len >= (MAX_MSGLEN - 1024))
    {
        // 接收缓冲区即将溢出
        reset_data();
    }

    if (m_data_len >= SITE_HEADER_LEN)
    {
        // 已经收到了部分数据
        char *next_buff = m_raw_buff.begin();
        if (xbasic::read_bigendian(next_buff, 2) == SITE_HEADER)
        {
            // 包头正确
            min_recv_len = (m_signed_len + SITE_HEADER_LEN - m_data_len);
        }
    }
    else
    {
        // 还没有收到数据
        min_recv_len = SITE_HEADER_LEN - m_data_len;
    }
    if (min_recv_len == (size_t)-1 || min_recv_len > (MAX_MSGLEN - 1024))
    {
        reset_data();
        min_recv_len = SITE_HEADER_LEN;
    }
    // 使用mutable_buffer能防止接受缓冲区溢出
    return boost::asio::buffer(boost::asio::buffer(m_raw_buff) + m_data_len);
}

xcell::xcell(std::string name, std::string value) : m_name(name), m_value(value)
{
}

xcell *xcell::clone()
{
    return new xcell(m_name, m_value);
}

xreturn_cell::xreturn_cell()
{
}

xreturn_cell::xreturn_cell(std::vector<boost::shared_ptr<xcell>> cells_vect)
{
    m_returncell_vect = cells_vect;
}

xreturn_cell *xreturn_cell::clone()
{
    return new xreturn_cell(m_returncell_vect);
}

int xreturn_cell::add_cell(std::string name, std::string value)
{
    // 写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_return);
    boost::shared_ptr<xcell> new_data(new xcell(name, value));
    m_returncell_vect.push_back(new_data);
    return 0;
}

int xreturn_cell::get_all_cell(std::vector<boost::shared_ptr<xcell>> &arr_value)
{
    arr_value.clear();
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_return);
    for (unsigned int i = 0; i < m_returncell_vect.size(); i++)
    {
        arr_value.push_back(boost::shared_ptr<xcell>(m_returncell_vect[i]->clone()));
    }
    return arr_value.size();
}

int xreturn_cell::get_cellsize()
{
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_return);
    return m_returncell_vect.size();
}

// 获取指定返回值
void xreturn_cell::set_returncell(std::string name, std::string value_set)
{
    int return_size = get_cellsize();
    boost::unique_lock<boost::shared_mutex> lock(m_mux_return); // 写锁
    for (int i = 0; i < return_size; i++)
    {
        boost::shared_ptr<xcell> return_item = m_returncell_vect[i];
        if (return_item->m_name == name)
        {
            return_item->m_value = value_set;
            return;
        }
    }
    boost::shared_ptr<xcell> new_data(new xcell(name, value_set));
    m_returncell_vect.push_back(new_data);
}

// 获取指定返回值
std::string xreturn_cell::get_returncell(std::string name, std::string default_val)
{
    int return_size = get_cellsize();
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_return);
    for (int i = 0; i < return_size; i++)
    {
        boost::shared_ptr<xcell> return_item = m_returncell_vect[i];
        if (return_item->m_name == name)
        {
            return return_item->m_value;
        }
    }
    return default_val;
}

xcmd::xcmd(std::string cmd_type, std::string cmd_cont) : m_cmd_type(cmd_type), m_cmd_cont(cmd_cont)
{
}

xcmd *xcmd::clone()
{
    xcmd *new_cmd = new xcmd(m_cmd_type, m_cmd_cont);
    get_all_param(new_cmd->m_vct_param);
    get_all_return(new_cmd->m_vct_return);
    return new_cmd;
}

// 获取参数的个数
int xcmd::get_param_size()
{
    return m_vct_param.size();
}

// 获取返回值的个数
int xcmd::get_return_size()
{
    return m_vct_return.size();
}

// 添加参数
int xcmd::add_param(std::string name, std::string value)
{
    // 写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_param);
    boost::shared_ptr<xcell> new_data(new xcell(name, value));
    m_vct_param.push_back(new_data);
    return m_vct_param.size();
}

// 添加返回值
int xcmd::add_return(xreturn_cell &return_cell)
{
    // 写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_return);
    boost::shared_ptr<xreturn_cell> new_data(return_cell.clone());
    m_vct_return.push_back(new_data);
    return m_vct_return.size();
}

boost::shared_ptr<xcell> xcmd::get_param(int index)
{
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_param);
    return m_vct_param[index];
}

boost::shared_ptr<xreturn_cell> xcmd::get_return(int index)
{
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_return);
    return m_vct_return[index];
}

// 获取指定参数
std::string xcmd::get_param(std::string name, std::string default_val)
{
    int param_size = get_param_size();
    boost::shared_lock<boost::shared_mutex> lock(m_mux_param); // 读锁
    for (int i = 0; i < param_size; i++)
    {
        boost::shared_ptr<xcell> param_item = m_vct_param[i];
        if (param_item->m_name == name)
        {
            return param_item->m_value;
        }
    }
    return default_val;
}

void xcmd::clear_param(const std::string &name)
{
    LOG_MSG(MSG_LOG, "Enter into xcmd::clear_param() name:%s", name.c_str());
    // 加写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_param);
    m_vct_param.erase(std::remove_if(m_vct_param.begin(), m_vct_param.end(), [&](const boost::shared_ptr<xcell> &tmp) {
        return tmp->m_name == name;
    }), m_vct_param.end());
    LOG_MSG(MSG_LOG, "Exited xcmd::clear_param()");
}

// 获取指定参数
void xcmd::set_param(std::string name, std::string value_set)
{
    int param_size = get_param_size();
    // 写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_param);
    for (int i = 0; i < param_size; i++)
    {
        boost::shared_ptr<xcell> param_item = m_vct_param[i];
        if (param_item->m_name == name)
        {
            param_item->m_value = value_set;
            return;
        }
    }
    boost::shared_ptr<xcell> new_data(new xcell(name, value_set));
    m_vct_param.push_back(new_data);
}

int xcmd::get_all_param(std::vector<boost::shared_ptr<xcell>> &arr_value)
{
    arr_value.clear();
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_param);
    for (unsigned int i = 0; i < m_vct_param.size(); i++)
    {
        arr_value.push_back(boost::shared_ptr<xcell>(m_vct_param[i]->clone()));
    }
    return arr_value.size();
}

int xcmd::get_all_return(std::vector<boost::shared_ptr<xreturn_cell>> &arr_value)
{
    arr_value.clear();
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_return);
    for (unsigned int i = 0; i < m_vct_return.size(); i++)
    {
        arr_value.push_back(boost::shared_ptr<xreturn_cell>(m_vct_return[i]->clone()));
    }
    return arr_value.size();
}

void xcmd::clear_param()
{
    // 写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_param);
    m_vct_param.clear();
}

void xcmd::clear_return()
{
    // 写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_return);
    m_vct_return.clear();
}

// 组合命令
std::string xcmd::compose_cmd()
{
    std::string ret_cmd = m_cmd_cont;
    int param_size = get_param_size();
    // 读锁
    boost::shared_lock<boost::shared_mutex> lock(m_mux_param);
    for (int i = 0; i < param_size; i++)
    {
        boost::shared_ptr<xcell> param_item = m_vct_param[i];
        if (!param_item || param_item->m_name != "cmd_param")
        {
            continue;
        }
        ret_cmd += (" " + param_item->m_value);
    }
    return ret_cmd;
}

// 设置 return_info 信息
void xcmd::add_return_info(const std::string &info)
{
    // 加入 return_info 信息
    xreturn_cell return_info;
    return_info.add_cell(SITE_NAME_ITEM, "return_info");
    return_info.add_cell(SITE_VALUE_ITEM, info);
    add_return(return_info);
}

// 设置 return_code 信息
void xcmd::add_return_code(const std::string &code)
{
    // 加入 return_code 信息
    xreturn_cell return_info;
    return_info.add_cell(SITE_NAME_ITEM, "return_code");
    return_info.add_cell(SITE_VALUE_ITEM, code);
    add_return(return_info);
}

std::string xcmd::get_return_value(const std::string &name)
{
    // 读锁
    std::string ret;
    boost::shared_lock<boost::shared_mutex> lock(m_mux_return);
    for (unsigned int i = 0; i < m_vct_return.size(); i++)
    {
        // 遍历每一个 xreturn_cell，这一个 xreturn_cell 就是 return json 对象数组中的一个 json 对象
        // 获取到该 json 对象的 name 字段
        std::string name_value = m_vct_return[i]->get_returncell(SITE_NAME_ITEM);
        if (name == name_value)
        {
            // 说明是 return_code json 对象
            ret = m_vct_return[i]->get_returncell(SITE_VALUE_ITEM);
            break;
        }
    }
    return ret;
}

// 将 JSON 数据格式化为 cmd 对象数组
int xcmd::parse_from_json(cjson_object &cmd_json, std::list<boost::shared_ptr<xcmd>> &list_cmd)
{
    int data_size = (!cmd_json.is_empty() ? cmd_json.get_array_size() : 0);
    if (data_size <= 0)
    {
        return true;
    }

    for (int i = 0; i < data_size; i++)
    {
        cjson_object &js_item = cmd_json[i];
        std::string cmd_type = js_item["cmd_type"];
        std::string cmd_cont = js_item["cmd_cont"];
        boost::shared_ptr<xcmd> new_cmd(new xcmd(cmd_type, cmd_cont));
        cjson_object &js_param = js_item["param"];
        cjson_object &js_return = js_item["return"];

        int param_size = (js_param.is_empty() ? 0 : js_param.get_array_size());
        int return_size = (js_return.is_empty() ? 0 : js_return.get_array_size());

        for (int iparam = 0; iparam < param_size; iparam++)
        {
            cjson_object &js_param_item = js_param[iparam];
            std::string str_value = js_param_item["value"].to_string();
            if (str_value[0] == '\"' && str_value.length() >= 2)
            {
                str_value = str_value.substr(1, str_value.length() - 2);
            }
            new_cmd->add_param(js_param_item["name"], str_value);
        }

        for (int ireturn = 0; ireturn < return_size; ireturn++)
        {
            cjson_object &js_return_item = js_return[ireturn];

            xreturn_cell return_cell;
            std::string name_itemval = js_return_item[SITE_NAME_ITEM];
            if (!name_itemval.empty())
            {
                return_cell.add_cell(SITE_NAME_ITEM, name_itemval);
            }

            std::string value_itemval = js_return_item[SITE_VALUE_ITEM].to_string();
            if (!value_itemval.empty())
            {
                if (value_itemval[0] == '\"' && value_itemval.length() >= 2)
                {
                    value_itemval = value_itemval.substr(1, value_itemval.length() - 2);
                }
                return_cell.add_cell(SITE_VALUE_ITEM, value_itemval);
            }
            new_cmd->add_return(return_cell);
        }
        list_cmd.push_back(new_cmd);
    }
    return list_cmd.size();
}

// 将 cmd 对象数组格式化为 JSON 数据
std::string xcmd::serial_to_json(std::list<boost::shared_ptr<xcmd>> &list_cmd)
{
    cjson_object cmd_json("[]");
    for (std::list<boost::shared_ptr<xcmd>>::iterator iter = list_cmd.begin(); iter != list_cmd.end(); iter++)
    {
        boost::shared_ptr<xcmd> cmd = *iter;
        cjson_object cmd_item;
        cmd_item.add("cmd_type", cmd->m_cmd_type);
        cmd_item.add("cmd_cont", cmd->m_cmd_cont);
        std::vector<boost::shared_ptr<xcell>> arr_param;
        std::vector<boost::shared_ptr<xreturn_cell>> arr_return;
        int param_size = cmd->get_all_param(arr_param);
        int return_size = cmd->get_all_return(arr_return);
        if (param_size > 0)
        {
            cjson_object param_json("[]");
            for (int iparam = 0; iparam < param_size; iparam++)
            {
                cjson_object param_cell("{}");
                cjson_object param_cell_value(arr_param[iparam]->m_value);
                param_cell.add("name", arr_param[iparam]->m_name);

                if ((!param_cell_value.is_object()) && (!param_cell_value.is_array()))
                {
                    // 以字符串形式
                    param_cell.add("value", arr_param[iparam]->m_value);
                }
                else
                {
                    // 以对象形式
                    param_cell.add("value", param_cell_value);
                }
                param_json.add(param_cell);
            }
            cmd_item.add("param", param_json);
        }
        if (return_size > 0)
        {
            cjson_object return_json("[]");
            for (int ireturn = 0; ireturn < return_size; ireturn++)
            {
                boost::shared_ptr<xreturn_cell> return_cell = arr_return[ireturn];
                std::vector<boost::shared_ptr<xcell>> arr_cell;
                int cell_sz = return_cell->get_all_cell(arr_cell);

                cjson_object return_cell_obj("{}");
                for (int jcell = 0; jcell < cell_sz; jcell++)
                {
                    std::string item_name = arr_cell[jcell]->m_name;
                    std::string item_value = arr_cell[jcell]->m_value;
                    if (item_name == SITE_NAME_ITEM)
                    {
                        return_cell_obj.add(item_name, item_value);
                    }
                    else
                    {
                        if (item_name == SITE_VALUE_ITEM)
                        {
                            cjson_object return_cell_value(item_value);
                            if (return_cell_value.is_empty())
                            {
                                // 以字符串形式
                                return_cell_obj.add(item_name, item_value);
                            }
                            else
                            {
                                // 以对象形式
                                return_cell_obj.add(item_name, return_cell_value);
                            }
                        }
                    }
                }
                return_json.add(return_cell_obj);
            }
            cmd_item.add("return", return_json);
        }
        cmd_json.add(cmd_item);
    }
    return cmd_json.to_string();
}

xsite_package::xsite_package()
{
    m_timestamp = time(NULL);
    set_header(SITE_MT_REQUEST, "", 0, 0);

    if ((m_msg_seq == 0) && (m_msg_type != SITE_MT_RESPOND))
    {
        // 自动生成序号
        m_msg_seq = create_msg_seq();
    }
}

xsite_package::xsite_package(std::string msg_type, std::string msg_session, std::string board_id)
{
    m_timestamp = time(NULL);
    set_header(msg_type, msg_session, 0, 0);
    m_board_id = board_id;

    if ((m_msg_seq == 0) && (m_msg_type != SITE_MT_RESPOND))
    {
        // 自动生成序号
        m_msg_seq = create_msg_seq();
    }

    if (m_board_id.length() <= 0)
    {
        // 获取自身板 ID
        m_board_id = xconfig::get_instance()->get_data("site_id");
    }
}

xsite_package::xsite_package(std::string msg_type, std::string msg_session, int msg_seq, int err_code, std::string board_id)
{
    m_timestamp = time(NULL);
    set_header(msg_type, msg_session, msg_seq, err_code);
    m_board_id = board_id;
}

if ((m_msg_seq == 0) && (m_msg_type != SITE_MT_RESPOND))
{
    // 自动生成序号
    m_msg_seq = create_msg_seq();
}

if (m_board_id.length() <= 0)
{
    // 获取自身板ID
    m_board_id = xconfig::get_instance()->get_data("self_id");
}

xsite_package::xsite_package(std::string &json_data)
{
    m_timestamp = time(NULL);
    set_header(SITE_MT_REQUEST, "", 0, 0);
    parse_from_json(json_data);
}

xsite_package::~xsite_package()
{
}

// 克隆对象
xpacket* xsite_package::clone()
{
    xsite_package *new_package = new xsite_package(m_msg_type, m_msg_session, m_msg_seq, m_err_code, m_board_id);
    new_package->m_timestamp = m_timestamp;
    get_cmd(new_package->m_lst_cmd);
    return new_package;
}

// 需要确认并重试的包
bool xsite_package::need_confirm()
{
    return (m_msg_type == SITE_MT_REQUEST);
}

// 是确认包
bool xsite_package::type_confirm()
{
    return (m_msg_type == SITE_MT_RESPOND);
}

// 产生消息序号
int xsite_package::create_msg_seq()
{
    // 防止多线程，会话id重复，造成异步消息回复处理异常
    // 写锁
    boost::unique_lock<boost::shared_mutex> lock(m_mux_sid);
    static int s_msg_seq = 0;
    s_msg_seq = ((s_msg_seq + 1) % 0x7FFFFFFF);
    return (s_msg_seq);
}

bool xsite_package::parse_from_json(std::string &json_data)
{
    std::string js_root_str(json_data);
    cjson_object js_root(js_root_str);
    if (js_root.is_empty())
    {
        return false;
    }

    m_board_id = js_root("board_id");
    if (m_board_id.length() <= 0)
    {
        // 获取自身板ID
        m_board_id = xconfig::get_instance()->get_data("self_id");
    }

    m_msg_type = js_root("msg_type");
    m_msg_session = js_root("msg_session");
    std::string msg_seq = js_root("msg_seq");
    std::string err_code = js_root("err_code");
    // 用于匹配请求和响应
    m_msg_seq = atoi(msg_seq.c_str());
    // 错误码
    m_err_code = atoi(err_code.c_str());
    cjson_object &js_data = js_root("msg_data");

    boost::mutex::scoped_lock lock(m_mux_cmd);
    xcmd::parse_from_json(js_data, m_lst_cmd);
    lock.unlock();

    return true;
}

// 串行化成JSON
std::string xsite_package::serial_to_json()
{
    cjson_object js_root;
    js_root.add("board_id", m_board_id);
    js_root.add("msg_type", m_msg_type);
    js_root.add("msg_session", m_msg_session);
    js_root.add("msg_seq", m_msg_seq);
    if (m_err_code != 0)
    {
        js_root.add("err_code", m_err_code);
    }
    js_root.add("time_stamp", xbasic::time_to_str(m_timestamp));

    boost::mutex::scoped_lock lock(m_mux_cmd);
    std::string cmd_data = xcmd::serial_to_json(m_lst_cmd);
    lock.unlock();

    cjson_object js_data(cmd_data);
    js_root.add("msg_data", js_data);

    return js_root.to_string();
}

void xsite_package::reset()
{
    m_msg_type = SITE_MT_REQUEST;
    m_msg_session = SITE_MC_NONE;
    m_msg_seq = 0;
    boost::mutex::scoped_lock lock(m_mux_cmd);
    m_lst_cmd.clear();
}

// 设置头部信息
void xsite_package::set_header(std::string msg_type, std::string msg_session, int msg_seq, int err_code)
{
    m_msg_type = msg_type;
    m_msg_session = msg_session;
    m_msg_seq = msg_seq;
    m_err_code = err_code;
}

// 获得命令个数
int xsite_package::get_cmd_size()
{
    return m_lst_cmd.size();
}

// 将TLV对象数组加入到包
int xsite_package::add_cmd(std::list<boost::shared_ptr<xcmd>> &list_cmd)
{
    for (std::list<boost::shared_ptr<xcmd>>::iterator iter = list_cmd.begin(); iter != list_cmd.end(); iter++)
    {
        boost::shared_ptr<xcmd> cmd = *iter;
        add_cmd(cmd);
    }

    return list_cmd.size();
}

// 获取TLV对象数组
int xsite_package::get_cmd(std::list<boost::shared_ptr<xcmd>> &list_cmd)
{
    boost::mutex::scoped_lock lock(m_mux_cmd);
    for (std::list<boost::shared_ptr<xcmd>>::iterator iter = m_lst_cmd.begin(); iter != m_lst_cmd.end(); iter++)
    {
        boost::shared_ptr<xcmd> cmd = *iter;
        list_cmd.push_back(boost::shared_ptr<xcmd>(cmd->clone()));
    }
    return list_cmd.size();
}

// 加入cmd数据单元到package
void xsite_package::add_cmd(boost::shared_ptr<xcmd> cmd_data)
{
    boost::mutex::scoped_lock lock(m_mux_cmd);
    m_lst_cmd.push_back(boost::shared_ptr<xcmd>(cmd_data->clone()));
}

// 从package寻找数据单元
boost::shared_ptr<xcmd> xsite_package::find_cmd(int index)
{
    int idx_this = 0;
    boost::mutex::scoped_lock lock(m_mux_cmd);
    for (std::list<boost::shared_ptr<xcmd>>::iterator iter = m_lst_cmd.begin(); iter != m_lst_cmd.end(); iter++, idx_this++)
    {
        boost::shared_ptr<xcmd> cmd = *iter;
        if (index == idx_this)
            return cmd;
    }

    return boost::shared_ptr<xcmd>();
}

// 从package删除命令单元
void xsite_package::del_cmd(xcmd *cmd)
{
    boost::mutex::scoped_lock lock(m_mux_cmd);
    for (std::list<boost::shared_ptr<xcmd>>::iterator iter = m_lst_cmd.begin(); iter != m_lst_cmd.end(); iter++)
    {
        boost::shared_ptr<xcmd> cmd_item = *iter;
        if (cmd_item.get() == cmd)
        {
            m_lst_cmd.erase(iter);
            break;
        }
    }
}

// 从package删除命令单元
void xsite_package::del_cmd(int index)
{
    int idx_this = 0;
    boost::mutex::scoped_lock lock(m_mux_cmd);
    // 删除所有
    if (index < 0)
    {
        m_lst_cmd.clear();
        return;
    }

    for (std::list<boost::shared_ptr<xcmd>>::iterator iter = m_lst_cmd.begin(); iter != m_lst_cmd.end(); iter++, idx_this++)
    {
        if (index == idx_this)
        {
            m_lst_cmd.erase(iter);
            break;
        }
    }
}
