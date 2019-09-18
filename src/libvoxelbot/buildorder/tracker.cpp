#include "tracker.h"
#include "../utilities/mappings.h"
#include "../utilities/predicates.h"
#include "../utilities/stdutils.h"

using namespace std;
using namespace sc2;

BuildOrderTracker::BuildOrderTracker (libvoxelbot::BuildOrder buildOrder) : buildOrderUnits(buildOrder.size()), buildOrder(buildOrder) {
}

void BuildOrderTracker::setBuildOrder (libvoxelbot::BuildOrder buildOrder) {
    this->buildOrder = buildOrder;
    buildOrderUnits = vector<const Unit*>(buildOrder.size());
    assert(buildOrder.size() == buildOrderUnits.size());
}

void BuildOrderTracker::tweakBuildOrder (std::vector<bool> keepMask, libvoxelbot::BuildOrder tweakBuildOrder) {
    assert(keepMask.size() == buildOrder.size());
    vector<const Unit*> newUnits;
	libvoxelbot::BuildOrder newBO;

    for (size_t i = 0; i < buildOrder.size(); i++) {
        if (keepMask[i]) {
            newBO.items.push_back(buildOrder[i]);
            newUnits.push_back(buildOrderUnits[i]);
        }
    }
    
    for (auto item : tweakBuildOrder.items) {
        newBO.items.push_back(item);
        newUnits.push_back(nullptr);
    }

    buildOrder = move(newBO);
    buildOrderUnits = move(newUnits);
}

void BuildOrderTracker::addExistingUnit(const Unit* unit) {
    knownUnits.insert(unit);
}

void BuildOrderTracker::ignoreUnit (UNIT_TYPEID type, int count) {
    ignoreUnits[canonicalize(type)] += count;
}

vector<bool> BuildOrderTracker::update(const ObservationInterface* observation, const vector<const Unit*>& ourUnits) {
    assert(buildOrder.size() == buildOrderUnits.size());
    tick++;

    for (auto*& u : buildOrderUnits) {
        if (u != nullptr) {
            // If the unit is dead
            // Not sure if e.g. workers that die inside of a refinery are marked as dead properly
            // or if they just disappear. So just to be on the same side we make sure we have seen the unit somewhat recently.
            bool deadOrGone = !u->is_alive || u->last_seen_game_loop - tick > 60;

            // If a structure is destroyed then we should rebuild it to ensure the build order remains valid.
            // However if units die that is normal and we should not try to rebuild them.
            if (deadOrGone && (isStructure(u->unit_type) || u->unit_type == UNIT_TYPEID::ZERG_OVERLORD)) {
                u = nullptr;
            }
        }
    }

    map<UNIT_TYPEID, int> inProgress;
    for (auto* u : ourUnits) {
        if (!knownUnits.count(u)) {
            knownUnits.insert(u);
            auto canonicalType = canonicalize(u->unit_type);

            // New unit
            if (ignoreUnits[canonicalType] > 0) {
                ignoreUnits[canonicalType]--;
                continue;
            }

            for (size_t i = 0; i < buildOrder.size(); i++) {
                if (buildOrderUnits[i] == nullptr && (buildOrder[i].rawType() == canonicalType)) {
                    buildOrderUnits[i] = u;
                    break;
                }
            }
        }

        for (auto order : u->orders) {
            auto createdUnit = abilityToUnit(order.ability_id);
            if (createdUnit != UNIT_TYPEID::INVALID) {
                if (order.target_unit_tag != NullTag) {
                    auto* unit = observation->GetUnit(order.target_unit_tag);
                    // This unit is already existing and is being constructed.
                    // It will already have been associated with the build order item
                    if (unit != nullptr && unit->unit_type == createdUnit) continue;
                }
                inProgress[canonicalize(createdUnit)]++;
            }

            // Only process the first order (this bot should never have more than one anyway)
            break;
        }
    }

    vector<bool> res(buildOrderUnits.size());
    for (size_t i = 0; i < buildOrderUnits.size(); i++) {
        res[i] = buildOrderUnits[i] != nullptr;
        if (!buildOrder[i].isUnitType()) {
            res[i] = isUpgradeDoneOrInProgress(observation, buildOrder[i].upgradeID());
        }

        // No concrete unit may be associated with the item yet, but it may be on its way
        if (!res[i] && inProgress[buildOrder[i].rawType()] > 0) {
            inProgress[buildOrder[i].rawType()]--;
            res[i] = true;
        }
    }
    return res;
}

