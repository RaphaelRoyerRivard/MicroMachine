#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "BaseLocation.h"

class CCBot;
struct RegionArmyInformation;

class CombatCommander
{
	const int FRAME_BEFORE_SIGHTING_INVALIDATED = 25;

    CCBot &         m_bot;

    SquadData       m_squadData;
    std::vector<Unit>  m_combatUnits;
	std::map<Unit, std::pair<CCPosition, uint32_t>> m_invisibleSighting;
	std::vector<std::vector<float>> m_groundInfluenceMap;
	std::vector<std::vector<float>> m_airInfluenceMap;
	std::vector<std::vector<float>> m_groundEffectInfluenceMap;
	std::vector<std::vector<float>> m_airEffectInfluenceMap;
	std::vector<std::vector<bool>> m_blockedTiles;
	
    bool            m_initialized;
    bool            m_attackStarted;
	int				m_currentBaseExplorationIndex;
	int				m_currentBaseScoutingIndex;
	std::vector<const BaseLocation*> m_visitedBaseLocations;

    void            updateScoutDefenseSquad();
	void            updateDefenseBuildings();
    void            updateDefenseSquads();
    void            updateBackupSquads();
	void            updateScoutSquad();
	void            updateHarassSquads();
	void            updateAttackSquads();
    void            updateIdleSquad();
    bool            isSquadUpdateFrame();

    Unit            findClosestDefender(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy, std::string type);
    Unit            findWorkerToAssignToSquad(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy);
	bool			ShouldWorkerDefend(const Unit & woker, const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy);

	CCPosition		exploreMap();
	CCPosition		GetNextBaseLocationToScout();

    void            updateDefenseSquadUnits(Squad & defenseSquad, bool flyingDefendersNeeded, bool groundDefendersNeeded, Unit & closestEnemy);
    bool            shouldWeStartAttacking();

	void			initInfluenceMaps();
	void			resetInfluenceMaps();
	void			updateInfluenceMaps();
	void			updateInfluenceMapsWithUnits();
	void			updateInfluenceMapsWithEffects();
	void			updateGroundInfluenceMapForUnit(const Unit& enemyUnit);
	void			updateAirInfluenceMapForUnit(const Unit& enemyUnit);
	void			updateInfluenceMapForUnit(const Unit& enemyUnit, const bool ground);
	void			updateInfluenceMap(const float dps, const float range, const float speed, const CCPosition & position, const bool ground, const bool effect);
	void			updateBlockedTilesWithUnit(const Unit& unit);
	void			drawInfluenceMaps();
	void			drawBlockedTiles();

public:

    CombatCommander(CCBot & bot);


    void onStart();
    void onFrame(const std::vector<Unit> & combatUnits);
	void lowPriorityCheck();

	std::map<Unit, std::pair<CCPosition, uint32_t>> & GetInvisibleSighting();

	const std::vector<std::vector<float>> & getGroundInfluenceMap() const { return m_groundInfluenceMap; }
	const std::vector<std::vector<float>> & getAirInfluenceMap() const { return m_airInfluenceMap; }
	const std::vector<std::vector<float>> & getGroundEffectInfluenceMap() const { return m_groundEffectInfluenceMap; }
	const std::vector<std::vector<float>> & getAirEffectInfluenceMap() const { return m_airEffectInfluenceMap; }
	const std::vector<std::vector<bool>> & getBlockedTiles() const { return m_blockedTiles; }

	CCPosition getMainAttackLocation();
    void drawSquadInformation();
};

