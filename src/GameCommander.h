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

	std::map<sc2::Tag, CCPosition> m_nearCarrier;//Contians all units that are moving towards a carrier, the float is the distsq with his targeted carrier.
	std::vector<Unit>	 m_carrying;
	std::vector<Unit>	 m_carried;
	std::map<sc2::Tag, bool> m_unitInside;//If the unit is inside something, only for Terran, doesn't consider CC
	std::map<sc2::Tag, Unit*> m_unitCarryingUnit;
	std::map<sc2::Tag, std::vector<Unit*>> m_carriedUnits;

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
	bool canEnterCarrier(Unit unit, Unit carrier);

    bool shouldSendInitialScout();

    void onUnitCreate(const Unit & unit);
    void onUnitDestroy(const Unit & unit);

	//Carrier and Carried
	bool isInside(sc2::Tag unit);
	void setInside(sc2::Tag unit, bool _inside);
	std::map<sc2::Tag, Unit*> getCarryingUnit();
	void setCarryingUnit(sc2::Tag carrier, Unit* carrying);
	std::map<sc2::Tag, std::vector<Unit*>> getCarriedUnits();
	void addCarriedUnit(sc2::Tag carrier, Unit* carried);
	void AddDelayedSmartAbility(Unit unit, sc2::AbilityID ability, CCPosition position);
	void GiveDelayedSmarAbilityOrders();
};
