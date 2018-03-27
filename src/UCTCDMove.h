#pragma once
#include <vector>
#include "UCTCDAction.h"

class UCTCDMove {
public:
    UCTCDMove();
    UCTCDMove(std::vector<UCTCDAction> actions);
    std::vector<UCTCDAction> actions;
};