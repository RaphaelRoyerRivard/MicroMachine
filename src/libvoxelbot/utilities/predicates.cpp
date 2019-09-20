#include "predicates.h"
#include "mappings.h"

using namespace sc2;

bool IsAttackable::operator()(const Unit& unit) {
    switch (unit.unit_type.ToType()) {
        case UNIT_TYPEID::ZERG_OVERLORD:
            return false;
        case UNIT_TYPEID::ZERG_OVERSEER:
            return false;
        case UNIT_TYPEID::PROTOSS_OBSERVER:
            return false;
        default:
            return true;
    }
}

bool IsFlying::operator()(const Unit& unit) {
    return unit.is_flying;
}

bool IsArmy::operator()(const Unit& unit) {
    auto attributes = getUnitData(unit.unit_type).attributes;
    for (const auto& attribute : attributes) {
        if (attribute == Attribute::Structure) {
            return false;
        }
    }
    switch (unit.unit_type.ToType()) {
        case UNIT_TYPEID::ZERG_OVERLORD:
            return false;
        case UNIT_TYPEID::PROTOSS_PROBE:
            return false;
        case UNIT_TYPEID::ZERG_DRONE:
            return false;
        case UNIT_TYPEID::TERRAN_SCV:
            return false;
        case UNIT_TYPEID::ZERG_QUEEN:
            return false;
        case UNIT_TYPEID::ZERG_LARVA:
            return false;
        case UNIT_TYPEID::ZERG_EGG:
            return false;
        case UNIT_TYPEID::TERRAN_MULE:
            return false;
        case UNIT_TYPEID::TERRAN_NUKE:
            return false;
        default:
            return true;
    }
}

bool isArmy(UNIT_TYPEID type) {
    auto& unitData = getUnitData(type);
    auto attributes = unitData.attributes;
    for (const auto& attribute : attributes) {
        if (attribute == Attribute::Structure) {
            return false;
        }
    }
    switch (type) {
        case UNIT_TYPEID::ZERG_OVERLORD:
            return false;
        case UNIT_TYPEID::PROTOSS_PROBE:
            return false;
        case UNIT_TYPEID::ZERG_DRONE:
            return false;
        case UNIT_TYPEID::TERRAN_SCV:
            return false;
        case UNIT_TYPEID::ZERG_QUEEN:
            return false;
        case UNIT_TYPEID::ZERG_LARVA:
            return false;
        case UNIT_TYPEID::ZERG_EGG:
            return false;
        case UNIT_TYPEID::TERRAN_MULE:
            return false;
        case UNIT_TYPEID::TERRAN_NUKE:
            return false;
        default:
            return true;
    }
}

bool IsTownHall::operator()(const Unit& unit) {
    switch (unit.unit_type.ToType()) {
        case UNIT_TYPEID::ZERG_HATCHERY:
            return true;
        case UNIT_TYPEID::ZERG_LAIR:
            return true;
        case UNIT_TYPEID::ZERG_HIVE:
            return true;
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
            return true;
        case UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
            return true;
        case UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
            return true;
        case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
            return true;
        case UNIT_TYPEID::PROTOSS_NEXUS:
            return true;
        default:
            return false;
    }
}

bool IsVespeneGeyser::operator()(const Unit& unit) {
    switch (unit.unit_type.ToType()) {
        case UNIT_TYPEID::NEUTRAL_VESPENEGEYSER:
        case UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER:
        case UNIT_TYPEID::NEUTRAL_PROTOSSVESPENEGEYSER:
        case UNIT_TYPEID::NEUTRAL_PURIFIERVESPENEGEYSER:
        case UNIT_TYPEID::NEUTRAL_RICHVESPENEGEYSER:
        case UNIT_TYPEID::NEUTRAL_SHAKURASVESPENEGEYSER:
            return true;
        default:
            return false;
    }
}

bool IsStructure::operator()(const Unit& unit) {
    return isStructure(getUnitData(unit.unit_type));
}

bool isStructure(const UnitTypeData& unitType) {
    auto& attributes = unitType.attributes;
    bool is_structure = false;
    for (const auto& attribute : attributes) {
        if (attribute == Attribute::Structure) {
            is_structure = true;
        }
    }
    return is_structure;
}

bool carriesResources(const Unit* unit) {
    // CARRYMINERALFIELDMINERALS = 271,
    // CARRYHIGHYIELDMINERALFIELDMINERALS = 272,
    // CARRYHARVESTABLEVESPENEGEYSERGAS = 273,
    // CARRYHARVESTABLEVESPENEGEYSERGASPROTOSS = 274,
    // CARRYHARVESTABLEVESPENEGEYSERGASZERG = 275,

    for (auto b : unit->buffs) {
        if (b >= 271 && b <= 275) return true;
    }
    return false;
}

bool isMelee(sc2::UNIT_TYPEID type) {
    switch(type) {
        case UNIT_TYPEID::PROTOSS_PROBE:
        case UNIT_TYPEID::PROTOSS_ZEALOT:
        case UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
        case UNIT_TYPEID::TERRAN_SCV:
        case UNIT_TYPEID::TERRAN_HELLIONTANK:
        case UNIT_TYPEID::ZERG_DRONE:
        case UNIT_TYPEID::ZERG_ZERGLING:
        case UNIT_TYPEID::ZERG_ZERGLINGBURROWED:
        case UNIT_TYPEID::ZERG_BANELING:
        case UNIT_TYPEID::ZERG_BANELINGBURROWED:
        case UNIT_TYPEID::ZERG_ULTRALISK:
        case UNIT_TYPEID::ZERG_BROODLING:
            return true;
        default:
            return false;
    }
}

bool isInfantry(sc2::UNIT_TYPEID type) {
    switch(type) {
        case UNIT_TYPEID::TERRAN_MARINE:
        case UNIT_TYPEID::TERRAN_MARAUDER:
        case UNIT_TYPEID::TERRAN_REAPER:
        case UNIT_TYPEID::TERRAN_GHOST:
        case UNIT_TYPEID::TERRAN_HELLIONTANK:
            return true;
        default:
            return false;
    }
}

bool isChangeling(sc2::UNIT_TYPEID type) {
    switch(type) {
        case UNIT_TYPEID::ZERG_CHANGELING:
        case UNIT_TYPEID::ZERG_CHANGELINGMARINE:
        case UNIT_TYPEID::ZERG_CHANGELINGMARINESHIELD:
        case UNIT_TYPEID::ZERG_CHANGELINGZEALOT:
        case UNIT_TYPEID::ZERG_CHANGELINGZERGLING:
        case UNIT_TYPEID::ZERG_CHANGELINGZERGLINGWINGS:
            return true;
        default:
            return false;
    }
}

bool hasBuff (const Unit* unit, BUFF_ID buff) {
    for (auto b : unit->buffs) if (b == buff) return true;
    return false;
}

bool isUpgradeWithLevels(sc2::UPGRADE_ID upgrade) {
    switch(upgrade) {
    case sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1:
    case sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1:
    case sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL1:
    case sc2::UPGRADE_ID::TERRANSHIPWEAPONSLEVEL1:
    case sc2::UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1:
    case sc2::UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1:
    case sc2::UPGRADE_ID::PROTOSSSHIELDSLEVEL1:
    case sc2::UPGRADE_ID::ZERGMELEEWEAPONSLEVEL1:
    case sc2::UPGRADE_ID::ZERGGROUNDARMORSLEVEL1:
    case sc2::UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL1:
    case sc2::UPGRADE_ID::ZERGFLYERWEAPONSLEVEL1:
    case sc2::UPGRADE_ID::ZERGFLYERARMORSLEVEL1:
    case sc2::UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL1:
    case sc2::UPGRADE_ID::PROTOSSAIRARMORSLEVEL1:
    case sc2::UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL1:
        return true;
    default:
        return false;
    }
}

bool isTownHall(sc2::UNIT_TYPEID type) {
    switch (type) {
        case UNIT_TYPEID::ZERG_HATCHERY:
        case UNIT_TYPEID::ZERG_LAIR:
        case UNIT_TYPEID::ZERG_HIVE:
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
        case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
        case UNIT_TYPEID::PROTOSS_NEXUS:
            return true;
        default:
            return false;
    }
}

bool isUpgradeDoneOrInProgress(const ObservationInterface* observation, UPGRADE_ID upgrade) {
    for (auto const i : observation->GetUpgrades()) {
        if (upgrade == i) {
            return true;
        }
    }

    const UpgradeData& data = getUpgradeData(upgrade);
    auto upgradeAbility = data.ability_id;

    for (auto const unit : observation->GetUnits(Unit::Alliance::Self)) {
        // Note: Upgrades with levels use generalized ability IDs
        if (!unit->orders.empty() && unit->orders[0].ability_id == upgradeAbility) {
            return true;
        }
    }
    return false;
}
