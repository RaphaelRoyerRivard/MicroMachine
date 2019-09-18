#include "sc2api/sc2_api.h"
#include <iostream>
#include <fstream>
#include <vector>

const std::string UNIT_DATA_CACHE_PATH = "sc2-libvoxelbot/libvoxelbot/generated/units.data";
const std::string UPGRADE_DATA_CACHE_PATH = "sc2-libvoxelbot/libvoxelbot/generated/upgrades.bin";
const std::string ABILITY_DATA_CACHE_PATH = "sc2-libvoxelbot/libvoxelbot/generated/abilities.bin";

void save_unit_data(const std::vector<sc2::UnitTypeData>& unit_types, std::string path=UNIT_DATA_CACHE_PATH);
std::vector<sc2::UnitTypeData> load_unit_data();
void save_ability_data(std::vector<sc2::AbilityData> abilities);
void save_upgrade_data(std::vector<sc2::UpgradeData> upgrades);
std::vector<sc2::AbilityData> load_ability_data();
std::vector<sc2::UpgradeData> load_upgrade_data();