#include "unit_data_caching.h"
#include "../generated/units_data.h"
#include "../generated/upgrades_data.h"
#include "../generated/abilities_data.h"
#include <string>
#include <sstream>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include "sc2_serialization.h"

using namespace std;
using namespace sc2;

void save_unit_data(const vector<UnitTypeData>& unit_types, string path) {
    auto stream = ofstream(path);
    stream << unit_types.size() << endl;
    for (const UnitTypeData& type : unit_types) {
        stream << type.unit_type_id << "\n";
        stream << '"' << type.name << "\"\n";
        stream << type.available << "\n";
        stream << type.cargo_size << "\n";
        stream << type.mineral_cost << "\n";
        stream << type.vespene_cost << "\n";
        stream << type.attributes.size() << "\n";
        for (auto a : type.attributes) {
            stream << (int)a << " ";
        }
        stream << endl;
        stream << type.movement_speed << "\n";
        stream << type.armor << "\n";

        stream << type.weapons.size() << endl;
        for (auto w : type.weapons) {
            stream << (int)w.type << " ";
            stream << w.damage_ << endl;
            stream << w.damage_bonus.size() << endl;
            for (auto b : w.damage_bonus) {
                stream << (int)b.attribute << " " << b.bonus;
            }
            stream << endl;
            stream << w.attacks << " ";
            stream << w.range << " ";
            stream << w.speed << endl;
        }
        stream << endl;

        stream << type.food_required << " ";
        stream << type.food_provided << " ";
        stream << type.ability_id << " ";
        stream << type.race << " ";
        stream << type.build_time << " ";
        stream << type.has_minerals << " ";
        stream << type.has_vespene << " ";
        stream << type.sight_range << " ";

        stream << type.tech_alias.size() << endl;
        for (auto a : type.tech_alias) {
            stream << a << " ";
        }
        stream << endl;

        stream << type.unit_alias << " ";
        stream << type.tech_requirement << " ";
        stream << type.require_attached << " ";
        stream << endl;
        stream << endl;
    }
    stream << endl;
    stream.close();
}

std::vector<sc2::UnitTypeData> load_unit_data() {
    // auto stream = ifstream(UNIT_DATA_CACHE_PATH);
	auto stream = stringstream(string((char*)LIBVOXELBOT_DATA_UNITS, LIBVOXELBOT_DATA_UNITS_SIZE));
    if (!stream) {
        cerr << "Could not open unit data cache file at " << UNIT_DATA_CACHE_PATH << endl;
        exit(1);
    }
    int nUnits;
    stream >> nUnits;
    vector<UnitTypeData> unit_types(nUnits);
    for (UnitTypeData& type : unit_types) {
        int type_id;
        stream >> type_id;
        type.unit_type_id = (UNIT_TYPEID)type_id;
        stream.ignore(numeric_limits<streamsize>::max(), '"');
        getline(stream, type.name, '"');
        stream >> type.available;
        stream >> type.cargo_size;
        stream >> type.mineral_cost;
        stream >> type.vespene_cost;
        int nAttributes;
        stream >> nAttributes;
        type.attributes = vector<Attribute>(nAttributes);
        for (auto& a : type.attributes) {
            int v;
            stream >> v;
            a = (Attribute)v;
        }
        stream >> type.movement_speed;
        stream >> type.armor;

        int nWeapons;
        stream >> nWeapons;
        type.weapons = vector<Weapon>(nWeapons);
        for (auto& w : type.weapons) {
            int typeI;
            stream >> typeI;
            w.type = (Weapon::TargetType)typeI;
            stream >> w.damage_;
            int nBonus;
            stream >> nBonus;
            w.damage_bonus = vector<DamageBonus>(nBonus);
            for (auto& b : w.damage_bonus) {
                int attr;
                stream >> attr >> b.bonus;
                b.attribute = (Attribute)attr;
            }
            stream >> w.attacks;
            stream >> w.range;
            stream >> w.speed;
        }

        stream >> type.food_required;
        stream >> type.food_provided;
        int id, race;
        stream >> id;
        type.ability_id = (ABILITY_ID)id;
        stream >> race;
        type.race = (Race)race;
        stream >> type.build_time;
        stream >> type.has_minerals;
        stream >> type.has_vespene;
        stream >> type.sight_range;

        int nTech;
        stream >> nTech;
        type.tech_alias = vector<UnitTypeID>(nTech);
        for (auto& a : type.tech_alias) {
            int v;
            stream >> v;
            a = (UNIT_TYPEID)v;
        }

        int unit_alias;
        stream >> unit_alias;
        type.unit_alias = (UNIT_TYPEID)unit_alias;
        int tech_req;
        stream >> tech_req;
        type.tech_requirement = (UNIT_TYPEID)tech_req;
        stream >> type.require_attached;
    }
    //stream.close();

    // Some sanity checks
    assert(unit_types[(int)UNIT_TYPEID::ZERG_OVERLORD].food_provided == 8);
    assert(unit_types[(int)UNIT_TYPEID::TERRAN_SCV].mineral_cost == 50);
    assert(unit_types[(int)UNIT_TYPEID::ZERG_ROACH].ability_id == ABILITY_ID::TRAIN_ROACH);
    return unit_types;
}

struct SerializableAbility {
    bool available;
    int ability_id;
    std::string link_name;
    uint32_t link_index;
    std::string button_name;
    std::string friendly_name;
    std::string hotkey;
    uint32_t remaps_to_ability_id;
    std::vector<uint32_t> remaps_from_ability_id;
    AbilityData::Target target;
    bool allow_minimap;
    bool allow_autocast;
    bool is_building;
    float footprint_radius;
    bool is_instant_placement;
    float cast_range;

    template<class Archive>
    void serialize(Archive & archive) {
        archive(
            available,
            ability_id,
            link_name,
            link_index,
            button_name,
            friendly_name,
            hotkey,
            remaps_to_ability_id,
            remaps_from_ability_id,
            target,
            allow_minimap,
            allow_autocast,
            is_building,
            footprint_radius,
            is_instant_placement,
            cast_range
        );
    }
};

void save_ability_data(vector<AbilityData> abilities) {
    vector<SerializableAbility> abilities2;
    for (auto& a : abilities) {
        SerializableAbility a2;

        a2.available = a.available;
        a2.ability_id = a.ability_id;
        a2.link_name = a.link_name;
        a2.link_index = a.link_index;
        a2.button_name = a.button_name;
        a2.friendly_name = a.friendly_name;
        a2.hotkey = a.hotkey;
        a2.remaps_to_ability_id = a.remaps_to_ability_id;
        a2.remaps_from_ability_id = a.remaps_from_ability_id;
        a2.target = a.target;
        a2.allow_minimap = a.allow_minimap;
        a2.allow_autocast = a.allow_autocast;
        a2.is_building = a.is_building;
        a2.footprint_radius = a.footprint_radius;
        a2.is_instant_placement = a.is_instant_placement;
        a2.cast_range = a.cast_range;
        abilities2.push_back(a2);
    }

    std::ofstream file(ABILITY_DATA_CACHE_PATH, std::ios::binary);
    {
        cereal::BinaryOutputArchive archive(file);
        archive(abilities2);
		cout << "Serialize abilities" << endl;
    }
    file.close();
}

std::vector<sc2::AbilityData> load_ability_data() {
	auto file = stringstream(string((char*)LIBVOXELBOT_DATA_ABILITIES, LIBVOXELBOT_DATA_ABILITIES_SIZE));
    // std::ifstream file(ABILITY_DATA_CACHE_PATH, std::ios::binary);
    if (!file) {
        cerr << "Could not open unit data cache file at " << ABILITY_DATA_CACHE_PATH << endl;
        exit(1);
    }

    vector<SerializableAbility> abilities2;
    {
        cereal::BinaryInputArchive archive(file);
        archive(abilities2);
		cout << "Deserialize abilities" << endl;
    }
    //file.close();

    vector<AbilityData> abilities;
    for (auto& a : abilities2) {
        AbilityData a2;

        a2.available = a.available;
        a2.ability_id = a.ability_id;
        a2.link_name = a.link_name;
        a2.link_index = a.link_index;
        a2.button_name = a.button_name;
        a2.friendly_name = a.friendly_name;
        a2.hotkey = a.hotkey;
        a2.remaps_to_ability_id = a.remaps_to_ability_id;
        a2.remaps_from_ability_id = a.remaps_from_ability_id;
        a2.target = a.target;
        a2.allow_minimap = a.allow_minimap;
        a2.allow_autocast = a.allow_autocast;
        a2.is_building = a.is_building;
        a2.footprint_radius = a.footprint_radius;
        a2.is_instant_placement = a.is_instant_placement;
        a2.cast_range = a.cast_range;
        abilities.push_back(a2);
    }

    return abilities;
}

void save_upgrade_data(vector<UpgradeData> upgrades) {
    std::ofstream file(UPGRADE_DATA_CACHE_PATH, std::ios::binary);
    {
        cereal::BinaryOutputArchive archive(file);
        archive(upgrades);
		cout << "Serialize upgrades" << endl;
    }
    file.close();
}


std::vector<sc2::UpgradeData> load_upgrade_data() {
	auto file = stringstream(string((char*)LIBVOXELBOT_DATA_UPGRADES, LIBVOXELBOT_DATA_UPGRADES_SIZE));
    // std::ifstream file(UPGRADE_DATA_CACHE_PATH, std::ios::binary);
    if (!file) {
        cerr << "Could not open upgrade data cache file at " << UPGRADE_DATA_CACHE_PATH << endl;
        exit(1);
    }

    vector<sc2::UpgradeData> upgrades;
    {
        cereal::BinaryInputArchive archive(file);
        archive(upgrades);
		cout << "Deserialize upgrades" << endl;
    }
    //file.close();
    return upgrades;
}
