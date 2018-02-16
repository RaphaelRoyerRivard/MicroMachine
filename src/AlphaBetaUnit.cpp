#include "AlphaBetaUnit.h"
#include "AlphaBetaAction.h"

AlphaBetaUnit::AlphaBetaUnit() { }

AlphaBetaUnit::AlphaBetaUnit(const sc2::Unit * actual_unit, CCBot * bot) {
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
	UpdateIsDead();
}

AlphaBetaUnit::AlphaBetaUnit(const sc2::Unit * pactual_unit, float php_current, float php_max, float pdamage, float prange, float pcooldown_max, float pspeed, float pattack_time, float pmove_time, sc2::Point2D pposition, AlphaBetaAction * pprevious_action) {
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
	UpdateIsDead();
}
void AlphaBetaUnit::InflictDamage(float damage)
{
	hp_current -= damage;
	UpdateIsDead();
}
void AlphaBetaUnit::UpdateIsDead()
{
	is_dead = hp_current <= 0.f;
}
;