#pragma once
#include "sc2api/sc2_api.h"
#include "AlphaBetaUnit.h"

enum class AlphaBetaActionType { ATTACK, MOVE, WAIT };

class AlphaBetaAction {
public:
    AlphaBetaAction();
    AlphaBetaAction(AlphaBetaUnit unit, AlphaBetaUnit target, sc2::Point2D position, AlphaBetaActionType type, long time);
    AlphaBetaUnit unit; // unité faisant l'action
    AlphaBetaUnit target; // cible de l'action (si possible)
    sc2::Point2D position; // position du mouvement (si possible)
    AlphaBetaActionType type;
    long time; // temps requis pour finir l'action
};