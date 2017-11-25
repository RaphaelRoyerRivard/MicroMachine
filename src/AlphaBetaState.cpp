#include "AlphaBetaState.h"
#include "Util.h"

AlphaBetaState::AlphaBetaState()
{
}

AlphaBetaState::AlphaBetaState(AlphaBetaPlayer playerMin, AlphaBetaPlayer playerMax, AlphaBetaMove move, long time)
    : playerMin(playerMin),
    playerMax(playerMax),
    move(move),
    time(time) { }

// Here we virtually do the planned move. This involes changing the position, health, weapon cooldown and others
// of both player's units
void AlphaBetaState::doMove(AlphaBetaMove move) {
    for (AlphaBetaAction action : move.actions->begin) {
        // should probably use unit references
        if (action.type == AlphaBetaActionType::ATTACK) {
            // do attack
        }
        else if (action.type == AlphaBetaActionType::MOVE) {
            // do move
        }
        else {
            // do wait ?
        }
    } 
}

std::vector<AlphaBetaAction> AlphaBetaState::generateMoves(bool isMax) {
    // should probably be references
    AlphaBetaPlayer player = isMax ? playerMax : playerMin;
    AlphaBetaPlayer ennemy = !isMax ? playerMax : playerMin;
    std::vector<AlphaBetaAction> possible_actions;

    for (auto unit : player.units) {
        // if unit can do something (not cooling down or moving to somewhere)
        // TODO: Move cancelling ? Like if the unit is going somewhere, attack a unit instead
        if (unit.previous_action.type == AlphaBetaActionType::MOVE && Util::Dist(unit.position, unit.previous_action.position) < 0.1f ||
            unit.previous_action.type == AlphaBetaActionType::ATTACK && unit.previous_action.time + unit.cooldown_max < time) {
            // add attacking all targets in range as moves
            // TODO: Intelligent attacking (like focus fire)
            for (auto baddy : ennemy.units) {
                if (Util::Dist(unit.position, baddy.position) <= unit.range) {
                    AlphaBetaAction attack(unit, baddy, unit.position, AlphaBetaActionType::ATTACK, time);
                    possible_actions.push_back(attack);
                }
            }
            // add moving closer to targets
            for (auto baddy : ennemy.units) {
                if (Util::Dist(unit.position, baddy.position) > unit.range) {
                    // unit position + attack vector * distance until target in range
                    sc2::Point2D target = unit.position + (Util::Normalized(baddy.position - unit.position) * (Util::Dist(baddy.position, unit.position) - unit.range));                    
                    AlphaBetaAction move(unit, baddy, target, AlphaBetaActionType::MOVE, time);
                    possible_actions.push_back(move);
                }
            }
            // TOOD: Move back to retreat (for focus fire or kiting)
            // TODO: other moves ?

            // just so we have moves
            AlphaBetaAction pass(unit, AlphaBetaUnit(), unit.position, AlphaBetaActionType::WAIT, time);
            possible_actions.push_back(pass);
        }
    }
    return possible_actions;
}

AlphaBetaState AlphaBetaState::generateChild() {
    // move forward for one second
    // TODO: Set this as a parameter (move forward +0.1 second for example)
    return AlphaBetaState(playerMin, playerMax, move, time + 1);
}