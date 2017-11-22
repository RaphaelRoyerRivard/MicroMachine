#pragma once
#include "AlphaBetaAction.h"

class AlphaBetaValue {
public:
    AlphaBetaValue(size_t, AlphaBetaAction);
    AlphaBetaValue();
    AlphaBetaAction action;
    size_t score;
};