#pragma once

#include "Common.h"
#include "MicroManager.h"

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
		, executionFrame(0)
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
		, executionFrame(0)
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
		, executionFrame(0)
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
		, executionFrame(0)
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
		, executionFrame(0)
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
		, executionFrame(0)
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
		, executionFrame(0)
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

	RangedUnitAction& operator=(const RangedUnitAction&) = default;

	bool operator==(const RangedUnitAction& rangedUnitAction)
	{
		return microActionType == rangedUnitAction.microActionType && target == rangedUnitAction.target && position == rangedUnitAction.position && abilityID == rangedUnitAction.abilityID;
	}
};

enum FlyingHelperGoal
{
	ESCORT,
	TRACK
};

struct FlyingHelperMission
{
	FlyingHelperMission() : goal(ESCORT), position(CCPosition()) {}
	FlyingHelperMission(FlyingHelperGoal goal, CCPosition position) : goal(goal), position(position) {}
	
	FlyingHelperGoal goal;
	CCPosition position;
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
	std::map<const sc2::Unit *, uint32_t> nextPathFindingFrameForUnit;
	bool m_harassMode = false;
	std::map<const sc2::Unit *, FlyingHelperMission> m_cycloneFlyingHelpers;
	std::map<const sc2::Unit *, const sc2::Unit *> m_cyclonesWithHelper;

	bool isAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::Unit * rangedUnit) const;
	void setNextFrameAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::Unit * rangedUnit, uint32_t nextAvailableFrame);
	int getAttackDuration(const sc2::Unit* unit, const sc2::Unit* target) const;
	void HarassLogic(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets);
	void HarassLogicForUnit(const sc2::Unit* rangedUnit, sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets);
	bool ShouldSkipFrame(const sc2::Unit * rangedUnit) const;
	bool MonitorCyclone(const sc2::Unit * cyclone);
	bool IsCycloneLockOnCanceled(const sc2::Unit * cyclone, bool started) const;
	bool AllowUnitToPathFind(const sc2::Unit * rangedUnit, bool checkInfluence = true) const;
	bool ShouldBansheeCloak(const sc2::Unit * banshee, bool inDanger) const;
	bool ExecuteBansheeCloakLogic(const sc2::Unit * banshee, bool inDanger);
	bool ShouldBansheeUncloak(const sc2::Unit * banshee, CCPosition goal, sc2::Units & threats, bool unitShouldHeal) const;
	bool ExecuteBansheeUncloakLogic(const sc2::Unit * banshee, CCPosition goal, sc2::Units & threats, bool unitShouldHeal);
	bool ShouldUnitHeal(const sc2::Unit * rangedUnit) const;
	bool TeleportBattlecruiser(const sc2::Unit * battlecruiser, CCPosition location);
	CCPosition GetBestSupportPosition(const sc2::Unit* supportUnit, const sc2::Units & rangedUnits) const;
	bool ExecuteVikingMorphLogic(const sc2::Unit * viking, CCPosition goal, const sc2::Unit* target, bool unitShouldHeal, bool isCycloneHelper);
	bool ExecuteThorMorphLogic(const sc2::Unit * thor);
	bool MoveToGoal(const sc2::Unit * rangedUnit, sc2::Units & threats, const sc2::Unit * target, CCPosition & goal, bool unitShouldHeal, bool force);
	bool ShouldAttackTarget(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats) const;
	bool IsInRangeOfSlowerUnit(const sc2::Unit * rangedUnit, const sc2::Unit * target) const;
	CCPosition GetDirectionVectorTowardsGoal(const sc2::Unit * rangedUnit, const sc2::Unit * target, CCPosition goal, bool targetInAttackRange, bool unitShouldHeal) const;
	const sc2::Unit * ExecuteLockOnLogic(const sc2::Unit * cyclone, bool shouldHeal, bool & shouldAttack, bool & shouldUseLockOn, const sc2::Units & rangedUnits, const sc2::Units & threats, const sc2::Unit * target);
	void LockOnTarget(const sc2::Unit * cyclone, const sc2::Unit * target);
	bool CycloneHasTarget(const sc2::Unit * cyclone) const;
	bool ExecuteThreatFightingLogic(const sc2::Unit * rangedUnit, sc2::Units & rangedUnits, sc2::Units & threats);
	void ExecuteCycloneLogic(const sc2::Unit * rangedUnit, bool & unitShouldHeal, bool & shouldAttack, bool & cycloneShouldUseLockOn, bool & cycloneShouldStayCloseToTarget, const sc2::Units & rangedUnits, const sc2::Units & threats, const sc2::Unit * & target, CCPosition & goal);
	bool ExecutePrioritizedUnitAbilitiesLogic(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats, sc2::Units & targets, CCPosition goal, bool unitShouldHeal, bool isCycloneHelper);
	bool ExecuteUnitAbilitiesLogic(const sc2::Unit * rangedUnit, sc2::Units & threats);
	bool ExecuteOffensiveTeleportLogic(const sc2::Unit * battlecruiser, const sc2::Units & threats, CCPosition goal);
	bool ExecuteYamatoCannonLogic(const sc2::Unit * battlecruiser, const sc2::Units & targets);
	bool QueryIsAbilityAvailable(const sc2::Unit* unit, sc2::ABILITY_ID abilityId) const;
	bool CanUseKD8Charge(const sc2::Unit * reaper);
	bool ExecuteKD8ChargeLogic(const sc2::Unit * reaper, const sc2::Units & threats);
	bool ShouldBuildAutoTurret(const sc2::Unit * raven, const sc2::Units & threats) const;
	bool ExecuteAutoTurretLogic(const sc2::Unit * raven, const sc2::Units & threats);
	CCPosition GetFleeVectorFromThreat(const sc2::Unit * rangedUnit, const sc2::Unit * threat, CCPosition fleeVec, float distance, float threatRange) const;
	void AdjustSummedFleeVec(CCPosition & summedFleeVec) const;
	CCPosition GetRepulsionVectorFromFriendlyReapers(const sc2::Unit * reaper, sc2::Units & rangedUnits) const;
	CCPosition GetAttractionVectorToFriendlyHellions(const sc2::Unit * hellion, sc2::Units & rangedUnits) const;
	bool MoveUnitWithDirectionVector(const sc2::Unit * rangedUnit, CCPosition & directionVector, CCPosition & outPathableTile) const;
	CCPosition AttenuateZigzag(const sc2::Unit* rangedUnit, std::vector<const sc2::Unit*>& threats, CCPosition safeTile, CCPosition summedFleeVec) const;
	const sc2::Unit * getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets, bool filterHigherUnits = false) const;
	bool PlanAction(const sc2::Unit* rangedUnit, RangedUnitAction action);
	void CleanActions(sc2::Units &rangedUnits);
	void ExecuteActions();
	void CleanLockOnTargets() const;
	void CalcBestFlyingCycloneHelpers();
};
