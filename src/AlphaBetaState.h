#pragma once
#include <vector>
#include "sc2api/sc2_api.h"
#include "AlphaBetaPlayer.h"
#include "AlphaBetaMove.h"

class AlphaBetaState {
    long time; //temps du jeu
    AlphaBetaPlayer playerMin; // joueur ayant fait l'action move pour générer ce state
    AlphaBetaPlayer playerMax; // joueur ayant fait l'action move pour générer ce state
    AlphaBetaMove move; // move précédent

public:
    AlphaBetaState();
    AlphaBetaState(AlphaBetaPlayer playerMin, AlphaBetaPlayer playerMax, AlphaBetaMove move, long time);
    void doMove(AlphaBetaMove move);
    AlphaBetaMove * generateMove(bool isMax);
    AlphaBetaState generateChild();
};