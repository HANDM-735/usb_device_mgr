#ifndef MGR_SLOTS_CONFIG_H
#define MGR_SLOTS_CONFIG_H

#include "xconfig.h"
#include "CJsonObject.h"
#include "mgr_usb_def.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>


class slots_info;
typedef std::vector<std::shared_ptr<slots_info> > SlotVector;
class slots_info
{
public:
    slots_info();
    slots_info(const slots_info& other);
    slots_info& operator=(const slots_info& other);
    ~slots_info();

public:
    int         m_board_type;
    int         m_board_slot;
};

class cp_slots
{
public:
    cp_slots();
    ~cp_slots();

public:
    std::string     m_product_name;
    slots_info      m_sync_info;
    SlotVector      m_ppm_info;
    SlotVector      m_dps_info;
};

class ft_slots
{
public:
    ft_slots();
    ~ft_slots();

public:

    std::string     m_product_name;
    slots_info      m_sync_info;
    SlotVector      m_ppb_info;
    SlotVector      m_pps_info;
    SlotVector      m_asic_info;
};

class mgr_slotsconfig
{
public:
    typedef std::unordered_map<int, std::shared_ptr<cp_slots> > CP_SlotsMap;
    typedef std::unordered_map<int, std::shared_ptr<ft_slots> > FT_SlotsMap;

public:
    mgr_slotsconfig();
    ~mgr_slotsconfig();

public:
    mgr_slotsconfig(const mgr_slotsconfig&) = delete;
    mgr_slotsconfig& operator=(const mgr_slotsconfig&) = delete;
    static mgr_slotsconfig* get_instance();

public:
    bool load_slots_config(const std::string& filename);
    std::shared_ptr<cp_slots> get_cp_slots(int sync_id);
    std::shared_ptr<ft_slots> get_ft_slots(int sync_id);
private:
    bool load_cp_slots(cjson_object& slots_obj);
    bool load_ft_slots(cjson_object& slots_obj);

    void print_ft_slots();
    void print_cp_slots();
public:
    CP_SlotsMap     m_cp_slots;
    FT_SlotsMap     m_ft_slots;
};

#endif
