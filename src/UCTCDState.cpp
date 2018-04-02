#include "UCTCDState.h"
#include "UCTCDUnit.h"
#include "UCTCDMove.h"
#include "UCTCDAction.h"
#include "Util.h"
#ifdef SC2API
#include "sc2api/sc2_typeenums.h"
#endif
#include <algorithm>

float getUnitPriority(UCTCDUnit unit, UCTCDUnit target);

UCTCDState::UCTCDState(UCTCDPlayer pplayerMin, UCTCDPlayer pplayerMax, float ptime, bool pattackClosest, bool pattackWeakest, bool pattackPriority, bool pconsiderDistance)
    : playerMin(pplayerMin)
    , playerMax(pplayerMax)
    , time(ptime)
    , attackWeakest(pattackWeakest)
    , attackClosest(pattackClosest)
    , attackPriority(pattackPriority)
    , considerDistance(pconsiderDistance) { }

// Here we virtually do the planned move. This involes changing the position, health, weapon cooldown and others
// of both player's units
void UCTCDState::doMove(UCTCDMove & move) {
    // should probably use unit references
    float minTime = INFINITY;
    for (auto action : move.actions) {
        if (action.type == UCTCDActionType::ATTACK) {
            // do attack
            action.target.InflictDamage(action.unit.damage);
            action.unit.attack_time = action.time;
        }
        else if (action.type == UCTCDActionType::MOVE_BACK || action.type == UCTCDActionType::MOVE_FORWARD) {
            // do move
            action.unit.position = action.position;
            action.unit.move_time = action.time;
        }
        action.unit.previous_action = &action;
        minTime = std::min(action.time, minTime);
    }
    if (minTime != INFINITY)
        this->time = minTime;
}

std::tuple<float, float> getPlayersTime(UCTCDPlayer min, UCTCDPlayer max, float time) {
    float minTime = INFINITY;
    float maxTime = INFINITY;
    for (auto unit : min.units) {
        if (unit.previous_action == nullptr) minTime = std::min(time, 0.f);
        else
            minTime = std::min(time, unit.previous_action->time);
    }
    for (auto unit : max.units) {
        if (unit.previous_action == nullptr) maxTime = std::min(time, 0.f);
        else
            maxTime = std::min(time, unit.previous_action->time);
    }
    return std::make_tuple(minTime, maxTime);
}

bool UCTCDState::bothCanMove() {
    std::tuple<float, float> times = getPlayersTime(playerMin, playerMax, time);
    float minTime = std::get<0>(times);
    float maxTime = std::get<1>(times);
    return std::abs(minTime - maxTime) <= std::numeric_limits<float>::epsilon();
}

bool UCTCDState::playerToMove() {
    std::tuple<float, float> times = getPlayersTime(playerMin, playerMax, time);
    float minTime = std::get<0>(times);
    float maxTime = std::get<1>(times);
    return maxTime <= minTime;
}

bool UCTCDState::isTerminal()
{
    bool oneMinIsAlive = false;
    bool oneMaxIsAlive = false;
    for (auto unit : playerMin.units) {
        oneMinIsAlive |= !unit.is_dead;
    }
    for (auto unit : playerMax.units) {
        oneMaxIsAlive |= !unit.is_dead;
    }
    return !oneMaxIsAlive || !oneMinIsAlive;
}

std::vector<UCTCDMove> UCTCDState::generateMoves(bool isMax) {
    // should probably be references
    UCTCDPlayer player = isMax ? playerMax : playerMin;
    UCTCDPlayer ennemy = !isMax ? playerMax : playerMin;
    std::vector<UCTCDMove> possible_moves;
    possible_moves.clear();
    int max_actions = 0;
    std::unordered_map<sc2::Tag, std::vector<UCTCDAction>> actions_per_unit;

    for (auto unit : player.units) {
        std::vector<UCTCDAction> actions;
        int nb_actions = 0;
        // if unit can do something (not cooling down or moving to somewhere)
        if (unit.CanAttack(time)) {
            // add attacking closest target in range as actions

            UCTCDUnit * closest_ennemy = nullptr;
            UCTCDUnit * weakest_enemy = nullptr;
            UCTCDUnit * highest_priority = nullptr;

            float closest_dist = INFINITY;
            float min_health = INFINITY;
            float priority = 0;
            for (UCTCDUnit & baddy : ennemy.units) {
                if (baddy.is_dead)
                    continue;
                float dist = Util::Dist(unit.position, baddy.position);
                float health = baddy.hp_current;
                float prio = getUnitPriority(unit, baddy);
                if (!considerDistance || dist < (unit.range * 3)) {
                    if (dist < closest_dist && attackClosest) {
                        closest_dist = dist;
                        closest_ennemy = &baddy;
                    }
                    if (health <= min_health && attackWeakest) {
                        min_health = health;
                        weakest_enemy = &baddy;
                    }
                    if (prio > priority && attackPriority) {
                        priority = prio;
                        highest_priority = &baddy;
                    }
                }
            }

            std::set<UCTCDUnit*> attackTargets;
            if (closest_ennemy != nullptr) {
                UCTCDAction attack;
                attack = UCTCDAction(unit, *closest_ennemy, unit.position, 0.f, UCTCDActionType::ATTACK, time + unit.cooldown_max);
                actions.push_back(attack);
                attackTargets.insert(closest_ennemy);
                ++nb_actions;
            }
            if (weakest_enemy != nullptr && attackTargets.find(weakest_enemy) == attackTargets.end()) {
                UCTCDAction attack;
                attack = UCTCDAction(unit, *weakest_enemy, unit.position, 0.f, UCTCDActionType::ATTACK, time + unit.cooldown_max);
                actions.push_back(attack);
                attackTargets.insert(weakest_enemy);
                ++nb_actions;
            }
            if (highest_priority != nullptr && attackTargets.find(highest_priority) == attackTargets.end()) {
                UCTCDAction attack;
                attack = UCTCDAction(unit, *highest_priority, unit.position, 0.f, UCTCDActionType::ATTACK, time + unit.cooldown_max);
                actions.push_back(attack);
                attackTargets.insert(weakest_enemy);
                ++nb_actions;
            }
        }

        // Move closer to nearest unit
        if (considerDistance) {
            if (unit.CanMoveForward(time, ennemy.units)) {
                // add moving closer to targets
                float closest_dist = INFINITY;
                sc2::Point2D closest_point;
                for (auto baddy : ennemy.units) {
                    if (Util::Dist(unit.position, baddy.position) > unit.range) {
                        // unit position + attack vector * distance until target in range
                        sc2::Point2D target = unit.position + (Util::Normalized(baddy.position - unit.position) * (Util::Dist(baddy.position, unit.position) - unit.range));
                        if (Util::Dist(target, unit.position) < closest_dist) {
                            closest_dist = Util::Dist(target, unit.position);
                            closest_point = target;
                        }
                    }
                }
                UCTCDAction move = UCTCDAction(unit, UCTCDUnit(), closest_point, closest_dist, UCTCDActionType::MOVE_FORWARD, time + (closest_dist / unit.speed));
                actions.push_back(move);
                ++nb_actions;
            }
        }
        // Kite or escape fire
        if (unit.ShouldMoveBack(time, ennemy.units)) {
            UCTCDUnit closest_ennemy;
            bool has_closest_ennemy;
            float closest_dist = INFINITY;
            for (auto baddy : ennemy.units) {
                float dist = Util::Dist(unit.position, baddy.position);
                if (dist < closest_dist) {
                    closest_dist = dist;
                    closest_ennemy = baddy;
                    has_closest_ennemy = true;
                }
            }
            if (has_closest_ennemy) {
                UCTCDAction back;
                sc2::Point2D back_position = unit.position - closest_ennemy.position + unit.position;
                closest_dist = Util::Dist(unit.position, back_position);
                back = UCTCDAction(unit, UCTCDUnit(), back_position, closest_dist, UCTCDActionType::MOVE_BACK, time + (closest_dist / unit.speed));
                actions.push_back(back);
                ++nb_actions;
            }
        }

        if (unit.CanWait(time)) {
            UCTCDAction wait = UCTCDAction(unit, UCTCDUnit(), unit.position, 0, UCTCDActionType::WAIT, unit.attack_time);
            actions.push_back(wait);
            ++nb_actions;
        }
        // TODO: other moves ?   
        actions_per_unit.insert({ unit.actual_unit->tag, actions });
        max_actions = std::max(max_actions, nb_actions);
    }

    // ca ajouter some what toute les actions possibles
    // 1.1 avec toutes le autre a x.1
    // 1.2 avec toute les autres a  x.1
    //...
    //1.1  avec x.2
    // 1.2 avec x.2
    // oh my god quadrule for-loops all the way across the sky
    for (int h = 0; h < max_actions; ++h) {
        for (int i = 0; i < player.units.size(); ++i) {
            std::vector<UCTCDAction> actions_for_this_unit = actions_per_unit.at(player.units[i].actual_unit->tag);
            for (int j = 0; j < actions_for_this_unit.size(); ++j) {
                std::vector<UCTCDAction> actions;
                for (int k = 0; k < player.units.size(); ++k) {
                    std::vector<UCTCDAction> actions_for_k = actions_per_unit.at(player.units[k].actual_unit->tag);
                    if (k == i) {
                        actions.push_back(actions_for_k[j]);
                    }
                    else {
                        actions.push_back(actions_for_k[std::min((long)h, (long)actions_for_k.size() - 1)]);
                    }
                }
                if (actions.size() > 0)
                    possible_moves.push_back(UCTCDMove(actions));
            }
        }
    }
    return possible_moves;
}

float getUnitPriority(UCTCDUnit unit, UCTCDUnit target) {
    //dps: damage per seconds = damage/cooldown
    float dps = target.damage;
    if (target.cooldown_max != 0)
        dps /= target.cooldown_max;

    if (dps == 0.f){
#ifdef SC2API
        // There is no dps for this unit, so it's hard coded
        // see: http://liquipedia.net/starcraft2/Damage_per_second
        switch (target.actual_unit->unit_type.ToType()) {
        case sc2::UNIT_TYPEID::ZERG_BANELING:
            dps = 20.f;
            break;
        default:
            dps = 15.f;
        }
#else
        dps = 15.f;
#endif
    }
    float healthValue = 1 / (target.hp_current + target.shield);
    float distanceValue = 1 / (Util::Dist(unit.position, target.position)/target.speed);

    //TODO try to give different weights to each variables
    return 5 + dps * healthValue * distanceValue;
}

int UCTCDState::eval()
{
    float totalPlayerDamage = 0;
    float totalEnemyDamage = 0;
    // We check if at least one unit is still alive. If no unit is alive, we add a huge value.
    bool oneMinIsAlive = false;
    bool oneMaxIsAlive = false;
    for (auto unit : playerMin.units) {
        oneMinIsAlive |= !unit.is_dead;
        totalEnemyDamage += (unit.hp_max - unit.hp_current);
    }
    if (!oneMinIsAlive)
        totalEnemyDamage += 100000;

    for (auto unit : playerMax.units) {
        oneMaxIsAlive |= !unit.is_dead;
        totalPlayerDamage += (unit.hp_max - unit.hp_current);
    }
    if (!oneMaxIsAlive)
        totalPlayerDamage += 100000;

    return totalPlayerDamage > totalEnemyDamage ? -1 : 1;
}
