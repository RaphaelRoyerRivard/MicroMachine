#pragma once

#include "Common.h"
#include "MicroManager.h"

class CCBot;

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
    void setTargets(const std::vector<Unit> & targets) override;
    void executeMicro() override;
	bool isTargetRanged(const sc2::Unit * target);
	void CalcCloseUnits(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & allyCombatUnits, sc2::Units & rangedUnitTargets, std::set<const sc2::Unit *> & closeUnitsSet);
	const sc2::Unit * getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets, bool harass = true, bool filterHigherUnits = false, bool considerOnlyUnitsInRange = false, bool filterPassiveBuildings = true, bool considerOnlyVisibleUnits = false);

private:
	std::vector<sc2::UNIT_TYPEID> supportTypes = {
		sc2::UNIT_TYPEID::TERRAN_RAVEN,
		sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER,
		sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT,
		sc2::UNIT_TYPEID::TERRAN_HELLION,
		sc2::UNIT_TYPEID::TERRAN_HELLIONTANK
	};
	std::map<const sc2::Unit *, std::map<std::string, uint32_t>> nextPathFindingFrameForUnit;
	std::map<std::set<const sc2::Unit *>, std::vector<bool>> m_combatSimulationResults;	// <ally units, <ground result, air result>>
	std::map<sc2::Tag, sc2::Unit> m_dummyAssaultVikings;
	std::map<sc2::Tag, sc2::Unit> m_dummyFighterVikings;
	std::map<sc2::Tag, sc2::Unit> m_dummyStimedUnits;
	std::map<const sc2::Unit *, sc2::Units> m_threatsForUnit;
	std::map<const sc2::Unit *, std::map<std::pair<int, std::set<const sc2::Unit *>>, const sc2::Unit *>> m_threatTargetForUnit;	//<unit, <<parameters, potential targets>, target>>
	std::map<const sc2::Unit *, long> m_siegedTanksLastValidTargetFrame;
	std::map<const sc2::Unit *, long> m_tanksLastFrameFarFromRetreatGoal;
	std::map<const sc2::Unit *, long> m_tanksLastSiegeFrame;
	std::map<const sc2::Unit *, long> m_lastStimFrame;
	bool m_flyingBarracksShouldReachEnemyRamp = true;
	bool m_marauderAttackInitiated = false;

	bool isAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::Unit * rangedUnit) const;
	void setNextFrameAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::Unit * rangedUnit, uint32_t nextAvailableFrame);
	int getAttackDuration(const sc2::Unit* unit, const sc2::Unit* target) const;
	void HarassLogic(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets, sc2::Units &otherSquadsUnits);
	void HarassLogicForUnit(const sc2::Unit* rangedUnit, sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets, sc2::AvailableAbilities &rangedUnitAbilities, sc2::Units &otherSquadsUnits);
	void GetInfiltrationGoalPosition(const sc2::Unit * rangedUnit, CCPosition & goal, std::string & goalDescription) const;
	bool MonitorCyclone(const sc2::Unit * cyclone, sc2::AvailableAbilities & abilities);
	bool IsCycloneLockOnCanceled(const sc2::Unit * cyclone, bool started, const sc2::AvailableAbilities & abilities) const;
	bool AllowUnitToPathFind(const sc2::Unit * rangedUnit, bool checkInfluence, std::string pathfindingType) const;
	void PreventUnitToPathFind(const sc2::Unit * rangedUnit, std::string pathfindingType, bool hasTarget);
	bool ShouldBansheeCloak(const sc2::Unit * banshee, bool inDanger) const;
	bool ExecuteBansheeCloakLogic(const sc2::Unit * banshee, bool inDanger);
	bool ShouldBansheeUncloak(const sc2::Unit * banshee, CCPosition goal, sc2::Units & threats, bool unitShouldHeal) const;
	bool ExecuteBansheeUncloakLogic(const sc2::Unit * banshee, CCPosition goal, sc2::Units & threats, bool unitShouldHeal);
	bool TeleportBattlecruiser(const sc2::Unit * battlecruiser, CCPosition location);
	CCPosition GetBestSupportPosition(const sc2::Unit* supportUnit, const sc2::Units & rangedUnits) const;
	bool ExecuteVikingMorphLogic(const sc2::Unit * viking, CCPosition goal, const sc2::Unit* target, sc2::Units & threats, sc2::Units & targets, bool unitShouldHeal, bool isCycloneHelper);
	bool ExecuteTankMorphLogic(const sc2::Unit * tank, CCPosition goal, const sc2::Unit* target, sc2::Units & threats, sc2::Units & targets, sc2::Units & rangedUnits);
	bool IsTankVulnerable(const sc2::Unit * tank, sc2::Units & targets, sc2::Units & rangedUnits);
	bool ShouldTankUnsiege(const sc2::Unit * tank, sc2::Units & targets, sc2::Units & threats, float minRange, bool wouldHaveTarget);
	bool AreEnemiesCloserToOurMineralLine(const sc2::Unit * tank, sc2::Units & targets);
	bool ExecuteThorMorphLogic(const sc2::Unit * thor);
	bool MoveToGoal(const sc2::Unit * rangedUnit, sc2::Units & threats, const sc2::Unit * target, CCPosition goal, std::string goalDescription, bool unitShouldHeal, bool force);
	bool ShouldAttackTarget(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats) const;
	bool IsInRangeOfSlowerUnit(const sc2::Unit * rangedUnit, const sc2::Unit * target) const;
	CCPosition GetDirectionVectorTowardsGoal(const sc2::Unit * rangedUnit, const sc2::Unit * target, CCPosition goal, bool targetInAttackRange, bool unitShouldBack) const;
	const sc2::Unit * ExecuteLockOnLogic(const sc2::Unit * cyclone, bool shouldHeal, bool & shouldAttack, bool & shouldUseLockOn, bool & lockOnAvailable, const sc2::Units & rangedUnits, const sc2::Units & threats, const sc2::Units & rangedUnitTargets, const sc2::Unit * target, sc2::AvailableAbilities & abilities);
	void LockOnTarget(const sc2::Unit * cyclone, const sc2::Unit * target);
	bool CycloneHasTarget(const sc2::Unit * cyclone) const;
	bool ExecuteThreatFightingLogic(const sc2::Unit * rangedUnit, bool unitShouldHeal, sc2::Units & rangedUnits, sc2::Units & threats, sc2::Units & rangedUnitTargets, sc2::Units & otherSquadsUnits);
	const sc2::Unit* GetSimulatedUnit(const sc2::Unit * rangedUnit);
	std::map<sc2::Tag, sc2::Unit> & GetDummyMap(sc2::UNIT_TYPEID type);
	void CalcCloseUnits(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & allyCombatUnits, sc2::Units & rangedUnitTargets, bool ignoreCyclones, std::set<const sc2::Unit *> & closeUnitsSet, bool & morphFlyingVikings, bool & morphLandedVikings, std::map<const sc2::Unit *, const sc2::Unit *> & simulatedStimedUnits, float & stimedUnitsPowerDifference, std::map<const sc2::Unit*, const sc2::Unit*> & closeUnitsTarget, float & unitsPower, float & groundUnitsPower, float & airUnitsPower, float & minUnitRange);
	bool ChangeBehaviorFromBuffs(const sc2::Unit * rangedUnit, bool isUnitDisabled, sc2::Units & allCombatAllies, CCPosition & goal, std::string & goalDescription, bool & unitShouldHeal);
	void ExecuteCycloneLogic(const sc2::Unit * cyclone, bool isUnitDisabled, bool & unitShouldHeal, bool & shouldAttack, bool & cycloneShouldUseLockOn, bool & cycloneShouldStayCloseToTarget, const sc2::Units & rangedUnits, const sc2::Units & threats, const sc2::Units & rangedUnitTargets, const sc2::Unit * & target, CCPosition & goal, std::string & goalDescription, sc2::AvailableAbilities & abilities);
	bool ExecutePrioritizedUnitAbilitiesLogic(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats, sc2::Units & targets, sc2::Units & allyUnits, CCPosition goal, bool unitShouldHeal, bool isCycloneHelper);
	bool ExecuteUnitAbilitiesLogic(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats, sc2::Units & targets, sc2::Units & allyUnits, CCPosition goal, bool unitShouldHeal, bool isCycloneHelper, sc2::AvailableAbilities & abilities);
	bool ExecuteOffensiveTeleportLogic(const sc2::Unit * battlecruiser, const sc2::Units & threats, CCPosition goal);
	bool ExecuteYamatoCannonLogic(const sc2::Unit * battlecruiser, const sc2::Units & targets);
	bool ExecuteHealLogic(const sc2::Unit * medivac, const sc2::Units & allyUnits, bool shouldHeal, bool prioritize = false);
	const sc2::Unit * GetHealTarget(const sc2::Unit * medivac, const sc2::Units & allyUnits, bool filterFullHealthUnits) const;
	bool ExecuteHealCommand(const sc2::Unit * medivac, const sc2::Unit * target, bool prioritize = false);
	bool ExecuteStimLogic(const sc2::Unit * unit);
	bool CanUseStim(const sc2::Unit * unit);
	bool QueryIsAbilityAvailable(const sc2::Unit* unit, sc2::ABILITY_ID abilityId) const;
	bool CanUseKD8Charge(const sc2::Unit * reaper);
	bool ExecuteKD8ChargeLogic(const sc2::Unit * reaper, const sc2::Units & threats);
	bool ShouldBuildAutoTurret(const sc2::Unit * raven, const sc2::Units & threats) const;
	bool ExecuteAutoTurretLogic(const sc2::Unit * raven, const sc2::Units & threats);
	CCPosition GetFleeVectorFromThreat(const sc2::Unit * rangedUnit, const sc2::Unit * threat, CCPosition fleeVec, float distance, float threatRange) const;
	void AdjustSummedFleeVec(CCPosition & summedFleeVec) const;
	CCPosition GetRepulsionVectorFromFriendlyReapers(const sc2::Unit * reaper, sc2::Units & rangedUnits) const;
	CCPosition GetAttractionVectorToFriendlyUnits(const sc2::Unit * rangedUnit, sc2::Units & rangedUnits) const;
	bool MoveUnitWithDirectionVector(const sc2::Unit * rangedUnit, CCPosition & directionVector, CCPosition & outPathableTile) const;
	CCPosition AttenuateZigzag(const sc2::Unit* rangedUnit, std::vector<const sc2::Unit*>& threats, CCPosition safeTile, CCPosition summedFleeVec) const;
	sc2::Units & getThreats(const sc2::Unit * rangedUnit, const sc2::Units & targets);
	const sc2::Unit * getTargetOnHighGround(const sc2::Unit * rangedUnit, const sc2::Units & targets, const sc2::Units & threats);
	void cleanLastStimFrame();
};
