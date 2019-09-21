#pragma once
#include "sc2api/sc2_interfaces.h"

struct IsAttackable {
    bool operator()(const sc2::Unit& unit);
};
struct IsFlying {
    bool operator()(const sc2::Unit& unit);
};
struct IsArmy {
    const sc2::ObservationInterface* observation_;
    // Ignores Overlords, workers, and structures
    IsArmy(const sc2::ObservationInterface* obs) : observation_(obs) {}
    bool operator()(const sc2::Unit& unit);
};
struct IsTownHall {
    bool operator()(const sc2::Unit& unit);
};
struct IsVespeneGeyser {
    bool operator()(const sc2::Unit& unit);
};
struct IsStructure {
    const sc2::ObservationInterface* observation_;
    IsStructure(const sc2::ObservationInterface* obs) : observation_(obs){};
    bool operator()(const sc2::Unit& unit);
};

bool isArmy(sc2::UNIT_TYPEID type);
bool isStructure(const sc2::UnitTypeData& unitType);

bool carriesResources(const sc2::Unit* unit);

bool isMelee(sc2::UNIT_TYPEID type);
bool isInfantry(sc2::UNIT_TYPEID type);
bool isChangeling(sc2::UNIT_TYPEID type);
bool hasBuff (const sc2::Unit* unit, sc2::BUFF_ID buff);
bool isUpgradeWithLevels(sc2::UPGRADE_ID upgrade);
bool isTownHall(sc2::UNIT_TYPEID type);
bool isUpgradeDoneOrInProgress(const sc2::ObservationInterface* observation, sc2::UPGRADE_ID upgrade);