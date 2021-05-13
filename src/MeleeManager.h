#pragma once

#include "Common.h"
#include "MicroManager.h"

class CCBot;

class MeleeManager: public MicroManager
{
protected:
	CCPosition m_stackPosition = CCPosition();
	Unit m_stackingMineral;
	Unit m_enemyMineral;
	std::set<Unit> m_stackedUnits;

public:

    MeleeManager(CCBot & bot);
    void setTargets(const std::vector<Unit> & targets);
    void executeMicro();
	void microUnit(const Unit & meleeUnit);
	bool areUnitsStackedUp();
	void stackUnits();
	void identifyStackingMinerals();
	void microStack();
	void updateBackstabbers();
    Unit getTarget(Unit meleeUnit, const std::vector<Unit> & targets) const;
};
