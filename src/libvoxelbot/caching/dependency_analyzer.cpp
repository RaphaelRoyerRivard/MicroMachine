#include <map>
#include <stack>
#include "dependency_analyzer.h"
#include "../utilities/mappings.h"
#include "../utilities/predicates.h"
#include <iostream>

using namespace std;
using namespace sc2;

// //! Stable ID. This ID will not change between patches.
// UnitTypeID unit_type_id;
// //! Unit type name, corresponds to the game's catalog.
// std::string name;
// //! Cost in minerals to build this unit type.
// int mineral_cost;
// //! Cost in vespene to build this unit type.
// int vespene_cost;
// //! Weapons on this unit type.
// std::vector<Weapon> weapons;
// //! How much food the unit requires.
// float food_required;
// //! How much food the unit provides.
// float food_provided;
// //! Which ability id creates the unit.
// AbilityID ability_id;
// //! The race the unit belongs to.
// Race race;
// //! How long the unit takes to build.
// float build_time;
// //! Units this is equivalent to in terms of satisfying tech requirements.
// std::vector<UnitTypeID> tech_alias;
// //! Units that are morphed variants of the same unit.
// UnitTypeID unit_alias;
// //! Structure required to build this unit. (Or any with the same tech_alias)
// UnitTypeID tech_requirement;
// //! Whether tech_requirement is an add-on.
// bool require_attached;

map<Race, UNIT_TYPEID> gasBuilding = {
    { Race::Terran, UNIT_TYPEID::TERRAN_REFINERY },
    { Race::Protoss, UNIT_TYPEID::PROTOSS_ASSIMILATOR },
    { Race::Zerg, UNIT_TYPEID::ZERG_EXTRACTOR },
};

map<Race, UNIT_TYPEID> supplyUnit = {
    { Race::Terran, UNIT_TYPEID::TERRAN_SUPPLYDEPOT },
    { Race::Protoss, UNIT_TYPEID::PROTOSS_PYLON },
    { Race::Zerg, UNIT_TYPEID::ZERG_OVERLORD },
};

Race RaceNeutral = (Race)3;

void DependencyAnalyzer::analyze() {
    auto unitTypes = getUnitTypes();

    map<UnitTypeID, set<UnitTypeID>> requirements;
    for (UnitTypeData& unitType : unitTypes) {
        if (!unitType.available || unitType.race == RaceNeutral)
            continue;
        // if (unitType.unit_type_id != UNIT_TYPEID::ZERG_GREATERSPIRE) continue;

        if (unitType.ability_id != ABILITY_ID::INVALID) {
            // cout << "Ability: " << AbilityTypeToName(unitType.ability_id) << endl;
            try {
                auto requiredUnitOptions = abilityToCasterUnit(unitType.ability_id);
                if (requiredUnitOptions.size() == 0) {
                    // cout << UnitTypeToName(unitType.unit_type_id) << " requires ability " << AbilityTypeToName(unitType.ability_id) << " but that ability is not cast by any known unit" << endl;
                } else {
                    auto requiredUnit = UNIT_TYPEID::INVALID;
                    int minCost = 1000000;

                    // In case there are several buildings with the same ability, use the one with the lowest cost
                    // Since costs are accumulative, this will find the one lowest in the tech tree.
                    // This is useful to pick e.g. command center instead of orbital command, even though both can train SCVs.
                    for (auto opt : requiredUnitOptions) {
                        int cost = unitTypes[(int)opt].mineral_cost + unitTypes[(int)opt].vespene_cost;
                        if (cost < minCost) {
                            minCost = cost;
                            requiredUnit = opt;
                        }
                    }

                    if (requiredUnit != UNIT_TYPEID::INVALID) {
                        requirements[unitType.unit_type_id].insert(requiredUnit);

                        if (unitTypes[(int)requiredUnit].race != unitType.race) {
                            cerr << "Error in definitions" << endl;
                            cerr << unitType.name << " requires unit (via ability): " << UnitTypeToName(requiredUnit) << " via " << AbilityTypeToName(unitType.ability_id) << endl;
                        }
                    }
                }
            } catch (exception e) {
                cout << "Unhandled ability id: " << unitType.ability_id << "(" << AbilityTypeToName(unitType.ability_id) << ") for unit " << unitType.name << endl;
            }
        }

        auto requiredTechBuilding = unitType.tech_requirement;
        if (requiredTechBuilding != UNIT_TYPEID::INVALID) {
            // cout << "Requires tech: " << UnitTypeToName(requiredTechBuilding) << endl;
            requirements[unitType.unit_type_id].insert(requiredTechBuilding);
            if (unitTypes[requiredTechBuilding].race != unitType.race) {
                cerr << "Error in definitions" << endl;
                cerr << unitType.name << " requires tech: " << UnitTypeToName(requiredTechBuilding) << endl;
            }
        }

        if (unitType.vespene_cost > 0) {
            // cout << "Requires gas: " << UnitTypeToName(gasBuilding[unitType.race]) << endl;
            requirements[unitType.unit_type_id].insert(gasBuilding[unitType.race]);
        }

        if (unitType.race == Race::Protoss && isStructure(unitType) && unitType.unit_type_id != UNIT_TYPEID::PROTOSS_NEXUS && unitType.unit_type_id != UNIT_TYPEID::PROTOSS_PYLON && unitType.unit_type_id != UNIT_TYPEID::PROTOSS_ASSIMILATOR) {
            // cout << "Requires pylon" << endl;
            requirements[unitType.unit_type_id].insert(UNIT_TYPEID::PROTOSS_PYLON);
        }
    }

    allUnitDependencies = vector<vector<UNIT_TYPEID>>(unitTypes.size());
    for (size_t i = 0; i < unitTypes.size(); i++) {
        UnitTypeData& unitType = unitTypes[i];
        if (!unitType.available || unitType.race == RaceNeutral)
            continue;
        // if (unitType.unit_type_id != UNIT_TYPEID::ZERG_GREATERSPIRE) continue;
        // cout << "Unit: " << unitType.name << endl;
        vector<UNIT_TYPEID> reqs;
        stack<UNIT_TYPEID> st;
        st.push(unitType.unit_type_id);
        while (st.size() > 0) {
            auto u = st.top();
            st.pop();
            for (auto r : requirements[u]) {
                if (find(reqs.begin(), reqs.end(), r) == reqs.end()) {
                    reqs.push_back(r);
                    st.push(r);
                }
            }
        }

        allUnitDependencies[i] = reqs;
        // for (auto u : reqs) {
            // cout << unitType.name << " requires " << unitTypes[(int)u].name << endl;
        // }
    }
}
