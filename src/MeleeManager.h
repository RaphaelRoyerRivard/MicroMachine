#pragma once

#include "Common.h"
#include "MicroManager.h"

class CCBot;

class MeleeManager: public MicroManager
{

public:

    MeleeManager(CCBot & bot);
    void setTargets(const std::vector<Unit> & targets);
    void executeMicro();
    float getAttackPriority(const sc2::Unit* attacker, const sc2::Unit* target) const;
    Unit getTarget(Unit meleeUnit, const std::vector<Unit> & targets) const;
    bool meleeUnitShouldRetreat(Unit meleeUnit, const std::vector<Unit> & targets);
};
