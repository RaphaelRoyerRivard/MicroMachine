#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "BaseLocation.h"
#include <list>  

class CCBot;
struct RegionArmyInformation;

struct RangedUnitAction
{
	RangedUnitAction()
		: microActionType()
		, target(nullptr)
		, position(CCPosition())
		, abilityID(0)
		, prioritized(false)
		, executed(true)
		, finished(true)
		, duration(0)
		, executionFrame(0)
		, description("")
	{}
	RangedUnitAction(MicroActionType microActionType, bool prioritize, int duration, std::string description)
		: microActionType(microActionType)
		, target(nullptr)
		, position(CCPosition())
		, abilityID(0)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
		, executionFrame(0)
		, description(description)
	{}
	RangedUnitAction(MicroActionType microActionType, const sc2::Unit* target, bool prioritize, int duration, std::string description)
		: microActionType(microActionType)
		, target(target)
		, position(CCPosition())
		, abilityID(0)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
		, executionFrame(0)
		, description(description)
	{}
	RangedUnitAction(MicroActionType microActionType, CCPosition position, bool prioritize, int duration, std::string description)
		: microActionType(microActionType)
		, target(nullptr)
		, position(position)
		, abilityID(0)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
		, executionFrame(0)
		, description(description)
	{}
	RangedUnitAction(MicroActionType microActionType, sc2::AbilityID abilityID, bool prioritize, int duration, std::string description)
		: microActionType(microActionType)
		, target(nullptr)
		, position(CCPosition())
		, abilityID(abilityID)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
		, executionFrame(0)
		, description(description)
	{}
	RangedUnitAction(MicroActionType microActionType, sc2::AbilityID abilityID, CCPosition position, bool prioritize, int duration, std::string description)
		: microActionType(microActionType)
		, target(nullptr)
		, position(position)
		, abilityID(abilityID)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
		, executionFrame(0)
		, description(description)
	{}
	RangedUnitAction(MicroActionType microActionType, sc2::AbilityID abilityID, const sc2::Unit* target, bool prioritize, int duration, std::string description)
		: microActionType(microActionType)
		, target(target)
		, position(CCPosition())
		, abilityID(abilityID)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
		, executionFrame(0)
		, description(description)
	{}
	RangedUnitAction(const RangedUnitAction& rangedUnitAction) = default;
	MicroActionType microActionType;
	const sc2::Unit* target;
	CCPosition position;
	sc2::AbilityID abilityID;
	bool prioritized;
	bool executed;
	bool finished;
	int duration;
	uint32_t executionFrame;
	std::string description;

	RangedUnitAction& operator=(const RangedUnitAction&) = default;

	bool operator==(const RangedUnitAction& rangedUnitAction)
	{
		return microActionType == rangedUnitAction.microActionType && target == rangedUnitAction.target && position == rangedUnitAction.position && abilityID == rangedUnitAction.abilityID;
	}
};

const float CYCLONE_PREFERRED_MAX_DISTANCE_TO_HELPER = 4.f;

class CombatCommander
{
	const int FRAME_BEFORE_SIGHTING_INVALIDATED = 25;

    CCBot &         m_bot;
	uint32_t m_lastLowPriorityFrame = 0;
	uint32_t m_lastBlockedTilesResetFrame = 0;
	uint32_t m_lastBlockedTilesUpdateFrame = 0;
	uint32_t m_lastIdlePositionUpdateFrame = 0;
	CCPosition m_idlePosition;
    SquadData       m_squadData;
    std::vector<Unit>  m_combatUnits;
	std::map<const sc2::Unit *, RangedUnitAction> unitActions;
	std::map<const sc2::Unit *, uint32_t> nextCommandFrameForUnit;
	std::map<Unit, std::pair<CCPosition, uint32_t>> m_invisibleSighting;
	std::vector<std::vector<float>> m_groundFromGroundCombatInfluenceMap;
	std::vector<std::vector<float>> m_groundFromAirCombatInfluenceMap;
	std::vector<std::vector<float>> m_airFromGroundCombatInfluenceMap;
	std::vector<std::vector<float>> m_airFromAirCombatInfluenceMap;
	std::vector<std::vector<float>> m_groundEffectInfluenceMap;
	std::vector<std::vector<float>> m_airEffectInfluenceMap;
	std::vector<std::vector<float>> m_groundFromGroundCloakedCombatInfluenceMap;
	std::vector<std::vector<bool>> m_blockedTiles;
	std::vector<CCPosition> m_enemyScans;
	std::list<CCPosition> m_allyScans;
	std::map<sc2::ABILITY_ID, std::map<const sc2::Unit *, uint32_t>> m_nextAvailableAbility;
	std::map<sc2::ABILITY_ID, float> m_abilityCastingRanges;
	std::set<const sc2::Unit *> m_unitsBeingRepaired;
	std::set<const sc2::Unit *> m_queryYamatoAvailability;
	std::map<const sc2::Tag, std::map<const sc2::Tag, uint32_t>> m_yamatoTargets;	// <target, <bc, frame>>
	std::map<const sc2::Unit *, std::pair<const sc2::Unit *, uint32_t>> m_lockOnCastedFrame;
	std::map<const sc2::Unit *, std::pair<const sc2::Unit *, uint32_t>> m_lockOnTargets;	// <cyclone, <target, frame>>
	std::set<sc2::Tag> m_newCyclones;
	std::set<sc2::Tag> m_toggledCyclones;
	bool m_hasEnoughVikingsAgainstTempests = true;
	bool m_winAttackSimulation = true;
	int m_lastRetreatFrame = 0;
	bool m_logVikingActions = false;
	bool m_allowEarlyBuildingAttack = false;
    bool            m_initialized;
    bool            m_attackStarted;
	int				m_currentBaseExplorationIndex;
	int				m_currentBaseScoutingIndex;
	std::vector<const BaseLocation*> m_visitedBaseLocations;
	uint32_t			m_lastWorkerRushDetectionFrame = 0;
	std::map<const sc2::Unit *, FlyingHelperMission> m_cycloneFlyingHelpers;
	std::map<const sc2::Unit *, const sc2::Unit *> m_cyclonesWithHelper;
	std::vector<sc2::AvailableAbilities> m_unitsAbilities;

	void			clearYamatoTargets();
	void			clearAllyScans();
	void			updateIdlePosition();
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
    Unit            findWorkerToAssignToSquad(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy, const std::vector<Unit> & enemyUnits);
	bool			ShouldWorkerDefend(const Unit & woker, const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy, const std::vector<Unit> & enemyUnits) const;
	bool			WorkerHasFastEnemyThreat(const sc2::Unit * worker, const std::vector<Unit> & enemyUnits) const;

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
	void			updateInfluenceMap(float dps, float range, float speed, const CCPosition & position, bool ground, bool fromGround, bool effect, bool cloaked);
	void			updateBlockedTilesWithUnit(const Unit& unit);
	void			drawCombatInformation();
	void			drawInfluenceMaps();
	void			drawBlockedTiles();

public:

    CombatCommander(CCBot & bot);


    void onStart();
    void onFrame(const std::vector<Unit> & combatUnits);
	void lowPriorityCheck();
	const std::vector<Unit> & GetCombatUnits() const { return m_combatUnits; }
	std::map<Unit, std::pair<CCPosition, uint32_t>> & GetInvisibleSighting();
	const std::vector<CCPosition> & GetEnemyScans() const { return m_enemyScans; }
	std::map<sc2::ABILITY_ID, std::map<const sc2::Unit *, uint32_t>> & getNextAvailableAbility() { return m_nextAvailableAbility; }
	std::map<sc2::ABILITY_ID, float> & getAbilityCastingRanges() { return m_abilityCastingRanges; }
	std::set<const sc2::Unit *> & getUnitsBeingRepaired() { return m_unitsBeingRepaired; }
	std::set<const sc2::Unit *> & getQueryYamatoAvailability() { return m_queryYamatoAvailability; }
	std::map<const sc2::Tag, std::map<const sc2::Tag, uint32_t>> & getYamatoTargets() { return m_yamatoTargets; }
	std::map<const sc2::Unit *, std::pair<const sc2::Unit *, uint32_t>> & getLockOnCastedFrame() { return m_lockOnCastedFrame; }
	std::map<const sc2::Unit *, std::pair<const sc2::Unit *, uint32_t>> & getLockOnTargets() { return m_lockOnTargets; }
	std::set<sc2::Tag> & getNewCyclones() { return m_newCyclones; }
	std::set<sc2::Tag> & getToggledCyclones() { return m_toggledCyclones; }
	const std::vector<std::vector<bool>> & getBlockedTiles() const { return m_blockedTiles; }
	const std::map<const sc2::Unit *, FlyingHelperMission> & getCycloneFlyingHelpers() const { return m_cycloneFlyingHelpers; }
	const std::map<const sc2::Unit *, const sc2::Unit *> & getCyclonesWithHelper() const { return m_cyclonesWithHelper; }
	const std::list<CCPosition> & getAllyScans() const { return m_allyScans; }
	void addAllyScan(CCPosition scanPos) { m_allyScans.push_back(scanPos); }
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
	float getGroundFromGroundCloakedCombatInfluence(CCTilePosition tilePosition) const;
	bool isTileBlocked(int x, int y);
	bool hasEnoughVikingsAgainstTempests() const { return m_hasEnoughVikingsAgainstTempests; }
	bool winAttackSimulation() const { return m_winAttackSimulation; }
	void initInfluenceMaps();
	CCPosition getMainAttackLocation();
	void updateBlockedTilesWithNeutral();
	void SetLogVikingActions(bool log);
	bool getAllowEarlyBuildingAttack() const { return m_allowEarlyBuildingAttack; }
	void setAllowEarlyBuildingAttack(bool allowEarlyBuildingAttack) { m_allowEarlyBuildingAttack = allowEarlyBuildingAttack; }
	bool ShouldSkipFrame(const sc2::Unit * combatUnit) const;
	bool PlanAction(const sc2::Unit* rangedUnit, RangedUnitAction action);
	void CleanActions(const std::vector<Unit> &rangedUnits);
	void ExecuteActions();
	RangedUnitAction& GetRangedUnitAction(const sc2::Unit * combatUnit);
	void CleanLockOnTargets();
	void CalcBestFlyingCycloneHelpers();
	bool ShouldUnitHeal(const sc2::Unit * unit) const;
	bool GetUnitAbilities(const sc2::Unit * unit, sc2::AvailableAbilities & outUnitAbilities) const;
};

