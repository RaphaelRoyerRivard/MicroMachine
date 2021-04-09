#include "mappings.h"
#include <iostream>
#include "../generated/abilities.h"
#include "../utilities/predicates.h"
#include "sc2api/sc2_agent.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_map_info.h"
#include "../utilities/stdutils.h"
#include "../utilities/unit_data_caching.h"

using namespace std;
using namespace sc2;

static vector<vector<UNIT_TYPEID>> mAbilityToCasterUnit;
static vector<UNIT_TYPEID> mAbilityToCreatedUnit;
static vector<UPGRADE_ID> mAbilityToUpgrade;
static vector<vector<UNIT_TYPEID>> mCanBecome;
static vector<vector<UNIT_TYPEID>> mHasBeen;
static vector<vector<ABILITY_ID>> mUnitTypeHasAbilities;
static vector<UnitTypeData> mUnitTypes;
static vector<AbilityData> mAbilities;
static vector<UpgradeData> mUpgrades;
static vector<bool> mIsStationary;
static vector<bool> mIsStructure;

static vector<UPGRADE_ID> mUpgradeUpgradeDependency;
static vector<UNIT_TYPEID> mUpgradeUnitDependency;

static bool mappingInitialized = false;

UNIT_TYPEID canonicalize(UNIT_TYPEID unitType) {
    const UnitTypeData& unitTypeData = getUnitData(unitType);

    // Use canonical representation (e.g SUPPLY_DEPOT instead of SUPPLY_DEPOT_LOWERED)
    if (unitTypeData.unit_alias != UNIT_TYPEID::INVALID) {
        return unitTypeData.unit_alias;
    }
    return unitType;
}

UNIT_TYPEID upgradedFrom(sc2::UNIT_TYPEID type) {
    auto& h = hasBeen(type);
    // First element is the unit itself
    // second element is the one it was created from
    // third element is the one that one was created from etc.
    // E.g. hasBeen[hatchery][1] = drone
    //      hasBeen[lair][1] = hatchery
    if (h.size() > 1) return h[1];
    return UNIT_TYPEID::INVALID;
}

UNIT_TYPEID getSpecificAddonType(UNIT_TYPEID caster, UNIT_TYPEID addon) {
    assert(addon == UNIT_TYPEID::TERRAN_REACTOR || addon == UNIT_TYPEID::TERRAN_TECHLAB);
    switch (caster) {
        case UNIT_TYPEID::TERRAN_BARRACKS:
            return addon == UNIT_TYPEID::TERRAN_TECHLAB ? UNIT_TYPEID::TERRAN_BARRACKSTECHLAB : UNIT_TYPEID::TERRAN_BARRACKSREACTOR;
        case UNIT_TYPEID::TERRAN_FACTORY:
            return addon == UNIT_TYPEID::TERRAN_TECHLAB ? UNIT_TYPEID::TERRAN_FACTORYTECHLAB : UNIT_TYPEID::TERRAN_FACTORYREACTOR;
        case UNIT_TYPEID::TERRAN_STARPORT:
            return addon == UNIT_TYPEID::TERRAN_TECHLAB ? UNIT_TYPEID::TERRAN_STARPORTTECHLAB : UNIT_TYPEID::TERRAN_STARPORTREACTOR;
        default:
            assert(false);
            return UNIT_TYPEID::INVALID;
    }
}

const vector<UNIT_TYPEID>& hasBeen(sc2::UNIT_TYPEID type) {
    return mHasBeen[(int)type];
}

const vector<UNIT_TYPEID>& canBecome(sc2::UNIT_TYPEID type) {
    return mCanBecome[(int)type];
}

const vector<ABILITY_ID>& unitAbilities(UNIT_TYPEID type) {
    return mUnitTypeHasAbilities[(int)type];
}

const sc2::UnitTypeData& getUnitData(sc2::UNIT_TYPEID type) {
    return mUnitTypes[(int)type];
}

const vector<UnitTypeData>& getUnitTypes() {
    return mUnitTypes;
}

const sc2::AbilityData& getAbilityData(sc2::ABILITY_ID ability) {
    return mAbilities[(int)ability];
}

const sc2::UpgradeData& getUpgradeData(sc2::UPGRADE_ID upgrade) {
    return mUpgrades[(int)upgrade];
}

static void addEnumeratedUpgradeDependency(UPGRADE_ID level1) {
    mUpgradeUpgradeDependency[(int)level1 + 2] = (UPGRADE_ID)((int)level1 + 1);
    mUpgradeUpgradeDependency[(int)level1 + 1] = level1;
}

void init() {
    if (mappingInitialized)
        return;
    mappingInitialized = true;

    int maxAbilityID = 0;
    for (auto pair : unit_type_has_ability) {
        maxAbilityID = max(maxAbilityID, pair.second);
    }
    int maxUpgradeID = 0;
    for (auto u : mUpgrades) {
        maxUpgradeID = max(maxUpgradeID, (int)u.upgrade_id);
    }

    const vector<UnitTypeData>& unitTypes = mUnitTypes;
    const auto& abilities = mAbilities;

    mAbilityToCreatedUnit = vector<UNIT_TYPEID>(abilities.size(), UNIT_TYPEID::INVALID);
    mAbilityToUpgrade = vector<UPGRADE_ID>(abilities.size(), UPGRADE_ID::INVALID);
    for (auto type : unitTypes) {
        mAbilityToCreatedUnit[type.ability_id] = (UNIT_TYPEID)type.unit_type_id;
    }
    for (auto upgrade : mUpgrades) {
        mAbilityToUpgrade[upgrade.ability_id] = (UPGRADE_ID)upgrade.upgrade_id;
    }

    mUnitTypeHasAbilities = vector<vector<ABILITY_ID>>(unitTypes.size());
    mAbilityToCasterUnit = vector<vector<UNIT_TYPEID>>(maxAbilityID + 1);

    for (auto pair : unit_type_has_ability) {
        auto unit = (UNIT_TYPEID)pair.first;
        mUnitTypeHasAbilities[(int)unit].push_back((ABILITY_ID)pair.second);

        // Avoid warpgate because the rest of the code cannot handle it at the moment (requires research)
        if (unit != UNIT_TYPEID::PROTOSS_WARPGATE) {
            mAbilityToCasterUnit[pair.second].push_back(unit);
        }
    }

    // Addons are not really inferred correctly, so we need to correct the definitions here.
    mAbilityToCasterUnit[(int)ABILITY_ID::BUILD_TECHLAB] = { UNIT_TYPEID::INVALID };
    mAbilityToCasterUnit[(int)ABILITY_ID::BUILD_TECHLAB_BARRACKS] = { UNIT_TYPEID::TERRAN_BARRACKS };
    mAbilityToCasterUnit[(int)ABILITY_ID::BUILD_TECHLAB_FACTORY] = { UNIT_TYPEID::TERRAN_FACTORY };
    mAbilityToCasterUnit[(int)ABILITY_ID::BUILD_TECHLAB_STARPORT] = { UNIT_TYPEID::TERRAN_STARPORT };
    mAbilityToCasterUnit[(int)ABILITY_ID::BUILD_REACTOR] = { UNIT_TYPEID::INVALID };
    mAbilityToCasterUnit[(int)ABILITY_ID::BUILD_REACTOR_BARRACKS] = { UNIT_TYPEID::TERRAN_BARRACKS };
    mAbilityToCasterUnit[(int)ABILITY_ID::BUILD_REACTOR_FACTORY] = { UNIT_TYPEID::TERRAN_FACTORY };
    mAbilityToCasterUnit[(int)ABILITY_ID::BUILD_REACTOR_STARPORT] = { UNIT_TYPEID::TERRAN_STARPORT };

    // Not technically correct, but it makes the build order optimizer simpler
    mAbilityToCasterUnit[(int)ABILITY_ID::TRAIN_ZEALOT].push_back(UNIT_TYPEID::PROTOSS_WARPGATE);
    mAbilityToCasterUnit[(int)ABILITY_ID::TRAIN_SENTRY].push_back(UNIT_TYPEID::PROTOSS_WARPGATE);
    mAbilityToCasterUnit[(int)ABILITY_ID::TRAIN_STALKER].push_back(UNIT_TYPEID::PROTOSS_WARPGATE);
    mAbilityToCasterUnit[(int)ABILITY_ID::TRAIN_ADEPT].push_back(UNIT_TYPEID::PROTOSS_WARPGATE);
    mAbilityToCasterUnit[(int)ABILITY_ID::TRAIN_HIGHTEMPLAR].push_back(UNIT_TYPEID::PROTOSS_WARPGATE);
    mAbilityToCasterUnit[(int)ABILITY_ID::TRAIN_DARKTEMPLAR].push_back(UNIT_TYPEID::PROTOSS_WARPGATE);

    // Some upgrades are inferred incorrectly due to not simultaneously being available on a unit
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSGROUNDARMORLEVEL1].push_back(UNIT_TYPEID::PROTOSS_FORGE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSGROUNDARMORLEVEL2].push_back(UNIT_TYPEID::PROTOSS_FORGE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSGROUNDARMORLEVEL3].push_back(UNIT_TYPEID::PROTOSS_FORGE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONSLEVEL1].push_back(UNIT_TYPEID::PROTOSS_FORGE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONSLEVEL2].push_back(UNIT_TYPEID::PROTOSS_FORGE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONSLEVEL3].push_back(UNIT_TYPEID::PROTOSS_FORGE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSSHIELDSLEVEL1].push_back(UNIT_TYPEID::PROTOSS_FORGE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSSHIELDSLEVEL2].push_back(UNIT_TYPEID::PROTOSS_FORGE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSSHIELDSLEVEL3].push_back(UNIT_TYPEID::PROTOSS_FORGE);
    
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSAIRARMORLEVEL1].push_back(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSAIRARMORLEVEL2].push_back(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSAIRARMORLEVEL3].push_back(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSAIRWEAPONSLEVEL1].push_back(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSAIRWEAPONSLEVEL2].push_back(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PROTOSSAIRWEAPONSLEVEL3].push_back(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);

    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_GRAVITICBOOSTER] = { UNIT_TYPEID::PROTOSS_ROBOTICSBAY };
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_GRAVITICDRIVE] = { UNIT_TYPEID::PROTOSS_ROBOTICSBAY };
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_EXTENDEDTHERMALLANCE] = { UNIT_TYPEID::PROTOSS_ROBOTICSBAY };
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_CHARGE] = { UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL };
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_BLINK] = { UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL };
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_ADEPTRESONATINGGLAIVES] = { UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL };
    mAbilityToCasterUnit[(int)ABILITY_ID::RESEARCH_PSISTORM] = { UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE };
    


    mCanBecome = vector<vector<UNIT_TYPEID>>(unitTypes.size());
    mHasBeen = vector<vector<UNIT_TYPEID>>(unitTypes.size());
    for (size_t i = 0; i < unitTypes.size(); i++) {
        const UnitTypeData& unitTypeData = unitTypes[i];
        mHasBeen[i].push_back(unitTypeData.unit_type_id);

        if (unitTypeData.unit_alias != UNIT_TYPEID::INVALID)
            continue;

        // Handle building upgrades
        if (isStructure(unitTypeData)) {
            auto currentTypeData = unitTypeData;
            while (true) {
                auto createdByAlternatives = abilityToCasterUnit(currentTypeData.ability_id);
                if (createdByAlternatives.size() == 0 || createdByAlternatives[0] == UNIT_TYPEID::INVALID) {
                    // cout << "Cannot determine how " << currentTypeData.name << " was created (ability: " << AbilityTypeToName(currentTypeData.ability_id) << endl;
                    break;
                }
                UNIT_TYPEID createdBy = createdByAlternatives[0];

                currentTypeData = unitTypes[(int)createdBy];
                if (isStructure(currentTypeData) || createdBy == UNIT_TYPEID::ZERG_DRONE) {
                    if (contains(mHasBeen[i], createdBy)) {
                        // cout << "Loop in dependency tree for " << currentTypeData.name << endl;
                        break;
                    }
                    mHasBeen[i].push_back(createdBy);

                    if (createdBy == UNIT_TYPEID::ZERG_DRONE) {
                        break;
                    }
                } else {
                    break;
                }
            }
        } else {
            // Special case the relevant units
            switch ((UNIT_TYPEID)unitTypeData.unit_type_id) {
                case UNIT_TYPEID::ZERG_RAVAGER:
                    mHasBeen[i].push_back(UNIT_TYPEID::ZERG_ROACH);
                    break;
                case UNIT_TYPEID::ZERG_OVERSEER:
                    mHasBeen[i].push_back(UNIT_TYPEID::ZERG_OVERLORD);
                    break;
                case UNIT_TYPEID::ZERG_BANELING:
                    mHasBeen[i].push_back(UNIT_TYPEID::ZERG_ZERGLING);
                    break;
                case UNIT_TYPEID::ZERG_BROODLORD:
                    mHasBeen[i].push_back(UNIT_TYPEID::ZERG_CORRUPTOR);
                    break;
                case UNIT_TYPEID::ZERG_LURKERMP:
                    mHasBeen[i].push_back(UNIT_TYPEID::ZERG_HYDRALISK);
                    break;
                    // TODO: Tech labs?
                default:
                    break;
            }
        }

        for (UNIT_TYPEID previous : mHasBeen[i]) {
            auto& canBecomeArr = mCanBecome[(int)previous];
            if (previous != (UNIT_TYPEID)i && find(canBecomeArr.begin(), canBecomeArr.end(), unitTypeData.unit_type_id) == canBecomeArr.end()) {
                canBecomeArr.push_back(unitTypeData.unit_type_id);
            }
        }
    }

    for (size_t i = 0; i < unitTypes.size(); i++) {
        if (mCanBecome[i].size() == 0)
            continue;

        // Sort by lowest cost first
        sort(mCanBecome[i].begin(), mCanBecome[i].end(), [&](UNIT_TYPEID a, UNIT_TYPEID b) {
            return unitTypes[(int)a].mineral_cost + unitTypes[(int)a].vespene_cost < unitTypes[(int)b].mineral_cost + unitTypes[(int)b].vespene_cost;
        });
    }

    Weapon bc1;
    bc1.type = Weapon::TargetType::Ground;
    bc1.damage_ = 8;
    bc1.damage_bonus = {};
    bc1.attacks = 1;
    bc1.range = 6;
    bc1.speed = 0.16 * 1.4; // Note: this is in normal game speed

    Weapon bc2;
    bc2.type = Weapon::TargetType::Air;
    bc2.damage_ = 5;
    bc2.damage_bonus = {};
    bc2.attacks = 1;
    bc2.range = 6;
    bc2.speed = 0.16 * 1.4; // Note: this is in normal game speed

    Weapon carrier1;
    carrier1.type = Weapon::TargetType::Any;
    carrier1.damage_ = 5;
    carrier1.damage_bonus = {};
    carrier1.attacks = 8 * 2; // 8 interceptors with 2 attacks each
    carrier1.range = 9; // Start range is 8, but after the interceptors are launched the carrier can move up to 14 units away
    carrier1.speed = 2.14 * 1.4; // Note: this is in normal game speed

	Weapon oracle1;
	oracle1.type = Weapon::TargetType::Ground;
	oracle1.damage_ = 15;
	DamageBonus oracleBonus;
	oracleBonus.attribute = sc2::Attribute::Light;
	oracleBonus.bonus = 7;
	oracle1.damage_bonus = { oracleBonus };
	oracle1.attacks = 1;
	oracle1.range = 6;
	oracle1.speed = 0.61 * 1.4; // Note: this is in normal game speed

    // Battlecruiser's attacks don't seem to be included...
    mUnitTypes[(int)UNIT_TYPEID::TERRAN_BATTLECRUISER].weapons = { bc1, bc2 };

    mUnitTypes[(int)UNIT_TYPEID::PROTOSS_CARRIER].weapons = { carrier1 };

	mUnitTypes[(int)UNIT_TYPEID::PROTOSS_ORACLE].weapons = { oracle1 };

    // Hacky fix for build order optimizer not taking this into account
    mUnitTypes[(int)UNIT_TYPEID::PROTOSS_GATEWAY].tech_requirement = UNIT_TYPEID::PROTOSS_PYLON;

    mIsStationary = vector<bool>(mUnitTypes.size());
    mIsStructure = vector<bool>(mUnitTypes.size());
    for (size_t i = 0; i < mUnitTypes.size(); i++) {
        mIsStationary[i] = mUnitTypes[i].movement_speed <= 0.0f;
        mIsStructure[i] = isStructure(getUnitData((UNIT_TYPEID)i));
    }

    mUpgradeUnitDependency.resize(maxUpgradeID);
    mUpgradeUpgradeDependency.resize(maxUpgradeID);

    mUpgradeUnitDependency[(int)UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2] = UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL;
    mUpgradeUnitDependency[(int)UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2] = UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL;
    mUpgradeUnitDependency[(int)UPGRADE_ID::PROTOSSSHIELDSLEVEL2] = UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL;
    mUpgradeUnitDependency[(int)UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL2] = UNIT_TYPEID::PROTOSS_FLEETBEACON;
    mUpgradeUnitDependency[(int)UPGRADE_ID::PROTOSSAIRARMORSLEVEL2] = UNIT_TYPEID::PROTOSS_FLEETBEACON;

    addEnumeratedUpgradeDependency(UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::TERRANSHIPWEAPONSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::PROTOSSSHIELDSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::ZERGMELEEWEAPONSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::ZERGGROUNDARMORSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::ZERGFLYERWEAPONSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::ZERGFLYERARMORSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::PROTOSSAIRARMORSLEVEL1);
    addEnumeratedUpgradeDependency(UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL1);

    // Fix initial healths for some units
    unit_type_initial_health[(int)UNIT_TYPEID::TERRAN_REACTOR] = {50, 0};
    unit_type_initial_health[(int)UNIT_TYPEID::TERRAN_TECHLAB] = {50, 0};
    unit_type_initial_health[(int)UNIT_TYPEID::TERRAN_BARRACKSTECHLAB] = {50, 0};
    unit_type_initial_health[(int)UNIT_TYPEID::TERRAN_BARRACKSREACTOR] = {50, 0};
    unit_type_initial_health[(int)UNIT_TYPEID::TERRAN_FACTORYTECHLAB] = {50, 0};
    unit_type_initial_health[(int)UNIT_TYPEID::TERRAN_FACTORYREACTOR] = {50, 0};
    unit_type_initial_health[(int)UNIT_TYPEID::TERRAN_STARPORTTECHLAB] = {50, 0};
    unit_type_initial_health[(int)UNIT_TYPEID::TERRAN_STARPORTREACTOR] = {50, 0};
}


sc2::UNIT_TYPEID getUpgradeUnitDependency(sc2::UPGRADE_ID upgrade) {
    return mUpgradeUnitDependency[(int)upgrade];
}

sc2::UPGRADE_ID getUpgradeUpgradeDependency(sc2::UPGRADE_ID upgrade) {
    return mUpgradeUpgradeDependency[(int)upgrade];
}

void assertMappingsInitialized() {
    if (!mappingInitialized) {
        cerr << "Mappings have not been initialized, but they need to be at this point. You sohuld call initMappings()" << endl;
        assert(mappingInitialized);
    }
}

void initMappings(const ObservationInterface* observation) {
    if (mappingInitialized)
        return;
    mUnitTypes = observation->GetUnitTypeData();
    mAbilities = observation->GetAbilityData();
    mUpgrades = observation->GetUpgradeData();
    init();
}

void initMappings() {
    if (mappingInitialized)
        return;
    mUnitTypes = load_unit_data();
    mAbilities = load_ability_data();
    mUpgrades = load_upgrade_data();
    init();
}

bool isStructure(UNIT_TYPEID type) {
    return mIsStructure[(int)type];
}

bool isStationary(UNIT_TYPEID type) {
    return mIsStationary[(int)type];
}

static vector<UNIT_TYPEID> emptyVector;

/** Maps an ability to the unit that primarily uses it.
 * E.g. ABILITY_ID::BUILD_BUNKER -> UNIT_TYPEID::TERRAN_SCV
 */
const std::vector<UNIT_TYPEID>& abilityToCasterUnit(ABILITY_ID ability) {
    if ((int)ability >= (int)mAbilityToCasterUnit.size()) return emptyVector;

    assert((int)ability < (int)mAbilityToCasterUnit.size());
    return mAbilityToCasterUnit[(int)ability];
}

float maxHealth(sc2::UNIT_TYPEID type) {
    return unit_type_initial_health[(int)type].first;
}

float maxShield(sc2::UNIT_TYPEID type) {
    return unit_type_initial_health[(int)type].second;
}

bool isFlying(sc2::UNIT_TYPEID type) {
    return unit_type_is_flying[(int)type];
}

float unitRadius(sc2::UNIT_TYPEID type) {
    return unit_type_radius[(int)type];
}

/** Maps an ability to the unit that is built or trained by that ability.
 * E.g. ABILITY_ID::BUILD_ARMORY -> UNIT_TYPEID::TERRAN_ARMORY
 */
UNIT_TYPEID abilityToUnit(ABILITY_ID ability) {
    return mAbilityToCreatedUnit[(int)ability];
}

UPGRADE_ID abilityToUpgrade(ABILITY_ID ability) {
    return mAbilityToUpgrade[(int)ability];
}

UNIT_TYPEID simplifyUnitType(UNIT_TYPEID type) {
    // TODO: Extend
    switch (type) {
        case UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED:
            return UNIT_TYPEID::TERRAN_SUPPLYDEPOT;
        case UNIT_TYPEID::TERRAN_BARRACKSFLYING:
            return UNIT_TYPEID::TERRAN_BARRACKS;
        case UNIT_TYPEID::TERRAN_FACTORYFLYING:
            return UNIT_TYPEID::TERRAN_FACTORY;
        case UNIT_TYPEID::TERRAN_STARPORTFLYING:
            return UNIT_TYPEID::TERRAN_STARPORT;
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
        case UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
        case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
            return UNIT_TYPEID::TERRAN_COMMANDCENTER;
        case UNIT_TYPEID::TERRAN_LIBERATORAG:
            return UNIT_TYPEID::TERRAN_LIBERATOR;
        case UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
        case UNIT_TYPEID::TERRAN_FACTORYREACTOR:
        case UNIT_TYPEID::TERRAN_STARPORTREACTOR:
            return UNIT_TYPEID::TERRAN_REACTOR;
        case UNIT_TYPEID::TERRAN_BARRACKSTECHLAB:
        case UNIT_TYPEID::TERRAN_FACTORYTECHLAB:
        case UNIT_TYPEID::TERRAN_STARPORTTECHLAB:
            return UNIT_TYPEID::TERRAN_TECHLAB;
        default:
            return type;
    }
}

/** Maps a buff to approximately which ability that causes it */
ABILITY_ID buffToAbility(BUFF_ID type) {
    switch (type) {
        case BUFF_ID::INVALID:
            return ABILITY_ID::INVALID;
        case BUFF_ID::GRAVITONBEAM:
            return ABILITY_ID::EFFECT_GRAVITONBEAM;
        case BUFF_ID::GHOSTCLOAK:
            return ABILITY_ID::BEHAVIOR_CLOAKON_GHOST;
        case BUFF_ID::BANSHEECLOAK:
            return ABILITY_ID::BEHAVIOR_CLOAKON_BANSHEE;
        case BUFF_ID::POWERUSERWARPABLE:
            return ABILITY_ID::INVALID;
        case BUFF_ID::QUEENSPAWNLARVATIMER:
            return ABILITY_ID::INVALID;
        case BUFF_ID::GHOSTHOLDFIRE:
            return ABILITY_ID::INVALID;
        case BUFF_ID::GHOSTHOLDFIREB:
            return ABILITY_ID::INVALID;
        case BUFF_ID::EMPDECLOAK:
            return ABILITY_ID::EFFECT_EMP;
        case BUFF_ID::FUNGALGROWTH:
            return ABILITY_ID::EFFECT_FUNGALGROWTH;
        case BUFF_ID::GUARDIANSHIELD:
            return ABILITY_ID::INVALID;
        case BUFF_ID::TIMEWARPPRODUCTION:
            return ABILITY_ID::EFFECT_TIMEWARP;
        case BUFF_ID::NEURALPARASITE:
            return ABILITY_ID::EFFECT_NEURALPARASITE;
        case BUFF_ID::STIMPACKMARAUDER:
            return ABILITY_ID::EFFECT_STIM_MARAUDER;
        case BUFF_ID::SUPPLYDROP:
            return ABILITY_ID::INVALID;
        case BUFF_ID::STIMPACK:
            return ABILITY_ID::EFFECT_STIM_MARINE;
        case BUFF_ID::PSISTORM:
            return ABILITY_ID::EFFECT_PSISTORM;
        case BUFF_ID::CLOAKFIELDEFFECT:
            return ABILITY_ID::INVALID;
        case BUFF_ID::CHARGING:
            return ABILITY_ID::EFFECT_CHARGE;
        case BUFF_ID::SLOW:
            return ABILITY_ID::INVALID;
        case BUFF_ID::CONTAMINATED:
            return ABILITY_ID::EFFECT_CONTAMINATE;
        case BUFF_ID::BLINDINGCLOUDSTRUCTURE:
            return ABILITY_ID::EFFECT_BLINDINGCLOUD;
        case BUFF_ID::ORACLEREVELATION:
            return ABILITY_ID::EFFECT_ORACLEREVELATION;
        case BUFF_ID::VIPERCONSUMESTRUCTURE:
            return ABILITY_ID::EFFECT_VIPERCONSUME;
        case BUFF_ID::BLINDINGCLOUD:
            return ABILITY_ID::EFFECT_BLINDINGCLOUD;
        case BUFF_ID::MEDIVACSPEEDBOOST:
            return ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS;
        case BUFF_ID::PURIFY:
            return ABILITY_ID::EFFECT_PURIFICATIONNOVA;
        case BUFF_ID::ORACLEWEAPON:
            return ABILITY_ID::INVALID;
        case BUFF_ID::IMMORTALOVERLOAD:
            return ABILITY_ID::EFFECT_IMMORTALBARRIER;
        case BUFF_ID::LOCKON:
            return ABILITY_ID::EFFECT_LOCKON;
        case BUFF_ID::SEEKERMISSILE:
            return ABILITY_ID::EFFECT_HUNTERSEEKERMISSILE;
        case BUFF_ID::TEMPORALFIELD:
            return ABILITY_ID::INVALID;
        case BUFF_ID::VOIDRAYSWARMDAMAGEBOOST:
            return ABILITY_ID::EFFECT_VOIDRAYPRISMATICALIGNMENT;
        case BUFF_ID::ORACLESTASISTRAPTARGET:
            return ABILITY_ID::BUILD_STASISTRAP;
        case BUFF_ID::PARASITICBOMB:
            return ABILITY_ID::EFFECT_PARASITICBOMB;
        case BUFF_ID::PARASITICBOMBUNITKU:
            return ABILITY_ID::EFFECT_PARASITICBOMB;
        case BUFF_ID::PARASITICBOMBSECONDARYUNITSEARCH:
            return ABILITY_ID::EFFECT_PARASITICBOMB;
        case BUFF_ID::LURKERHOLDFIREB:
            return ABILITY_ID::BURROWDOWN_LURKER;
        case BUFF_ID::CHANNELSNIPECOMBAT:
            return ABILITY_ID::INVALID;
        case BUFF_ID::TEMPESTDISRUPTIONBLASTSTUNBEHAVIOR:
            return ABILITY_ID::EFFECT_TEMPESTDISRUPTIONBLAST;
        case BUFF_ID::CARRYMINERALFIELDMINERALS:
            return ABILITY_ID::INVALID;
        case BUFF_ID::CARRYHIGHYIELDMINERALFIELDMINERALS:
            return ABILITY_ID::INVALID;
        case BUFF_ID::CARRYHARVESTABLEVESPENEGEYSERGAS:
            return ABILITY_ID::INVALID;
        case BUFF_ID::CARRYHARVESTABLEVESPENEGEYSERGASPROTOSS:
            return ABILITY_ID::INVALID;
        case BUFF_ID::CARRYHARVESTABLEVESPENEGEYSERGASZERG:
            return ABILITY_ID::INVALID;
    }
}
