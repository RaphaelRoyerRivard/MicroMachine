#include "UCTCDUnit.h"
#include "UCTCDAction.h"

UCTCDUnit::UCTCDUnit() { }

UCTCDUnit::UCTCDUnit(const sc2::Unit * actual_unit, CCBot * bot, bool phas_played/*=false*/) {
    this->actual_unit = actual_unit;
    this->hp_current = actual_unit->health;
    this->hp_max = actual_unit->health_max;

    sc2::UnitTypeData unitTypeData = Util::GetUnitTypeDataFromUnitTypeId(actual_unit->unit_type, *bot);
    float pdamage = 0.f;
    float prange = 0.f;
    float pcooldown = 0.f;
    for (sc2::Weapon & weapon : unitTypeData.weapons)
    {
        float weaponDps = weapon.attacks * weapon.damage_;
        pdamage = std::max(weaponDps, pdamage);
        float weaponRange = weapon.range;
        prange = std::max(weaponRange, range);
        float weaponCooldown = weapon.speed;
        pcooldown = std::max(weaponCooldown, pcooldown);
    }
    damage = pdamage;
    range = prange;
    cooldown_max = pcooldown;
    speed = unitTypeData.movement_speed;
    position = actual_unit->pos;
    previous_action = nullptr;
    attack_time = actual_unit->weapon_cooldown;
    move_time = 0.f;
    shield = actual_unit->shield;
    has_played = phas_played;
    UpdateIsDead();
}

UCTCDUnit::UCTCDUnit(const sc2::Unit * pactual_unit, float php_current, float php_max, float pdamage, float prange, float pcooldown_max, float pspeed, float pattack_time, float pmove_time, float pshield, sc2::Point2D pposition, UCTCDAction * pprevious_action, bool phas_played) {
    actual_unit = pactual_unit;
    hp_current = php_current;
    hp_max = php_max;
    damage = pdamage;
    range = prange;
    cooldown_max = pcooldown_max;
    speed = pspeed;
    position = pposition;
    previous_action = pprevious_action;
    attack_time = pattack_time;
    move_time = pmove_time;
    shield = pshield;
    has_played = phas_played;
    UpdateIsDead();
}
void UCTCDUnit::InflictDamage(float damage)
{
    hp_current -= damage;
    UpdateIsDead();
}
void UCTCDUnit::UpdateIsDead()
{
    is_dead = hp_current <= 0.f;
}

bool UCTCDUnit::CanAttack(float time) {
    return attack_time <= time;
}

bool UCTCDUnit::CanMoveForward(float time, std::vector<UCTCDUnit> targets) {
    if (time < move_time) return false;
    for (auto target : targets) {
        if (Util::Dist(target.position, position) < range) {
            return false; // don't move if you can attack an ennemy, shoot at it !
        }
    }
    return true;
}

bool UCTCDUnit::ShouldMoveBack(float time, std::vector<UCTCDUnit> targets) {
    for (auto target : targets) {
        float target_range(target.range);
        float dist = Util::Dist(position, target.position);
        float timeUntilAttacked = std::max(0.f, (dist - target_range) / target.speed);
        float cooldown = attack_time - time;
        if (timeUntilAttacked < cooldown)
            return true;
    }
    return false;
}

bool UCTCDUnit::CanWait(float time) {
    return move_time <= time && time < attack_time;
}
