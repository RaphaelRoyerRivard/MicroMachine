#include "AlphaBetaPlayer.h"

AlphaBetaPlayer::AlphaBetaPlayer(std::vector<std::shared_ptr<AlphaBetaUnit>> punits, bool pisMax)
    :units(punits),
    isMax(pisMax) { }
