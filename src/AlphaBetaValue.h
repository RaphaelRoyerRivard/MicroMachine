#pragma once

class AlphaBetaState;
class AlphaBetaMove;
class AlphaBetaValue {
public:
    AlphaBetaValue(float value, AlphaBetaMove * action, AlphaBetaState * state);
    AlphaBetaMove * move;
    float score;
    AlphaBetaState * state;
};