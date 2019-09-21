#include "build_state.h"
#include "../utilities/mappings.h"
#include "../common/unit_lists.h"

using namespace std;
using namespace sc2;

libvoxelbot::BuildState::BuildState(std::vector<std::pair<sc2::UNIT_TYPEID, int>> unitCounts) {
    assert(unitCounts.size() > 0);
    race = getUnitData(unitCounts[0].first).race;
    for (auto u : unitCounts) {
        addUnits(u.first, u.second);
        if (u.first == sc2::UNIT_TYPEID::PROTOSS_NEXUS) {
            // Add a nexus with some dummy energy
            for (int i = 0; i < u.second; i++) chronoInfo.addNexusWithEnergy(time, 25);
        }
    }
}

libvoxelbot::BuildState::BuildState(const ObservationInterface* observation, Unit::Alliance alliance, Race race, BuildResources resources, float time) : time(time), race(race), resources(resources) {
    map<UNIT_TYPEID, int> startingUnitsCount;
    map<UNIT_TYPEID, int> targetUnitsCount;

    auto ourUnits = observation->GetUnits(alliance);

    for (size_t i = 0; i < ourUnits.size(); i++) {
        auto unitType = ourUnits[i]->unit_type;

        // Addons are handled when the unit they are attached to are handled
        if (isAddon(unitType)) continue;

        // Exists at the same time as the non-phaseshift unit. Ignore it to avoid duplicates
        if (unitType == UNIT_TYPEID::PROTOSS_ADEPTPHASESHIFT) continue;

        // If the building is still in progress.
        // Don't do this for warpgates as they are just morphed, not in progress really
        if (ourUnits[i]->build_progress < 1 && unitType != UNIT_TYPEID::PROTOSS_WARPGATE) {
            if (race == Race::Terran) {
                // Ignore (will be handled by the order code below)
            } else {
                unitType = canonicalize(unitType);
                float buildTime = ticksToSeconds(getUnitData(unitType).build_time);
                float remainingTime = (1 - ourUnits[i]->build_progress) * buildTime;
                auto event = BuildEvent(BuildEventType::FinishedUnit, time + remainingTime, UNIT_TYPEID::INVALID, getUnitData(unitType).ability_id);
                // Note: don't bother to handle addons as this code never runs for terran
                addEvent(event);
            }
        } else {
            if (ourUnits[i]->add_on_tag != NullTag) {
                auto* addon = observation->GetUnit(ourUnits[i]->add_on_tag);
                // Simplify to e.g. TERRAN_REACTOR instead of TERRAN_BARRACKSREACTOR
                auto addonType = simplifyUnitType(addon->unit_type);
                assert(addonType != UNIT_TYPEID::INVALID);
                addUnits(canonicalize(unitType), addonType, 1);
            } else {
                addUnits(canonicalize(unitType), 1);
            }
        }

        // Note: at least during replays, orders don't seem to contain the target_unit_tag for build construction
        
        for (auto order : ourUnits[i]->orders) {
            auto createdUnit = abilityToUnit(order.ability_id);
            if (createdUnit != UNIT_TYPEID::INVALID) {
                if (canonicalize(createdUnit) == canonicalize(unitType)) {
                    // This is just a morph ability (e.g. lower a supply depot)
                    break;
                }

                float buildProgress = order.progress;
                if (order.target_unit_tag != NullTag) {
                    auto* targetUnit = observation->GetUnit(order.target_unit_tag);
                    if (targetUnit == nullptr) {
                        cerr << "Target for order does not seem to exist!??" << endl;
                        break;
                    }

                    assert(targetUnit != nullptr);

                    // In some cases the target unit can be something else, for example when constructing a refinery the target unit is the vespene geyser for a while
                    if (targetUnit->owner == ourUnits[i]->owner) {
                        buildProgress = targetUnit->build_progress;
                        // TODO: build_progress is not empirically always order.progress, when is it not so?
                        assert(targetUnit->build_progress >= 0 && targetUnit->build_progress <= 1);
                    }
                }

                float buildTime = ticksToSeconds(getUnitData(createdUnit).build_time);
                // TODO: Is this linear?
                float remainingTime = (1 - buildProgress) * buildTime;
                auto event = BuildEvent(BuildEventType::FinishedUnit, time + remainingTime, canonicalize(unitType), order.ability_id);
                
                if (ourUnits[i]->add_on_tag != NullTag) {
                    auto* addon = observation->GetUnit(ourUnits[i]->add_on_tag);
                    assert(addon != nullptr);
                    // Normalize from e.g. TERRAN_BARRACKSTECHLAB to TERRAN_TECHLAB
                    event.casterAddon = simplifyUnitType(addon->unit_type);
                }

                // Make the caster busy first
                makeUnitsBusy(event.caster, event.casterAddon, 1);
                if (event.caster == UNIT_TYPEID::PROTOSS_PROBE) {
                    // Probes are special cased as they are not busy for the whole duration of the build.
                    // Assume the probe will be free in a few seconds
                    addEvent(BuildEvent(BuildEventType::MakeUnitAvailable, time + min(remainingTime, 4.0f), event.caster, sc2::ABILITY_ID::INVALID));
                }

                addEvent(event);
            }

            // Only process the first order (this bot should never have more than one anyway)
            break;
        }
    }

    if (alliance == Unit::Alliance::Self) {
    // Add all upgrades
        auto& availableUnits = getAvailableUnitsForRace(race, UnitCategory::BuildOrderOptions);
        for (size_t i = 0; i < availableUnits.size(); i++) {
            auto item = availableUnits.getBuildOrderItem(i);
            if (!item.isUnitType()) {
                // Note: slightly incorrect. Assumes warpgate research is already done when it may actually be in progress.
                // But since the proper event for upgrades is not added above, this is better than nothing.
                if (isUpgradeDoneOrInProgress(observation, item.upgradeID())) {
                    upgrades.add(item.upgradeID());
                }
            }
        }
    }

    vector<Point2D> basePositions;
    for (auto u : ourUnits) {
        if (isTownHall(u->unit_type) && u->build_progress >= 1) {
            basePositions.push_back(u->pos);
            baseInfos.push_back(BaseInfo(0, 0, 0));

            if (u->unit_type == UNIT_TYPEID::PROTOSS_NEXUS) {
                chronoInfo.addNexusWithEnergy(time, u->energy);
            }
        }
    }
    auto neutralUnits = observation->GetUnits(Unit::Alliance::Neutral);
    for (auto u : neutralUnits) {
        if (u->mineral_contents > 0) {
            for (size_t i = 0; i < baseInfos.size(); i++) {
                if (DistanceSquared2D(u->pos, basePositions[i]) < 10*10) {
                    baseInfos[i].remainingMinerals += u->mineral_contents;
                    break;
                }
            }
        }
    }

    // Sanity check
    for (auto u : units) {
        assert(u.availableUnits() >= 0);
    }
}

void libvoxelbot::BuildState::transitionToWarpgates (const function<void(const BuildEvent&)>* eventCallback) {
    assert(upgrades.hasUpgrade(sc2::UPGRADE_ID::WARPGATERESEARCH));
    const float WarpGateTransitionTime = 7;
    for (auto& u : units) {
        if (u.type == UNIT_TYPEID::PROTOSS_GATEWAY && u.busyUnits < u.units) {
            int delta = u.units - u.busyUnits;
            u.units -= delta;
            assert(u.units >= 0);
            assert(u.availableUnits() >= 0);
            addUnits(UNIT_TYPEID::PROTOSS_WARPGATE, delta);
            makeUnitsBusy(UNIT_TYPEID::PROTOSS_WARPGATE, UNIT_TYPEID::INVALID, delta);
            for (int i = 0; i < delta; i++) {
                addEvent(BuildEvent(BuildEventType::MakeUnitAvailable, time + WarpGateTransitionTime, UNIT_TYPEID::PROTOSS_WARPGATE, ABILITY_ID::MORPH_WARPGATE));
            }

            // Note: event not actually used in the simulator, only used for the callback
            if (eventCallback != nullptr) {
                for(int i = 0; i < delta; i++) {
                    (*eventCallback)(BuildEvent(BuildEventType::WarpGateTransition, time, UNIT_TYPEID::PROTOSS_GATEWAY, ABILITY_ID::MORPH_WARPGATE));
                }
            }
            // Note: important to break as the addUnits call may invalidate the iterator
            break;
        }
    }
}

void libvoxelbot::BuildState::makeUnitsBusy(UNIT_TYPEID type, UNIT_TYPEID addon, int delta) {
    if (delta == 0)
        return;

    for (auto& u : units) {
        if (u.type == type && u.addon == addon) {
            u.busyUnits += delta;
            assert(u.availableUnits() >= 0);
            assert(u.busyUnits >= 0);

            // Ensure gateways transition to warpgates as soon as possible
            // Note: cannot do this because it may invalidate the events vector which may mess with the BuildEvent::apply method.
            // if (type == UNIT_TYPEID::PROTOSS_GATEWAY && hasWarpgateResearch && delta < 0) transitionToWarpgates();
            return;
        }
    }
    assert(false);
}

void libvoxelbot::BuildState::addUnits(UNIT_TYPEID type, int delta) {
    addUnits(type, UNIT_TYPEID::INVALID, delta);
}

void libvoxelbot::BuildState::addUnits(UNIT_TYPEID type, UNIT_TYPEID addon, int delta) {
    if (delta == 0)
        return;

    for (auto& u : units) {
        if (u.type == type && u.addon == addon) {
            u.units += delta;
            if (u.availableUnits() < 0) {
                cout << "Buggy units? " << UnitTypeToName(u.type) << " " << u.availableUnits() << " " << u.units << " " << u.busyUnits << endl;
            }
            assert(u.availableUnits() >= 0);
            return;
        }
    }

    if (delta > 0) {
        units.emplace_back(type, addon, delta);
    } else {
        cerr << "Cannot remove " << UnitTypeToName(type) << endl;
        assert(false);
    }
}

void libvoxelbot::BuildState::killUnits(UNIT_TYPEID type, UNIT_TYPEID addon, int count) {
    if (count == 0)
        return;
    
    assert(count > 0);

    for (auto& u : units) {
        if (u.type == type && u.addon == addon) {
            u.units -= count;
            assert(u.units >= 0);
            while(u.availableUnits() < 0) {
                bool found = false;
                for (int i = events.size() - 1; i >= 0; i--) {
                    auto& ev = events[i];
                    if (ev.caster == type && ev.casterAddon == addon && (ev.type == BuildEventType::MakeUnitAvailable || (ev.type == BuildEventType::FinishedUnit && type != UNIT_TYPEID::PROTOSS_PROBE) || ev.type == BuildEventType::FinishedUpgrade)) {
                        // This event is guaranteed to keep a unit busy
                        // Let's erase the event to free the unit for other work
                        // Note that FinishedUnit events with caster==Probe do not keep the probe busy: there will be a second MakeUnitAvailable event that marks the probe as busy for a shorter time
                        events.erase(events.begin() + i);
                        u.busyUnits--;
                        found = true;
                        break;
                    }
                }

                // TODO: Check if this happens oftens, if so it might be worth it to optimize this case
                if (!found) {
                    // Forcefully remove busy units.
                    // Usually they are occupied with some event, but in some cases they are just marked as busy.
                    // For example workers for a few seconds at the start of the game to simulate a delay.
                    u.busyUnits--;
                    cerr << "Forcefully removed busy unit " << UnitTypeToName(u.type) << " " << u.units << " " << u.busyUnits << " " << count << endl;
                    for (auto u : units) {
                        cerr << "Unit " << UnitTypeToName(u.type) << " " << u.units << "(-" << u.busyUnits << ")" << endl;
                    }
                    cerr << "Event count: " << events.size() << endl;
                    for (auto e : events) {
                        cerr << "Event " << e.time << " " << e.type << " " << UnitTypeToName(e.caster) << endl;
                    }
                    assert(false);
                }
            }
            assert(u.availableUnits() >= 0);
            return;
        }
    }

    assert(false);
}

void MiningSpeed::simulateMining (libvoxelbot::BuildState& state, float dt) const {
    float totalWeight = 0;
    for (auto& base : state.baseInfos) {
        auto slots = base.mineralSlots();
        float weight = slots.first * 1.5f + slots.second;
        totalWeight += weight;
    }
    float normalizationFactor = 1.0f / (totalWeight + 0.0001f);
    float deltaMineralsPerWeight = mineralsPerSecond * dt * normalizationFactor;
    for (auto& base : state.baseInfos) {
        auto slots = base.mineralSlots();
        float weight = slots.first * 1.5f + slots.second;
        base.mineMinerals(deltaMineralsPerWeight * weight);
    }
    state.resources.minerals += mineralsPerSecond * dt;
    state.resources.vespene += vespenePerSecond * dt;
}

/** Returns a new build time assuming the process is accelerated using a chrono boost that ends at the specified time */
float modifyBuildTimeWithChronoBoost (float currentTime, float chronoEndTime, float buildTime) {
    if (chronoEndTime <= currentTime) return buildTime;

    // Amount of time which will be chrono boosted
    float chronoTime = min(buildTime * 0.666666f, chronoEndTime - currentTime);
    buildTime -= chronoTime/2.0f;
    return buildTime;
}

std::pair<bool, float> ChronoBoostInfo::getChronoBoostEndTime(sc2::UNIT_TYPEID caster, float currentTime) {
    for (int i = chronoEndTimes.size() - 1; i >= 0; i--) {
        auto p = chronoEndTimes[i];
        if (p.first == caster) {
            if (p.second <= currentTime) {
                // Too late, chrono has ended already
                chronoEndTimes.erase(chronoEndTimes.begin() + i);
                continue;
            }

            // Chrono still active!
            float endTime = p.second;
            chronoEndTimes.erase(chronoEndTimes.begin() + i);
            return make_pair(true, endTime);
        }
    }

    return make_pair(false, 0);
}

std::pair<bool, float> ChronoBoostInfo::useChronoBoost(float time) {
    for (auto& offset : energyOffsets) {
        float currentEnergy = time * NexusEnergyPerSecond + offset;
        if (currentEnergy >= ChronoBoostEnergy) {
            offset -= ChronoBoostEnergy;
            return make_pair(true, time + ChronoBoostDuration);
        }
    }

    return make_pair(false, 0);
}

MiningSpeed libvoxelbot::BuildState::miningSpeed() const {
    int harvesters = 0;
    int mules = 0;
    int bases = 0;
    int geysers = 0;
    for (auto& unit : units) {
        // TODO: Normalize type?
        if (isBasicHarvester(unit.type)) {
            harvesters += unit.availableUnits();
        }

        if (unit.type == UNIT_TYPEID::TERRAN_MULE) {
            mules += unit.availableUnits();
        }

        if (isTownHall(unit.type)) {
            bases += unit.units;
        }

        if (isVespeneHarvester(unit.type)) {
            geysers += unit.units;
        }
    }

    int highYieldMineralHarvestingSlots = 0;
    int lowYieldMineralHarvestingSlots = 0;
    for (int i = 0; i < bases; i++) {
        if (i < (int)baseInfos.size()) {
            auto t = baseInfos[i].mineralSlots();
            highYieldMineralHarvestingSlots += t.first;
            lowYieldMineralHarvestingSlots += t.second;
        } else {
            // Assume lots of minerals
            highYieldMineralHarvestingSlots += 16;
            lowYieldMineralHarvestingSlots += 8;
        }
    }

    int vespeneMining = min(harvesters / 2, geysers * 3);
    int mineralMining = harvesters - vespeneMining;

    // Maximum effective harvesters (todo: account for more things)
    // First 2 harvesters per mineral field yield more minerals than the 3rd one.
    int highYieldHarvesters = min(highYieldMineralHarvestingSlots, mineralMining);
    int lowYieldHarvesters = min(lowYieldMineralHarvestingSlots, mineralMining - highYieldHarvesters);

    // TODO: Check units here!
    const float FasterSpeedMultiplier = 1.4f;
    const float LowYieldMineralsPerMinute = 22 * FasterSpeedMultiplier;
    const float HighYieldMineralsPerMinute = 40 * FasterSpeedMultiplier;
    const float VespenePerMinute = 38 * FasterSpeedMultiplier;
    const float MinutesPerSecond = 1 / 60.0f;

    MiningSpeed speed;
    // cout << mineralMining << " " << highYieldHarvesters << " " << lowYieldHarvesters << " " << foodAvailable() << endl;
    speed.mineralsPerSecond = (lowYieldHarvesters * LowYieldMineralsPerMinute + highYieldHarvesters * HighYieldMineralsPerMinute) * MinutesPerSecond;
    speed.vespenePerSecond = vespeneMining * VespenePerMinute * MinutesPerSecond;

    return speed;
}

float libvoxelbot::BuildState::timeToGetResources(MiningSpeed miningSpeed, float mineralCost, float vespeneCost) const {
    mineralCost -= resources.minerals;
    vespeneCost -= resources.vespene;
    float time = 0;
    if (mineralCost > 0) {
        if (miningSpeed.mineralsPerSecond == 0)
            return numeric_limits<float>::infinity();
        time = mineralCost / miningSpeed.mineralsPerSecond;
    }
    if (vespeneCost > 0) {
        if (miningSpeed.vespenePerSecond == 0)
            return numeric_limits<float>::infinity();
        time = max(time, vespeneCost / miningSpeed.vespenePerSecond);
    }
    return time;
}

void libvoxelbot::BuildState::addEvent(BuildEvent event) {
    // TODO: Insertion sort or something
    events.push_back(event);
    sort(events.begin(), events.end());
}

// All actions up to and including the end time will have been completed
void libvoxelbot::BuildState::simulate(float endTime, const function<void(const BuildEvent&)>* eventCallback) {
    if (endTime <= time)
        return;

    auto currentMiningSpeed = miningSpeed();
    // int eventIndex;
    // for (eventIndex = 0; eventIndex < events.size(); eventIndex++) {
    while(events.size() > 0) {
        // TODO: Slow
        auto ev = *events.begin();

        // auto& ev = events[eventIndex];
        if (ev.time > endTime) {
            break;
        }

        events.erase(events.begin());
        float dt = ev.time - time;
        currentMiningSpeed.simulateMining(*this, dt);
        time = ev.time;

        ev.apply(*this);

        if (ev.impactsEconomy()) {
            currentMiningSpeed = miningSpeed();
        } else {
            // Ideally this would always hold, but when we simulate actual bases with mineral patches the approximations used are not entirely accurate and the mining rate may change even when there is no economically significant event
            assert(baseInfos.size() > 0 || currentMiningSpeed == miningSpeed());
        }

        if (eventCallback != nullptr) (*eventCallback)(ev);
        
        // TODO: Maybe a bit slow...
        if (upgrades.hasUpgrade(sc2::UPGRADE_ID::WARPGATERESEARCH)) transitionToWarpgates(eventCallback);
    }

    // events.erase(events.begin(), events.begin() + eventIndex);

    {
        float dt = endTime - time;
        currentMiningSpeed.simulateMining(*this, dt);
        time = endTime;
    }
}

bool libvoxelbot::BuildState::simulateBuildOrder(const BuildOrder& buildOrder, const function<void(int)> callback, bool waitUntilItemsFinished) {
    BuildOrderState state(make_shared<BuildOrder>(buildOrder));
    return simulateBuildOrder(state, callback, waitUntilItemsFinished);
}

/** Returns the time it takes for a warpgate to build the unit.
 * If the unit cannot be built in a warpgate the default building time is returned.
 */
float getWarpgateBuildingTime(UNIT_TYPEID unitType, float defaultTime) {
    switch(unitType) {
        case UNIT_TYPEID::PROTOSS_ZEALOT:
            return 20;
        case UNIT_TYPEID::PROTOSS_SENTRY:
            return 23;
        case UNIT_TYPEID::PROTOSS_STALKER:
            return 23;
        case UNIT_TYPEID::PROTOSS_ADEPT:
            return 20;
        case UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
            return 32;
        case UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
            return 32;
        default:
            return defaultTime;
    }
}

bool libvoxelbot::BuildState::simulateBuildOrder(BuildOrderState& buildOrder, const function<void(int)> callback, bool waitUntilItemsFinished, float maxTime, const function<void(const BuildEvent&)>* eventCallback) {
    float lastEventInBuildOrder = 0;

    // Loop through the build order
    for (; buildOrder.buildIndex < (int)buildOrder.buildOrder->size(); buildOrder.buildIndex++) {
        auto item = (*buildOrder.buildOrder)[buildOrder.buildIndex];
        if (item.chronoBoosted) buildOrder.lastChronoUnit = item.rawType();

        while (true) {
            float nextSignificantEvent = numeric_limits<float>::infinity();
            for (auto& ev : events) {
                if (ev.impactsEconomy()) {
                    nextSignificantEvent = ev.time;
                    break;
                }
            }

            bool isUnitAddon;
            int mineralCost, vespeneCost;
            ABILITY_ID ability;
            UNIT_TYPEID techRequirement;
            bool techRequirementIsAddon, isItemStructure;
            float buildTime;

            if (item.isUnitType()) {
                auto unitType = item.typeID();
                auto& unitData = getUnitData(unitType);

                if ((unitData.tech_requirement != UNIT_TYPEID::INVALID && !unitData.require_attached && !hasEquivalentTech(unitData.tech_requirement)) || (unitData.food_required > 0 && foodAvailable() < unitData.food_required)) {
                    if (events.empty()) {
                        // cout << "No tech at index " << buildOrder.buildIndex << endl;
                        return false;
                    }

                    if (events[0].time > maxTime) {
                        simulate(maxTime, eventCallback);
                        return true;
                    }
                    simulate(events[0].time, eventCallback);
                    continue;
                }

                isUnitAddon = isAddon(unitType);

                // TODO: Maybe just use lookup table
                mineralCost = unitData.mineral_cost;
                vespeneCost = unitData.vespene_cost;
                UNIT_TYPEID previous = upgradedFrom(unitType);
                if (previous != UNIT_TYPEID::INVALID && !isUnitAddon) {
                    auto& previousUnitData = getUnitData(previous);
                    mineralCost -= previousUnitData.mineral_cost;
                    vespeneCost -= previousUnitData.vespene_cost;
                }

                ability = unitData.ability_id;
                techRequirement = unitData.tech_requirement;
                techRequirementIsAddon = unitData.require_attached;
                isItemStructure = isStructure(unitType);
                buildTime = ticksToSeconds(unitData.build_time);
            } else {
                auto upgrade = item.upgradeID();
                auto upgradeDependency = getUpgradeUpgradeDependency(upgrade);
                auto unitDependency = getUpgradeUnitDependency(upgrade);
                if ((upgradeDependency != UPGRADE_ID::INVALID && !upgrades.hasUpgrade(upgradeDependency)) || (unitDependency != UNIT_TYPEID::INVALID && !hasEquivalentTech(unitDependency))) {
                    if (events.empty()) {
                        cout << "No tech at index " << buildOrder.buildIndex << endl;
                        return false;
                    }

                    if (events[0].time > maxTime) {
                        simulate(maxTime, eventCallback);
                        return true;
                    }
                    simulate(events[0].time, eventCallback);
                    continue;
                }

                auto& upgradeData = getUpgradeData(item.upgradeID());

                isUnitAddon = false;
                mineralCost = upgradeData.mineral_cost;
                vespeneCost = upgradeData.vespene_cost;
                ability = upgradeData.ability_id;
                techRequirement = UNIT_TYPEID::INVALID;
                techRequirementIsAddon = false;
                isItemStructure = false;
                buildTime = ticksToSeconds(upgradeData.research_time);
            }

            auto currentMiningSpeed = miningSpeed();
            // When do we have enough resources for this item
            float eventTime = time + timeToGetResources(currentMiningSpeed, mineralCost, vespeneCost);

            // If it would be after the next economically significant event then the time estimate is likely not accurate (the mining speed might change in the middle)
            if (eventTime > nextSignificantEvent && nextSignificantEvent <= maxTime) {
                simulate(nextSignificantEvent, eventCallback);
                continue;
            }
            
            if (eventTime > maxTime) {
                simulate(maxTime, eventCallback);
                return true;
            }

            if (isinf(eventTime)) {
                // This can happen in some cases.
                // Most common is when the unit requires vespene gas, but the player only has 1 scv and that one will be allocated to minerals.
                return false;
            }

            // Fast forward until we can pay for the item
            simulate(eventTime, eventCallback);

            // Make sure that some unit can cast this ability
            if (abilityToCasterUnit(ability).size() == 0) {
                cerr << "No known caster of the ability " << AbilityTypeToName(ability) << " for unit " << UnitTypeToName(item.rawType()) << endl;
                assert(false);
            }

            // Find an appropriate caster for this ability
            BuildUnitInfo* casterUnit = nullptr;
            UNIT_TYPEID casterUnitType = UNIT_TYPEID::INVALID;
            UNIT_TYPEID casterAddonType = UNIT_TYPEID::INVALID;
            for (UNIT_TYPEID caster : abilityToCasterUnit(ability)) {
                for (auto& casterCandidate : units) {
                    if (casterCandidate.type == caster && casterCandidate.availableUnits() > 0 && (!techRequirementIsAddon || casterCandidate.addon == techRequirement)) {
                        // Addons can only be added to units that do not yet have any other addons
                        if (isUnitAddon && casterCandidate.addon != UNIT_TYPEID::INVALID)
                            continue;

                        // Prefer to use casters that do not have addons
                        if (casterUnit == nullptr || casterUnit->addon != UNIT_TYPEID::INVALID) {
                            casterUnit = &casterCandidate;
                            casterUnitType = caster;
                            casterAddonType = casterCandidate.addon;
                        }
                    }
                }
            }

            // If we don't have a caster yet, then we might have to simulate a bit (the caster might be training right now, or currently busy)
            if (casterUnit == nullptr) {
                if (casterUnitType == UNIT_TYPEID::ZERG_LARVA) {
                    addUnits(UNIT_TYPEID::ZERG_LARVA, 1);
                    continue;
                }

                if (events.empty()) {
                    // cout << "No possible caster " << UnitTypeToName(unitType) << endl;
                    return false;
                }

                if (events[0].time > maxTime) {
                    simulate(maxTime, eventCallback);
                    return true;
                }
                simulate(events[0].time, eventCallback);
                continue;
            }

            // Pay for the item
            resources.minerals -= mineralCost;
            resources.vespene -= vespeneCost;

            // Mark the caster as being busy
            casterUnit->busyUnits++;
            assert(casterUnit->availableUnits() >= 0);

            if (casterUnit->type == UNIT_TYPEID::PROTOSS_WARPGATE) {
                buildTime = getWarpgateBuildingTime(item.typeID(), buildTime);
            }

            // Try to use an existing chrono boost
            pair<bool, float> chrono = chronoInfo.getChronoBoostEndTime(casterUnit->type, time);

            if (buildOrder.lastChronoUnit == item.rawType()) {
                // Use a new chrono boost if this unit is supposed to be chrono boosted
                if (!chrono.first) {
                    chrono = chronoInfo.useChronoBoost(time);
                }

                if (chrono.first) {
                    // Clear the flag because the unit was indeed boosted
                    // If it was not, the flag will remain and the next unit of the same type will be boosted
                    buildOrder.lastChronoUnit = UNIT_TYPEID::INVALID;
                }
            }

            if (chrono.first) {
                // cout << "Modified build time " << time << "," << chrono.second << " = " << buildTime << " -- > ";
                buildTime = modifyBuildTimeWithChronoBoost(time, chrono.second, buildTime);                
                // cout << buildTime << endl;
            }

            // Compensate for workers having to move to the building location
            /*if (isItemStructure) {
                buildTime += 4;
            }*/

            // Create a new event for when the item is complete
            auto newEvent = BuildEvent(item.isUnitType() ? BuildEventType::FinishedUnit : BuildEventType::FinishedUpgrade, time + buildTime, casterUnit->type, ability);
            newEvent.casterAddon = casterUnit->addon;
            newEvent.chronoEndTime = chrono.second;
            lastEventInBuildOrder = max(lastEventInBuildOrder, newEvent.time);
            addEvent(newEvent);
            if (casterUnit->type == UNIT_TYPEID::PROTOSS_PROBE) {
                addEvent(BuildEvent(BuildEventType::MakeUnitAvailable, time + 6, UNIT_TYPEID::PROTOSS_PROBE, ABILITY_ID::INVALID));
            }

            if (callback != nullptr)
                callback(buildOrder.buildIndex);
            break;
        }
    }

    if (waitUntilItemsFinished) simulate(lastEventInBuildOrder, eventCallback);
    return true;
}

float libvoxelbot::BuildState::foodCap() const {
    float totalSupply = 0;
    for (auto& unit : units) {
        auto& data = getUnitData(unit.type);
        totalSupply += data.food_provided * unit.units;
    }
    return totalSupply;
}

// Note that food is a floating point number, zerglings in particular use 0.5 food.
// It is still safe to work with floating point numbers because they can exactly represent whole numbers and whole numbers + 0.5 exactly up to very large values.
float libvoxelbot::BuildState::foodAvailable() const {
    float totalSupply = 0;
    for (auto& unit : units) {
        auto& data = getUnitData(unit.type);
        totalSupply += (data.food_provided - data.food_required) * unit.units;
    }
    // Units in construction use food, but they don't provide food (yet)
    for (auto& ev : events) {
        UNIT_TYPEID unit = abilityToUnit(ev.ability);
        if (unit != UNIT_TYPEID::INVALID) {
            totalSupply -= getUnitData(unit).food_required;
        }
    }

    // Not necessarily true in all game states
    // assert(totalSupply >= 0);
    return totalSupply;
}

float libvoxelbot::BuildState::foodAvailableInFuture() const {
    float totalSupply = 0;
    for (auto& unit : units) {
        auto& data = getUnitData(unit.type);
        totalSupply += (data.food_provided - data.food_required) * unit.units;
    }
    // Units in construction use food and will provide food
    for (auto& ev : events) {
        UNIT_TYPEID unit = abilityToUnit(ev.ability);
        if (unit != UNIT_TYPEID::INVALID) {
            totalSupply += getUnitData(unit).food_provided - getUnitData(unit).food_required;
        }
    }

    // Not necessarily true in all game states
    // assert(totalSupply >= 0);
    return totalSupply;
}

bool libvoxelbot::BuildState::hasEquivalentTech(UNIT_TYPEID type) const {
    for (auto& unit : units) {
        auto& unitData = getUnitData(unit.type);
        if (unit.units > 0) {
            if (unit.type == type) {
                return true;
            }
            for (auto t : unitData.tech_alias)
                if (t == type)
                    return true;
        }
    }
    return false;
}

bool BuildEvent::impactsEconomy() const {
    // TODO: Optimize?
    // TODO: isStructure is very aggressive, isTownHall is more appropriate?
    UNIT_TYPEID unit = abilityToUnit(ability);
    return isBasicHarvester(unit) || isBasicHarvester(caster) || isStructure(unit) || getUnitData(unit).food_provided > 0;
}

void BuildEvent::apply(libvoxelbot::BuildState& state) const {
    switch (type) {
        case FinishedUnit: {
            UNIT_TYPEID unit = abilityToUnit(ability);
            assert(unit != UNIT_TYPEID::INVALID);

            // Probes are special because they don't actually have to stay while the building is being built
            // Another event will make them available a few seconds after the order has been issued.
            if (caster != UNIT_TYPEID::PROTOSS_PROBE && caster != UNIT_TYPEID::INVALID) {
                state.makeUnitsBusy(caster, casterAddon, -1);
            }

            if (isAddon(unit)) {
                // Normalize from e.g. TERRAN_BARRACKSTECHLAB to TERRAN_TECHLAB
                assert(caster != UNIT_TYPEID::INVALID);
                state.addUnits(caster, simplifyUnitType(unit), 1);
            } else {
                state.addUnits(unit, 1);
            }

            auto upgradedFromUnit = upgradedFrom(unit);
            if (upgradedFromUnit != UNIT_TYPEID::INVALID) {
                state.addUnits(upgradedFromUnit, casterAddon, -1);
            }

            if (unit == UNIT_TYPEID::PROTOSS_NEXUS) {
                state.chronoInfo.addNexusWithEnergy(state.time, NexusInitialEnergy);
            }
            break;
        }
        case FinishedUpgrade: {
            state.upgrades.add(abilityToUpgrade(ability));
            // Make the caster unit available again
            assert(caster != UNIT_TYPEID::INVALID);
            state.makeUnitsBusy(caster, casterAddon, -1);
            break;
        }
        case SpawnLarva: {
            state.addUnits(UNIT_TYPEID::ZERG_LARVA, 3);
            break;
        }
        case MuleTimeout: {
            state.addUnits(UNIT_TYPEID::TERRAN_MULE, -1);
            break;
        }
        case MakeUnitAvailable: {
            assert(caster != UNIT_TYPEID::INVALID);
            state.makeUnitsBusy(caster, casterAddon, -1);
            break;
        }
        case WarpGateTransition: {
            assert(false);
            break;
        }
    }

    // Add remaining chrono boost to be used for other things
    if (chronoEndTime > state.time) state.chronoInfo.addRemainingChronoBoost(caster, chronoEndTime);
}