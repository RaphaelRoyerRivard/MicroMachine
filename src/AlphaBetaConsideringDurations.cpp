#pragma once
#include "AlphaBetaConsideringDurations.h"

AlphaBetaConsideringDurations::AlphaBetaConsideringDurations(size_t time, size_t depth)
    : time_limit(time),
    depth_limit(depth) { }


AlphaBetaValue AlphaBetaConsideringDurations::doSearch()
{
    AlphaBetaState state;
    AlphaBetaPlayer min, max;
    AlphaBetaValue alpha, beta;
    AlphaBetaAction * action;
    // start timer here
    return alphaBeta(state, depth_limit, false, action, alpha, beta);
}

AlphaBetaValue AlphaBetaConsideringDurations::alphaBeta(AlphaBetaState state, size_t depth, bool isMax, AlphaBetaAction * action, AlphaBetaValue alpha, AlphaBetaValue beta) {
    if (actual_time == 0 || depth == 0) return eval(state);
    std::vector<AlphaBetaAction> bestMove;

    // Sparcraft utilise un vecteur d'actions, donc plusieurs actions (ordonnées)
    // sont consiférées pendant une boucle de d'AB.
    // Ici, une seule action est considérée par boucle.
    for (auto a : state.generateMove(!isMax)->actions->begin) {

        AlphaBetaValue val;

        // no stuff about simultaneous moves right now

        AlphaBetaState child = state.generateChild();
        if (action != nullptr) {
            child.doMove(a);
        }
        child.doMove(a);

        val = alphaBeta(child, depth - 1, !isMax, nullptr, alpha, beta);

        if (isMax && (val.score > alpha.score)) {
            alpha = AlphaBetaValue(val.score, a);
        }
        if (!isMax && (val.score < beta.score)) {
            beta = AlphaBetaValue(val.score, a);
        }
        if (alpha.score >= beta.score) break;
    }

    return isMax ? alpha : beta;
}


AlphaBetaValue eval(AlphaBetaState state) {
    // TODO
}