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
	std::vector<std::vector<float>> m_groundFromGroundCombatInfluenceMap;
	std::vector<std::vector<float>> m_groundFromAirCombatInfluenceMap;
	std::vector<std::vector<float>> m_airFromGroundCombatInfluenceMap;
	std::vector<std::vector<float>> m_airFromAirCombatInfluenceMap;
	std::vector<std::vector<float>> m_groundEffectInfluenceMap;
	std::vector<std::vector<float>> m_airEffectInfluenceMap;
	std::vector<std::vector<bool>> m_blockedTiles;
	std::vector<CCPosition> m_enemyScans;
	std::map<sc2::ABILITY_ID, std::map<const sc2::Unit *, uint32_t>> m_nextAvailableAbility;
	std::map<sc2::ABILITY_ID, float> m_abilityCastingRanges;
	std::set<const sc2::Unit *> m_unitsBeingRepaired;
	std::set<const sc2::Unit *> m_queryYamatoAvailability;
	std::map<const sc2::Unit *, std::map<const sc2::Unit *, uint32_t>> m_yamatoTargets;	// <target, <bc, frame>>
	std::map<const sc2::Unit *, std::pair<const sc2::Unit *, uint32_t>> m_lockOnCastedFrame;
	std::map<const sc2::Unit *, std::pair<const sc2::Unit *, uint32_t>> m_lockOnTargets;	// <cyclone, target, frame>>
	std::set<sc2::Tag> m_newCyclones;
	std::set<sc2::Tag> m_toggledCyclones;
	
    bool            m_initialized;
    bool            m_attackStarted;
	int				m_currentBaseExplorationIndex;
	int				m_currentBaseScoutingIndex;
	std::vector<const BaseLocation*> m_visitedBaseLocations;
	uint32_t			m_lastWorkerRushDetectionFrame = 0;

	void			clearYamatoTargets();

    void            updateScoutDefenseSquad();
	void            updateDefenseBuildings();
	void			handleWall();
	void			lowerSupplyDepots();
    void            updateDefenseSquads();
    void            updateBackupSquads();
	void			updateClearExpandSquads();
	void            updateScoutSquad();
	void            updateHarassSquads();
	void            updateAttackSquads();
	void            updateIdleSquad();
	void            updateWorkerFleeSquad();
    bool            isSquadUpdateFrame();

    Unit            findClosestDefender(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy, std::string type);
    Unit            findWorkerToAssignToSquad(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy);
	bool			ShouldWorkerDefend(const Unit & woker, const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy) const;

	CCPosition		exploreMap();
	CCPosition		GetClosestEnemyBaseLocation();
	CCPosition		GetNextBaseLocationToScout();

    void            updateDefenseSquadUnits(Squad & defenseSquad, bool flyingDefendersNeeded, bool groundDefendersNeeded, Unit & closestEnemy);
    bool            shouldWeStartAttacking();
	
	void			resetInfluenceMaps();
	void			updateInfluenceMaps();
	void			updateInfluenceMapsWithUnits();
	void			updateInfluenceMapsWithEffects();
	void			updateGroundInfluenceMapForUnit(const Unit& enemyUnit);
	void			updateAirInfluenceMapForUnit(const Unit& enemyUnit);
	void			updateInfluenceMapForUnit(const Unit& enemyUnit, const bool ground);
	void			updateInfluenceMap(float dps, float range, float speed, const CCPosition & position, bool ground, bool fromGround, bool effect);
	void			updateBlockedTilesWithUnit(const Unit& unit);
	void			drawInfluenceMaps();
	void			drawBlockedTiles();

public:

    CombatCommander(CCBot & bot);


    void onStart();
    void onFrame(const std::vector<Unit> & combatUnits);
	void lowPriorityCheck();

	std::map<Unit, std::pair<CCPosition, uint32_t>> & GetInvisibleSighting();
	const std::vector<CCPosition> & GetEnemyScans() const { return m_enemyScans; }
	std::map<sc2::ABILITY_ID, std::map<const sc2::Unit *, uint32_t>> & getNextAvailableAbility() { return m_nextAvailableAbility; }
	std::map<sc2::ABILITY_ID, float> & getAbilityCastingRanges() { return m_abilityCastingRanges; }
	std::set<const sc2::Unit *> & getUnitsBeingRepaired() { return m_unitsBeingRepaired; }
	std::set<const sc2::Unit *> & getQueryYamatoAvailability() { return m_queryYamatoAvailability; }
	std::map<const sc2::Unit *, std::map<const sc2::Unit *, uint32_t>> & getYamatoTargets() { return m_yamatoTargets; }
	std::map<const sc2::Unit *, std::pair<const sc2::Unit *, uint32_t>> & getLockOnCastedFrame() { return m_lockOnCastedFrame; }
	std::map<const sc2::Unit *, std::pair<const sc2::Unit *, uint32_t>> & getLockOnTargets() { return m_lockOnTargets; }
	std::set<sc2::Tag> & getNewCyclones() { return m_newCyclones; }
	std::set<sc2::Tag> & getToggledCyclones() { return m_toggledCyclones; }
	const std::vector<std::vector<bool>> & getBlockedTiles() const { return m_blockedTiles; }
	float getTotalGroundInfluence(CCTilePosition tilePosition) const;
	float getTotalAirInfluence(CCTilePosition tilePosition) const;
	float getGroundCombatInfluence(CCTilePosition tilePosition) const;
	float getAirCombatInfluence(CCTilePosition tilePosition) const;
	float getGroundFromGroundCombatInfluence(CCTilePosition tilePosition) const;
	float getGroundFromAirCombatInfluence(CCTilePosition tilePosition) const;
	float getAirFromGroundCombatInfluence(CCTilePosition tilePosition) const;
	float getAirFromAirCombatInfluence(CCTilePosition tilePosition) const;
	float getGroundEffectInfluence(CCTilePosition tilePosition) const;
	float getAirEffectInfluence(CCTilePosition tilePosition) const;
	bool isTileBlocked(int x, int y);

	void initInfluenceMaps();
	CCPosition getMainAttackLocation();
	void updateBlockedTilesWithNeutral();
    void drawSquadInformation();
};

