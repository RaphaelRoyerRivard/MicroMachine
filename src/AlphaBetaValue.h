#pragma once

class AlphaBetaState;
class AlphaBetaMove;
class AlphaBetaValue {
public:
    AlphaBetaValue();
    AlphaBetaValue(float score, AlphaBetaMove * action, AlphaBetaState * state);
    AlphaBetaMove * move;
    float score;
    AlphaBetaState * state;
};