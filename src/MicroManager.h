#pragma once

#include "Common.h"
#include "SquadOrder.h"
#include "Unit.h"
#include "Micro.h"

class CCBot;

class MicroManager
{
protected:

    CCBot & m_bot;
    SquadOrder m_order;
    std::vector<Unit> m_units;
    std::vector<Unit> m_targets;

    virtual void executeMicro() = 0;

public:

    MicroManager(CCBot & bot);

    const std::vector<Unit> & getUnits() const;
    void setUnits(const std::vector<Unit> & u);
	std::vector <Unit> getTargets() const { return m_targets; }
	virtual void setTargets(const std::vector<Unit> & targets) = 0;
    void setOrder(SquadOrder order) { m_order = order; }
    void regroup(const CCPosition & regroupPosition) const;
	float getAverageSquadSpeed() const;
	float getAverageTargetsSpeed() const;
    float getSquadPower() const;
    float getTargetsPower(const std::vector<Unit>& units) const;
	float getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * target, bool filterHighRangeUnits) const;
};