#pragma once
#include "AlphaBetaConsideringDurations.h"

AlphaBetaConsideringDurations::AlphaBetaConsideringDurations(size_t time, size_t depth, bool pClosestEnemy, bool pWeakestEnemy, bool pHighestPriority)
    : time_limit(time),
    depth_limit(depth),
    closestEnemy(pClosestEnemy),
    weakestEnemy(pWeakestEnemy),
    highestPriority(pHighestPriority)
{
    nodes_evaluated = 0;
}


AlphaBetaValue AlphaBetaConsideringDurations::doSearch(std::vector<std::shared_ptr<AlphaBetaUnit>> units, std::vector<std::shared_ptr<AlphaBetaUnit>> targets, CCBot * bot)
{
    AlphaBetaPlayer min = AlphaBetaPlayer(targets, false);

    AlphaBetaPlayer max = AlphaBetaPlayer(units, false);

    AlphaBetaState state = AlphaBetaState(min, max, 0);
    AlphaBetaValue alpha(-10000, nullptr, nullptr), beta(10000, nullptr, nullptr);
    return alphaBeta(state, depth_limit, nullptr, alpha, beta);
}

bool isTerminal(AlphaBetaState state, size_t depth) {
    if (depth == 0) return true;
    return state.playerMin.units.size() == 0 || state.playerMax.units.size() == 0;
}

AlphaBetaValue AlphaBetaConsideringDurations::alphaBeta(AlphaBetaState state, size_t depth, AlphaBetaMove * m0, AlphaBetaValue alpha, AlphaBetaValue beta) {
    // TODO: Handle timeout
    if (isTerminal(state, depth)) return state.eval();

    // MAX == true
    bool toMove = state.playerToMove();

    std::vector<AlphaBetaMove *> moves = state.generateMoves(toMove, closestEnemy, weakestEnemy, highestPriority);
    for (auto m : moves) {
        AlphaBetaValue val;
        if (state.bothCanMove() && m0 == nullptr && depth != 1)
            val = alphaBeta(state, depth - 1, m, alpha, beta);
        AlphaBetaState child = state.generateChild();
        if (m0 != nullptr) {
            child.doMove(m0);
        }
        child.doMove(m);

        val = alphaBeta(child, depth - 1, nullptr, alpha, beta);

        if (toMove && (val.score > alpha.score)) {
            alpha = AlphaBetaValue(val.score, m, &child);
        }
        if (!toMove && (val.score < beta.score)) {
            beta = AlphaBetaValue(val.score, m, &child);
        }
        if (alpha.score > beta.score) break;
    }
    // TODO: More stats ?
    ++nodes_evaluated;
    // TODO: free-up memory it's getting huge in here
    return toMove ? alpha : beta;
}
