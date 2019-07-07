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

	int					m_lastRegroupFrame;
    int                 m_regroupStartFrame;
    int                 m_maxRegroupDuration;
    int                 m_regroupCooldown;

	int                 m_retreatStartFrame;
	int					m_minRetreatDuration;

    float               m_maxDistanceFromCenter;

	bool				m_isSuiciding;				//used by the main attack squad when not retreating from a bad fight

    size_t              m_priority;

    SquadOrder          m_order;
    MeleeManager        m_meleeManager;
    RangedManager       m_rangedManager;

    std::map<Unit, bool> m_nearEnemy;

    Unit unitClosestToEnemy() const;

    void updateUnits();
    void addUnitsToMicroManagers();
    void setNearEnemyUnits();
    void setAllUnits();

    bool isUnitNearEnemy(const Unit & unit) const;
    int  squadUnitsNear(const CCPosition & pos) const;

public:

    Squad(const std::string & name, const SquadOrder & order, size_t priority, CCBot & bot);
    Squad(const std::string & name, const SquadOrder & order, int maxRegroupDuration, int regroupCooldown, int minRetreatDuration, float maxDistanceFromCenter, size_t priority, CCBot & bot);
    Squad(CCBot & bot);

	std::vector<Unit> getTargets() { return m_targets; }

    void onFrame();
    void setSquadOrder(const SquadOrder & so);
    void addUnit(const Unit & unit);
    void removeUnit(const Unit & unit);
    void giveBackWorkers();
    bool needsToRetreat();
	bool isSuiciding() const;
    bool needsToRegroup();
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
	std::vector<Unit> calcVisibleTargets();
    std::vector<Unit> calcTargets(bool visibilityFilter = false);

    const std::vector<Unit> & getUnits() const;
    const SquadOrder & getSquadOrder() const;
};
