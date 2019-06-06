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
    Unit getTarget(Unit meleeUnit, const std::vector<Unit> & targets) const;
};
