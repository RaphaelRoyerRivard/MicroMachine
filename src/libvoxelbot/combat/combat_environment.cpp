#include "combat_environment.h"
#include "simulator.h"
#include "../utilities/predicates.h"
#include <iostream>

using namespace std;
using namespace sc2;

int getDamageBonus(UNIT_TYPEID unit, const CombatUpgrades& upgrades) {
    if (isStructure(unit)) return 0;

    int bonus = 0;
    switch(getUnitData(unit).race) {
    case Race::Protoss:
        if (isFlying(unit)) {
            if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL1)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL2)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL3)) bonus += 1;
        } else {
            if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL3)) bonus += 1;
        }
        break;
    case Race::Zerg:
        if (isFlying(unit)) {
            if (upgrades.hasUpgrade(UPGRADE_ID::ZERGFLYERWEAPONSLEVEL1)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::ZERGFLYERWEAPONSLEVEL2)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::ZERGFLYERWEAPONSLEVEL3)) bonus += 1;
        } else if (isMelee(unit)) {
            // TODO: Might need to check which units are actually melee
            if (upgrades.hasUpgrade(UPGRADE_ID::ZERGMELEEWEAPONSLEVEL1)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::ZERGMELEEWEAPONSLEVEL2)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::ZERGMELEEWEAPONSLEVEL3)) bonus += 1;
        } else {
            // Ranged
            if (upgrades.hasUpgrade(UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL1)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL2)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL3)) bonus += 1;
        }
        break;
    case Race::Terran: {
        auto canonical = canonicalize(unit);
        auto& casters = abilityToCasterUnit(getUnitData(canonical).ability_id);
        if (casters.size() > 0 && casters[0] == UNIT_TYPEID::TERRAN_BARRACKS) {
            if (upgrades.hasUpgrade(UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL2)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL3)) bonus += 1;
        } else if (casters.size() > 0 && casters[0] == UNIT_TYPEID::TERRAN_FACTORY) {
            if (upgrades.hasUpgrade(UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL1)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL2)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL3)) bonus += 1;
        } else if (casters.size() > 0 && casters[0] == UNIT_TYPEID::TERRAN_STARPORT) {
            if (upgrades.hasUpgrade(UPGRADE_ID::TERRANSHIPWEAPONSLEVEL1)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::TERRANSHIPWEAPONSLEVEL2)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::TERRANSHIPWEAPONSLEVEL3)) bonus += 1;
        }
        break;
    }
    default:
        break;
    }
    return bonus;
}

float getArmorBonus(UNIT_TYPEID unit, const CombatUpgrades& upgrades) {
    if (isStructure(unit)) {
        if (getUnitData(unit).race == Race::Terran && upgrades.hasUpgrade(UPGRADE_ID::TERRANBUILDINGARMOR)) return 2;
        return 0;
    }

    float bonus = 0;
    switch(getUnitData(unit).race) {
        case Race::Protoss:
            if (isFlying(unit)) {
                if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSAIRARMORSLEVEL1)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSAIRARMORSLEVEL2)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSAIRARMORSLEVEL3)) bonus += 1;
            } else {
                if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL3)) bonus += 1;
            }

            if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSSHIELDSLEVEL1)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSSHIELDSLEVEL2)) bonus += 1;
            if (upgrades.hasUpgrade(UPGRADE_ID::PROTOSSSHIELDSLEVEL3)) bonus += 1;
            break;
        case Race::Zerg:
            if (isFlying(unit)) {
                if (upgrades.hasUpgrade(UPGRADE_ID::ZERGFLYERARMORSLEVEL1)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::ZERGFLYERARMORSLEVEL2)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::ZERGFLYERARMORSLEVEL3)) bonus += 1;
            } else {
                if (upgrades.hasUpgrade(UPGRADE_ID::ZERGGROUNDARMORSLEVEL1)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::ZERGGROUNDARMORSLEVEL2)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::ZERGGROUNDARMORSLEVEL3)) bonus += 1;
            }
        case Race::Terran: {
            auto canonical = canonicalize(unit);
            auto& casters = abilityToCasterUnit(getUnitData(canonical).ability_id);
            if (casters.size() > 0 && casters[0] == UNIT_TYPEID::TERRAN_BARRACKS) {
                if (upgrades.hasUpgrade(UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::TERRANINFANTRYARMORSLEVEL2)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::TERRANINFANTRYARMORSLEVEL3)) bonus += 1;
            } else if (casters.size() > 0 && (casters[0] == UNIT_TYPEID::TERRAN_FACTORY || casters[0] == UNIT_TYPEID::TERRAN_STARPORT)) {
                if (upgrades.hasUpgrade(UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL1)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL2)) bonus += 1;
                if (upgrades.hasUpgrade(UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL3)) bonus += 1;
            }
            break;
        }
        default:
            break;
    }
    return bonus;
}

CombatEnvironment::CombatEnvironment(const CombatUpgrades& upgrades, const CombatUpgrades& targetUpgrades) : upgrades({{ upgrades, targetUpgrades }}) {
    assertMappingsInitialized();
    auto& unitTypes = getUnitTypes();
    for (size_t i = 0; i < unitTypes.size(); i++) {
        combatInfo[0].push_back(UnitCombatInfo((UNIT_TYPEID)i, upgrades, targetUpgrades));
    }
    for (size_t i = 0; i < unitTypes.size(); i++) {
        combatInfo[1].push_back(UnitCombatInfo((UNIT_TYPEID)i, targetUpgrades, upgrades));
    }

    for (int owner = 0; owner < 2; owner++) {
        combatInfo[owner][(int)UNIT_TYPEID::TERRAN_LIBERATOR].airWeapon.splash = 3.0;
        combatInfo[owner][(int)UNIT_TYPEID::TERRAN_MISSILETURRET].airWeapon.splash = 3.0;
        combatInfo[owner][(int)UNIT_TYPEID::TERRAN_SIEGETANKSIEGED].groundWeapon.splash = 4.0;
        combatInfo[owner][(int)UNIT_TYPEID::TERRAN_HELLION].groundWeapon.splash = 2;
        combatInfo[owner][(int)UNIT_TYPEID::TERRAN_HELLIONTANK].groundWeapon.splash = 3;
        combatInfo[owner][(int)UNIT_TYPEID::ZERG_MUTALISK].groundWeapon.splash = 1.44f;
        combatInfo[owner][(int)UNIT_TYPEID::ZERG_MUTALISK].airWeapon.splash = 1.44f;
        combatInfo[owner][(int)UNIT_TYPEID::TERRAN_THOR].airWeapon.splash = 3;
        combatInfo[owner][(int)UNIT_TYPEID::PROTOSS_ARCHON].groundWeapon.splash = 3;
        combatInfo[owner][(int)UNIT_TYPEID::PROTOSS_ARCHON].airWeapon.splash = 3;
        combatInfo[owner][(int)UNIT_TYPEID::PROTOSS_COLOSSUS].groundWeapon.splash = 3;
    }
}

const CombatEnvironment& CombatPredictor::combineCombatEnvironment(const CombatEnvironment* env, const CombatUpgrades& upgrades, int upgradesOwner) const {
    assert(upgradesOwner == 1 || upgradesOwner == 2);

    array<CombatUpgrades, 2> newUpgrades = env != nullptr ? env->upgrades : array<CombatUpgrades, 2>{{ {}, {} }};
    newUpgrades[upgradesOwner - 1].combine(upgrades);

    return getCombatEnvironment(newUpgrades[0], newUpgrades[1]);
}

const CombatEnvironment& CombatPredictor::getCombatEnvironment(const CombatUpgrades& upgrades, const CombatUpgrades& targetUpgrades) const {
    uint64_t hash = (upgrades.hash() * 5123143) ^ targetUpgrades.hash();
    auto it = combatEnvironments.find(hash);
    if (it != combatEnvironments.end()) {
        return (*it).second;
    }

    // Note: references to map values are guaranteed to remain valid even after additional insertion into the map
    return (*combatEnvironments.emplace(make_pair(hash, CombatEnvironment(upgrades, targetUpgrades))).first).second;
}

// TODO: Air?
float CombatEnvironment::attackRange(int owner, UNIT_TYPEID type) const {
    auto& info = combatInfo[owner - 1][(int)type];
    return max(info.airWeapon.range(), info.groundWeapon.range());
}

float CombatEnvironment::attackRange(const CombatUnit& unit) const {
    auto& info = combatInfo[unit.owner - 1][(int)unit.type];
    float range = max(info.airWeapon.range(), info.groundWeapon.range());

    if (unit.type == UNIT_TYPEID::PROTOSS_COLOSSUS && upgrades[unit.owner-1].hasUpgrade(UPGRADE_ID::EXTENDEDTHERMALLANCE)) range += 2;

    return range;
}

const UnitCombatInfo& CombatEnvironment::getCombatInfo(const CombatUnit& unit) const {
    return combatInfo[unit.owner - 1][(int)unit.type];
}

float CombatEnvironment::calculateDPS(int owner, UNIT_TYPEID type, bool air) const {
    auto& info = combatInfo[owner - 1][(int)type];
    return air ? info.airWeapon.getDPS() : info.groundWeapon.getDPS();
}

float CombatEnvironment::calculateDPS(const vector<CombatUnit>& units, bool air) const {
    float dps = 0;
    for (auto& u : units)
        dps += calculateDPS(u.owner, u.type, air);
    return dps;
}

float CombatEnvironment::calculateDPS(const CombatUnit& unit, bool air) const {
    auto& info = combatInfo[unit.owner - 1][(int)unit.type];
    return air ? info.airWeapon.getDPS() : info.groundWeapon.getDPS();
}

float CombatEnvironment::calculateDPS(const CombatUnit& unit1, const CombatUnit& unit2) const {
    auto& info = combatInfo[unit1.owner - 1][(int)unit1.type];
    return max(info.groundWeapon.getDPS(unit2.type), info.airWeapon.getDPS(unit2.type));
}


float calculateDPS(UNIT_TYPEID attacker, UNIT_TYPEID target, const Weapon& weapon, const CombatUpgrades& attackerUpgrades, const CombatUpgrades& targetUpgrades) {
    // canBeAttackedByAirWeapons is primarily for coloussus.
    if (weapon.type == Weapon::TargetType::Any || (weapon.type == Weapon::TargetType::Air ? canBeAttackedByAirWeapons(target) : !isFlying(target))) {
        float dmg = weapon.damage_;
        for (auto& b : weapon.damage_bonus) {
            if (contains(getUnitData(target).attributes, b.attribute)) {
                dmg += b.bonus;
            }
        }

        dmg += getDamageBonus(attacker, attackerUpgrades);

        float armor = getUnitData(target).armor + getArmorBonus(target, targetUpgrades);

        // Note: cannot distinguish between damage to shields and damage to health yet, so average out so that the armor is about 0.5 over the whole shield+health of the unit
        // Important only for protoss
        armor *= maxHealth(target) / (maxHealth(target) + maxShield(target));

        float timeBetweenAttacks = weapon.speed;

        if (attacker == UNIT_TYPEID::PROTOSS_ADEPT && attackerUpgrades.hasUpgrade(UPGRADE_ID::ADEPTPIERCINGATTACK)) timeBetweenAttacks /= 1.45f;

        return max(0.0f, dmg - armor) * weapon.attacks / timeBetweenAttacks;
    }

    return 0;
}

float WeaponInfo::getDPS(UNIT_TYPEID target, float modifier) const {
    if (!available || !weapon || weapon->speed == 0)
        return 0;

	if (dpsCache.find(unsigned(target)) == dpsCache.end()) {
		cerr << "CombatSimulator error: dpsCache does not contain unit of type " << unsigned(target) << " named " << UnitTypeToName(target) << endl;
		dpsCache[unsigned(target)] = 0;
    }

    // TODO: Modifier ignores speed upgrades
    return max(0.0f, dpsCache[unsigned(target)] + modifier*weapon->attacks/weapon->speed);
}

float WeaponInfo::range() const {
    return weapon != nullptr ? weapon->range : 0;
}

float UnitCombatInfo::attackInterval() const {
    float v = numeric_limits<float>::infinity();
    if (airWeapon.weapon != nullptr) v = airWeapon.weapon->speed;
    if (groundWeapon.weapon != nullptr) v = min(v, groundWeapon.weapon->speed);
    return v;
}

WeaponInfo::WeaponInfo(const Weapon* weapon, UNIT_TYPEID type, const CombatUpgrades& upgrades, const CombatUpgrades& targetUpgrades) {
    available = true;
    splash = 0;
    this->weapon = weapon;
    // TODO: Range upgrades!

    baseDPS = (weapon->damage_ + getDamageBonus(type, upgrades)) * weapon->attacks / weapon->speed;

    auto& unitTypes = getUnitTypes();
    for (size_t i = 0; i < unitTypes.size(); i++) {
        dpsCache[i] = calculateDPS(type, UNIT_TYPEID(i), *weapon, upgrades, targetUpgrades);
    }
}

UnitCombatInfo::UnitCombatInfo(UNIT_TYPEID type, const CombatUpgrades& upgrades, const CombatUpgrades& targetUpgrades) {
    auto& data = getUnitData(type);

    for (const Weapon& weapon : data.weapons) {
        if (weapon.type == Weapon::TargetType::Any || weapon.type == Weapon::TargetType::Air) {
            if (airWeapon.available) {
                cerr << "For unit type " << UnitTypeToName(type) << endl;
                cerr << "Weapon slot is already used";
                assert(false);
            }
            airWeapon = WeaponInfo(&weapon, type, upgrades, targetUpgrades);
        }
        if (weapon.type == Weapon::TargetType::Any || weapon.type == Weapon::TargetType::Ground) {
            if (groundWeapon.available) {
                cerr << "For unit type " << UnitTypeToName(type) << endl;
                cerr << "Weapon slot is already used";
                assert(false);
            }
            groundWeapon = WeaponInfo(&weapon, type, upgrades, targetUpgrades);
        }
    }
}