#include "AlphaBetaState.h"

AlphaBetaState::AlphaBetaState()
{
}

AlphaBetaState::AlphaBetaState(AlphaBetaPlayer playerMin, AlphaBetaPlayer playerMax, AlphaBetaMove move, long time)
    : playerMin(playerMin),
    playerMax(playerMax),
    move(move),
    time(time) { }

// Here we virtually do the planned move. This involes changing the position, health, weapon cooldown and others
// of both player's units
void AlphaBetaState::doMove(AlphaBetaMove move) {
    for (AlphaBetaAction action : move.actions->begin) {
        // should probably use unit references
        if (action.type == AlphaBetaActionType::ATTACK) {
            // do attack
        }
        else if (action.type == AlphaBetaActionType::MOVE) {
            // do move
        }
        else {
            // do wait ?
        }
    } 
}

AlphaBetaMove * AlphaBetaState::generateMove(bool isMax) {
    // should probably be references
    AlphaBetaPlayer player = isMax ? playerMax : playerMin;
    for (auto unit : player.units) {

    }
}

AlphaBetaState AlphaBetaState::generateChild() {
    return AlphaBetaState(playerMin, playerMax, move, time + 1);
}