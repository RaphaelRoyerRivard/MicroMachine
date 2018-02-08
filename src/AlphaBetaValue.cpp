#include "AlphaBetaValue.h"
#include "AlphaBetaState.h"
#include "AlphaBetaMove.h"

AlphaBetaValue::AlphaBetaValue() { }

AlphaBetaValue::AlphaBetaValue(float pValue, AlphaBetaMove * pmove, AlphaBetaState * pstate)
    :score(pValue),
    move(pmove),
    state(pstate) { }
