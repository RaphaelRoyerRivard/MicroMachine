#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "UnitState.h"

class CCBot;

class CombatCommander
{
	const int FRAME_BEFORE_SIGHTING_INVALIDATED = 25;

    CCBot &         m_bot;

    SquadData       m_squadData;
    std::vector<Unit>  m_combatUnits;
	std::map<CCUnitID, UnitState> m_unitStates;
	std::map<Unit, std::pair<CCPosition, uint32_t>> m_invisibleSighting;
    bool            m_initialized;
    bool            m_attackStarted;
	int				m_currentBaseExplorationIndex;

    void            updateScoutDefenseSquad();
    void            updateDefenseSquads();
    void            updateBackupSquads();
	void            updateHarassSquads();
	void            updateAttackSquads();
    void            updateIdleSquad();
	void			checkUnitsState();
    bool            isSquadUpdateFrame();

    Unit            findClosestDefender(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy);
    Unit            findWorkerToAssignToSquad(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy);
	bool			ShouldWorkerDefend(const Unit & woker, const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy);

    CCPosition      getMainAttackLocation();
	CCPosition		exploreMap();

    void            updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, Unit & closestEnemy);
    bool            shouldWeStartAttacking();

public:

    CombatCommander(CCBot & bot);


    void onStart();
    void onFrame(const std::vector<Unit> & combatUnits);
	void lowPriorityCheck();

	std::map<Unit, std::pair<CCPosition, uint32_t>> & GetInvisibleSighting();

    void drawSquadInformation();
};

