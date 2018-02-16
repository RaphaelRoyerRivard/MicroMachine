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
    std::vector<AlphaBetaMove*> generateMoves(bool isMax, bool attackClosest, bool attackWeakest, bool attackPriority);
    AlphaBetaState generateChild();
    bool unitCanAttack(std::shared_ptr<AlphaBetaUnit> unit);
    bool unitCanMoveForward(std::shared_ptr<AlphaBetaUnit> unit, std::vector<std::shared_ptr<AlphaBetaUnit>> targets);
    bool unitShouldMoveBack(std::shared_ptr<AlphaBetaUnit> unit, std::vector<std::shared_ptr<AlphaBetaUnit>> targets);
    AlphaBetaValue eval();
};