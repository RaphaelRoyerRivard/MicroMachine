#include "AlphaBetaState.h"
#include "AlphaBetaValue.h"
#include "AlphaBetaUnit.h"
#include "AlphaBetaMove.h"
#include "AlphaBetaAction.h"
#include "Util.h"
#include <algorithm>

AlphaBetaState::AlphaBetaState(AlphaBetaPlayer pplayerMin, AlphaBetaPlayer pplayerMax, long ptime)
    : playerMin(pplayerMin),
    playerMax(pplayerMax),
    time(ptime) { }

// Here we virtually do the planned move. This involes changing the position, health, weapon cooldown and others
// of both player's units
void AlphaBetaState::doMove(AlphaBetaMove * move) {
    // should probably use unit references
    for (auto action : move->actions) {
        if (action->type == AlphaBetaActionType::ATTACK) {
            // do attack
            action->target->hp_current = action->unit->dps;
            action->unit->actual_cooldown = 0;
        }
        else if (action->type == AlphaBetaActionType::MOVE) {
            // do move
            action->unit->position = action->position;
        }
        action->unit->previous_action = action;
    }
}

std::vector<AlphaBetaMove *> AlphaBetaState::generateMoves(bool isMax) {
    // should probably be references
    AlphaBetaPlayer player = isMax ? playerMax : playerMin;
    AlphaBetaPlayer ennemy = !isMax ? playerMax : playerMin;
    std::vector<AlphaBetaMove *> possible_moves;
    possible_moves.clear();
    int max_actions = 0;
    std::unordered_map<sc2::Tag, std::vector<AlphaBetaAction *>> actions_per_unit;

    for (auto unit : player.units) {
        std::vector<AlphaBetaAction *> actions;
        int nb_actions = 0;
        // if unit can do something (not cooling down or moving to somewhere)
        // TODO: Move cancelling ? Like if the unit is going somewhere, attack a unit instead
        if (unitCanAttack(unit)) {
            // add attacking closest target in range as actions
            // TODO: Intelligent attacking (like focus fire, weakest ennemy or reuse RangedManager priority)
            // TODO: don't attack dead units
            AlphaBetaUnit * closest_ennemy = nullptr;
            float closest_dist = INFINITY;
            for (auto baddy : ennemy.units) {
                float dist = Util::Dist(unit->position, baddy->position);
                if (dist <= unit->range && dist < closest_dist) {
                    closest_dist = dist;
                    closest_ennemy = baddy;
                }
            }
            if (closest_ennemy != nullptr) {
                AlphaBetaAction * attack;
                attack = new AlphaBetaAction(unit, closest_ennemy, unit->position, 0.f, AlphaBetaActionType::ATTACK, time);
                actions.push_back(attack);
                ++nb_actions;
            }
        }

        // Move closer to nearest unit
        // TODO: Inconporate with Attack ?
        if (unitCanMoveForward(unit, ennemy.units)) {
            // add moving closer to targets
            float closest_dist = INFINITY;
            sc2::Point2D closest_point;
            for (auto baddy : ennemy.units) {
                if (Util::Dist(unit->position, baddy->position) > unit->range) {
                    // unit position + attack vector * distance until target in range
                    sc2::Point2D target = unit->position + (Util::Normalized(baddy->position - unit->position) * (Util::Dist(baddy->position, unit->position) - unit->range));
                    if (Util::Dist(target, unit->position) < closest_dist) {
                        closest_dist = Util::Dist(target, unit->position);
                        closest_point = target;
                    }
                }
            }
            AlphaBetaAction * move = new AlphaBetaAction(unit, nullptr, closest_point, closest_dist, AlphaBetaActionType::MOVE, time);
            actions.push_back(move);
            ++nb_actions;
        }

        if (unitShouldMoveBack(unit, ennemy.units)) {
            AlphaBetaUnit * closest_ennemy = nullptr;
            float closest_dist = INFINITY;
            for (auto baddy : ennemy.units) {
                float dist = Util::Dist(unit->position, baddy->position);
                if (dist < closest_dist) {
                    closest_dist = dist;
                    closest_ennemy = baddy;
                }
            }
            if (closest_ennemy != nullptr) {
                AlphaBetaAction * back;
                sc2::Point2D back_position = unit->position - closest_ennemy->position + unit->position;
                back = new AlphaBetaAction(unit, nullptr, back_position, Util::Dist(unit->position, back_position), AlphaBetaActionType::MOVE, time);
                actions.push_back(back);
                ++nb_actions;
            }
        }
        // TODO: other moves ?   
        actions_per_unit.insert({ unit->actual_unit->tag, actions });
        max_actions = std::max(max_actions, nb_actions);
    }

    // TODO: Generate better moves, now it's just random
    for (int i = 0; i <= max_actions; ++i) {
        std::vector<AlphaBetaAction *> actions;
        for (auto unit : player.units) {
            std::vector<AlphaBetaAction *> actions_for_this_unit = actions_per_unit.at(unit->actual_unit->tag);
            if (actions_for_this_unit.size()) {
                int randomIndex = rand() % actions_for_this_unit.size();
                AlphaBetaAction * action = actions_for_this_unit.at(randomIndex);
                actions.push_back(action);
            }
        }
        if (actions.size() > 0)
            possible_moves.push_back(new AlphaBetaMove(actions));
    }
    return possible_moves;
}

AlphaBetaState AlphaBetaState::generateChild() {
    // TODO: Set this as a parameter (move forward +0.1 second for example)
    float time_diff = 1.f; // 1 second;
    std::vector<AlphaBetaUnit *> minUnits;
    std::vector<AlphaBetaUnit *> maxUnits;

    // so other branches won't influence this one
    for (auto unit : playerMin.units) {
        float actual_cooldown = std::min(unit->cooldown_max, unit->actual_cooldown + time_diff);
        AlphaBetaUnit * new_unit = new AlphaBetaUnit(*unit);
        new_unit->actual_cooldown = actual_cooldown;
        minUnits.push_back(new_unit);
    }
    for (auto unit : playerMax.units) {
        float actual_cooldown = std::min(unit->cooldown_max, unit->actual_cooldown + time_diff);
        AlphaBetaUnit * new_unit = new AlphaBetaUnit(*unit);
        new_unit->actual_cooldown = actual_cooldown;
        maxUnits.push_back(new_unit);
    }

    AlphaBetaPlayer newMin = AlphaBetaPlayer(minUnits, false);
    AlphaBetaPlayer newMax = AlphaBetaPlayer(maxUnits, true);
    return AlphaBetaState(newMin, newMax, time + time_diff);
}

// TODO: Better decision of which action the unit can do

// TODO: move this into unit
bool AlphaBetaState::unitCanAttack(AlphaBetaUnit * unit) {
    if (unit->previous_action == nullptr) return true; // new unit
    return unit->previous_action->type != AlphaBetaActionType::ATTACK || unit->actual_cooldown >= unit->cooldown_max;
}

// TODO: move this into unit
bool AlphaBetaState::unitCanMoveForward(AlphaBetaUnit * unit, std::vector<AlphaBetaUnit *> targets) {
    for (auto target : targets) {
        if (Util::Dist(target->position, unit->position) < unit->range) {
            return false; // don't move if you can attack an ennemy, shoot at it !
        }
    }
    return true;
}

// TODO: move this into unit
bool AlphaBetaState::unitShouldMoveBack(AlphaBetaUnit * unit, std::vector<AlphaBetaUnit *> targets) {

    // use the correct weapon range regardless of target type
    // if (unit->previous_action != nullptr && unit->previous_action->type != AlphaBetaActionType::ATTACK) return false;
    for (auto target : targets) {
        float range(unit->range);
        float dist = Util::Dist(unit->position, target->position);
        float timeUntilAttacked = std::max(0.f, (dist - range) / unit->speed);
        float cooldown = unit->actual_cooldown;
        if (timeUntilAttacked < cooldown) return true;
    }
    return false;
}

// TODO: Better evaluation function (something that takes into account own damage)
AlphaBetaValue AlphaBetaState::eval(bool isMax) {
    float totalDamage = 0;
    AlphaBetaPlayer player = isMax ? this->playerMax : this->playerMin;
    AlphaBetaPlayer ennemy = isMax ? this->playerMin : this->playerMax;
    for (auto unit : ennemy.units) {
        totalDamage += (unit->hp_max - unit->hp_current);
    }
    return AlphaBetaValue(totalDamage, nullptr, this);
}