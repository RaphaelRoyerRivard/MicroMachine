#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"

class CCBot;

class CombatCommander
{
    CCBot &         m_bot;

    SquadData       m_squadData;
    std::vector<Unit>  m_combatUnits;
    bool            m_initialized;
    bool            m_attackStarted;
	int				m_currentBaseExplorationIndex;

    void            updateScoutDefenseSquad();
    void            updateDefenseSquads();
    void            updateBackupSquads();
    void            updateAttackSquads();
    void            updateIdleSquad();
    bool            isSquadUpdateFrame();

    Unit            findClosestDefender(const Squad & defenseSquad, const CCPosition & pos);
    Unit            findWorkerToAssignToSquad(const Squad & defenseSquad, const CCPosition & pos);

    CCPosition      getMainAttackLocation();
	CCPosition		exploreMap();

    void            updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded);
    bool            shouldWeStartAttacking();

public:

    CombatCommander(CCBot & bot);


    void onStart();
    void onFrame(const std::vector<Unit> & combatUnits);

    void drawSquadInformation();
};

