#include "UCTCDAction.h"

UCTCDAction::UCTCDAction() { }

UCTCDAction::UCTCDAction(UCTCDUnit punit, UCTCDUnit ptarget, sc2::Point2D pposition, float pdistance, UCTCDActionType ptype, float ptime) {
    unit = punit;
    target = ptarget;
    position = pposition;
    distance = pdistance;
    type = ptype;
    time = ptime;
};