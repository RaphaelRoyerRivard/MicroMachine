#pragma once

#include "sc2api/sc2_api.h"
#include "UCTCDUnit.h"

enum class UCTCDActionType { ATTACK, MOVE_BACK, MOVE_FORWARD, WAIT };

class UCTCDAction {
public:
    UCTCDUnit unit; // unité faisant l'action
    UCTCDUnit target; // cible de l'action (si possible)
    sc2::Point2D position; // position du mouvement (si possible)
    float distance; // distance entre unité et nouvelle position
    UCTCDActionType type;
    float time; // temps à laquelle l'action sera terminée

    UCTCDAction();
    UCTCDAction(UCTCDUnit punit, UCTCDUnit ptarget, sc2::Point2D pposition, float pdistance, UCTCDActionType ptype, float ptime);
};