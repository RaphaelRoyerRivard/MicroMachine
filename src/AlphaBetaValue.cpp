#include "AlphaBetaValue.h"

AlphaBetaValue::AlphaBetaValue(float pvalue, AlphaBetaAction * paction)
    :score(pvalue),
    action(paction) { }
