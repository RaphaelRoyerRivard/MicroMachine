#pragma once
#include <vector>
#include "sc2api/sc2_api.h"
#include "UCTCDUnit.h"

class UCTCDPlayer {
public:
    UCTCDPlayer(std::vector<UCTCDUnit> units, bool isMax);
    std::vector<UCTCDUnit> units;
    bool isMax;
};