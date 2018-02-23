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
    float minTime = INFINITY;
    for (auto action : move->actions) {
        if (action->type == AlphaBetaActionType::ATTACK) {
            // do attack
            action->target->InflictDamage(action->unit->damage);
            action->unit->attack_time = action->time;
        }
        else if (action->type == AlphaBetaActionType::MOVE_BACK || action->type == AlphaBetaActionType::MOVE_FORWARD) {
            // do move
            action->unit->position = action->position;
            action->unit->move_time = action->time;
        }
        action->unit->previous_action = action;
        minTime = std::min(action->time, minTime);
    }
    this->time = minTime;
}

std::tuple<float, float> getPlayersTime(AlphaBetaPlayer min, AlphaBetaPlayer max, float time) {
    float minTime = INFINITY;
    float maxTime = INFINITY;
    for (auto unit : min.units) {
        if (unit->previous_action == nullptr) minTime = std::min(time, 0.f);
        else
            minTime = std::min(time, unit->previous_action->time);
    }
    for (auto unit : max.units) {
        if (unit->previous_action == nullptr) maxTime = std::min(time, 0.f);
        else
            maxTime = std::min(time, unit->previous_action->time);
    }
    return std::make_tuple(minTime, maxTime);
}

bool AlphaBetaState::bothCanMove() {
    std::tuple<float, float> times = getPlayersTime(playerMin, playerMax, time);
    float minTime = std::get<0>(times);
    float maxTime = std::get<1>(times);
    return minTime == maxTime;
}

bool AlphaBetaState::playerToMove() {
    std::tuple<float, float> times = getPlayersTime(playerMin, playerMax, time);
    float minTime = std::get<0>(times);
    float maxTime = std::get<1>(times);
    return maxTime <= minTime;
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
        if (unitCanAttack(unit.get())) {
            // add attacking closest target in range as actions
            // TODO: Intelligent attacking (like focus fire, weakest ennemy or reuse RangedManager priority)
            // TODO: don't attack dead units
			std::shared_ptr<AlphaBetaUnit> closest_ennemy = nullptr;
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
                attack = new AlphaBetaAction(unit, closest_ennemy, unit->position, 0.f, AlphaBetaActionType::ATTACK, time + unit->cooldown_max);
                actions.push_back(attack);
                ++nb_actions;
            }
        }

        // Move closer to nearest unit
        // TODO: Incorporate with Attack ?
        if (unitCanMoveForward(unit.get(), ennemy.units)) {
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
            AlphaBetaAction * move = new AlphaBetaAction(unit, nullptr, closest_point, closest_dist, AlphaBetaActionType::MOVE_FORWARD, time + (closest_dist / unit->speed));
            actions.push_back(move);
            ++nb_actions;
        }
        // Kite or escape fire
        if (unitShouldMoveBack(unit.get(), ennemy.units)) {
            AlphaBetaUnit * closest_ennemy = nullptr;
            float closest_dist = INFINITY;
            for (auto baddy : ennemy.units) {
                float dist = Util::Dist(unit->position, baddy->position);
                if (dist < closest_dist) {
                    closest_dist = dist;
                    closest_ennemy = baddy.get();
                }
            }
            if (closest_ennemy != nullptr) {
                AlphaBetaAction * back;
                sc2::Point2D back_position = unit->position - closest_ennemy->position + unit->position;
                closest_dist = Util::Dist(unit->position, back_position);
                back = new AlphaBetaAction(unit, nullptr, back_position, closest_dist, AlphaBetaActionType::MOVE_BACK, time + (closest_dist / unit->speed));
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
    std::vector<std::shared_ptr<AlphaBetaUnit>> minUnits;
    std::vector<std::shared_ptr<AlphaBetaUnit>> maxUnits;

    // so other branches won't influence this one
    for (auto unit : playerMin.units) {
		std::shared_ptr<AlphaBetaUnit> new_unit = std::make_shared<AlphaBetaUnit>(*unit);
        minUnits.push_back(new_unit);
    }
    for (auto unit : playerMax.units) {
		std::shared_ptr<AlphaBetaUnit> new_unit = std::make_shared<AlphaBetaUnit>(*unit);
        maxUnits.push_back(new_unit);
    }

    AlphaBetaPlayer newMin = AlphaBetaPlayer(minUnits, false);
    AlphaBetaPlayer newMax = AlphaBetaPlayer(maxUnits, true);
    return AlphaBetaState(newMin, newMax, time);
}

// TODO: Better decision of which action the unit can do

// TODO: move this into unit
bool AlphaBetaState::unitCanAttack(AlphaBetaUnit * unit) {
    return unit->attack_time <= time;
}

// TODO: move this into unit
bool AlphaBetaState::unitCanMoveForward(AlphaBetaUnit * unit, std::vector<std::shared_ptr<AlphaBetaUnit>> targets) {
    if (time < unit->move_time) return false;
    for (auto target : targets) {
        if (Util::Dist(target->position, unit->position) < unit->range) {
            return false; // don't move if you can attack an ennemy, shoot at it !
        }
    }
    return true;
}

// TODO: move this into unit
bool AlphaBetaState::unitShouldMoveBack(AlphaBetaUnit * unit, std::vector<std::shared_ptr<AlphaBetaUnit>> targets) {
    for (auto target : targets) {
        float range(target->range);
        float dist = Util::Dist(unit->position, target->position);
        float timeUntilAttacked = std::max(0.f, (dist - range) / target->speed);
        float cooldown = unit->attack_time - time;
        if (timeUntilAttacked < cooldown) 
            return true;
    }
    return false;
}

// TODO: Better evaluation function and end conditions (player dead == -100000) 
AlphaBetaValue AlphaBetaState::eval() {
    float totalPlayerDamage = 0;
    float totalEnemyDamage = 0;
	// We check if at least one unit is still alive. If no unit is alive, we add a huge value.
	bool oneMinIsAlive = false;
	bool oneMaxIsAlive = false;
    for (auto unit : playerMin.units) {
		oneMinIsAlive |= !unit->is_dead;
        totalEnemyDamage += (unit->hp_max - unit->hp_current);
    }
	if (!oneMinIsAlive)
		totalEnemyDamage += 100000;

    for (auto unit : playerMax.units) {
		oneMaxIsAlive |= !unit->is_dead;
        totalPlayerDamage += (unit->hp_max - unit->hp_current);
    }
	if (!oneMaxIsAlive)
		totalPlayerDamage += 100000;

    return AlphaBetaValue(totalEnemyDamage - totalPlayerDamage, nullptr, this);
}