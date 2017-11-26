#pragma once
#include "AlphaBetaConsideringDurations.h"

AlphaBetaConsideringDurations::AlphaBetaConsideringDurations(size_t time, size_t depth)
    : time_limit(time),
    depth_limit(depth) { }


AlphaBetaValue AlphaBetaConsideringDurations::doSearch(std::vector<const sc2::Unit *> units, std::vector<const sc2::Unit *> targets, CCBot * bot)
{
    std::vector<AlphaBetaUnit *> minUnits;
    for (auto sc2_unit : targets) {
        minUnits.push_back(new AlphaBetaUnit(sc2_unit, bot));
    }
    AlphaBetaPlayer min = AlphaBetaPlayer(minUnits, false);

    std::vector<AlphaBetaUnit *> maxUnits;
    for (auto sc2_unit : units) {
        maxUnits.push_back(new AlphaBetaUnit(sc2_unit, bot));
    } 
    AlphaBetaPlayer max = AlphaBetaPlayer(maxUnits, false);

    AlphaBetaState state = AlphaBetaState(min, max, 0);
    AlphaBetaValue alpha(-10000, nullptr), beta(10000, nullptr);
    return alphaBeta(state, depth_limit, true, alpha, beta);
}

AlphaBetaValue AlphaBetaConsideringDurations::alphaBeta(AlphaBetaState state, size_t depth, bool isMax, AlphaBetaValue alpha, AlphaBetaValue beta) {
    if (actual_time == 0 || depth == 0) return state.eval(isMax);
    std::vector<AlphaBetaAction> bestMove;

    // Sparcraft utilise un vecteur d'actions, donc plusieurs actions (ordonnées)
    // sont consiférées pendant une boucle de d'AB.
    // Ici, une seule action est considérée par boucle.
    for (auto a : state.generateMoves(isMax)) {

        // no stuff about simultaneous moves right now

        AlphaBetaState child = state.generateChild();
        child.doMove(a);

        AlphaBetaValue val = alphaBeta(child, depth - 1, !isMax, alpha, beta);

        if (isMax && (val.score > alpha.score)) {
            alpha = AlphaBetaValue(val.score, a);
        }
        if (!isMax && (val.score < beta.score)) {
            beta = AlphaBetaValue(val.score, a);
        }
        if (alpha.score > beta.score) break;
    }
    return isMax ? alpha : beta;
}
