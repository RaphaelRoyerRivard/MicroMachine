#include "AlphaBetaState.h"
#include "Util.h"

AlphaBetaState::AlphaBetaState(AlphaBetaPlayer pplayerMin, AlphaBetaPlayer pplayerMax, long ptime)
    : playerMin(pplayerMin),
    playerMax(pplayerMax),
    time(ptime) { }

// Here we virtually do the planned move. This involes changing the position, health, weapon cooldown and others
// of both player's units
void AlphaBetaState::doMove(AlphaBetaAction * action) {
    // should probably use unit references
    if (action->type == AlphaBetaActionType::ATTACK) {
        // do attack
        action->target->hp_current = action->unit->dps;
    }
    else if (action->type == AlphaBetaActionType::MOVE) {
        // do move
        action->unit->position = action->position;
    }
    action->unit->previous_action = action;
}

std::vector<AlphaBetaAction *> AlphaBetaState::generateMoves(bool isMax) {
    // should probably be references
    AlphaBetaPlayer player = isMax ? playerMax : playerMin;
    AlphaBetaPlayer ennemy = !isMax ? playerMax : playerMin;
    std::vector<AlphaBetaAction *> possible_actions;
    possible_actions.clear();

    for (auto unit : player.units) {
        // if unit can do something (not cooling down or moving to somewhere)
        // TODO: Move cancelling ? Like if the unit is going somewhere, attack a unit instead
        if (unitCanAttack(unit)) {
            // add attacking all targets in range as moves
            // TODO: Intelligent attacking (like focus fire)
            // TODO: don't attack dead units
            for (auto baddy : ennemy.units) {
                if (Util::Dist(unit->position, baddy->position) <= unit->range) {
                    AlphaBetaAction * attack;
                    attack = new AlphaBetaAction(unit, baddy, unit->position, 0.f, AlphaBetaActionType::ATTACK, time);
                    possible_actions.push_back(attack);
                }
            }
        }
        if (unitCanMove(unit)) {
            // add moving closer to targets
            for (auto baddy : ennemy.units) {
                if (Util::Dist(unit->position, baddy->position) > unit->range) {
                    // unit position + attack vector * distance until target in range
                    sc2::Point2D target = unit->position + (Util::Normalized(baddy->position - unit->position) * (Util::Dist(baddy->position, unit->position) - unit->range));
                    AlphaBetaAction * move = new AlphaBetaAction(unit, baddy, target, (Util::Dist(baddy->position, unit->position) - unit->range), AlphaBetaActionType::MOVE, time);
                    possible_actions.push_back(move);
                }
            }
        }
        // TOOD: Move back to retreat (for focus fire or kiting)
        // TODO: other moves ?

        // just so we have moves (?)
        AlphaBetaAction * pass = new AlphaBetaAction(unit, nullptr, unit->position, 0.f, AlphaBetaActionType::WAIT, time);
        possible_actions.push_back(pass);
    }
    return possible_actions;
}

AlphaBetaState AlphaBetaState::generateChild() {
    // move forward for one second
    // TODO: Set this as a parameter (move forward +0.1 second for example)
    std::vector<AlphaBetaUnit *> minUnits;
    std::vector<AlphaBetaUnit *> maxUnits;

    // so other branches won't influence this one
    for (auto unit : playerMin.units) {
        minUnits.push_back(&AlphaBetaUnit(*unit));
    }
    for (auto unit : playerMax.units) {
        maxUnits.push_back(&AlphaBetaUnit(*unit));
    }

    AlphaBetaPlayer newMin = AlphaBetaPlayer(minUnits, false);
    AlphaBetaPlayer newMax = AlphaBetaPlayer(maxUnits, true);
    return AlphaBetaState(newMin, newMax, time + 1);
}

// TODO: move this into unit
bool AlphaBetaState::unitCanAttack(AlphaBetaUnit * unit) {
    if (unit->previous_action == nullptr) return true; // new unit
    return unit->previous_action->type != AlphaBetaActionType::ATTACK || unit->previous_action->time + unit->cooldown_max < time;
}

// TODO: move this into unit
bool AlphaBetaState::unitCanMove(AlphaBetaUnit * unit) {
    if (unit->previous_action == nullptr) return true; // new unit
    return unit->previous_action->type != AlphaBetaActionType::MOVE || unit->previous_action->time + (unit->previous_action->distance / unit->speed) < time;
}

AlphaBetaValue AlphaBetaState::eval(bool isMax) {
    float totalDamage = 0;
    AlphaBetaPlayer player = isMax ? this->playerMax : this->playerMin;
    AlphaBetaPlayer ennemy = isMax ? this->playerMin : this->playerMax;
    for (auto unit : ennemy.units) {
        totalDamage += (unit->hp_max - unit->hp_current);
    }
    return AlphaBetaValue(totalDamage, nullptr);
}