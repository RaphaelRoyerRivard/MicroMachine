#include "UCTCDMove.h"

UCTCDMove::UCTCDMove() 
    : actions(std::vector<UCTCDAction>())
{ }

UCTCDMove::UCTCDMove(std::vector<UCTCDAction> pactions)
    :actions(pactions) { };