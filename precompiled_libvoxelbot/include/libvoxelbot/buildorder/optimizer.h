#pragma once
#include <vector>
#include <cmath>
#include <functional>
#include "sc2api/sc2_interfaces.h"
#include <libvoxelbot/combat/simulator.h>
#include <libvoxelbot/buildorder/build_order.h>
#include <libvoxelbot/buildorder/build_state.h>

struct GeneUnitType {
    int type = -1;
    bool chronoBoosted = false;

    GeneUnitType() {}
    GeneUnitType(int type) : type(type), chronoBoosted(false) {}
    GeneUnitType(int type, bool chronoBoosted) : type(type), chronoBoosted(chronoBoosted) {}

    bool operator==(const GeneUnitType& other) const {
        return type == other.type && chronoBoosted == other.chronoBoosted;
    }

    bool operator!=(const GeneUnitType& other) const {
        return type != other.type || chronoBoosted != other.chronoBoosted;
    }
};

struct BuildOrderFitness {
    static const BuildOrderFitness ReallyBad;

    float time;
    BuildResources resources;
    MiningSpeed miningSpeed;
    MiningSpeed miningSpeedPerSecond;

    BuildOrderFitness () : time(0), resources(0,0), miningSpeed({0,0}), miningSpeedPerSecond({0, 0}) {}
    BuildOrderFitness (float time, BuildResources resources, MiningSpeed miningSpeed, MiningSpeed miningSpeedPerSecond) : time(time), resources(resources), miningSpeed(miningSpeed), miningSpeedPerSecond(miningSpeedPerSecond) {}

    float score() const;

    bool operator<(const BuildOrderFitness& other) const;
};

struct BuildOptimizerParams {
    int genePoolSize = 25;
    int iterations = 512;
    float mutationRateAddRemove = 0.05f;
    float mutationRateMove = 0.025f;
    float varianceBias = 0;
    bool allowChronoBoost = true;
};

std::pair<BuildOrder, std::vector<bool>> expandBuildOrderWithImplicitSteps (const BuildState& startState, BuildOrder buildOrder);

BuildOrder findBestBuildOrderGenetic(const std::vector<std::pair<sc2::UNIT_TYPEID, int>>& startingUnits, const std::vector<std::pair<sc2::UNIT_TYPEID, int>>& target);
BuildOrder findBestBuildOrderGenetic(const BuildState& startState, const std::vector<std::pair<sc2::UNIT_TYPEID, int>>& target, const BuildOrder* seed = nullptr, BuildOptimizerParams params = BuildOptimizerParams());
std::pair<BuildOrder, BuildOrderFitness> findBestBuildOrderGeneticWithFitness(const BuildState& startState, const std::vector<std::pair<BuildOrderItem, int>>& target, const BuildOrder* seed = nullptr, BuildOptimizerParams params = BuildOptimizerParams());
BuildOrder findBestBuildOrderGenetic(const BuildState& startState, const std::vector<std::pair<BuildOrderItem, int>>& target, const BuildOrder* seed = nullptr, BuildOptimizerParams params = BuildOptimizerParams());
void unitTestBuildOptimizer();
void printBuildOrderDetailed(const BuildState& startState, const BuildOrder& buildOrder, const std::vector<bool>* highlight = nullptr);
void optimizeExistingBuildOrder(const sc2::ObservationInterface* observation, const std::vector<const sc2::Unit*>& ourUnits, const BuildState& buildOrderStartingState, BuildOrderTracker& buildOrder, bool serialize);
BuildOrderFitness calculateFitness(const BuildState& startState, const BuildOrder& buildOrder);