#pragma once

#include <chrono>
#include "AlphaBetaState.h"
#include "AlphaBetaValue.h"
#include "AlphaBetaMove.h"
#include "CCBot.h"

class AlphaBetaConsideringDurations {
    std::chrono::milliseconds time_limit;
    std::chrono::high_resolution_clock::time_point start;
    size_t actual_time;
    size_t depth_limit;

    AlphaBetaValue alphaBeta(AlphaBetaState state, size_t depth, AlphaBetaMove * m0, AlphaBetaValue alpha, AlphaBetaValue beta);

public:
    size_t nodes_evaluated;
    AlphaBetaConsideringDurations(std::chrono::milliseconds time, size_t depth);
    AlphaBetaValue doSearch(std::vector<std::shared_ptr<AlphaBetaUnit>> units, std::vector<std::shared_ptr<AlphaBetaUnit>> targets, CCBot * bot);
};