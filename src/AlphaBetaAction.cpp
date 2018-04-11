#include "AlphaBetaAction.h"
#include "AlphaBetaUnit.h"

AlphaBetaAction::AlphaBetaAction() { }

AlphaBetaAction::AlphaBetaAction(std::shared_ptr<AlphaBetaUnit> punit, std::shared_ptr<AlphaBetaUnit> ptarget, sc2::Point2D pposition, float pdistance, AlphaBetaActionType ptype, float ptime) {
    unit = punit;
    target = ptarget;
    position = pposition;
    distance = pdistance;
    type = ptype;
    time = ptime;
};