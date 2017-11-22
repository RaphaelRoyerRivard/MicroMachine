#pragma once
#include "sc2api/sc2_api.h"

class AlphaBetaUnit {
public:
    const sc2::Unit * actualUnit;
    float hp_current;
    float hp_max;
    float dps;
    float range;
    float cooldown_max;
    float cooldown_current;
    float speed;
    sc2::Point2D position;
};