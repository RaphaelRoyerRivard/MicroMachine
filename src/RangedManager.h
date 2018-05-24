#pragma once

#include "Common.h"
#include "MicroManager.h"

class CCBot;

class RangedManager: public MicroManager
{
public:

    RangedManager(CCBot & bot);
    void setTargets(const std::vector<Unit> & targets);
    void executeMicro();
    float getAttackPriority(const sc2::Unit * rangedUnit, const sc2::Unit * target);
    const sc2::Unit * getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets);
	bool isTargetRanged(const sc2::Unit * target);
    const sc2::Unit * getClosestMineral(const sc2::Unit * rangedUnit);
};
