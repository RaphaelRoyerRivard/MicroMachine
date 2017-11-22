#pragma once
#include <vector>
#include "AlphaBetaAction.h"

class AlphaBetaMove {
public:
    AlphaBetaMove();
    AlphaBetaMove(std::vector<AlphaBetaAction> * actions);
    std::vector<AlphaBetaAction> * actions;
};