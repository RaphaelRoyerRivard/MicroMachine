#pragma once
#include "sc2api/sc2_api.h"

class AlphaBetaUnit {
public:
    const sc2::Unit * actual_unit;
    float hp_current;
    float hp_max;
    float dps;
    float range;
    float cooldown_max;
    float speed;
    sc2::Point2D position;
    AlphaBetaAction previous_action;
};