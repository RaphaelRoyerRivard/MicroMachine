#include "AlphaBetaAction.h"

AlphaBetaAction::AlphaBetaAction(AlphaBetaUnit unit, AlphaBetaUnit target, sc2::Point2D position, AlphaBetaActionType type, long time)
    : unit(unit),
    target(target),
    position(position),
    type(type),
    time(time) { }