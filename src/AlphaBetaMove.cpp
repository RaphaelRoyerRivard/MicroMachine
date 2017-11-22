#include "AlphaBetaMove.h"

AlphaBetaMove::AlphaBetaMove() {
    this->actions = nullptr;
}
AlphaBetaMove::AlphaBetaMove(std::vector<AlphaBetaAction> * actions) {
    this->actions = actions;
}