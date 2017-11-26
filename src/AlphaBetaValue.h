#pragma once
#include "AlphaBetaAction.h"

class AlphaBetaValue {
public:
    AlphaBetaValue(float value, AlphaBetaAction * action);
    AlphaBetaAction * action;
    float score;
};