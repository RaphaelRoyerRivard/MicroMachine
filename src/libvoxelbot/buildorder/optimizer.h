#pragma once
#include <vector>
#include "../combat/simulator.h"
#include "build_order.h"
#include "build_state.h"

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

std::pair<libvoxelbot::BuildOrder, std::vector<bool>> expandBuildOrderWithImplicitSteps (const libvoxelbot::BuildState& startState, libvoxelbot::BuildOrder buildOrder);

libvoxelbot::BuildOrder findBestBuildOrderGenetic(const std::vector<std::pair<sc2::UNIT_TYPEID, int>>& startingUnits, const std::vector<std::pair<sc2::UNIT_TYPEID, int>>& target);
libvoxelbot::BuildOrder findBestBuildOrderGenetic(const libvoxelbot::BuildState& startState, const std::vector<std::pair<sc2::UNIT_TYPEID, int>>& target, const libvoxelbot::BuildOrder* seed = nullptr, BuildOptimizerParams params = BuildOptimizerParams());
std::pair<libvoxelbot::BuildOrder, BuildOrderFitness> findBestBuildOrderGeneticWithFitness(const libvoxelbot::BuildState& startState, const std::vector<std::pair<BuildOrderItem, int>>& target, const libvoxelbot::BuildOrder* seed = nullptr, BuildOptimizerParams params = BuildOptimizerParams());
libvoxelbot::BuildOrder findBestBuildOrderGenetic(const libvoxelbot::BuildState& startState, const std::vector<std::pair<BuildOrderItem, int>>& target, const libvoxelbot::BuildOrder* seed = nullptr, BuildOptimizerParams params = BuildOptimizerParams());
void unitTestBuildOptimizer();
void printBuildOrderDetailed(const libvoxelbot::BuildState& startState, const libvoxelbot::BuildOrder& buildOrder, const std::vector<bool>* highlight = nullptr);
void optimizeExistingBuildOrder(const sc2::ObservationInterface* observation, const std::vector<const sc2::Unit*>& ourUnits, const libvoxelbot::BuildState& buildOrderStartingState, BuildOrderTracker& buildOrder, bool serialize);
BuildOrderFitness calculateFitness(const libvoxelbot::BuildState& startState, const libvoxelbot::BuildOrder& buildOrder);