#pragma once
#ifndef ALPHABETAUNIT_H
#define ALPHABETAUNIT_H

#include "sc2api/sc2_api.h"
#include "Util.h"
#include "CCBot.h"

class AlphaBetaAction;

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
    AlphaBetaAction * previous_action;

    // initial constructor
    AlphaBetaUnit(const sc2::Unit * actual_unit, CCBot * bot);
    AlphaBetaUnit(const sc2::Unit * pactual_unit, float php_current, float php_max, float pdps, float prange, float pcooldown_max, float pspeed, sc2::Point2D pposition, AlphaBetaAction * pprevious_action);
};

#endif 