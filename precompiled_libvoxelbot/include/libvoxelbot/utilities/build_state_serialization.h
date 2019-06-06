#pragma once
#include <cereal/cereal.hpp>
#include <libvoxelbot/utilities/cereal_json.h>
#include <libvoxelbot/buildorder/build_time_estimator.h>
#include <libvoxelbot/combat/simulator.h>
#include <cereal/types/string.hpp>
#include <cereal/types/bitset.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/common.hpp>
#include <cereal/types/utility.hpp>


template <class Archive>
void serialize(Archive& archive, MiningSpeed& data) {
    archive(
        cereal::make_nvp("mineralsPerSecond", data.mineralsPerSecond),
        cereal::make_nvp("vespenePerSecond", data.vespenePerSecond)
    );
}

template <class Archive>
void serialize(Archive& archive, BuildOrderFitness& data) {
    archive(
        cereal::make_nvp("time", data.time),
        cereal::make_nvp("resources", data.resources),
        cereal::make_nvp("miningSpeed", data.miningSpeed),
        cereal::make_nvp("miningSpeedPerSecond", data.miningSpeedPerSecond)
    );
}

template <class Archive>
void serialize(Archive& archive, BuildOrderItem& data) {
    archive(
        cereal::make_nvp("type", data.internalType),
        cereal::make_nvp("chronoBoosted", data.chronoBoosted)
    );
}

template <class Archive>
void serialize(Archive& archive, BuildOrder& data) {
    archive(
        cereal::make_nvp("items", data.items)
    );
}

template <class Archive>
void serialize(Archive& archive, CombatUpgrades& data) {
    archive(
        cereal::make_nvp("upgrades", data.upgrades)
    );
}

template <class Archive>
void serialize(Archive& archive, ChronoBoostInfo& data) {
    archive(
        cereal::make_nvp("energyOffsets", data.energyOffsets),
        cereal::make_nvp("chronoEndTimes", data.chronoEndTimes)
    );
}

template <class Archive>
void serialize(Archive& archive, BaseInfo& data) {
    archive(
        cereal::make_nvp("remainingMinerals", data.remainingMinerals),
        cereal::make_nvp("remainingVespene1", data.remainingVespene1),
        cereal::make_nvp("remainingVespene2", data.remainingVespene2)
    );
}

template <class Archive>
void serialize(Archive& archive, BuildEvent& data) {
    archive(
        cereal::make_nvp("type", data.type),
        cereal::make_nvp("ability", data.ability),
        cereal::make_nvp("caster", data.caster),
        cereal::make_nvp("casterAddon", data.casterAddon),
        cereal::make_nvp("time", data.time),
        cereal::make_nvp("chronoEndTime", data.chronoEndTime)
    );
}

template <class Archive>
void serialize(Archive& archive, BuildResources& data) {
    archive(
        cereal::make_nvp("minerals", data.minerals),
        cereal::make_nvp("vespene", data.vespene)
    );
}

template <class Archive>
void serialize(Archive& archive, BuildUnitInfo& data) {
    archive(
        cereal::make_nvp("type", data.type),
        cereal::make_nvp("addon", data.addon),
        cereal::make_nvp("units", data.units),
        cereal::make_nvp("busyUnits", data.busyUnits)
    );
}

template <class Archive>
void serialize(Archive& archive, BuildState& state) {
    archive(
        cereal::make_nvp("time", state.time),
        cereal::make_nvp("race", state.race),
        cereal::make_nvp("units", state.units),
        cereal::make_nvp("events", state.events),
        cereal::make_nvp("resources", state.resources),
        cereal::make_nvp("baseInfos", state.baseInfos),
        cereal::make_nvp("chronoInfo", state.chronoInfo),
        cereal::make_nvp("upgrades", state.upgrades)
    );
}
