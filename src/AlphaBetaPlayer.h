#pragma once
#include <vector>
#include "sc2api/sc2_api.h"

class AlphaBetaPlayer {
public:
    std::vector<AlphaBetaUnit> units;
    bool isMax;
};