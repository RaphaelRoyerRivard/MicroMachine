#pragma once

#include "Common.h"
#include "UnitType.h"

class CCBot;
class Unit;

namespace Util
{
	static const int DELAY_BETWEEN_ERROR = 120;
	static std::vector<std::string> displayedError;
	static std::ofstream file;
	static std::string mapName;

	//used for optimisation
	static UnitType refineryType;
	static UnitType richRefineryType;
	static UnitType depotType;
	static UnitType workerType;
	static UnitType supplyType;

	static bool allowDebug;
	
	struct UnitCluster
	{
		CCPosition m_center;
		std::vector<const sc2::Unit *> m_units;

		UnitCluster(CCPosition center, std::vector<const sc2::Unit *> units)
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
	};

	static std::list<UnitCluster> m_unitClusters;
	static uint32_t m_lastUnitClusterFrame;

    struct IsUnit 
    {
        sc2::UNIT_TYPEID m_type;

        IsUnit(sc2::UNIT_TYPEID type);
        bool operator()(const sc2::Unit * unit, const sc2::ObservationInterface*);
    };

	namespace PathFinding
	{
		struct IMNode;

		struct SafePathResult
		{
			CCPosition m_position;
			uint32_t m_frame;
			bool m_result;

			SafePathResult()
				: m_position(CCPosition())
				, m_frame(0)
				, m_result(true)
			{}

			SafePathResult(CCPosition position, uint32_t frame, bool result)
				: m_position(position)
				, m_frame(frame)
				, m_result(result)
			{}
		};

		static std::map<sc2::Tag, std::vector<SafePathResult>> m_lastPathFindingResultsForUnit;

		bool SetContainsNode(const std::set<IMNode*> & set, IMNode* node, bool mustHaveLowerCost);
		bool IsPathToGoalSafe(const sc2::Unit * unit, CCPosition goal, CCBot & bot);
		CCPosition FindOptimalPathToTarget(const sc2::Unit * unit, CCPosition goal, const sc2::Unit* target, float maxRange, CCBot & bot);
		CCPosition FindOptimalPathToSafety(const sc2::Unit * unit, CCPosition goal, CCBot & bot);
		CCPosition FindOptimalPathToSaferRange(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot);
		float FindOptimalPathDistance(const sc2::Unit * unit, CCPosition goal, bool ignoreInfluence, CCBot & bot);
		CCPosition FindOptimalPathPosition(const sc2::Unit * unit, CCPosition goal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, CCBot & bot);
		CCPosition FindOptimalPathToDodgeEffectTowardsGoal(const sc2::Unit * unit, CCPosition goal, float range, CCBot & bot);
		std::list<CCPosition> FindOptimalPath(const sc2::Unit * unit, CCPosition goal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, bool ignoreInfluence, bool flee, CCBot & bot);
		CCTilePosition GetNeighborNodePosition(int x, int y, IMNode* currentNode, const sc2::Unit * rangedUnit, CCBot & bot);
		CCPosition GetCommandPositionFromPath(std::list<CCPosition> & path, const sc2::Unit * rangedUnit, CCBot & bot);
		std::list<CCPosition> GetPositionListFromPath(IMNode* currentNode, const sc2::Unit * rangedUnit, CCBot & bot);
		float CalcEuclidianDistanceHeuristic(CCTilePosition from, CCTilePosition to);
		bool HasInfluenceOnTile(const CCTilePosition position, bool isFlying, CCBot & bot);
		bool HasCombatInfluenceOnTile(const IMNode* node, const sc2::Unit * unit, CCBot & bot);
		bool HasCombatInfluenceOnTile(const CCTilePosition position, bool isFlying, CCBot & bot);
		bool HasCombatInfluenceOnTile(const IMNode* node, const sc2::Unit * unit, bool fromGround, CCBot & bot);
		bool HasCombatInfluenceOnTile(const CCTilePosition position, bool isFlying, bool fromGround, CCBot & bot);
		float GetTotalInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot);
		float GetTotalInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, CCBot & bot);
		float GetCombatInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot);
		float GetCombatInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, CCBot & bot);
		float GetCombatInfluenceOnTile(CCTilePosition tile, bool isFlying, bool fromGround, CCBot & bot);
		float GetCombatInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, bool fromGround, CCBot & bot);
		bool HasEffectInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot);
		bool HasEffectInfluenceOnTile(const IMNode* node, const sc2::Unit * unit, CCBot & bot);
		bool IsUnitOnTileWithInfluence(const sc2::Unit * unit, CCBot & bot);
		float GetEffectInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, CCBot & bot);
		float GetEffectInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot);
	}

	void Initialize(CCBot & bot, CCRace race);
	void SetAllowDebug(bool _allowDebug);

	void SetMapName(std::string _mapName);
	std::string GetMapName();
	
	template< typename O, typename S>
	bool Contains(O object, S structure) { return std::find(structure.begin(), structure.end(), object) != structure.end(); }
	template< typename O, typename S>
	typename S::iterator Find(O object, S structure) { return std::find(structure.begin(), structure.end(), object); }

	std::list<UnitCluster> & GetUnitClusters(const sc2::Units & units, const std::vector<sc2::UNIT_TYPEID> & typesToIgnore, CCBot & bot);

	void CCUnitsToSc2Units(const std::vector<Unit> & units, sc2::Units & outUnits);
	void Sc2UnitsToCCUnits(const sc2::Units & units, std::vector<Unit> & outUnits, CCBot & bot);

	bool CanUnitAttackAir(const sc2::Unit * unit, CCBot & bot);
	bool CanUnitAttackGround(const sc2::Unit * unit, CCBot & bot);
    float GetAttackRangeForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot);
    float GetMaxAttackRangeForTargets(const sc2::Unit * unit, const std::vector<const sc2::Unit *> & targets, CCBot & bot);
	float GetMaxAttackRange(const sc2::Unit * unit, CCBot & bot);
    float GetMaxAttackRange(const sc2::UnitTypeID unitType, CCBot & bot);
    float GetMaxAttackRange(sc2::UnitTypeData unitTypeData, CCBot & bot);
	float GetSpecialCaseRange(const sc2::Unit* unit, sc2::Weapon::TargetType where = sc2::Weapon::TargetType::Any);
	float GetSpecialCaseRange(const sc2::UNIT_TYPEID unitType, sc2::Weapon::TargetType where = sc2::Weapon::TargetType::Any);
	float GetGroundAttackRange(const sc2::Unit * unit, CCBot & bot);
	float GetAirAttackRange(const sc2::Unit * unit, CCBot & bot);
	float GetAttackRangeBonus(const sc2::UnitTypeID unitType, CCBot & bot);
    float GetArmor(const sc2::Unit * unit, CCBot & bot);
	float GetGroundDps(const sc2::Unit * unit, CCBot & bot);
	float GetAirDps(const sc2::Unit * unit, CCBot & bot);
	float GetDps(const sc2::Unit * unit, CCBot & bot);
	float GetDps(const sc2::Unit * unit, const sc2::Weapon::TargetType targetType, CCBot & bot);
    float GetDpsForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot);
    float GetSpecialCaseDps(const sc2::Unit * unit, CCBot & bot, sc2::Weapon::TargetType where = sc2::Weapon::TargetType::Any);
	std::vector<const sc2::Unit *> getThreats(const sc2::Unit * unit, const std::vector<const sc2::Unit *> & targets, CCBot & bot);
	std::vector<const sc2::Unit *> getThreats(const sc2::Unit * unit, const std::vector<Unit> & targets, CCBot & bot);
	float getThreatRange(const sc2::Unit * unit, const sc2::Unit * threat, CCBot & m_bot);
	float getAverageSpeedOfUnits(const std::vector<Unit>& units, CCBot & bot);
	float getSpeedOfUnit(const sc2::Unit * unit, CCBot & bot);

	bool IsPositionUnderDetection(CCPosition position, CCBot & bot);
    
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
	float           GetNorm(const sc2::Point2D& point);
    void            Normalize(sc2::Point2D& point);
    sc2::Point2D    Normalized(const sc2::Point2D& point);
    float           GetDotProduct(const sc2::Point2D& v1, const sc2::Point2D& v2);
    sc2::UnitTypeData GetUnitTypeDataFromUnitTypeId(const sc2::UnitTypeID unitTypeId, CCBot & bot);

    sc2::UnitTypeID GetUnitTypeIDFromName(const std::string & name, CCBot & bot);
    sc2::UpgradeID  GetUpgradeIDFromName(const std::string & name, CCBot & bot);

    sc2::Point2D CalcLinearRegression(const std::vector<const sc2::Unit *> & units);
    sc2::Point2D CalcPerpendicularVector(const sc2::Point2D & vector);
    
    // Kevin-provided helper functions
    void    VisualizeGrids(const sc2::ObservationInterface* obs, sc2::DebugInterface* debug);
    float   TerainHeight(const sc2::GameInfo& info, const sc2::Point2D& point);
    bool    Placement(const sc2::GameInfo& info, const sc2::Point2D& point);
    bool    Pathable(const sc2::GameInfo& info, const sc2::Point2D& point);


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
	void			Log(const std::string & function, const std::string & message, CCBot & bot);
    UnitType        GetRessourceDepotType();
    UnitType        GetRefineryType();
	UnitType		GetRichRefineryType();
	UnitType        GetSupplyProvider();
	UnitType        GetWorkerType();
    bool            IsZerg(const CCRace & race);
    bool            IsProtoss(const CCRace & race);
    bool            IsTerran(const CCRace & race);
	int				ToMapKey(const CCTilePosition position);
	CCTilePosition	FromCCTilePositionMapKey(const int mapKey);
    CCPositionType  TileToPosition(float tile);

#ifdef SC2API
    sc2::BuffID     GetBuffFromName(const std::string & name, CCBot & bot);
    sc2::AbilityID  GetAbilityFromName(const std::string & name, CCBot & bot);
    sc2::Point2DI   ConvertWorldToCamera(const sc2::GameInfo& game_info, const sc2::Point2D camera_world, const sc2::Point2D& world);
#endif

    float Dist(const Unit & unit, const CCPosition & p2);
    float Dist(const Unit & unit1, const Unit & unit2);
	float Dist(const CCTilePosition & p1, const CCTilePosition & p2);
    float Dist(const CCPosition & p1, const CCPosition & p2);
	CCPositionType DistSq(const CCTilePosition & p1, const CCTilePosition & p2);
	CCPositionType DistSq(const Unit & unit, const CCPosition & p2);
	CCPositionType DistSq(const Unit & unit, const Unit & unit2);
    CCPositionType DistSq(const CCPosition & p1, const CCPosition & p2);
};
