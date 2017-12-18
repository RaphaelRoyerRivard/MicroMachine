#include "AlphaBetaAction.h"
#include "AlphaBetaUnit.h"

AlphaBetaAction::AlphaBetaAction(AlphaBetaUnit * punit, AlphaBetaUnit * ptarget, sc2::Point2D pposition, float pdistance, AlphaBetaActionType ptype, long ptime) {
    unit = punit;
    target = ptarget;
    position = pposition;
    distance = pdistance;
    type = ptype;
    time = ptime;
};