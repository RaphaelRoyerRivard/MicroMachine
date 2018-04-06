#pragma once
#include <vector>
#include "sc2api/sc2_api.h"
#include "UCTCDPlayer.h"

class UCTCDAction;
class UCTCDUnit;
class UCTCDValue;
class UCTCDMove;
class UCTCDNode;

class UCTCDState {
public:
    // parameters for move generation
    bool attackClosest;
    bool attackWeakest;
    bool attackPriority;
    bool considerDistance;
    bool unitOwnAgent;

    // actual state
    float time; //temps du jeu
    UCTCDPlayer playerMin; // joueur ayant fait l'action move pour générer ce state
    UCTCDPlayer playerMax; // joueur ayant fait l'action move pour générer ce state

    UCTCDState(UCTCDPlayer playerMin, UCTCDPlayer playerMax, float time, bool attackClosest, bool attackWeakest, bool attackPriority, bool considerDistance, bool unitOwnAgent);
    void doMove(UCTCDMove & move);
    bool bothCanMove();
    bool playerToMove();
    bool isTerminal();
    std::vector<UCTCDMove> generateMoves(bool isMax, UCTCDNode & currentNode, std::map<const sc2::Unit *, UCTCDAction> command_for_unit);
    int eval();
};