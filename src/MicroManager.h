#pragma once

#include "Common.h"
#include "SquadOrder.h"
#include "Unit.h"
#include "FocusFireFiniteStateMachine.h"
#include "KitingFiniteStateMachine.h"

class CCBot;
class AlphaBetaUnit;

class MicroManager
{
    //std::vector<const sc2::Unit *> m_unitsPtr;
    std::vector<Unit> m_units;
    std::vector<Unit> m_targets;

protected:

    CCBot & m_bot;
    SquadOrder order;

    virtual void executeMicro(const std::vector<Unit> & targets) = 0;
    void trainSubUnits(const Unit & unit) const;

public:

    MicroManager(CCBot & bot);

    const std::vector<Unit> & getUnits() const;
    void setUnits(const std::vector<Unit> & u);
    void setTargets(const std::vector<Unit> & t);
    void execute(const SquadOrder & order);
    void regroup(const CCPosition & regroupPosition) const;
    float getSquadPower() const;
    float getTargetsPower() const;
    float getUnitPower(const Unit & unit) const;

    std::unordered_map<sc2::Tag, FocusFireFiniteStateMachine*> m_focusFireStates;
    std::unordered_map<sc2::Tag, KitingFiniteStateMachine*> m_kittingStates;
    std::unordered_map<sc2::Tag, float> m_unitHealth;
    std::unordered_map<sc2::Tag, AlphaBetaUnit *> m_units_actions;
};