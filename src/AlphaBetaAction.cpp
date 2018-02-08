#include "AlphaBetaAction.h"
#include "AlphaBetaUnit.h"

AlphaBetaAction::AlphaBetaAction(AlphaBetaUnit * punit, AlphaBetaUnit * ptarget, sc2::Point2D pposition, float pdistance, AlphaBetaActionType ptype, float ptime) {
    unit = punit;
    target = ptarget;
    position = pposition;
    distance = pdistance;
    type = ptype;
    time = ptime;
};