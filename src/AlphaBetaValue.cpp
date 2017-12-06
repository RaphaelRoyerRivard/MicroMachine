#include "AlphaBetaValue.h"
#include "AlphaBetaState.h"
#include "AlphaBetaMove.h"

AlphaBetaValue::AlphaBetaValue(float pvalue, AlphaBetaMove * pmove, AlphaBetaState * pstate)
    :score(pvalue),
    move(pmove),
    state(pstate) { }
