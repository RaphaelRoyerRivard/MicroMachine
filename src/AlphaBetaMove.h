#pragma once
#include <vector>

class AlphaBetaAction;
class AlphaBetaMove {
    public:
        AlphaBetaMove();
        AlphaBetaMove(std::vector<AlphaBetaAction *> actions);
        std::vector<AlphaBetaAction *> actions;
};