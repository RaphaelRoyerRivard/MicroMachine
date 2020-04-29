#pragma once

#include "Common.h"
#include "Timer.hpp"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "CombatCommander.h"

class CCBot;

class GameCommander
{
    CCBot &                 m_bot;

    ProductionManager       m_productionManager;
    ScoutManager            m_scoutManager;
    CombatCommander         m_combatCommander;

    std::vector<Unit>    m_validUnits;
    std::vector<Unit>    m_combatUnits;
    std::vector<Unit>    m_scoutUnits;

	std::vector<sc2::Tag> m_unitInside;
	std::map<sc2::Tag, Unit*> m_unitCarrier;

	std::vector<std::pair<Unit, std::pair<sc2::AbilityID, CCPosition>>> m_delayedSmartAbility;

    bool                    m_initialScoutSet;

    void assignUnit(const Unit & unit, std::vector<Unit> & units);
    bool isAssigned(const Unit & unit) const;

public:

    GameCommander(CCBot & bot);

    void onStart();
    void onFrame(bool executeMacro);

	ProductionManager& Production();
	CombatCommander& Combat();

    void handleUnitAssignments();
    void setValidUnits();
	std::vector<Unit> getValidUnits() const;
    void setScoutUnits();
    void setCombatUnits();
	void setCarryingAndCarried();

    bool shouldSendInitialScout();

    void onUnitCreate(const Unit & unit);
    void onUnitDestroy(const Unit & unit);

	//Carrier and Carried
	bool isInside(sc2::Tag unit);
	void setInside(sc2::Tag unit);
	Unit* getCarrierForUnit(sc2::Tag unitTag);
	Unit* setCarrierForUnit(sc2::Tag unitTag, Unit* carrier);
	void AddDelayedSmartAbility(Unit unit, sc2::AbilityID ability, CCPosition position);
	void GiveDelayedSmarAbilityOrders();
};
