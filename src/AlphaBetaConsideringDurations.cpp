#pragma once
#include "AlphaBetaConsideringDurations.h"

AlphaBetaConsideringDurations::AlphaBetaConsideringDurations(size_t time, size_t depth)
    : time_limit(time),
    depth_limit(depth)
{
    nodes_evaluated = 0;
}


AlphaBetaValue AlphaBetaConsideringDurations::doSearch(std::vector<AlphaBetaUnit *> units, std::vector<AlphaBetaUnit *> targets, CCBot * bot)
{
    AlphaBetaPlayer min = AlphaBetaPlayer(targets, false);

    AlphaBetaPlayer max = AlphaBetaPlayer(units, false);

    AlphaBetaState state = AlphaBetaState(min, max, 0);
    AlphaBetaValue alpha(-10000, nullptr, nullptr), beta(10000, nullptr, nullptr);
    return alphaBeta(state, depth_limit, true, alpha, beta);
}

AlphaBetaValue AlphaBetaConsideringDurations::alphaBeta(AlphaBetaState state, size_t depth, bool isMax, AlphaBetaValue alpha, AlphaBetaValue beta) {
    // TODO: Handle timeout
    if (actual_time == 0 || depth == 0) return state.eval(isMax);

    std::vector<AlphaBetaMove *> moves = state.generateMoves(isMax);
    for (auto m : moves) {

        AlphaBetaState child = state.generateChild();
        child.doMove(m);

        AlphaBetaValue val = alphaBeta(child, depth - 1, !isMax, alpha, beta);

        if (isMax && (val.score > alpha.score)) {
            alpha = AlphaBetaValue(val.score, m, &child);
        }
        if (!isMax && (val.score < beta.score)) {
            beta = AlphaBetaValue(val.score, m, &child);
        }
        if (alpha.score > beta.score) break;
    }
    // TODO: More stats ?
    ++nodes_evaluated;
    // TODO: free-up memory it's getting huge in here
    return isMax ? alpha : beta;
}
