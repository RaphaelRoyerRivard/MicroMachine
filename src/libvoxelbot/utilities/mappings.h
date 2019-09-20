#pragma once
#include "sc2api/sc2_api.h"

void assertMappingsInitialized();
void initMappings(const sc2::ObservationInterface* observation);
void initMappings();
const sc2::UnitTypeData& getUnitData(sc2::UNIT_TYPEID type);
const std::vector<sc2::UnitTypeData>& getUnitTypes();
const sc2::AbilityData& getAbilityData(sc2::ABILITY_ID ability);
const sc2::UpgradeData& getUpgradeData(sc2::UPGRADE_ID upgrade);
sc2::UNIT_TYPEID getUpgradeUnitDependency(sc2::UPGRADE_ID upgrade);
sc2::UPGRADE_ID getUpgradeUpgradeDependency(sc2::UPGRADE_ID upgrade);
sc2::UNIT_TYPEID canonicalize(sc2::UNIT_TYPEID unitType);
const std::vector<sc2::UNIT_TYPEID>& abilityToCasterUnit(sc2::ABILITY_ID ability);
sc2::UNIT_TYPEID abilityToUnit(sc2::ABILITY_ID ability);
sc2::UPGRADE_ID abilityToUpgrade(sc2::ABILITY_ID ability);
sc2::UNIT_TYPEID simplifyUnitType(sc2::UNIT_TYPEID type);
const std::vector<sc2::UNIT_TYPEID>& hasBeen(sc2::UNIT_TYPEID type);
const std::vector<sc2::UNIT_TYPEID>& canBecome(sc2::UNIT_TYPEID type);
sc2::UNIT_TYPEID upgradedFrom(sc2::UNIT_TYPEID type);
float maxHealth(sc2::UNIT_TYPEID type);
float maxShield(sc2::UNIT_TYPEID type);
bool isFlying(sc2::UNIT_TYPEID type);
bool isStationary(sc2::UNIT_TYPEID type);
bool isStructure(sc2::UNIT_TYPEID type);
float unitRadius(sc2::UNIT_TYPEID type);
const std::vector<sc2::ABILITY_ID>& unitAbilities(sc2::UNIT_TYPEID type);
sc2::UNIT_TYPEID getSpecificAddonType(sc2::UNIT_TYPEID caster, sc2::UNIT_TYPEID addon);

/** Times in the SC2 API are often defined in ticks, instead of seconds.
 * This method assumes the 'Faster' game speed.
 */
inline float ticksToSeconds(float ticks) {
    return ticks / 22.4f;
}

inline bool isBasicHarvester(sc2::UNIT_TYPEID type) {
    switch (type) {
        case sc2::UNIT_TYPEID::TERRAN_SCV:
        case sc2::UNIT_TYPEID::ZERG_DRONE:
        case sc2::UNIT_TYPEID::PROTOSS_PROBE:
            return true;
        default:
            return false;
    }
}

inline bool isVespeneHarvester(sc2::UNIT_TYPEID type) {
    switch (type) {
        case sc2::UNIT_TYPEID::TERRAN_REFINERY:
        case sc2::UNIT_TYPEID::ZERG_EXTRACTOR:
        case sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR:
            return true;
        default:
            return false;
    }
}

inline sc2::UNIT_TYPEID getHarvesterUnitForRace(sc2::Race race) {
    switch (race) {
        case sc2::Race::Protoss:
            return sc2::UNIT_TYPEID::PROTOSS_PROBE;
        case sc2::Race::Terran:
            return sc2::UNIT_TYPEID::TERRAN_SCV;
        case sc2::Race::Zerg:
            return sc2::UNIT_TYPEID::ZERG_DRONE;
        default:
            assert(false);
            return sc2::UNIT_TYPEID::INVALID;
    }
}

inline sc2::UNIT_TYPEID getSupplyUnitForRace(sc2::Race race) {
    switch (race) {
        case sc2::Race::Protoss:
            return sc2::UNIT_TYPEID::PROTOSS_PYLON;
        case sc2::Race::Terran:
            return sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT;
        case sc2::Race::Zerg:
            return sc2::UNIT_TYPEID::ZERG_OVERLORD;
        default:
            assert(false);
            return sc2::UNIT_TYPEID::INVALID;
    }
}

inline sc2::UNIT_TYPEID getVespeneHarvesterForRace(sc2::Race race) {
    switch (race) {
        case sc2::Race::Protoss:
            return sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR;
        case sc2::Race::Terran:
            return sc2::UNIT_TYPEID::TERRAN_REFINERY;
        case sc2::Race::Zerg:
            return sc2::UNIT_TYPEID::ZERG_EXTRACTOR;
        default:
            assert(false);
            return sc2::UNIT_TYPEID::INVALID;
    }
}

inline sc2::UNIT_TYPEID getTownHallForRace(sc2::Race race) {
    switch (race) {
        case sc2::Race::Protoss:
            return sc2::UNIT_TYPEID::PROTOSS_NEXUS;
        case sc2::Race::Terran:
            return sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER;
        case sc2::Race::Zerg:
            return sc2::UNIT_TYPEID::ZERG_HATCHERY;
        default:
            assert(false);
            return sc2::UNIT_TYPEID::INVALID;
    }
}

inline bool isAddon(sc2::UNIT_TYPEID unit) {
    switch (unit) {
        case sc2::UNIT_TYPEID::TERRAN_TECHLAB:
        case sc2::UNIT_TYPEID::TERRAN_REACTOR:
        case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
        case sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR:
        case sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR:
        case sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB:
        case sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB:
        case sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB:
            return true;
        default:
            return false;
    }
}

inline bool isMineralField(sc2::UNIT_TYPEID unit) {
    switch (unit) {
        case sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD:
        case sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD750:
        case sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD:
        case sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD750:
        case sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD:
        case sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD750:
        case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD:
        case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD750:
        case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD:
        case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD750:
        case sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD:
        case sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750:
            return true;
        default:
            return false;
    }
}
