#pragma once

#include "Common.h"
#include "UnitType.h"
#include <list>
#include "libvoxelbot/combat/simulator.h"

class CCBot;
class Unit;

namespace Util
{
	static size_t HELLION_SQUAD_COUNT = 10;
	static float HARASS_REPAIR_STATION_MAX_HEALTH_PERCENTAGE = 0.3f;
	static const int DELAY_BETWEEN_ERROR = 120;
	static std::vector<std::string> displayedError;
	static std::map<std::string, std::vector<int>> statistics;
	static std::ofstream file;
	static std::string mapName;
	static CombatPredictor* m_simulator;

	//used for optimisation
	static sc2::UNIT_TYPEID richRefineryId;
	static sc2::UNIT_TYPEID richAssimilatorId;
	static sc2::UNIT_TYPEID richExtractorId;
	static UnitType refineryType;
	static UnitType richRefineryType;
	static UnitType depotType;
	static UnitType workerType;
	static UnitType supplyType;
	static const sc2::GameInfo * gameInfo;
	static std::vector<std::vector<bool>> m_pathable;
	static std::vector<std::vector<bool>> m_placement;
	static std::vector<std::vector<float>> m_terrainHeight;
	static sc2::Unit * m_dummyVikingAssault;
	static sc2::Unit * m_dummyVikingFighter;
	static sc2::Unit * m_dummyStimedMarine;
	static sc2::Unit * m_dummyStimedMarauder;
	static sc2::Unit * m_dummySiegeTank;
	static sc2::Unit * m_dummySiegeTankSieged;
	static std::map<std::pair<const sc2::Unit *, sc2::UNIT_TYPEID>, sc2::Unit *> m_dummyUnits;	// <<real_unit, type>, dummy_unit>
	static std::list<const sc2::Unit *> m_dummyBurrowedZerglings;
	static std::map<const sc2::Unit *, std::pair<std::set<const sc2::Unit *>, std::set<const sc2::Unit *>>> m_seenEnemies;	// <enemy, <allies_with_vision, allies_without_vision>

	static bool allowDebug;
	
	struct UnitCluster
	{
		CCPosition m_center;
		sc2::Units m_units;

		UnitCluster()
			: m_center(CCPosition())
			, m_units({})
		{};

		UnitCluster(CCPosition center, sc2::Units units)
			: m_center(center)
			, m_units(units)
		{};

		bool operator<(const UnitCluster & rhs) const
		{
			if(m_units.size() == rhs.m_units.size())
			{
				if(m_center.x == rhs.m_center.x)
				{
					return m_center.y < rhs.m_center.y;
				}
				return m_center.x < rhs.m_center.x;
			}
			return m_units.size() < rhs.m_units.size();
		}
		bool operator>(const UnitCluster & rhs) const
		{
			return rhs < *this;
		}
		bool operator==(const UnitCluster & rhs)
		{
			return m_center == rhs.m_center && m_units == rhs.m_units;
		}
	};

	static std::map<std::string, std::list<UnitCluster>> m_unitClusters;
	static uint32_t m_lastUnitClusterFrame;
	static uint32_t m_lastSpecialUnitClusterFrame;

    struct IsUnit 
    {
        sc2::UNIT_TYPEID m_type;

        IsUnit(sc2::UNIT_TYPEID type);
        bool operator()(const sc2::Unit * unit, const sc2::ObservationInterface*);
    };

	struct CombatSimulationResult
	{
		float supplyLost;
		float supplyPercentageRemaining;
		float enemySupplyLost;
		float enemySupplyPercentageRemaining;
	};

	namespace PathFinding
	{
		enum FailureReason
		{
			TIMEOUT,
			INFLUENCE,
			NO_NEED_TO_MOVE
		};
		
		struct IMNode;

		struct PathFindingResult
		{
			CCPosition m_from;
			CCPosition m_to;
			uint32_t m_expiration;
			bool m_safe;

			PathFindingResult()
				: m_from(CCPosition())
				, m_to(CCPosition())
				, m_expiration(0)
				, m_safe(true)
			{}

			PathFindingResult(CCPosition from, CCPosition to, uint32_t expiration, bool safe)
				: m_from(from)
				, m_to(to)
				, m_expiration(expiration)
				, m_safe(safe)
			{}
		};

		static std::map<sc2::UNIT_TYPEID, std::list<PathFindingResult>> m_lastPathFindingResultsForUnitType;

		bool SetContainsNode(const std::set<IMNode*> & set, IMNode* node, bool mustHaveLowerCost);
		void ClearExpiredPathFindingResults(long currentFrame);
		bool IsPathToGoalSafe(const sc2::Unit * unit, CCPosition goal, bool addBuffer, CCBot & bot);
		CCPosition FindOptimalPathToTarget(const sc2::Unit * unit, CCPosition goal, CCPosition secondaryGoal, const sc2::Unit* target, float maxRange, bool considerOnlyEffects, float maxInfluence, CCBot & bot);
		CCPosition FindEngagePosition(const sc2::Unit * unit, const sc2::Unit* target, float maxRange, CCBot & bot);
		CCPosition FindOptimalPathToSafety(const sc2::Unit * unit, CCPosition goal, bool shouldHeal, CCBot & bot);
		CCPosition FindOptimalPathToSaferRange(const sc2::Unit * unit, const sc2::Unit * target, float range, bool moveFarther, CCBot & bot);
		float FindOptimalPathDistance(const sc2::Unit * unit, CCPosition goal, bool ignoreInfluence, CCBot & bot);
		CCPosition FindOptimalPathPosition(const sc2::Unit * unit, CCPosition goal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, CCBot & bot);
		CCPosition FindOptimalPathToDodgeEffectAwayFromGoal(const sc2::Unit * unit, CCPosition goal, float range, CCBot & bot);
		std::list<CCPosition> FindOptimalPathWithoutLimit(const sc2::Unit * unit, CCPosition goal, CCBot & bot);
		std::list<CCPosition> FindOptimalPath(const sc2::Unit * unit, CCPosition goal, CCPosition secondaryGoal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, bool ignoreInfluence, float maxInfluence, bool flee, bool checkVisibility, CCBot & bot);
		std::list<CCPosition> FindOptimalPath(const sc2::Unit * unit, CCPosition goal, CCPosition secondaryGoal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, bool ignoreInfluence, float maxInfluence, bool flee, bool checkVisibility, bool limitSearch, FailureReason & failureReason, CCBot & bot);
		CCTilePosition GetNeighborNodePosition(int x, int y, IMNode* currentNode, const sc2::Unit * rangedUnit, CCBot & bot);
		CCPosition GetCommandPositionFromPath(std::list<CCPosition> & path, const sc2::Unit * rangedUnit, bool moveFarther, CCBot & bot);
		std::list<CCPosition> GetPositionListFromPath(IMNode* currentNode, const sc2::Unit * rangedUnit, CCBot & bot);
		float CalcEuclidianDistanceHeuristic(CCTilePosition from, CCTilePosition to, CCTilePosition secondaryGoal, CCBot & bot);
		bool HasInfluenceOnTile(const CCTilePosition position, bool isFlying, CCBot & bot);
		bool HasCombatInfluenceOnTile(const IMNode* node, const sc2::Unit * unit, CCBot & bot);
		bool HasCombatInfluenceOnTile(const CCTilePosition position, bool isFlying, CCBot & bot);
		bool HasCombatInfluenceOnTile(const IMNode* node, const sc2::Unit * unit, bool fromGround, CCBot & bot);
		bool HasCombatInfluenceOnTile(const CCTilePosition position, bool isFlying, bool fromGround, CCBot & bot);
		float GetTotalInfluenceOnTiles(CCPosition position, bool isFlying, float radius, CCBot & bot);
		float GetMaxInfluenceOnTiles(CCPosition position, bool isFlying, float radius, CCBot & bot);
		float GetTotalInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot);
		float GetTotalInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, CCBot & bot);
		float GetCombatInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot);
		float GetCombatInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, CCBot & bot);
		float GetCombatInfluenceOnTile(CCTilePosition tile, bool isFlying, bool fromGround, CCBot & bot);
		float GetCombatInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, bool fromGround, CCBot & bot);
		float GetGroundFromGroundCloakedInfluenceOnTile(CCTilePosition tile, CCBot & bot);
		bool HasGroundFromGroundCloakedInfluenceOnTile(CCTilePosition tile, CCBot & bot);
		bool HasEffectInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot);
		bool HasEffectInfluenceOnTile(const IMNode* node, const sc2::Unit * unit, CCBot & bot);
		bool IsUnitOnTileWithInfluence(const sc2::Unit * unit, CCBot & bot);
		float GetEffectInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, CCBot & bot);
		float GetEffectInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot);
	}

	void Initialize(CCBot & bot, CCRace race, const sc2::GameInfo & _gameInfo);
	void InitializeCombatSimulator();
	void SetAllowDebug(bool _allowDebug);

	void SetMapName(std::string _mapName);
	std::string GetMapName();
	
	template< typename O, typename S>
	bool Contains(O object, S structure) { return std::find(structure.begin(), structure.end(), object) != structure.end(); }
	template< typename O, typename S>
	typename S::iterator Find(O object, S structure) { return std::find(structure.begin(), structure.end(), object); }
	inline bool StringStartsWith(std::string s, std::string find) { return s.rfind(find, 0) == 0; }

	std::list<UnitCluster> GetUnitClusters(const sc2::Units & units, const std::vector<sc2::UNIT_TYPEID> & specialTypes, bool ignoreSpecialTypes, CCBot & bot);
	std::list<UnitCluster> & GetUnitClusters(const sc2::Units & units, const std::vector<sc2::UNIT_TYPEID> & specialTypes, bool ignoreSpecialTypes, std::string queryName, CCBot & bot);
	
	void CCUnitsToSc2Units(const std::vector<Unit> & units, sc2::Units & outUnits);
	void Sc2UnitsToCCUnits(const sc2::Units & units, std::vector<Unit> & outUnits, CCBot & bot);

	void CreateDummyUnits(CCBot & bot);
	void CreateDummyVikingAssault(CCBot & bot);
	void CreateDummyVikingFighter(CCBot & bot);
	void CreateDummyStimedMarine(CCBot & bot);
	void CreateDummyStimedMarauder(CCBot & bot);
	void CreateDummySiegeTank(CCBot & bot);
	void CreateDummySiegeTankSieged(CCBot & bot);
	sc2::Unit * CreateDummyBurrowedZergling(CCPosition pos, CCBot & bot);
	void ReleaseDummyBurrowedZergling(const sc2::Unit * burrowedZergling);
	void ReleaseDummyUnit(std::pair<const sc2::Unit *, sc2::UNIT_TYPEID> & unitPair);
	void ClearDeadDummyUnits();
	void SetBaseUnitValues(sc2::Unit * unit, CCBot & bot);
	sc2::Unit * CreateDummyFromUnit(sc2::Unit * dummyModel, const sc2::Unit * unit);
	void UpdateDummyUnit(sc2::Unit * dummy, const sc2::Unit * unit);
	sc2::Unit * CreateDummyFromUnit(const sc2::Unit * unit);
	sc2::Unit * CreateDummyVikingAssaultFromUnit(const sc2::Unit * unit);
	sc2::Unit * CreateDummyVikingFighterFromUnit(const sc2::Unit * unit);
	sc2::Unit * CreateDummyStimedMarineFromUnit(const sc2::Unit * unit);
	sc2::Unit * CreateDummyStimedMarauderFromUnit(const sc2::Unit * unit);
	sc2::Unit * CreateDummySiegeTankFromUnit(const sc2::Unit * unit);
	sc2::Unit * CreateDummySiegeTankSiegedFromUnit(const sc2::Unit * unit);
	bool CanUnitAttackAir(const sc2::Unit * unit, CCBot & bot);
	bool CanUnitAttackGround(const sc2::Unit * unit, CCBot & bot);
    float GetAttackRangeForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot, bool ignoreSpells = false);
    float GetMaxAttackRangeForTargets(const sc2::Unit * unit, const std::vector<const sc2::Unit *> & targets, CCBot & bot);
	float GetMaxAttackRange(const sc2::Unit * unit, CCBot & bot);
    float GetMaxAttackRange(const sc2::UnitTypeID unitType, CCBot & bot);
    float GetMaxAttackRange(sc2::UnitTypeData unitTypeData, CCBot & bot);
	float GetSpecialCaseRange(const sc2::Unit* unit, CCBot & bot, sc2::Weapon::TargetType where = sc2::Weapon::TargetType::Any, bool ignoreSpells = false);
	float GetSpecialCaseRange(const sc2::UNIT_TYPEID unitType, sc2::Weapon::TargetType where = sc2::Weapon::TargetType::Any, bool ignoreSpells = false);
	float GetGroundAttackRange(const sc2::Unit * unit, CCBot & bot);
	float GetAirAttackRange(const sc2::Unit * unit, CCBot & bot);
	float GetAttackRangeBonus(const sc2::UnitTypeID unitType, CCBot & bot);
    float GetArmor(const sc2::Unit * unit, CCBot & bot);
	float GetGroundDps(const sc2::Unit * unit, CCBot & bot);
	float GetAirDps(const sc2::Unit * unit, CCBot & bot);
	float GetDps(const sc2::Unit * unit, CCBot & bot);
	float GetDps(const sc2::Unit * unit, const sc2::Weapon::TargetType targetType, CCBot & bot);
    float GetDpsForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot);
	float GetAttackSpeedMultiplier(const sc2::Unit * unit);
    float GetSpecialCaseDps(const sc2::Unit * unit, CCBot & bot, sc2::Weapon::TargetType where = sc2::Weapon::TargetType::Any);
	float GetDamageForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot);
	float GetSpecialCaseDamage(const sc2::Unit * unit, CCBot & bot, sc2::Weapon::TargetType where = sc2::Weapon::TargetType::Any);
	float GetWeaponCooldown(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot);
	void getThreats(const sc2::Unit * unit, const sc2::Units & targets, sc2::Units & outThreats, CCBot & bot);
	sc2::Units getThreats(const sc2::Unit * unit, const sc2::Units & targets, CCBot & bot);
	sc2::Units getThreats(const sc2::Unit * unit, const std::vector<Unit> & targets, CCBot & bot);
	float getThreatRange(const sc2::Unit * unit, const sc2::Unit * threat, CCBot & m_bot);
	float getThreatRange(bool isFlying, CCPosition position, float radius, const sc2::Unit * threat, CCBot & m_bot);
	float getAverageSpeedOfUnits(const std::vector<Unit>& units, CCBot & bot);
	float getSpeedOfUnit(const sc2::Unit * unit, CCBot & bot);
	float getRealMovementSpeedOfUnit(const sc2::Unit * unit, CCBot & bot);
	CCPosition getFacingVector(const sc2::Unit * unit);
	bool isUnitFacingAnother(const sc2::Unit * unitA, const sc2::Unit * unitB);
	bool isUnitLockedOn(const sc2::Unit * unit);
	bool isUnitDisabled(const sc2::Unit * unit);
	bool isUnitLifted(const sc2::Unit * unit);
	bool isUnitAffectedByParasiticBomb(const sc2::Unit * unit);
	bool unitHasBuff(const sc2::Unit * unit, sc2::BUFF_ID buffId);
	void ClearSeenEnemies();
	bool AllyUnitSeesEnemyUnit(const sc2::Unit * exceptUnit, const sc2::Unit * enemyUnit, float visionBuffer, bool filterStationaryUnits, CCBot & bot);
	bool CanUnitSeeEnemyUnit(const sc2::Unit * unit, const sc2::Unit * enemyUnit, float buffer, CCBot & bot);
	bool IsEnemyHiddenOnHighGround(const sc2::Unit * unit, const sc2::Unit * enemyUnit, CCBot & bot);
	bool IsPositionUnderDetection(CCPosition position, CCBot & bot);
	bool IsUnitCloakedAndSafe(const sc2::Unit * unit, CCBot & bot);
	bool IsAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::Unit * unit, const std::vector<sc2::AvailableAbilities> & availableAbilitiesForUnits);
	bool IsAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::AvailableAbilities & availableAbilities);
    
    std::string     GetStringFromRace(const sc2::Race & race);
    sc2::Race       GetRaceFromString(const std::string & race);
    sc2::Point2D    CalcCenter(const std::vector<const sc2::Unit *> & units);
	sc2::Point2D    CalcCenter(const std::vector<const sc2::Unit *> & units, float& varianceOut);
	CCPosition      CalcCenter(const std::vector<Unit> & units);
	const sc2::Unit* CalcClosestUnit(const sc2::Unit* unit, const sc2::Units & targets);
	float           GetUnitsPower(const std::vector<Unit> & units, const std::vector<Unit> & targets, CCBot& bot);
	float			GetUnitsPower(const sc2::Units & units, const sc2::Units & targets, CCBot& bot);
	float			GetUnitPower(const sc2::Unit* unit, const sc2::Unit* closestUnit, CCBot& bot);
	float			GetUnitPower(const Unit &unit, const Unit& closestUnit, CCBot& m_bot);
	float			GetSpecialCasePower(const Unit &unit);
	float           GetNorm(const sc2::Point2D& point);
    void            Normalize(sc2::Point2D& point);
    sc2::Point2D    Normalized(const sc2::Point2D& point);
    float           GetDotProduct(const sc2::Point2D& v1, const sc2::Point2D& v2);
    void			GetOrthogonalVectors(const sc2::Point2D& referenceVector, sc2::Point2D& clockwiseVector, sc2::Point2D& counterClockwiseVector);
    const sc2::UnitTypeData & GetUnitTypeDataFromUnitTypeId(const sc2::UnitTypeID unitTypeId, CCBot & bot);

    sc2::UnitTypeID GetUnitTypeIDFromName(const std::string & name, CCBot & bot);
    sc2::UpgradeID  GetUpgradeIDFromName(const std::string & name, CCBot & bot);

    sc2::Point2D CalcLinearRegression(const std::vector<const sc2::Unit *> & units);
    sc2::Point2D CalcPerpendicularVector(const sc2::Point2D & vector);
    
    // Kevin-provided helper functions
    void    VisualizeGrids(CCBot& bot);
    float   TerrainHeight(const sc2::Point2D& point);
	float	TerrainHeight(const CCTilePosition pos);
	float	TerrainHeight(const int x, const int y);
    bool    Placement(const sc2::Point2D& point);
    bool    Pathable(const sc2::Point2D& point);


    CCRace          GetRaceFromString(const std::string & str);
    CCTilePosition  GetTilePosition(const CCPosition & pos);
    CCPosition      GetPosition(const CCTilePosition & tile);
    std::string     GetStringFromRace(const CCRace & race);
    bool            UnitCanMetaTypeNow(const Unit & unit, const UnitType & type, CCBot & bot);
	void			DisplayError(const std::string & error, const std::string & errorCode, CCBot & bot, bool isCritical = false);
	void			ClearDisplayedErrors();
	void			CreateLog(CCBot & bot);
	void			DebugLog(const std::string & function, CCBot & bot);
	void			DebugLog(const std::string & function, const std::string & message, CCBot & bot);
	void			LogNoFrame(const std::string & function, CCBot & bot);
	void			Log(const std::string & function, CCBot & bot);
	void			Log(const std::string & function, const std::string & message, const CCBot & bot);
	void			AddStatistic(const std::string & statisticName, int value);
	void			DisplayStatistic(CCBot & bot, const std::string & statisticName);
	void			ClearChat(CCBot & bot);
	int				GetTimeControlSpeed();
	int				GetTimeControlMaxSpeed();
	void			TimeControlIncreaseSpeed();
	void			TimeControlDecreaseSpeed();
    UnitType        GetResourceDepotType();
    UnitType        GetRefineryType();
	UnitType		GetRichRefineryType();
	UnitType        GetSupplyProvider();
	UnitType        GetWorkerType();
	sc2::UNIT_TYPEID GetRichRefineryId();
	sc2::UNIT_TYPEID GetRichAssimilatorId();
	sc2::UNIT_TYPEID GetRichExtractorId();
    bool            IsZerg(const CCRace & race);
    bool            IsProtoss(const CCRace & race);
    bool            IsTerran(const CCRace & race);
	bool			IsWorker(sc2::UNIT_TYPEID type);
	int				ToMapKey(const CCTilePosition position);
	CCTilePosition	FromCCTilePositionMapKey(const int mapKey);
    CCPositionType  TileToPosition(float tile);

#ifdef SC2API
    sc2::BuffID     GetBuffFromName(const std::string & name, CCBot & bot);
    sc2::AbilityID  GetAbilityFromName(const std::string & name, CCBot & bot);
    sc2::Point2DI   ConvertWorldToCamera(const sc2::Point2D camera_world, const sc2::Point2D& world);
#endif

    float Dist(const Unit & unit, const CCPosition & p2);
    float Dist(const Unit & unit1, const Unit & unit2);
	float Dist(const CCTilePosition & p1, const CCTilePosition & p2);
    float Dist(const CCPosition & p1, const CCPosition & p2);
	CCPositionType DistSq(const CCTilePosition & p1, const CCTilePosition & p2);
	CCPositionType DistSq(const Unit & unit, const CCPosition & p2);
	CCPositionType DistSq(const Unit & unit, const Unit & unit2);
    CCPositionType DistSq(const CCPosition & p1, const CCPosition & p2);
	float DistBetweenLineAndPoint(const CCPosition & linePoint1, const CCPosition & linePoint2, const CCPosition & point);

	CombatSimulationResult SimulateCombat(const sc2::Units & units, const sc2::Units & enemyUnits, bool considerOurTanksUnsieged, bool stopSimulationWhenGroupHasNoTarget, CCBot & bot);
	CombatSimulationResult SimulateCombat(const sc2::Units & units, const sc2::Units & simulatedUnits, const sc2::Units & enemyUnits, bool considerOurTanksUnsieged, bool stopSimulationWhenGroupHasNoTarget, CCBot & bot);
	int GetSelfPlayerId(const CCBot & bot);

	int GetSupplyOfUnits(const sc2::Units & units, CCBot & bot);
};
