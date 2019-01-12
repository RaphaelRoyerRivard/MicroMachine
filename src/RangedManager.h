#pragma once

#include "Common.h"
#include "MicroManager.h"

struct IMNode;
class CCBot;

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
	{}
	RangedUnitAction(MicroActionType microActionType, bool prioritize, int duration)
		: microActionType(microActionType)
		, target(nullptr)
		, position(CCPosition())
		, abilityID(0)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
	{}
	RangedUnitAction(MicroActionType microActionType, const sc2::Unit* target, bool prioritize, int duration)
		: microActionType(microActionType)
		, target(target)
		, position(CCPosition())
		, abilityID(0)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
	{}
	RangedUnitAction(MicroActionType microActionType, CCPosition position, bool prioritize, int duration)
		: microActionType(microActionType)
		, target(nullptr)
		, position(position)
		, abilityID(0)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
	{}
	RangedUnitAction(MicroActionType microActionType, sc2::AbilityID abilityID, bool prioritize, int duration)
		: microActionType(microActionType)
		, target(nullptr)
		, position(CCPosition())
		, abilityID(abilityID)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
	{}
	RangedUnitAction(MicroActionType microActionType, sc2::AbilityID abilityID, CCPosition position, bool prioritize, int duration)
		: microActionType(microActionType)
		, target(nullptr)
		, position(position)
		, abilityID(abilityID)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
	{}
	RangedUnitAction(MicroActionType microActionType, sc2::AbilityID abilityID, const sc2::Unit* target, bool prioritize, int duration)
		: microActionType(microActionType)
		, target(target)
		, position(CCPosition())
		, abilityID(abilityID)
		, prioritized(prioritize)
		, executed(false)
		, finished(false)
		, duration(duration)
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

	RangedUnitAction& operator=(const RangedUnitAction&) = default;

	bool operator==(const RangedUnitAction& rangedUnitAction)
	{
		return microActionType == rangedUnitAction.microActionType && target == rangedUnitAction.target && position == rangedUnitAction.position && abilityID == rangedUnitAction.abilityID;
	}
};

class RangedManager : public MicroManager
{
public:

    RangedManager(CCBot & bot);
	void setHarassMode(bool harass) { m_harassMode = harass; }
    void setTargets(const std::vector<Unit> & targets) override;
    void executeMicro() override;
	bool isTargetRanged(const sc2::Unit * target);

private:
	std::map<const sc2::Unit *, RangedUnitAction> unitActions;
	std::map<const sc2::Unit *, uint32_t> nextCommandFrameForUnit;
	std::map<const sc2::Unit *, uint32_t> nextAvailableKD8ChargeFrameForReaper;
	std::map<const sc2::Unit *, uint32_t> nextPathFindingFrameForUnit;
	std::set<const sc2::Unit *> unitsBeingRepaired;
	bool isCommandDone = false;
	bool m_harassMode = false;

	void RunBehaviorTree(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets);
	void setNextCommandFrameAfterAttack(const sc2::Unit* unit);
	int getAttackDuration(const sc2::Unit* unit) const;
	void HarassLogic(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets);
	void HarassLogicForUnit(const sc2::Unit* rangedUnit, sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets);
	bool ShouldSkipFrame(const sc2::Unit * rangedUnit) const;
	bool AllowUnitToPathFind(const sc2::Unit * rangedUnit) const;
	bool ExecuteBansheeCloakLogic(const sc2::Unit * banshee);
	bool ShouldUnitHeal(const sc2::Unit * rangedUnit);
	bool ExecuteVikingMorphLogic(const sc2::Unit * viking, float squaredDistanceToGoal, const sc2::Unit* target, bool unitShouldHeal);
	bool MoveToGoal(const sc2::Unit * rangedUnit, sc2::Units & threats, const sc2::Unit * target, CCPosition & goal, float squaredDistanceToGoal, bool unitShouldHeal);
	bool ShouldAttackTarget(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats) const;
	CCPosition GetDirectionVectorTowardsGoal(const sc2::Unit * rangedUnit, const sc2::Unit * target, CCPosition goal, bool targetInAttackRange) const;
	bool ExecuteThreatFightingLogic(const sc2::Unit * rangedUnit, sc2::Units & rangedUnits, sc2::Units & threats);
	bool CanUseKD8Charge(const sc2::Unit * rangedUnit) const;
	bool ExecuteKD8ChargeLogic(const sc2::Unit * rangedUnit, const sc2::Unit * threat);
	CCPosition GetFleeVectorFromThreat(const sc2::Unit * rangedUnit, const sc2::Unit * threat, CCPosition fleeVec, float distance, float threatRange) const;
	void AdjustSummedFleeVec(CCPosition & summedFleeVec) const;
	CCPosition GetRepulsionVectorFromFriendlyReapers(const sc2::Unit * reaper, sc2::Units & rangedUnits) const;
	CCPosition GetAttractionVectorToFriendlyHellions(const sc2::Unit * hellion, sc2::Units & rangedUnits) const;
	bool MoveUnitWithDirectionVector(const sc2::Unit * rangedUnit, CCPosition & directionVector, CCPosition & outPathableTile) const;
	CCPosition FindOptimalPathToTarget(const sc2::Unit * rangedUnit, CCPosition goal, float maxRange) const;
	CCPosition FindOptimalPathToSafety(const sc2::Unit * rangedUnit, CCPosition goal) const;
	CCPosition FindOptimalPath(const sc2::Unit * rangedUnit, CCPosition goal, float maxRange) const;
	bool IsNeighborNodeValid(int x, int y, IMNode* currentNode, const sc2::Unit * rangedUnit) const;
	CCPosition GetCommandPositionFromPath(IMNode* currentNode, const sc2::Unit * rangedUnit) const;
	float CalcEuclidianDistanceHeuristic(CCTilePosition from, CCTilePosition to) const;
	bool ShouldTriggerExit(const IMNode* node, const sc2::Unit * unit, CCPosition goal, float maxRange) const;
	bool ShouldTriggerExit(const IMNode* node, const sc2::Unit * unit) const;
	float GetInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit) const;
	CCPosition AttenuateZigzag(const sc2::Unit* rangedUnit, std::vector<const sc2::Unit*>& threats, CCPosition safeTile, CCPosition summedFleeVec) const;
	float getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * target) const;
	const sc2::Unit * getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets);
	bool PlanAction(const sc2::Unit* rangedUnit, RangedUnitAction action);
	void FlagActionsAsFinished();
	void ExecuteActions();
};
