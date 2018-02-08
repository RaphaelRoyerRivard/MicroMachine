#pragma once
#include <vector>
#include "sc2api/sc2_api.h"
#include "AlphaBetaPlayer.h"

class AlphaBetaAction;
class AlphaBetaUnit;
class AlphaBetaValue;
class AlphaBetaMove;

class AlphaBetaState {
public:
    float time; //temps du jeu
    AlphaBetaPlayer playerMin; // joueur ayant fait l'action move pour générer ce state
    AlphaBetaPlayer playerMax; // joueur ayant fait l'action move pour générer ce state

    AlphaBetaState(AlphaBetaPlayer playerMin, AlphaBetaPlayer playerMax, long time);
    void doMove(AlphaBetaMove * move);
    bool bothCanMove();
    bool playerToMove();
    std::vector<AlphaBetaMove *> generateMoves(bool isMax);
    AlphaBetaState generateChild();
    bool unitCanAttack(AlphaBetaUnit * unit);
    bool unitCanMoveForward(AlphaBetaUnit * unit, std::vector<AlphaBetaUnit *> targets);
    bool unitShouldMoveBack(AlphaBetaUnit * unit, std::vector<AlphaBetaUnit*> targets);
    AlphaBetaValue eval();
};