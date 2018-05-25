#pragma once

#include "Common.h"
#include "MeleeManager.h"
#include "RangedManager.h"
#include "SquadOrder.h"
#include <ctime>

class CCBot;

class Squad
{
    CCBot &             m_bot;

    std::string         m_name;
    std::vector<Unit>   m_units;
    std::vector<Unit>   m_targets;
    int                 m_regroupStartFrame;
    int                 m_maxRegroupDuration;
    int                 m_regroupCooldown;
    float               m_maxDistanceFromCenter;
    size_t              m_priority;

    SquadOrder          m_order;
    MeleeManager        m_meleeManager;
    RangedManager       m_rangedManager;

    std::map<Unit, bool> m_nearEnemy;

    Unit unitClosestToEnemy() const;

    Unit calcClosestAllyFromTargets(std::vector<Unit>& targets) const;
    void updateUnits();
    void addUnitsToMicroManagers();
    void setNearEnemyUnits();
    void setAllUnits();

    bool isUnitNearEnemy(const Unit & unit) const;
    int  squadUnitsNear(const CCPosition & pos) const;

public:

    Squad(const std::string & name, const SquadOrder & order, size_t priority, CCBot & bot);
    Squad(const std::string & name, const SquadOrder & order, int maxRegroupDuration, int regroupCooldown, float maxDistanceFromCenter, size_t priority, CCBot & bot);
    Squad(CCBot & bot);

    void onFrame();
    void setSquadOrder(const SquadOrder & so);
    void addUnit(const Unit & unit);
    void removeUnit(const Unit & unit);
    void giveBackWorkers();
    bool needsToRetreat() const;
    bool needsToRegroup() const;
    void clear();

    bool containsUnit(const Unit & unit) const;
    bool isEmpty() const;
    float getMaxDistanceFromCenter() const;
    void setMaxDistanceFromCenter(const float & maxDistanceFromCenter);
    size_t getPriority() const;
    void setPriority(const size_t & priority);
    const std::string & getName() const;

    CCPosition calcCenter() const;
    float calcAverageHeight() const;
    CCPosition calcRetreatPosition() const;
    std::vector<Unit> calcTargets() const;

    const std::vector<Unit> & getUnits() const;
    const SquadOrder & getSquadOrder() const;
};
