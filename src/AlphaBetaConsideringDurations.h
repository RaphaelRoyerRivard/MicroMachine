#pragma once

#include "AlphaBetaState.h"
#include "AlphaBetaValue.h"

class AlphaBetaConsideringDurations {
    size_t time_limit;
    size_t actual_time;
    size_t depth_limit;
    AlphaBetaValue alphaBeta(AlphaBetaState state, size_t depth, bool isMax, AlphaBetaAction * action, AlphaBetaValue alpha, AlphaBetaValue beta);
    AlphaBetaValue eval(AlphaBetaState state);

public:
    AlphaBetaConsideringDurations(size_t time, size_t depth);
    AlphaBetaValue doSearch(); // sc2 specific parameters will go here

};