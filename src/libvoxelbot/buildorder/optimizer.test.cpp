#include "build_state.h"
#include "../common/unit_lists.h"
#include "../utilities/mappings.h"

using namespace std;
using namespace sc2;

int main () {
    initMappings();
	libvoxelbot::BuildState state {{
        { UNIT_TYPEID::PROTOSS_NEXUS, 1 },
        { UNIT_TYPEID::PROTOSS_PROBE, 12 },
    }};
    state.resources.minerals = 50;
    state.resources.vespene = 0;

    auto buildOrder = findBestBuildOrderGenetic(state, {
        { BuildOrderItem(UNIT_TYPEID::PROTOSS_ZEALOT), 1 }
    });

    int numZealots = 0;
    int numGateways = 0;
    int numPylons = 0;
    int numOthers = 0;

    for (const auto item : buildOrder) {
        if (item.typeID() == UNIT_TYPEID::PROTOSS_ZEALOT) numZealots++;
        if (item.typeID() == UNIT_TYPEID::PROTOSS_GATEWAY) numGateways++;
        if (item.typeID() == UNIT_TYPEID::PROTOSS_PYLON) numPylons++;
        if (item.typeID() != UNIT_TYPEID::PROTOSS_PROBE) numOthers++;
    }
    assert(numZealots == 1);
    assert(numGateways == 1);
    assert(numPylons == 1);
    assert(numOthers == 0);
    return 0;
}
