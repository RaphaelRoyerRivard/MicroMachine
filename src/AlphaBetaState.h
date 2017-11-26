#pragma once
#include <vector>
#include "sc2api/sc2_api.h"
#include "AlphaBetaPlayer.h"
#include "AlphaBetaValue.h"
#include "AlphaBetaUnit.h"
#include "AlphaBetaAction.h"

class AlphaBetaState {
    long time; //temps du jeu
    AlphaBetaPlayer playerMin; // joueur ayant fait l'action move pour générer ce state
    AlphaBetaPlayer playerMax; // joueur ayant fait l'action move pour générer ce state

public:
    AlphaBetaState(AlphaBetaPlayer playerMin, AlphaBetaPlayer playerMax, long time);
    void doMove(AlphaBetaAction * move);
    std::vector<AlphaBetaAction *> generateMoves(bool isMax);
    AlphaBetaState generateChild();
    bool unitCanAttack(AlphaBetaUnit * unit);
    bool unitCanMove(AlphaBetaUnit * unit);
    AlphaBetaValue eval(bool isMax);
};