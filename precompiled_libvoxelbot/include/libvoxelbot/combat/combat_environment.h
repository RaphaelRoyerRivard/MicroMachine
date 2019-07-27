#pragma once
#include "sc2api/sc2_interfaces.h"
#include <vector>
#include <array>
#include <libvoxelbot/combat/combat_upgrades.h>

struct CombatUnit;

struct WeaponInfo {
   private:
    mutable std::vector<float> dpsCache;
    float baseDPS;

   public:
    bool available;
    float splash;
    const sc2::Weapon* weapon;

    float getDPS() const {
        return baseDPS;
    }

    float getDPS(sc2::UNIT_TYPEID target, float modifier = 0) const;

    float range() const;

    WeaponInfo()
        : baseDPS(0), available(false), splash(0), weapon(nullptr) {
    }

    WeaponInfo(const sc2::Weapon* weapon, sc2::UNIT_TYPEID type, const CombatUpgrades& upgrades, const CombatUpgrades& targetUpgrades);
};

struct UnitCombatInfo {
    WeaponInfo groundWeapon;
    WeaponInfo airWeapon;

	// In case the unit has multiple weapons this is the fastest of the two weapons
	float attackInterval() const;
	
    UnitCombatInfo(sc2::UNIT_TYPEID type, const CombatUpgrades& upgrades, const CombatUpgrades& targetUpgrades);
};

struct CombatEnvironment {
	std::array<std::vector<UnitCombatInfo>, 2> combatInfo;
	std::array<CombatUpgrades, 2> upgrades;

	CombatEnvironment(const CombatUpgrades& upgrades, const CombatUpgrades& targetUpgrades);

	float attackRange(int owner, sc2::UNIT_TYPEID type) const;
	float attackRange(const CombatUnit& unit) const;
	const UnitCombatInfo& getCombatInfo(const CombatUnit& unit) const;
	float calculateDPS(int owner, sc2::UNIT_TYPEID type, bool air) const;
	float calculateDPS(const std::vector<CombatUnit>& units, bool air) const;
	float calculateDPS(const CombatUnit& unit, bool air) const;
	float calculateDPS(const CombatUnit& unit1, const CombatUnit& unit2) const;
};
