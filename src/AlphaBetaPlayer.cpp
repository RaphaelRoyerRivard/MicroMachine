#include "AlphaBetaPlayer.h"

AlphaBetaPlayer::AlphaBetaPlayer(std::vector<AlphaBetaUnit*> punits, bool pisMax)
    :units(punits),
    isMax(pisMax) { }
