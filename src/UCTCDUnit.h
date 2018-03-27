#pragma once
#ifndef UCTCDUNIT_H
#define UCTCDUNIT_H

#include "sc2api/sc2_api.h"
#include "Util.h"
#include "CCBot.h"

class UCTCDAction;

class UCTCDUnit {
public:
    const sc2::Unit * actual_unit;
    float hp_current;
    float hp_max;
    float damage;
    float range;
    float cooldown_max;
    float attack_time; // time unit completed attack
    float move_time; // time unit last completed movement
    float speed;
    float shield;
    bool is_dead;
    sc2::Point2D position;
    UCTCDAction * previous_action;

    // initial constructor
    UCTCDUnit();
    UCTCDUnit(const sc2::Unit * actual_unit, CCBot * bot);
    UCTCDUnit(const sc2::Unit * pactual_unit, float php_current, float php_max, float pdamage, float prange, float pcooldown_max, float pspeed, float pattack_time, float pmove_time, float pshield, sc2::Point2D pposition, UCTCDAction * pprevious_action);

    void InflictDamage(float damage);
    bool CanAttack(float time);
    bool CanMoveForward(float time, std::vector<UCTCDUnit> targets);
    bool ShouldMoveBack(float time, std::vector<UCTCDUnit> targets);
    bool CanWait(float time);
private:
    void UpdateIsDead();
};

#endif 