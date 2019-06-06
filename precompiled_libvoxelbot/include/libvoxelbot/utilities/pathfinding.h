#pragma once
#include <libvoxelbot/utilities/influence.h>
#include "sc2api/sc2_api.h"
#include <vector>

std::vector<sc2::Point2DI> getPath (const sc2::Point2DI from, const sc2::Point2DI to, const InfluenceMap& costs);
InfluenceMap getDistances (const InfluenceMap& startingPoints, const InfluenceMap& costs);