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
	std::vector<std::vector<float>> m_groundInfluenceMap;
	std::vector<std::vector<float>> m_airInfluenceMap;
	std::vector<std::vector<bool>> m_blockedTiles;
	std::map<sc2::UNIT_TYPEID, float> totalDamage;
	std::map<sc2::UNIT_TYPEID, float> totalhealthLoss;
	
    bool            m_initialized;
    bool            m_attackStarted;
	int				m_currentBaseExplorationIndex;

    void            updateScoutDefenseSquad();
	void            updateDefenseBuildings();
    void            updateDefenseSquads();
    void            updateBackupSquads();
	void            updateHarassSquads();
	void            updateAttackSquads();
    void            updateIdleSquad();
	void			checkUnitsState();
	void			UpdateTotalHealthLoss();
    bool            isSquadUpdateFrame();

    Unit            findClosestDefender(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy, std::string type);
    Unit            findWorkerToAssignToSquad(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy);
	bool			ShouldWorkerDefend(const Unit & woker, const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy);

    CCPosition      getMainAttackLocation();
	CCPosition		exploreMap();

    void            updateDefenseSquadUnits(Squad & defenseSquad, size_t flyingDefendersNeeded, size_t groundDefendersNeeded, Unit & closestEnemy);
    bool            shouldWeStartAttacking();

	void			initInfluenceMaps();
	void			resetInfluenceMaps();
	void			updateInfluenceMaps();
	void			updateInfluenceMapsWithUnits();
	void			updateInfluenceMapsWithEffects();
	void			updateGroundInfluenceMapForUnit(const Unit& enemyUnit);
	void			updateAirInfluenceMapForUnit(const Unit& enemyUnit);
	void			updateInfluenceMapForUnit(const Unit& enemyUnit, const bool ground);
	void			updateInfluenceMap(const float dps, const float range, const float speed, const CCPosition & position, const bool ground);
	void			updateBlockedTilesWithUnit(const Unit& unit);
	void			drawInfluenceMaps();
	void			drawBlockedTiles();
	void			drawDamageHealthRatio();

public:

    CombatCommander(CCBot & bot);


    void onStart();
    void onFrame(const std::vector<Unit> & combatUnits);
	void lowPriorityCheck();
	void increaseTotalDamage(float damageDealt, sc2::UNIT_TYPEID unittype);
	void increaseTotalHealthLoss(float healthLoss, sc2::UNIT_TYPEID unittype);

	std::map<Unit, std::pair<CCPosition, uint32_t>> & GetInvisibleSighting();

	const std::vector<std::vector<float>> & getGroundInfluenceMap() const { return m_groundInfluenceMap; }
	const std::vector<std::vector<float>> & getAirInfluenceMap() const { return m_airInfluenceMap; }
	const std::vector<std::vector<bool>> & getBlockedTiles() const { return m_blockedTiles; }

    void drawSquadInformation();
};

