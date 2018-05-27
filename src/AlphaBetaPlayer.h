#pragma once
#include <vector>
#include "sc2api/sc2_api.h"
#include "AlphaBetaUnit.h"

class AlphaBetaPlayer {
public:
	AlphaBetaPlayer(std::vector<std::shared_ptr<AlphaBetaUnit>> units, bool isMax);
    std::vector<std::shared_ptr<AlphaBetaUnit>> units;
    bool isMax;
};