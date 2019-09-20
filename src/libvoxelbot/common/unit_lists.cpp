#include "unit_lists.h"
#include "../utilities/mappings.h"

using namespace std;
using namespace sc2;

AvailableUnitTypes::AvailableUnitTypes(std::initializer_list<BuildOrderItem> types) {
    int maxIndex = 0;
    int maxUpgrade = 0;
    for (auto item : types) {
        index2item.push_back(item);
        if (item.isUnitType()) {
            maxIndex = std::max(maxIndex, (int)item.typeID());
            unitTypes.push_back(item.typeID());
        } else {
            maxUpgrade = std::max(maxUpgrade, (int)item.upgradeID());
        }
    }

    type2index = std::vector<int>(maxIndex + 1, -1);
    upgrade2index = std::vector<int>(maxUpgrade + 1, -1);
    for (size_t i = 0; i < index2item.size(); i++) {
        if (index2item[i].isUnitType()) {
            type2index[(int)index2item[i].typeID()] = i;
        } else {
            upgrade2index[(int)index2item[i].upgradeID()] = i;
        }
        arbitraryType2index[(int)index2item[i].rawType()] = i;
    }
    
}

bool AvailableUnitTypes::canBeChronoBoosted (int index) const {
    assert((size_t)index < index2item.size());
    auto& item = index2item[index];

    // Upgrades can always be chrono boosted
    if (!item.isUnitType()) return true;

    auto unitType = item.typeID();

    // Only allow chrono boosting non-structures that are built in structures
    return !isStructure(unitType) && isStructure(abilityToCasterUnit(getUnitData(unitType).ability_id)[0]);
}

static AvailableUnitTypes unitTypesTerranBuildOrder = {
    BuildOrderItem(UNIT_TYPEID::TERRAN_ARMORY),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BANSHEE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BARRACKS),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BARRACKSREACTOR),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BARRACKSTECHLAB),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BATTLECRUISER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BUNKER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_COMMANDCENTER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_CYCLONE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_ENGINEERINGBAY),
    BuildOrderItem(UNIT_TYPEID::TERRAN_FACTORY),
    BuildOrderItem(UNIT_TYPEID::TERRAN_FACTORYREACTOR),
    BuildOrderItem(UNIT_TYPEID::TERRAN_FACTORYTECHLAB),
    BuildOrderItem(UNIT_TYPEID::TERRAN_FUSIONCORE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_GHOST),
    BuildOrderItem(UNIT_TYPEID::TERRAN_GHOSTACADEMY),
    BuildOrderItem(UNIT_TYPEID::TERRAN_HELLION),
    BuildOrderItem(UNIT_TYPEID::TERRAN_HELLIONTANK),
    BuildOrderItem(UNIT_TYPEID::TERRAN_LIBERATOR),
    BuildOrderItem(UNIT_TYPEID::TERRAN_MARAUDER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_MARINE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_MEDIVAC),
    BuildOrderItem(UNIT_TYPEID::TERRAN_MISSILETURRET),
    BuildOrderItem(UNIT_TYPEID::TERRAN_MULE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_ORBITALCOMMAND),
    BuildOrderItem(UNIT_TYPEID::TERRAN_PLANETARYFORTRESS),
    BuildOrderItem(UNIT_TYPEID::TERRAN_RAVEN),
    BuildOrderItem(UNIT_TYPEID::TERRAN_REAPER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_REFINERY),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SCV),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SENSORTOWER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SIEGETANK),
    BuildOrderItem(UNIT_TYPEID::TERRAN_STARPORT),
    BuildOrderItem(UNIT_TYPEID::TERRAN_STARPORTREACTOR),
    BuildOrderItem(UNIT_TYPEID::TERRAN_STARPORTTECHLAB),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SUPPLYDEPOT),
    BuildOrderItem(UNIT_TYPEID::TERRAN_THOR),
    BuildOrderItem(UNIT_TYPEID::TERRAN_VIKINGFIGHTER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_WIDOWMINE),
};

const AvailableUnitTypes unitTypesProtossBuildOrder = {
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ADEPT),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_ADEPTPHASESHIFT),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_ARCHON, // TODO: Special case creation rul)e
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ASSIMILATOR),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_CARRIER),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_COLOSSUS),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_DARKSHRINE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_DARKTEMPLAR),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_DISRUPTOR),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_DISRUPTORPHASED),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_FLEETBEACON),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_FORGE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_GATEWAY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_HIGHTEMPLAR),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_IMMORTAL),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_INTERCEPTOR),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_MOTHERSHIP),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_MOTHERSHIPCORE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_NEXUS),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_OBSERVER),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ORACLE),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_ORACLESTASISTRAP),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PHOENIX),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PHOTONCANNON),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PROBE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PYLON),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_PYLONOVERCHARGED),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ROBOTICSBAY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_SENTRY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_SHIELDBATTERY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_STALKER),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_STARGATE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_TEMPEST),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_VOIDRAY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_WARPGATE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_WARPPRISM),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_WARPPRISMPHASING),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ZEALOT),


    // Upgrades
    BuildOrderItem(UPGRADE_ID::WARPGATERESEARCH),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL3),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRARMORSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRARMORSLEVEL3),
    BuildOrderItem(UPGRADE_ID::PROTOSSSHIELDSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSSHIELDSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSSHIELDSLEVEL3),
    BuildOrderItem(UPGRADE_ID::ADEPTPIERCINGATTACK),
    BuildOrderItem(UPGRADE_ID::CHARGE),
    BuildOrderItem(UPGRADE_ID::EXTENDEDTHERMALLANCE),
};

const AvailableUnitTypes unitTypesZergBuildOrder = {
    BuildOrderItem(UNIT_TYPEID::ZERG_BANELING),
    BuildOrderItem(UNIT_TYPEID::ZERG_BANELINGNEST),
    BuildOrderItem(UNIT_TYPEID::ZERG_BROODLORD),
    BuildOrderItem(UNIT_TYPEID::ZERG_CORRUPTOR),
    BuildOrderItem(UNIT_TYPEID::ZERG_DRONE),
    BuildOrderItem(UNIT_TYPEID::ZERG_EVOLUTIONCHAMBER),
    BuildOrderItem(UNIT_TYPEID::ZERG_EXTRACTOR),
    BuildOrderItem(UNIT_TYPEID::ZERG_GREATERSPIRE),
    BuildOrderItem(UNIT_TYPEID::ZERG_HATCHERY),
    BuildOrderItem(UNIT_TYPEID::ZERG_HIVE),
    BuildOrderItem(UNIT_TYPEID::ZERG_HYDRALISK),
    BuildOrderItem(UNIT_TYPEID::ZERG_HYDRALISKDEN),
    BuildOrderItem(UNIT_TYPEID::ZERG_INFESTATIONPIT),
    BuildOrderItem(UNIT_TYPEID::ZERG_INFESTOR),
    BuildOrderItem(UNIT_TYPEID::ZERG_LAIR),
    BuildOrderItem(UNIT_TYPEID::ZERG_LURKERDENMP),
    // BuildOrderItem(UNIT_TYPEID::ZERG_LURKERMP),
    BuildOrderItem(UNIT_TYPEID::ZERG_MUTALISK),
    BuildOrderItem(UNIT_TYPEID::ZERG_NYDUSCANAL),
    BuildOrderItem(UNIT_TYPEID::ZERG_NYDUSNETWORK),
    BuildOrderItem(UNIT_TYPEID::ZERG_OVERLORD),
    BuildOrderItem(UNIT_TYPEID::ZERG_OVERLORDTRANSPORT),
    BuildOrderItem(UNIT_TYPEID::ZERG_OVERSEER),
    BuildOrderItem(UNIT_TYPEID::ZERG_QUEEN),
    BuildOrderItem(UNIT_TYPEID::ZERG_RAVAGER),
    BuildOrderItem(UNIT_TYPEID::ZERG_ROACH),
    BuildOrderItem(UNIT_TYPEID::ZERG_ROACHWARREN),
    BuildOrderItem(UNIT_TYPEID::ZERG_SPAWNINGPOOL),
    BuildOrderItem(UNIT_TYPEID::ZERG_SPINECRAWLER),
    BuildOrderItem(UNIT_TYPEID::ZERG_SPIRE),
    BuildOrderItem(UNIT_TYPEID::ZERG_SPORECRAWLER),
    BuildOrderItem(UNIT_TYPEID::ZERG_SWARMHOSTMP),
    BuildOrderItem(UNIT_TYPEID::ZERG_ULTRALISK),
    BuildOrderItem(UNIT_TYPEID::ZERG_ULTRALISKCAVERN),
    BuildOrderItem(UNIT_TYPEID::ZERG_VIPER),
    BuildOrderItem(UNIT_TYPEID::ZERG_ZERGLING),
};


static AvailableUnitTypes unitTypesTerranEconomic = {
    BuildOrderItem(UNIT_TYPEID::TERRAN_ARMORY),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BARRACKS),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BARRACKSREACTOR),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BARRACKSTECHLAB),
    BuildOrderItem(UNIT_TYPEID::TERRAN_COMMANDCENTER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_ENGINEERINGBAY),
    BuildOrderItem(UNIT_TYPEID::TERRAN_FACTORY),
    BuildOrderItem(UNIT_TYPEID::TERRAN_FACTORYREACTOR),
    BuildOrderItem(UNIT_TYPEID::TERRAN_FACTORYTECHLAB),
    BuildOrderItem(UNIT_TYPEID::TERRAN_FUSIONCORE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_GHOSTACADEMY),
    // BuildOrderItem(UNIT_TYPEID::TERRAN_MULE), // Mules are not built in a build order
    BuildOrderItem(UNIT_TYPEID::TERRAN_ORBITALCOMMAND),
    BuildOrderItem(UNIT_TYPEID::TERRAN_PLANETARYFORTRESS),
    BuildOrderItem(UNIT_TYPEID::TERRAN_REFINERY),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SCV),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SCV),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SCV),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SCV),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SCV),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SCV),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SCV),
    BuildOrderItem(UNIT_TYPEID::TERRAN_STARPORT),
    BuildOrderItem(UNIT_TYPEID::TERRAN_STARPORTREACTOR),
    BuildOrderItem(UNIT_TYPEID::TERRAN_STARPORTTECHLAB),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SUPPLYDEPOT),
};


static AvailableUnitTypes unitTypesProtossEconomic = {
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ASSIMILATOR),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_DARKSHRINE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_FLEETBEACON),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_FORGE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_GATEWAY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_NEXUS),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_NEXUS),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PROBE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PROBE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PROBE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PROBE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PROBE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PROBE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PYLON),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ROBOTICSBAY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_STARGATE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_WARPGATE), // Note: Warpgates will automatically be transitioned to in the build order optimizer if the warpgate research is done

    BuildOrderItem(UPGRADE_ID::WARPGATERESEARCH),
};

static AvailableUnitTypes unitTypesZergEconomic = {
    BuildOrderItem(UNIT_TYPEID::ZERG_BANELINGNEST),
    BuildOrderItem(UNIT_TYPEID::ZERG_DRONE),
    BuildOrderItem(UNIT_TYPEID::ZERG_DRONE),
    BuildOrderItem(UNIT_TYPEID::ZERG_DRONE),
    BuildOrderItem(UNIT_TYPEID::ZERG_DRONE),
    BuildOrderItem(UNIT_TYPEID::ZERG_DRONE),
    BuildOrderItem(UNIT_TYPEID::ZERG_EVOLUTIONCHAMBER),
    BuildOrderItem(UNIT_TYPEID::ZERG_EXTRACTOR),
    BuildOrderItem(UNIT_TYPEID::ZERG_GREATERSPIRE),
    BuildOrderItem(UNIT_TYPEID::ZERG_HATCHERY),
    BuildOrderItem(UNIT_TYPEID::ZERG_HIVE),
    BuildOrderItem(UNIT_TYPEID::ZERG_HYDRALISKDEN),
    BuildOrderItem(UNIT_TYPEID::ZERG_INFESTATIONPIT),
    BuildOrderItem(UNIT_TYPEID::ZERG_LAIR),
    BuildOrderItem(UNIT_TYPEID::ZERG_LURKERDENMP),
    BuildOrderItem(UNIT_TYPEID::ZERG_NYDUSCANAL),
    BuildOrderItem(UNIT_TYPEID::ZERG_NYDUSNETWORK),
    BuildOrderItem(UNIT_TYPEID::ZERG_QUEEN),
    BuildOrderItem(UNIT_TYPEID::ZERG_SPAWNINGPOOL),
    BuildOrderItem(UNIT_TYPEID::ZERG_SPIRE),
    BuildOrderItem(UNIT_TYPEID::ZERG_ULTRALISKCAVERN),
};

const AvailableUnitTypes unitTypesTerranCombat = {
    BuildOrderItem(UNIT_TYPEID::TERRAN_LIBERATOR),
    // BuildOrderItem(UNIT_TYPEID::TERRAN_BATTLECRUISER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_BANSHEE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_CYCLONE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_GHOST),
    BuildOrderItem(UNIT_TYPEID::TERRAN_HELLION),
    BuildOrderItem(UNIT_TYPEID::TERRAN_HELLIONTANK),
    BuildOrderItem(UNIT_TYPEID::TERRAN_MARAUDER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_MARINE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_MEDIVAC),
    // BuildOrderItem(UNIT_TYPEID::TERRAN_MISSILETURRET),
    // BuildOrderItem(UNIT_TYPEID::TERRAN_MULE),
    BuildOrderItem(UNIT_TYPEID::TERRAN_RAVEN),
    BuildOrderItem(UNIT_TYPEID::TERRAN_REAPER),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SCV),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SIEGETANK),
    BuildOrderItem(UNIT_TYPEID::TERRAN_SIEGETANKSIEGED),
    BuildOrderItem(UNIT_TYPEID::TERRAN_THOR),
    BuildOrderItem(UNIT_TYPEID::TERRAN_THORAP),
    BuildOrderItem(UNIT_TYPEID::TERRAN_VIKINGASSAULT),
    BuildOrderItem(UNIT_TYPEID::TERRAN_VIKINGFIGHTER),
    // BuildOrderItem(UNIT_TYPEID::TERRAN_WIDOWMINE),
    // BuildOrderItem(UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED),
};

const AvailableUnitTypes unitTypesProtossCombat = {
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ADEPT),
    // Archon needs special rules before it is supported by the build optimizer
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ARCHON),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_CARRIER),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_COLOSSUS),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_DARKTEMPLAR),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_DISRUPTOR),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_HIGHTEMPLAR),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_IMMORTAL),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_MOTHERSHIP),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_OBSERVER),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_ORACLE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_PHOENIX),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_PROBE),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_SENTRY),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_STALKER),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_TEMPEST),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_VOIDRAY),
    // BuildOrderItem(UNIT_TYPEID::PROTOSS_WARPPRISM),
    BuildOrderItem(UNIT_TYPEID::PROTOSS_ZEALOT),
    
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSSHIELDSLEVEL1),
    BuildOrderItem(UPGRADE_ID::ADEPTPIERCINGATTACK),
    // BuildOrderItem(UPGRADE_ID::CHARGE),
    BuildOrderItem(UPGRADE_ID::EXTENDEDTHERMALLANCE),
};

const AvailableUnitTypes unitTypesZergCombat = {
    BuildOrderItem(UNIT_TYPEID::ZERG_BANELING),
    BuildOrderItem(UNIT_TYPEID::ZERG_BROODLORD),
    BuildOrderItem(UNIT_TYPEID::ZERG_CORRUPTOR),
    BuildOrderItem(UNIT_TYPEID::ZERG_DRONE),
    BuildOrderItem(UNIT_TYPEID::ZERG_HYDRALISK),
    BuildOrderItem(UNIT_TYPEID::ZERG_INFESTOR),
    // BuildOrderItem(UNIT_TYPEID::ZERG_INFESTORTERRAN),
    // BuildOrderItem(UNIT_TYPEID::ZERG_LOCUSTMP),
    // BuildOrderItem(UNIT_TYPEID::ZERG_LOCUSTMPFLYING),
    BuildOrderItem(UNIT_TYPEID::ZERG_LURKERMP),
    BuildOrderItem(UNIT_TYPEID::ZERG_MUTALISK),
    BuildOrderItem(UNIT_TYPEID::ZERG_OVERSEER),
    BuildOrderItem(UNIT_TYPEID::ZERG_QUEEN),
    BuildOrderItem(UNIT_TYPEID::ZERG_RAVAGER),
    BuildOrderItem(UNIT_TYPEID::ZERG_ROACH),
    // BuildOrderItem(UNIT_TYPEID::ZERG_SPINECRAWLER),
    // BuildOrderItem(UNIT_TYPEID::ZERG_SPORECRAWLER),
    BuildOrderItem(UNIT_TYPEID::ZERG_SWARMHOSTMP),
    BuildOrderItem(UNIT_TYPEID::ZERG_ULTRALISK),
    BuildOrderItem(UNIT_TYPEID::ZERG_VIPER),
    BuildOrderItem(UNIT_TYPEID::ZERG_ZERGLING),
};


const AvailableUnitTypes& getAvailableUnitsForRace (Race race) {
    return race == Race::Terran ? unitTypesTerranBuildOrder : (race == Race::Protoss ? unitTypesProtossBuildOrder : unitTypesZergBuildOrder);
}

const AvailableUnitTypes& getAvailableUnitsForRace (Race race, UnitCategory category) {
    switch(category) {
        case UnitCategory::BuildOrderOptions:
            return race == Race::Terran ? unitTypesTerranBuildOrder : (race == Race::Protoss ? unitTypesProtossBuildOrder : unitTypesZergBuildOrder);
        case UnitCategory::Economic:
            return race == Race::Terran ? unitTypesTerranEconomic : (race == Race::Protoss ? unitTypesProtossEconomic : unitTypesZergEconomic);
        case UnitCategory::ArmyCompositionOptions:
            return race == Race::Terran ? unitTypesTerranCombat : (race == Race::Protoss ? unitTypesProtossCombat : unitTypesZergCombat);
        default:
            assert(false);
    }
}

// All possible upgrades
// This is primarily used to provide a mapping from upgrade IDs to compact indices (0...numUpgrades) used for some algorithms
const AvailableUnitTypes availableUpgrades = {
    BuildOrderItem(UPGRADE_ID::CARRIERLAUNCHSPEEDUPGRADE),
    BuildOrderItem(UPGRADE_ID::GLIALRECONSTITUTION),
    BuildOrderItem(UPGRADE_ID::TUNNELINGCLAWS),
    BuildOrderItem(UPGRADE_ID::CHITINOUSPLATING),
    BuildOrderItem(UPGRADE_ID::HISECAUTOTRACKING),
    BuildOrderItem(UPGRADE_ID::TERRANBUILDINGARMOR),
    BuildOrderItem(UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::NEOSTEELFRAME),
    BuildOrderItem(UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::TERRANINFANTRYARMORSLEVEL2),
    BuildOrderItem(UPGRADE_ID::TERRANINFANTRYARMORSLEVEL3),
    BuildOrderItem(UPGRADE_ID::STIMPACK),
    BuildOrderItem(UPGRADE_ID::SHIELDWALL),
    BuildOrderItem(UPGRADE_ID::PUNISHERGRENADES),
    BuildOrderItem(UPGRADE_ID::HIGHCAPACITYBARRELS),
    BuildOrderItem(UPGRADE_ID::BANSHEECLOAK),
    BuildOrderItem(UPGRADE_ID::RAVENCORVIDREACTOR),
    BuildOrderItem(UPGRADE_ID::PERSONALCLOAKING),
    BuildOrderItem(UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::TERRANSHIPWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::TERRANSHIPWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::TERRANSHIPWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL3),
    BuildOrderItem(UPGRADE_ID::PROTOSSSHIELDSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSSHIELDSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSSHIELDSLEVEL3),
    BuildOrderItem(UPGRADE_ID::OBSERVERGRAVITICBOOSTER),
    BuildOrderItem(UPGRADE_ID::GRAVITICDRIVE),
    BuildOrderItem(UPGRADE_ID::EXTENDEDTHERMALLANCE),
    BuildOrderItem(UPGRADE_ID::PSISTORMTECH),
    BuildOrderItem(UPGRADE_ID::ZERGMELEEWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::ZERGMELEEWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::ZERGMELEEWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::ZERGGROUNDARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::ZERGGROUNDARMORSLEVEL2),
    BuildOrderItem(UPGRADE_ID::ZERGGROUNDARMORSLEVEL3),
    BuildOrderItem(UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::OVERLORDSPEED),
    BuildOrderItem(UPGRADE_ID::BURROW),
    BuildOrderItem(UPGRADE_ID::ZERGLINGATTACKSPEED),
    BuildOrderItem(UPGRADE_ID::ZERGLINGMOVEMENTSPEED),
    BuildOrderItem(UPGRADE_ID::ZERGFLYERWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::ZERGFLYERWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::ZERGFLYERWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::ZERGFLYERARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::ZERGFLYERARMORSLEVEL2),
    BuildOrderItem(UPGRADE_ID::ZERGFLYERARMORSLEVEL3),
    BuildOrderItem(UPGRADE_ID::INFESTORENERGYUPGRADE),
    BuildOrderItem(UPGRADE_ID::CENTRIFICALHOOKS),
    BuildOrderItem(UPGRADE_ID::BATTLECRUISERENABLESPECIALIZATIONS),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL3),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRARMORSLEVEL2),
    BuildOrderItem(UPGRADE_ID::PROTOSSAIRARMORSLEVEL3),
    BuildOrderItem(UPGRADE_ID::WARPGATERESEARCH),
    BuildOrderItem(UPGRADE_ID::CHARGE),
    BuildOrderItem(UPGRADE_ID::BLINKTECH),
    BuildOrderItem(UPGRADE_ID::PHOENIXRANGEUPGRADE),
    BuildOrderItem(UPGRADE_ID::NEURALPARASITE),
    BuildOrderItem(UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL1),
    BuildOrderItem(UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL2),
    BuildOrderItem(UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL3),
    BuildOrderItem(UPGRADE_ID::DRILLCLAWS),
    BuildOrderItem(UPGRADE_ID::ADEPTPIERCINGATTACK),
    BuildOrderItem(UPGRADE_ID::MAGFIELDLAUNCHERS),
    BuildOrderItem(UPGRADE_ID::EVOLVEGROOVEDSPINES),
    BuildOrderItem(UPGRADE_ID::EVOLVEMUSCULARAUGMENTS),
    BuildOrderItem(UPGRADE_ID::BANSHEESPEED),
    BuildOrderItem(UPGRADE_ID::RAVENRECALIBRATEDEXPLOSIVES),
    BuildOrderItem(UPGRADE_ID::MEDIVACINCREASESPEEDBOOST),
    BuildOrderItem(UPGRADE_ID::LIBERATORAGRANGEUPGRADE),
    BuildOrderItem(UPGRADE_ID::DARKTEMPLARBLINKUPGRADE),
    BuildOrderItem(UPGRADE_ID::SMARTSERVOS),
    BuildOrderItem(UPGRADE_ID::RAPIDFIRELAUNCHERS),
    BuildOrderItem(UPGRADE_ID::ENHANCEDMUNITIONS),
};
