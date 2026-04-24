#include "mgr_slots_config.h"
#include "mgr_log.h"

slots_info::slots_info()
{
	
}

slots_info::slots_info(const slots_info& other)
{
	if(this != &other) 
	{
		this->m_board_type = other.m_board_type;
		this->m_board_slot = other.m_board_slot;
	}
}

slots_info& slots_info::operator=(const slots_info& other)
{
	if(this != &other) 
	{
		this->m_board_type = other.m_board_type;
		this->m_board_slot = other.m_board_slot;
	}

	return *this;
}


slots_info::~slots_info()
{
	
}

cp_slots::cp_slots()
{

}

cp_slots::~cp_slots()
{

}


ft_slots::ft_slots()
{

}


ft_slots::~ft_slots()
{
	
}


mgr_slotsconfig::mgr_slotsconfig()
{

}

mgr_slotsconfig::~mgr_slotsconfig()
{

}

mgr_slotsconfig* mgr_slotsconfig::get_instance()
{
	static mgr_slotsconfig instance;
    return &instance;
}

bool mgr_slotsconfig::load_slots_config(const std::string& filename)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_slotsconfig::load_slots_config() filename=%s",filename.c_str());
    bool ret = false;

    //读取sys_slots.json
    const char* prog_path = xbasic::get_module_path();
    std::string json_file = std::string(prog_path)+filename;
    std::string json_str = xbasic::read_data_from_file(json_file);

    cJSON_object js_root(json_str);
    cJSON_object& js_sysslots = js_root["sys_slots"];
    int sysslots_sz = js_sysslots.get_array_size();
    if(sysslots_sz > 0)
    {
        for(int i = 0; i < sysslots_sz; i++)
        {
            cJSON_object slot_items = js_sysslots[i];
            std::string product = slot_items["product"];
            if(product == std::string("cp"))
            {
                load_cp_slots(slot_items);
            }
            else if(product == std::string("ft"))
            {
                load_ft_slots(slot_items);
            }
            else
            {
                LOG_MSG(ERR_LOG,"mgr_slotsconfig::load_slots_config() filename=%s sys_slots[i]=%d product=%s is invalid",filename.c_str(),i,product.c_str());
            }
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"mgr_slotsconfig::load_slots_config() filename=%s sysslots_sz=%d is invalid",filename.c_str(),sysslots_sz);
    }

    LOG_MSG(MSG_LOG,"Exited mgr_slotsconfig::load_slots_config() ret=%d",ret);
    return ret;
}

std::shared_ptr<cp_slots> mgr_slotsconfig::get_cp_slots(int sync_id)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_slotsconfig::get_cp_slots() sync_id=0x%x",sync_id);
    CP_SlotsMap::iterator iter = m_cp_slots.find(sync_id);
    if(iter != m_cp_slots.end())
    {
        return iter->second;
    }

    LOG_MSG(MSG_LOG,"Exited mgr_slotsconfig::get_cp_slots()");

    return std::shared_ptr<cp_slots>();
}


std::shared_ptr<ft_slots> mgr_slotsconfig::get_ft_slots(int sync_id)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_slotsconfig::get_ft_slots() sync_id=0x%x",sync_id);
    print_ftslots();
    FT_SlotsMap::iterator iter = m_ft_slots.find(sync_id);
    if(iter != m_ft_slots.end())
    {
        return iter->second;
    }

    LOG_MSG(MSG_LOG,"Exited mgr_slotsconfig::get_ft_slots()");

    return std::shared_ptr<ft_slots>();
}


bool mgr_slotsconfig::load_cp_slots(cJSON_object& slots_obj)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_slotsconfig::load_cp_slots()");
    bool ret = false;

    std::string product = slots_obj["product"];
    cJSON_object slots = slots_obj["slots"];
    int slots_sz = slots.get_array_size();

    if(slots_sz > 0)
    {
        slots_info sync_info;
        std::vector<slots_info> vect_pem_info;
        std::vector<slots_info> vect_dps_info;
        for(int i = 0; i < slots_sz; i++)
        {
            cJSON_object item = slots[i];
            std::string sync_slot = item["sync"];
            if(!sync_slot.empty())
            {
                sync_info.m_board_type = BOARDTYPE_CP_SYNC;
                sync_info.m_board_slot = atoi(sync_slot.c_str());
                LOG_MSG(MSG_LOG,"mgr_slotsconfig::load_cp_slots() sync_info.m_board_type=0x%x sync_info.m_board_slot=0x%x",sync_info.m_board_type,sync_info.m_board_slot);
            }

            cJSON_object pem_vect = item["pem"];
            int pem_vect_sz = pem_vect.get_array_size();
            for(int i = 0; i < pem_vect_sz; i++)
            {
                int slot = 0;
                pem_vect.get(i,slot);
                slots_info pem_info;
                pem_info.m_board_type = BOARDTYPE_CP_PEM;
                pem_info.m_board_slot = slot;
                LOG_MSG(MSG_LOG,"mgr_slotsconfig::load_cp_slots() pem_info.m_board_type=0x%x pem_info.m_board_slot=0x%x",pem_info.m_board_type,pem_info.m_board_slot);
                vect_pem_info.push_back(pem_info);
            }

            cJSON_object dps_vect = item["dps"];
            int dps_vect_sz = dps_vect.get_array_size();
            for(int i = 0; i < dps_vect_sz; i++)
            {
                int slot = 0;
                dps_vect.get(i,slot);
                slots_info dps_info;
                dps_info.m_board_type = BOARDTYPE_CP_DPS;
                dps_info.m_board_slot = slot;
                LOG_MSG(MSG_LOG,"mgr_slotsconfig::load_cp_slots() dps_info.m_board_type=0x%x dps_info.m_board_slot=0x%x",dps_info.m_board_type,dps_info.m_board_slot);
                vect_dps_info.push_back(dps_info);
            }

            int syncid = BORAID_VALUE(sync_info.m_board_type,sync_info.m_board_slot);

            //将相应信息保存到m_cp_slots中
            std::shared_ptr<cp_slots> new_slots(new cp_slots());
            new_slots->m_product_name = product;
            new_slots->m_sync_info = sync_info;
            for(auto pem : vect_pem_info)
            {
                std::shared_ptr<slots_info> new_pemslots(new slots_info());
                *new_pemslots = pem;
                new_slots->m_pem_info.push_back(new_pemslots);
            }

            for(auto dps : vect_dps_info)
            {
                std::shared_ptr<slots_info> new_dpsslots(new slots_info());
                *new_dpsslots = dps;
                new_slots->m_dps_info.push_back(new_dpsslots);
            }

            //加入到集合中
            m_cp_slots.insert(std::make_pair(syncid,new_slots));
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"mgr_slotsconfig::load_cp_slots() product=%s slots_sz=%d is invalid",product.c_str(),slots_sz);
    }

    int cpslots_sz = m_cp_slots.size();
    if(cpslots_sz > 0)
    {
        ret = true;
    }

    LOG_MSG(MSG_LOG,"Exited mgr_slotsconfig::load_cp_slots() slots_sz=%d cpslots_sz=%d ret=%d",slots_sz,cpslots_sz,ret);

    return ret;
}

bool mgr_slotsconfig::load_ft_slots(cJSON_object& slots_obj)
{
    LOG_MSG(MSG_LOG,"Enter into mgr_slotsconfig::load_ft_slots()");
    bool ret = false;

    std::string product = slots_obj["product"];
    cJSON_object slots = slots_obj["slots"];
    int slots_sz = slots.get_array_size();

    if(slots_sz > 0)
    {
        slots_info sync_info;
        std::vector<slots_info> vect_pgb_info;
        std::vector<slots_info> vect_asic_info;
        std::vector<slots_info> vect_pps_info;
        for(int i = 0; i < slots_sz; i++)
        {
            cJSON_object item = slots[i];
            std::string sync_slot = item["sync"];
            if(!sync_slot.empty())
            {
                sync_info.m_board_type = BOARDTYPE_FT_SYNC;
                sync_info.m_board_slot = atoi(sync_slot.c_str());
                LOG_MSG(MSG_LOG,"mgr_slotsconfig::load_ft_slots() sync_info.m_board_type=0x%x sync_info.m_board_slot=0x%x",sync_info.m_board_type,sync_info.m_board_slot);
            }

            cJSON_object pgb_vect = item["pgb"];
            int pgb_vect_sz = pgb_vect.get_array_size();
            for(int i = 0; i < pgb_vect_sz; i++)
            {
                int slot = 0;
                pgb_vect.get(i,slot);
                slots_info pgb_info;
                pgb_info.m_board_type = BOARDTYPE_FT_PGB;
                pgb_info.m_board_slot = slot;
                LOG_MSG(MSG_LOG,"mgr_slotsconfig::load_ft_slots() pgb_info.m_board_type=0x%x pgb_info.m_board_slot=0x%x",pgb_info.m_board_type,pgb_info.m_board_slot);
                vect_pgb_info.push_back(pgb_info);
            }

            cJSON_object asic_vect = item["asic"];
            int asic_vect_sz = asic_vect.get_array_size();
            for(int i = 0; i < asic_vect_sz; i++)
            {
                int slot = 0;
                asic_vect.get(i,slot);
                slots_info asic_info;
                asic_info.m_board_type = BOARDTYPE_asic;
                asic_info.m_board_slot = slot;
                LOG_MSG(MSG_LOG,"mgr_slotsconfig::load_ft_slots() asic_info.m_board_type=0x%x asic_info.m_board_slot=0x%x",asic_info.m_board_type,asic_info.m_board_slot);
                vect_asic_info.push_back(asic_info);
            }

            cJSON_object pps_vect = item["pps"];
            int pps_vect_sz = pps_vect.get_array_size();
            for(int i = 0; i < pps_vect_sz; i++)
            {
                int slot = 0;
                pps_vect.get(i,slot);
                slots_info pps_info;
                pps_info.m_board_type = BOARDTYPE_FT_PPS;
                pps_info.m_board_slot = slot;
                LOG_MSG(MSG_LOG,"mgr_slotsconfig::load_ft_slots() pps_info.m_board_type=0x%x pps_info.m_board_slot=0x%x",pps_info.m_board_type,pps_info.m_board_slot);
                vect_pps_info.push_back(pps_info);
            }

            int syncid = BORAID_VALUE(sync_info.m_board_type,sync_info.m_board_slot);

            //将相应信息保存到m_cp_slots中
            std::shared_ptr<ft_slots> new_slots(new ft_slots());
            new_slots->m_product_name = product;
            new_slots->m_sync_info = sync_info;
            for(auto pgb : vect_pgb_info)
            {
                std::shared_ptr<slots_info> new_pgbslots(new slots_info());
                *new_pgbslots = pgb;
                new_slots->m_pgb_info.push_back(new_pgbslots);
            }

            for(auto dps : vect_asic_info)
            {
                std::shared_ptr<slots_info> new_asicslots(new slots_info());
                *new_asicslots = dps;
                new_slots->m_asic_info.push_back(new_asicslots);
            }

            for(auto dps : vect_pps_info)
            {
                std::shared_ptr<slots_info> new_ppsslots(new slots_info());
                *new_ppsslots = dps;
                new_slots->m_pps_info.push_back(new_ppsslots);
            }

            //加入到集合中
            m_ft_slots.insert(std::make_pair(syncid,new_slots));
        }
    }
    else
    {
        LOG_MSG(ERR_LOG,"mgr_slotsconfig::load_ft_slots() product=%s slots_sz=%d is invalid",product.c_str(),slots_sz);
    }

    int ftslots_sz = m_ft_slots.size();
    if(ftslots_sz > 0)
    {
        ret = true;
    }

    LOG_MSG(MSG_LOG,"Exited mgr_slotsconfig::load_ft_slots() slots_sz=%d ftslots_sz=%d ret=%d",slots_sz,ftslots_sz,ret);
    return ret;
}

void mgr_slotsconfig::print_ftslots()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_slotsconfig::print_ftslots()");

    for(auto pair : m_ft_slots)
    {
        LOG_MSG(MSG_LOG,"==================begin mgr_slotsconfig::print_ftslots()==================");
        LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_ftslots() key=0x%x",pair.first);
        std::shared_ptr<ft_slots> ftslots = pair.second;
        LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_ftslots() m_product_name=%s",ftslots->m_product_name.c_str());
        LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_ftslots() m_sync_info.m_board_type=0x%x m_sync_info.m_board_slot=0x%x",ftslots->m_sync_info.m_board_type,ftslots->m_sync_info.m_board_slot);
        for(auto it1 : ftslots->m_pgb_info)
        {
            LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_ftslots() m_pgb_info.m_board_type=0x%x m_pgb_info.m_board_slot=0x%x",it1->m_board_type,it1->m_board_slot);
        }
        for(auto it2 : ftslots->m_pps_info)
        {
            LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_ftslots() m_pps_info.m_board_type=0x%x m_pps_info.m_board_slot=0x%x",it2->m_board_type,it2->m_board_slot);
        }
        for(auto it3 : ftslots->m_asic_info)
        {
            LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_ftslots() m_asic_info.m_board_type=0x%x m_asic_info.m_board_slot=0x%x",it3->m_board_type,it3->m_board_slot);
        }
        LOG_MSG(MSG_LOG,"==================end mgr_slotsconfig::print_ftslots()==================");
    }

    LOG_MSG(MSG_LOG,"Exited mgr_slotsconfig::print_ftslots()");
}

void mgr_slotsconfig::print_cpslots()
{
    LOG_MSG(MSG_LOG,"Enter into mgr_slotsconfig::print_cpslots()");

    for(auto pair : m_cp_slots)
    {
        LOG_MSG(MSG_LOG,"==================begin mgr_slotsconfig::print_cpslots()==================");
        LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_cpslots() key=0x%x",pair.first);
        std::shared_ptr<cp_slots> cpslots = pair.second;
        LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_cpslots() m_product_name=%s",cpslots->m_product_name.c_str());
        LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_cpslots() m_sync_info.m_board_type=0x%x m_sync_info.m_board_slot=0x%x",cpslots->m_sync_info.m_board_type,cpslots->m_sync_info.m_board_slot);
        for(auto it1 : cpslots->m_pem_info)
        {
            LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_cpslots() m_pem_info.m_board_type=0x%x m_pem_info.m_board_slot=0x%x",it1->m_board_type,it1->m_board_slot);
        }
        for(auto it2 : cpslots->m_dps_info)
        {
            LOG_MSG(MSG_LOG,"mgr_slotsconfig::print_cpslots() m_dps_info.m_board_type=0x%x m_dps_info.m_board_slot=0x%x",it2->m_board_type,it2->m_board_slot);
        }
        LOG_MSG(MSG_LOG,"==================end mgr_slotsconfig::print_cpslots()==================");
    }

    LOG_MSG(MSG_LOG,"Exited mgr_slotsconfig::print_cpslots()");
}
