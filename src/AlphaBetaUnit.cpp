#include "AlphaBetaUnit.h"
#include "AlphaBetaAction.h"

AlphaBetaUnit::AlphaBetaUnit() { }

AlphaBetaUnit::AlphaBetaUnit(const sc2::Unit * actual_unit, CCBot * bot, AlphaBetaAction * pprevious_action) {
    this->actual_unit = actual_unit;
    this->hp_current = actual_unit->health;
    this->hp_max = actual_unit->health_max;

    sc2::UnitTypeData unitTypeData = Util::GetUnitTypeDataFromUnitTypeId(actual_unit->unit_type, *bot);
    float pdps = 0.f;
    float prange = 0.f;
    float pcooldown = 0.f;
    for (sc2::Weapon & weapon : unitTypeData.weapons)
    {
        float weaponDps = weapon.attacks * weapon.damage_ * (1 / weapon.speed);
        pdps = std::max(weaponDps, dps);
        float weaponRange = weapon.range;
        prange = std::max(weaponRange, range);
        float weaponCooldown = weapon.speed;
        pcooldown = std::max(weaponCooldown, pcooldown);
    }
    dps = pdps;
    range = prange;
    cooldown_max = pcooldown;
    actual_cooldown = actual_unit->weapon_cooldown;
    speed = unitTypeData.movement_speed;
    position = actual_unit->pos;
    previous_action = pprevious_action;
}

AlphaBetaUnit::AlphaBetaUnit(const sc2::Unit * pactual_unit, float php_current, float php_max, float pdps, float prange, float pcooldown_max, float pspeed, sc2::Point2D pposition, AlphaBetaAction * pprevious_action) {
    actual_unit = pactual_unit;
    hp_current = php_current;
    hp_max = php_max;
    dps = pdps;
    range = prange;
    actual_cooldown = pactual_unit->weapon_cooldown;
    cooldown_max = pcooldown_max;
    speed = pspeed;
    position = pposition;
    previous_action = pprevious_action;
};