#pragma once
#include "optimizer.h"
#include <vector>
#include "../../BuildOrder.h"

struct BuildOrderTracker {
  private:
    std::map<sc2::UNIT_TYPEID, int> ignoreUnits;
    std::vector<const sc2::Unit*> buildOrderUnits;
    int tick;
  public:
    std::set<const sc2::Unit*> knownUnits;
	libvoxelbot::BuildOrder buildOrder;

    BuildOrderTracker () {}
    BuildOrderTracker (libvoxelbot::BuildOrder buildOrder);

    void setBuildOrder (libvoxelbot::BuildOrder buildOrder);
    void tweakBuildOrder (std::vector<bool> keepMask, libvoxelbot::BuildOrder buildOrder);
    void addExistingUnit(const sc2::Unit* unit);

    void ignoreUnit (sc2::UNIT_TYPEID type, int count);
    std::vector<bool> update(const sc2::ObservationInterface* observation, const std::vector<const sc2::Unit*>& ourUnits);
};
