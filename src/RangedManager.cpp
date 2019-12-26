#include "RangedManager.h"
#include "Util.h"
#include "CCBot.h"
#include "BehaviorTreeBuilder.h"
#include <algorithm>
#include <string>
#include <thread>
#include <list>

const float HARASS_FRIENDLY_SUPPORT_MIN_DISTANCE = 7.f;
const float HARASS_FRIENDLY_ATTRACTION_MIN_DISTANCE = 10.f;
const float HARASS_FRIENDLY_ATTRACTION_INTENSITY = 1.5f;
const float HARASS_FRIENDLY_REPULSION_MIN_DISTANCE = 5.f;
const float HARASS_FRIENDLY_REPULSION_INTENSITY = 1.f;
const float HARASS_MIN_RANGE_DIFFERENCE_FOR_TARGET = 2.f;
const float HARASS_THREAT_MIN_DISTANCE_TO_TARGET = 2.f;
const float HARASS_THREAT_MAX_REPULSION_INTENSITY = 1.5f;
const float HARASS_THREAT_RANGE_BUFFER = 1.f;
const float HARASS_THREAT_SPEED_MULTIPLIER_FOR_KD8CHARGE = 2.25f;
const int HARASS_PATHFINDING_COOLDOWN_AFTER_FAIL = 50;
const int BATTLECRUISER_TELEPORT_FRAME_COUNT = 90;
const int BATTLECRUISER_TELEPORT_COOLDOWN_FRAME_COUNT = 1591 + BATTLECRUISER_TELEPORT_FRAME_COUNT;
const int BATTLECRUISER_YAMATO_CANNON_FRAME_COUNT = 68;
const int BATTLECRUISER_YAMATO_CANNON_COOLDOWN_FRAME_COUNT = 1591;
const int CYCLONE_ATTACK_FRAME_COUNT = 1;
const int CYCLONE_LOCKON_CAST_FRAME_COUNT = 9;
const int CYCLONE_LOCKON_CHANNELING_FRAME_COUNT = 321 + CYCLONE_LOCKON_CAST_FRAME_COUNT;
const int CYCLONE_LOCKON_COOLDOWN_FRAME_COUNT = 97;
const float CYCLONE_PREFERRED_MAX_DISTANCE_TO_HELPER = 4.f;
const int HELLION_ATTACK_FRAME_COUNT = 9;
const int REAPER_KD8_CHARGE_FRAME_COUNT = 3;
const int REAPER_KD8_CHARGE_COOLDOWN = 314 + REAPER_KD8_CHARGE_FRAME_COUNT + 7;
const int REAPER_MOVE_FRAME_COUNT = 3;
const int VIKING_MORPH_FRAME_COUNT = 40;
const int THOR_MORPH_FRAME_COUNT = 40;
const int ACTION_REEXECUTION_FREQUENCY = 50;

RangedManager::RangedManager(CCBot & bot) : MicroManager(bot)
{
}

void RangedManager::setTargets(const std::vector<Unit> & targets)
{
    std::vector<Unit> rangedUnitTargets;
	bool filterPassiveBuildings = false;
	if(m_harassMode)
	{
		// In harass mode, we don't want to attack buildings (like a wall or proxy) if we never reached the enemy base
		const BaseLocation* enemyStartingBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
		if (enemyStartingBase && !m_bot.Map().isExplored(enemyStartingBase->getPosition()))
		{
			filterPassiveBuildings = true;
		}
		else
		{
			//Also, we do not want to target buildings unless there are no better targets
			for (auto & target : targets)
			{
				// Check if the target is a unit (not building) or a combat building. In this case, we won't consider passive buildings
				if (!target.getType().isBuilding() || target.getType().isCombatUnit())
				{
					filterPassiveBuildings = true;
					break;
				}
			}
		}
	}
    for (auto & target : targets)
    {
        const auto targetPtr = target.getUnitPtr();
        if (!targetPtr) { continue; }
        if (targetPtr->unit_type == sc2::UNIT_TYPEID::ZERG_EGG) { continue; }
        if (targetPtr->unit_type == sc2::UNIT_TYPEID::ZERG_LARVA) { continue; }
		if (filterPassiveBuildings && target.getType().isBuilding() && !target.getType().isCombatUnit() && target.getUnitPtr()->unit_type != sc2::UNIT_TYPEID::ZERG_CREEPTUMOR) { continue; }

        rangedUnitTargets.push_back(target);
    }
    m_targets = rangedUnitTargets;
}

void RangedManager::executeMicro()
{
    const std::vector<Unit> &units = getUnits();
	if (units.empty())
		return;

    std::vector<const sc2::Unit *> rangedUnits;
    for (auto & unit : units)
    {
        const sc2::Unit * rangedUnit = unit.getUnitPtr();
        rangedUnits.push_back(rangedUnit);
    }

    std::vector<const sc2::Unit *> rangedUnitTargets;
    for (auto target : m_targets)
    {
        rangedUnitTargets.push_back(target.getUnitPtr());
    }

    if (m_harassMode)
	{
		HarassLogic(rangedUnits, rangedUnitTargets);
	}
    else 
	{
		BOT_ASSERT(false, "Ranged micro is not harass mode");
    }
}

bool RangedManager::isAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::Unit * rangedUnit) const
{
	auto & nextAvailableAbility = m_bot.Commander().Combat().getNextAvailableAbility();
	const auto abilityIt = nextAvailableAbility.find(abilityId);
	if (abilityIt == nextAvailableAbility.end())
		return true;

	const auto unitIt = abilityIt->second.find(rangedUnit);
	if (unitIt == abilityIt->second.end())
		return true;

	if (m_bot.GetCurrentFrame() < unitIt->second)
		return false;

	// Query once when we think the ability is available
	if(QueryIsAbilityAvailable(rangedUnit, abilityId))
	{
		// If it is, remove the unit from the map to prevent further queries
		abilityIt->second.erase(unitIt);
		return true;
	}

	// Otherwise, wait a bit
	unitIt->second += 22;
	return false;
}

void RangedManager::setNextFrameAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::Unit * rangedUnit, uint32_t nextAvailableFrame)
{
	m_bot.Commander().Combat().getNextAvailableAbility()[abilityId][rangedUnit] = nextAvailableFrame;
}

int RangedManager::getAttackDuration(const sc2::Unit* unit, const sc2::Unit* target) const
{
	if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER)
		return 0;

	int attackFrameCount = 2;
	if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_HELLION)
		attackFrameCount = HELLION_ATTACK_FRAME_COUNT;
	else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_CYCLONE)
		attackFrameCount = CYCLONE_ATTACK_FRAME_COUNT;
	const CCPosition targetDirection = Util::Normalized(target->pos - unit->pos);
	const CCPosition facingVector = Util::getFacingVector(unit);
	const float dot = sc2::Dot2D(targetDirection, facingVector);
	const float value = 1 - dot;
	const int rotationMultiplier = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_CYCLONE || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER ? 4 : 2;
	attackFrameCount += value * rotationMultiplier;
	return attackFrameCount;
}

void RangedManager::HarassLogic(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets)
{
	m_bot.StartProfiling("0.10.4.1.5.0        CleanActions");
	CleanActions(rangedUnits);
	m_bot.StopProfiling("0.10.4.1.5.0        CleanActions");

	m_bot.StartProfiling("0.10.4.1.5.3        CalcBestFlyingCycloneHelpers");
	CalcBestFlyingCycloneHelpers();
	m_bot.StopProfiling("0.10.4.1.5.3        CalcBestFlyingCycloneHelpers");

	m_bot.StartProfiling("0.10.4.1.5.4        GetAbilitiesForUnits");
	auto rangedUnitsAbilities = m_bot.Query()->GetAbilitiesForUnits(rangedUnits);
	m_bot.StopProfiling("0.10.4.1.5.4        GetAbilitiesForUnits");

	m_bot.StartProfiling("0.10.4.1.5.1        HarassLogicForUnit");
	if (m_bot.Config().EnableMultiThreading)
	{
		std::list<std::thread*> threads;
		for (int i = 0; i < rangedUnits.size(); ++i)
		{
			auto rangedUnit = rangedUnits[i];
			std::thread* t = new std::thread(&RangedManager::HarassLogicForUnit, this, rangedUnit, std::ref(rangedUnits), std::ref(rangedUnitTargets), std::ref(rangedUnitsAbilities[i]));
			threads.push_back(t);
		}
		for (auto t : threads)
		{
			t->join();
			delete t;
		}
	}
	else
	{
		for (int i=0; i<rangedUnits.size(); ++i)
		{
			auto rangedUnit = rangedUnits[i];
			HarassLogicForUnit(rangedUnit, rangedUnits, rangedUnitTargets, rangedUnitsAbilities[i]);
		}
	}
	m_bot.StopProfiling("0.10.4.1.5.1        HarassLogicForUnit");

	m_bot.StartProfiling("0.10.4.1.5.2        ExecuteActions");
	ExecuteActions();
	m_bot.StopProfiling("0.10.4.1.5.2        ExecuteActions");
}

void RangedManager::HarassLogicForUnit(const sc2::Unit* rangedUnit, sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets, sc2::AvailableAbilities &rangedUnitAbilities)
{
	if (!rangedUnit)
		return;

	const bool isMarine = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE;
	const bool isReaper = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER;
	const bool isHellion = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_HELLION;
	const bool isBanshee = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE;
	const bool isRaven = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_RAVEN;
	const bool isViking = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT;
	const bool isCyclone = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_CYCLONE;
	const bool isBattlecruiser = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER;
	const bool isFlyingBarracks = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING;

	// Sometimes want to give an action only every few frames to allow slow attacks to occur and cliff jumps
	if (ShouldSkipFrame(rangedUnit))
		return;

	if (rangedUnit->is_selected)
		m_bot.Strategy();

	if (isCyclone && MonitorCyclone(rangedUnit, rangedUnitAbilities))
		return;

	m_bot.StartProfiling("0.10.4.1.5.1.0          getTarget");
	const sc2::Unit * target = getTarget(rangedUnit, rangedUnitTargets);
	m_bot.StopProfiling("0.10.4.1.5.1.0          getTarget");
	m_bot.StartProfiling("0.10.4.1.5.1.1          getThreats");
	sc2::Units threats = Util::getThreats(rangedUnit, rangedUnitTargets, m_bot);
	m_bot.StopProfiling("0.10.4.1.5.1.1          getThreats");

	CCPosition goal = m_order.getPosition();

	// Check if unit is chosen to be a Cyclone helper
	const auto cycloneFlyingHelperIt = m_cycloneFlyingHelpers.find(rangedUnit);
	const bool isCycloneHelper = cycloneFlyingHelperIt != m_cycloneFlyingHelpers.end();
	if (isCycloneHelper)
	{
		const auto helperGoalPosition = cycloneFlyingHelperIt->second.position;
		if (isFlyingBarracks || Util::DistSq(rangedUnit->pos, helperGoalPosition) < 20 * 20)
			goal = helperGoalPosition;
		if (m_bot.Config().DrawHarassInfo)
		{
			m_bot.Map().drawCircle(rangedUnit->pos, 0.75f, sc2::Colors::Purple);
			m_bot.Map().drawLine(rangedUnit->pos, goal, sc2::Colors::Purple);
			m_bot.Map().drawCircle(goal, 0.5f, sc2::Colors::Purple);
		}
	}

	m_bot.StartProfiling("0.10.4.1.5.1.2          ShouldUnitHeal");
	bool unitShouldHeal = ShouldUnitHeal(rangedUnit);
	if (isCyclone && unitShouldHeal)
	{
		for (const auto & ability : rangedUnitAbilities.abilities)
		{
			if (ability.ability_id == sc2::ABILITY_ID::CANCEL)	// Currently using Lock-On
			{
				unitShouldHeal = false;
				break;
			}
		}
	}
	if (unitShouldHeal)
	{
		CCPosition healGoal = isReaper ? m_bot.Map().center() : m_bot.RepairStations().getBestRepairStationForUnit(rangedUnit);
		if (healGoal != CCPosition())
			goal = healGoal;
		else
			Util::DisplayError("RangedManager healGoal is (0, 0)", "", m_bot, false);
		if (isBattlecruiser && Util::DistSq(rangedUnit->pos, goal) > 15.f * 15.f && TeleportBattlecruiser(rangedUnit, goal))
			return;
	}
	else
	{
		/*if ((m_order.getType() == SquadOrderTypes::Attack || m_order.getType() == SquadOrderTypes::Harass) && !m_bot.Commander().Combat().hasBiggerArmy() && !m_bot.Commander().Combat().winAttackSimulation())
		{
			goal = m_bot.GetStartLocation();
		}
		else*/ if (!isCycloneHelper && (isMarine || isRaven || isViking || isHellion))
		{
			goal = GetBestSupportPosition(rangedUnit, rangedUnits);
		}
		else if (isFlyingBarracks && (m_flyingBarracksShouldReachEnemyRamp || !isCycloneHelper))
		{
			goal = m_bot.Buildings().getEnemyMainRamp();
			if (Util::DistSq(rangedUnit->pos, goal) < 5 * 5 || !threats.empty() || (isCycloneHelper && cycloneFlyingHelperIt->second.goal == TRACK))
				m_flyingBarracksShouldReachEnemyRamp = false;
		}
		else if (isViking && !isCycloneHelper && !m_bot.Commander().Combat().hasEnoughVikingsAgainstTempests())
		{
			goal = m_bot.GetStartLocation();
		}
	}
	m_bot.StopProfiling("0.10.4.1.5.1.2          ShouldUnitHeal");

	// If our unit is targeted by an enemy Cyclone's Lock-On ability, it should back until the effect wears off
	if (Util::isUnitLockedOn(rangedUnit))
	{
		// Banshee in danger should cloak itself
		if (isBanshee && ExecuteBansheeCloakLogic(rangedUnit, true))
		{
			return;
		}

		goal = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getPosition();
		unitShouldHeal = true;
	}
	else if (Util::isUnitDisabled(rangedUnit))
	{
		goal = m_bot.Map().center();
		unitShouldHeal = true;
	}

	bool shouldAttack = true;
	bool cycloneShouldUseLockOn = false;
	bool cycloneShouldStayCloseToTarget = false;
	if (isCyclone)
	{
		ExecuteCycloneLogic(rangedUnit, unitShouldHeal, shouldAttack, cycloneShouldUseLockOn, cycloneShouldStayCloseToTarget, rangedUnits, threats, target, goal, rangedUnitAbilities);
	}

	const auto distSqToTarget = target ? Util::DistSq(rangedUnit->pos, target->pos) : 0.f;
	if (isReaper && !unitShouldHeal && !m_bot.Strategy().shouldFocusBuildings() && (!target || distSqToTarget > 10.f * 10.f) && m_order.getType() == SquadOrderTypes::Harass)
	{
		const auto enemyStartingBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
		if (enemyStartingBase)
		{
			goal = enemyStartingBase->getPosition();
		}
	}

	if (ExecutePrioritizedUnitAbilitiesLogic(rangedUnit, target, threats, rangedUnitTargets, goal, unitShouldHeal, isCycloneHelper))
	{
		return;
	}

	bool targetInAttackRange = false;
	float unitAttackRange = 0.f;
	if (target)
	{
		if (cycloneShouldUseLockOn)
			unitAttackRange = m_bot.Commander().Combat().getAbilityCastingRanges()[sc2::ABILITY_ID::EFFECT_LOCKON] + rangedUnit->radius + target->radius;
		else if (cycloneShouldStayCloseToTarget)
			unitAttackRange = 5.f;	// We want to stay close to the unit so we keep our Lock-On for a longer period
		else if (!shouldAttack)
		{
			bool allyUnitSeesTarget = false;
			for (const auto & allyUnit : m_bot.GetAllyUnits())
			{
				const auto allyUnitPtr = allyUnit.second.getUnitPtr();
				if (allyUnitPtr == rangedUnit)
					continue;
				if (Util::CanUnitSeeEnemyUnit(allyUnitPtr, target, m_bot))
				{
					allyUnitSeesTarget = true;
					break;
				}
			}
			unitAttackRange = (allyUnitSeesTarget ? 14.f : 10.f) + rangedUnit->radius + target->radius;
		}
		else
			unitAttackRange = Util::GetAttackRangeForTarget(rangedUnit, target, m_bot, true);
		targetInAttackRange = distSqToTarget <= unitAttackRange * unitAttackRange;

#ifndef PUBLIC_RELEASE
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(rangedUnit->pos, target->pos, targetInAttackRange ? sc2::Colors::Green : sc2::Colors::Yellow);
#endif
	}

	if (cycloneShouldUseLockOn && targetInAttackRange)
	{
		LockOnTarget(rangedUnit, target);
		return;
	}

	m_bot.StartProfiling("0.10.4.1.5.1.5          ThreatFighting");
	// Check if our units are powerful enough to exchange fire with the enemies
	if (shouldAttack && ExecuteThreatFightingLogic(rangedUnit, unitShouldHeal, rangedUnits, threats))
	{
		m_bot.StopProfiling("0.10.4.1.5.1.5          ThreatFighting");
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.1.5          ThreatFighting");

	m_bot.StartProfiling("0.10.4.1.5.1.4          ShouldAttackTarget");
	if (shouldAttack && targetInAttackRange && ShouldAttackTarget(rangedUnit, target, threats))
	{
		const auto action = RangedUnitAction(MicroActionType::AttackUnit, target, unitShouldHeal, getAttackDuration(rangedUnit, target));
		PlanAction(rangedUnit, action);
		m_bot.StopProfiling("0.10.4.1.5.1.4          ShouldAttackTarget");
		const float damageDealth = isBattlecruiser ? Util::GetDpsForTarget(rangedUnit, target, m_bot) / 22.4f : Util::GetDamageForTarget(rangedUnit, target, m_bot);
		m_bot.Analyzer().increaseTotalDamage(damageDealth, rangedUnit->unit_type);
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.1.4          ShouldAttackTarget");

	m_bot.StartProfiling("0.10.4.1.5.1.6          UnitAbilities");
	// Check if unit can use one of its abilities
	if(ExecuteUnitAbilitiesLogic(rangedUnit, threats))
	{
		m_bot.StopProfiling("0.10.4.1.5.1.6          UnitAbilities");
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.1.6          UnitAbilities");

	if (distSqToTarget < m_order.getRadius() * m_order.getRadius() && (target || !threats.empty()))
	{
		m_bot.StartProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
		m_bot.StartProfiling("0.10.4.1.5.1.7          OffensivePathFinding " + rangedUnit->unit_type.to_string());
		if (!unitShouldHeal)
		{
			if (cycloneShouldUseLockOn || cycloneShouldStayCloseToTarget || AllowUnitToPathFind(rangedUnit))
			{
				const CCPosition pathFindEndPos = target && !unitShouldHeal && !isCycloneHelper ? target->pos : goal;
				const bool ignoreInfluence = (cycloneShouldUseLockOn && target) || cycloneShouldStayCloseToTarget;
				const CCPosition secondaryGoal = (!cycloneShouldUseLockOn && !shouldAttack && !ignoreInfluence) ? m_bot.GetStartLocation() : CCPosition();	// Only set for Cyclones with lock-on target (other than Tempest)
				const float maxRange = target ? unitAttackRange : 3.f;
				CCPosition closePositionInPath = Util::PathFinding::FindOptimalPathToTarget(rangedUnit, pathFindEndPos, secondaryGoal, target, maxRange, ignoreInfluence, m_bot);
				if (closePositionInPath != CCPosition())
				{
					const int actionDuration = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? REAPER_MOVE_FRAME_COUNT : 0;
					const auto action = RangedUnitAction(MicroActionType::Move, closePositionInPath, unitShouldHeal, actionDuration);
					PlanAction(rangedUnit, action);
					m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
					m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding " + rangedUnit->unit_type.to_string());
					return;
				}
				else
				{
					nextPathFindingFrameForUnit[rangedUnit] = m_bot.GetGameLoop() + HARASS_PATHFINDING_COOLDOWN_AFTER_FAIL;
				}
			}
			else if (isCyclone && !shouldAttack && !cycloneShouldUseLockOn && target)
			{
				CCPosition movePosition = Util::PathFinding::FindOptimalPathToSaferRange(rangedUnit, target, unitAttackRange, m_bot);
				if (movePosition != CCPosition())
				{
					const auto action = RangedUnitAction(MicroActionType::Move, movePosition, unitShouldHeal, 0);
					PlanAction(rangedUnit, action);
					m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
					m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding " + rangedUnit->unit_type.to_string());
					return;
				}
			}
		}
		m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
		m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding " + rangedUnit->unit_type.to_string());
	}

	// if there is no potential target or threat, move to objective
	const bool forceMoveToGoal = isCycloneHelper && isFlyingBarracks && cycloneFlyingHelperIt->second.goal == TRACK;
	if (MoveToGoal(rangedUnit, threats, target, goal, unitShouldHeal, forceMoveToGoal))
	{
		return;
	}

	bool useInfluenceMap = false;
	CCPosition summedFleeVec(0, 0);
	// add normalied * 1.5 vector of potential threats
	for (auto threat : threats)
	{
		const CCPosition fleeVec = Util::Normalized(rangedUnit->pos - threat->pos);

		// If our unit is almost in range of threat, use the influence map to find the best flee path
		const float dist = Util::Dist(rangedUnit->pos, threat->pos);
		const float threatRange = Util::getThreatRange(rangedUnit, threat, m_bot);
		if (dist < threatRange + 0.5f)
		{
			useInfluenceMap = true;
		}
		summedFleeVec += GetFleeVectorFromThreat(rangedUnit, threat, fleeVec, dist, threatRange);
	}

	if (useInfluenceMap)
	{
		// Banshee in danger should cloak itself if low on hp
		if (isBanshee && unitShouldHeal && ExecuteBansheeCloakLogic(rangedUnit, unitShouldHeal))
		{
			return;
		}
		
		m_bot.StartProfiling("0.10.4.1.5.1.9          DefensivePathfinding");
		// If close to an unpathable position or in danger
		// Use influence map to find safest path
		// If should heal, try to go closer to goal
		CCPosition safeTile = Util::PathFinding::FindOptimalPathToSafety(rangedUnit, goal, unitShouldHeal, m_bot);
		if (safeTile != CCPosition())
		{
			const auto action = RangedUnitAction(MicroActionType::Move, safeTile, unitShouldHeal, isReaper ? REAPER_MOVE_FRAME_COUNT : 0);
			PlanAction(rangedUnit, action);
			m_bot.StopProfiling("0.10.4.1.5.1.9          DefensivePathfinding");
			return;
		}
	}

	m_bot.StartProfiling("0.10.4.1.5.1.8          PotentialFields");
	CCPosition dirVec = GetDirectionVectorTowardsGoal(rangedUnit, target, goal, targetInAttackRange, unitShouldHeal);

	// Sum up the threats vector with the direction vector
	if (!threats.empty())
	{
		AdjustSummedFleeVec(summedFleeVec);
		dirVec += summedFleeVec;
	}

	// We repulse the Reaper from our closest harass unit
	if (isReaper)
	{
		dirVec += GetRepulsionVectorFromFriendlyReapers(rangedUnit, rangedUnits);
	}
	// We attract the Hellion towards our other close Hellions
	else if (isHellion)
	{
		dirVec += GetAttractionVectorToFriendlyHellions(rangedUnit, rangedUnits);
	}

	// We move only if the vector is long enough
	const float vecLen = std::sqrt(std::pow(dirVec.x, 2) + std::pow(dirVec.y, 2));
	if (vecLen < 0.5f)
	{
		m_bot.StopProfiling("0.10.4.1.5.1.8          PotentialFields");
		return;
	}

	// Normalize the final direction vector so it's easier to work with
	Util::Normalize(dirVec);

	// If we find a pathable tile in the direction of the vector, we move at that tile
	CCPosition pathableTile(0, 0);
	if(MoveUnitWithDirectionVector(rangedUnit, dirVec, pathableTile))
	{
#ifndef PUBLIC_RELEASE
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(rangedUnit->pos, rangedUnit->pos+dirVec, sc2::Colors::Purple);
#endif

		const auto action = RangedUnitAction(Move, pathableTile, unitShouldHeal, isReaper ? REAPER_MOVE_FRAME_COUNT : 0);
		PlanAction(rangedUnit, action);
		m_bot.StopProfiling("0.10.4.1.5.1.8          PotentialFields");
		return;
	}
	
	// Unit cannot flee, we let it attack any close unit
	std::stringstream ss;
	ss << "Unit " << sc2::UnitTypeToName(rangedUnit->unit_type) << " cannot flee";
	Util::Log(__FUNCTION__, ss.str(), m_bot);
	const auto actionType = m_bot.Data(rangedUnit->unit_type).isBuilding ? Move : AttackMove;
	const auto action = RangedUnitAction(AttackMove, rangedUnit->pos, false, 0);
	PlanAction(rangedUnit, action);
	m_bot.StopProfiling("0.10.4.1.5.1.8          PotentialFields");
}

bool RangedManager::ShouldSkipFrame(const sc2::Unit * rangedUnit) const
{
	const uint32_t availableFrame = nextCommandFrameForUnit.find(rangedUnit) != nextCommandFrameForUnit.end() ? nextCommandFrameForUnit.at(rangedUnit) : 0;
	return m_bot.GetGameLoop() < availableFrame;
}

bool RangedManager::MonitorCyclone(const sc2::Unit * cyclone, sc2::AvailableAbilities & abilities)
{
	// Toggle off the auto-cast of Lock-On on the second frame of existence of the Cyclone (just to make sure there is no bug with the toggle)
	auto & newCyclones = m_bot.Commander().Combat().getNewCyclones();
	auto & toggledCyclones = m_bot.Commander().Combat().getToggledCyclones();
	if(!Util::Contains(cyclone->tag, newCyclones))
	{
		newCyclones.insert(cyclone->tag);
	}
	else if (!Util::Contains(cyclone->tag, toggledCyclones))
	{
		const auto action = RangedUnitAction(MicroActionType::ToggleAbility, sc2::ABILITY_ID::EFFECT_LOCKON, true, 0);
		PlanAction(cyclone, action);
		toggledCyclones.insert(cyclone->tag);
		return true;
	}

	// Check if Lock-On casting is canceled or over
	auto & lockOnCastedFrame = m_bot.Commander().Combat().getLockOnCastedFrame();
	auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
	const auto it = lockOnCastedFrame.find(cyclone);
	if (it != lockOnCastedFrame.end())
	{
		const uint32_t currentFrame = m_bot.GetCurrentFrame();
		// If the Cyclone is still casting the Lock On ability
		if (it->second.second + CYCLONE_LOCKON_CAST_FRAME_COUNT > currentFrame)
		{
			if (IsCycloneLockOnCanceled(cyclone, false, abilities))
			{
				// Query the game to make sure the Lock-On has really been canceled while casting
				if(QueryIsAbilityAvailable(cyclone, sc2::ABILITY_ID::EFFECT_LOCKON))
				{
					lockOnCastedFrame.erase(cyclone);
					lockOnTargets.erase(cyclone);
				}
				else
				{
					// The unit died right after the Lock-On was cast
					lockOnCastedFrame.erase(cyclone);
					lockOnTargets.erase(cyclone);
					setNextFrameAbilityAvailable(sc2::ABILITY_ID::EFFECT_LOCKON, cyclone, currentFrame + CYCLONE_LOCKON_COOLDOWN_FRAME_COUNT);
				}
			}
			else
			{
				if (m_bot.Config().DrawHarassInfo)
					m_bot.Map().drawLine(cyclone->pos, it->second.first->pos, sc2::Colors::Red);
				return true;
			}
		}
		else
		{
			lockOnCastedFrame.erase(cyclone);
		}
	}

	// Let the Analyzer know of the damage the Cyclone is doing with its Lock-On ability
	const auto lockOnTarget = lockOnTargets.find(cyclone);
	if(lockOnTarget != lockOnTargets.end())
	{
		auto damagePerFrame = 400.f / 14.3f / 22.4f;
		if(m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::CYCLONELOCKONDAMAGEUPGRADE))
		{
			const sc2::UnitTypeData unitTypeData = Util::GetUnitTypeDataFromUnitTypeId(lockOnTarget->first->unit_type, m_bot);
			if (Util::Contains(sc2::Attribute::Armored, unitTypeData.attributes))
				damagePerFrame *= 2;
		}
		m_bot.Analyzer().increaseTotalDamage(damagePerFrame, cyclone->unit_type);
	}

	return false;
}

bool RangedManager::IsCycloneLockOnCanceled(const sc2::Unit * cyclone, bool started, sc2::AvailableAbilities & abilities) const
{
	auto & lockOnCastedFrame = m_bot.Commander().Combat().getLockOnCastedFrame();
	auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
	const uint32_t currentFrame = m_bot.GetCurrentFrame();
	const auto it = started ? lockOnTargets.find(cyclone) : lockOnCastedFrame.find(cyclone);
	const auto & pair = it->second;
	const sc2::Unit * lockOnTarget = pair.first;
	const uint32_t frameCast = pair.second;

	// The effect is over
	if (started && currentFrame >= frameCast + CYCLONE_LOCKON_CHANNELING_FRAME_COUNT)
		return true;
	
	// The target is dead or not visible
	if (lockOnTarget == nullptr || !lockOnTarget->is_alive || lockOnTarget->last_seen_game_loop != currentFrame)
		return true;
	
	// The target is too far
	if (Util::Dist(cyclone->pos, lockOnTarget->pos) > 15.f + cyclone->radius + lockOnTarget->radius)
		return true;

	// The target is being lifted by a Pheonix
	if (Util::isUnitLifted(cyclone))
		return true;

	// Sometimes, even though the target is perfectly valid, the Lock-On command won't work.
	// Since the Lock-On ability is supposed to become unavailable the next frame it is cast, we know it didn't work if it is still available
	if (currentFrame == frameCast + 1)
	{
		const bool lockOnAvailable = QueryIsAbilityAvailable(cyclone, sc2::ABILITY_ID::EFFECT_LOCKON);
		if (lockOnAvailable)
			return true;
	}

	if (currentFrame >= frameCast + CYCLONE_LOCKON_CAST_FRAME_COUNT)
	{
		// Sometimes, even though the target is still in range and visible, the Cyclone will have no engaged target and the target will not have the buff.
		// In that case, we need to check if the ability can still be canceled. If so, the Lock-On isn't finished.
		if (cyclone->engaged_target_tag == sc2::NullTag && !Util::isUnitLockedOn(lockOnTarget))
		{
			bool canceled = true;
			for (auto & ability : abilities.abilities)
			{
				if (ability.ability_id == sc2::ABILITY_ID::CANCEL)
				{
					canceled = false;
					break;
				}
			}
			if (canceled)
				return true;
		}
	}
	
	return false;
}

bool RangedManager::AllowUnitToPathFind(const sc2::Unit * rangedUnit, bool checkInfluence) const
{
	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_HELLION)
		return false;
	if (checkInfluence && Util::PathFinding::HasInfluenceOnTile(Util::GetTilePosition(rangedUnit->pos), rangedUnit->is_flying, m_bot))
		return false;
	const uint32_t availableFrame = nextPathFindingFrameForUnit.find(rangedUnit) != nextPathFindingFrameForUnit.end() ? nextPathFindingFrameForUnit.at(rangedUnit) : m_bot.GetGameLoop();
	return m_bot.GetGameLoop() >= availableFrame;
}

bool RangedManager::ShouldBansheeCloak(const sc2::Unit * banshee, bool inDanger) const
{
	if (!m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::BANSHEECLOAK))
		return false;

	// Cloak if the amount of energy is rather high or HP is low
	return banshee->cloak == sc2::Unit::NotCloaked && (banshee->energy > 50.f || inDanger && banshee->energy > 25.f) && !Util::IsPositionUnderDetection(banshee->pos, m_bot);
}

bool RangedManager::ExecuteBansheeCloakLogic(const sc2::Unit * banshee, bool inDanger)
{
	if (ShouldBansheeCloak(banshee, inDanger))
	{
		const auto action = RangedUnitAction(MicroActionType::Ability, sc2::ABILITY_ID::BEHAVIOR_CLOAKON, true, 0);
		PlanAction(banshee, action);
		return true;
	}
	return false;
}

bool RangedManager::ShouldBansheeUncloak(const sc2::Unit * banshee, CCPosition goal, sc2::Units & threats, bool unitShouldHeal) const
{
	if (banshee->cloak != sc2::Unit::CloakedAllied)
		return false;

	if (!unitShouldHeal)
		return false;

	if (!threats.empty())
		return false;

	if (!Util::PathFinding::IsPathToGoalSafe(banshee, goal, false, m_bot))
		return false;
	
	return true;
}

bool RangedManager::ExecuteBansheeUncloakLogic(const sc2::Unit * banshee, CCPosition goal, sc2::Units & threats, bool unitShouldHeal)
{
	if (ShouldBansheeUncloak(banshee, goal, threats, unitShouldHeal))
	{
		const auto action = RangedUnitAction(MicroActionType::Ability, sc2::ABILITY_ID::BEHAVIOR_CLOAKOFF, true, 0);
		PlanAction(banshee, action);
		return true;
	}
	return false;
}

bool RangedManager::ShouldUnitHeal(const sc2::Unit * rangedUnit) const
{
	auto & unitsBeingRepaired = m_bot.Commander().Combat().getUnitsBeingRepaired();
	const UnitType unitType(rangedUnit->unit_type, m_bot);
	if (unitType.isRepairable() && !unitType.isBuilding())
	{
		const auto it = unitsBeingRepaired.find(rangedUnit);
		//If unit is being repaired
		if (it != unitsBeingRepaired.end())
		{
			//and is not fully repaired
			if (rangedUnit->health != rangedUnit->health_max)
			{
				return true;
			}
			else
			{
				unitsBeingRepaired.erase(rangedUnit);
			}
		}
		//if unit is damaged enough to go back for repair
		else
		{
			float percentageMultiplier = 1.f;
			switch(rangedUnit->unit_type.ToType())
			{
				case sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER:
					/*if (isAbilityAvailable(sc2::ABILITY_ID::EFFECT_TACTICALJUMP, rangedUnit))
						percentageMultiplier = 0.5f;*/	//BCs can fight longer since they can teleport back to safety
					break;
				case sc2::UNIT_TYPEID::TERRAN_CYCLONE:
					percentageMultiplier = 1.5f;
					break;
				default:
					break;
			}
			if (rangedUnit->health / rangedUnit->health_max < Util::HARASS_REPAIR_STATION_MAX_HEALTH_PERCENTAGE * percentageMultiplier)
			{
				unitsBeingRepaired.insert(rangedUnit);
				return true;
			}
		}
	}

	return rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER && rangedUnit->health / rangedUnit->health_max < 0.66f;
}

bool RangedManager::TeleportBattlecruiser(const sc2::Unit * battlecruiser, CCPosition location)
{
	// If the teleport ability is not on cooldown
	if (isAbilityAvailable(sc2::ABILITY_ID::EFFECT_TACTICALJUMP, battlecruiser))
	{
		const auto action = RangedUnitAction(MicroActionType::AbilityPosition, sc2::ABILITY_ID::EFFECT_TACTICALJUMP, location, true, BATTLECRUISER_TELEPORT_FRAME_COUNT);
		PlanAction(battlecruiser, action);
		setNextFrameAbilityAvailable(sc2::ABILITY_ID::EFFECT_TACTICALJUMP, battlecruiser, m_bot.GetCurrentFrame() + BATTLECRUISER_TELEPORT_COOLDOWN_FRAME_COUNT);
		m_bot.StopProfiling("0.10.4.1.5.1.2          ShouldUnitHeal");
		return true;
	}

	return false;
}

CCPosition RangedManager::GetBestSupportPosition(const sc2::Unit* supportUnit, const sc2::Units & rangedUnits) const
{
	const bool isMarine = supportUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE;
	const bool isRaven = supportUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_RAVEN;
	const std::vector<sc2::UNIT_TYPEID> typesToIgnore = supportTypes;
	const std::vector<sc2::UNIT_TYPEID> typesToConsider = { sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER };
	const auto clusterQueryName = "";// isRaven ? "Raven" : "";
	sc2::Units validUnits;
	for (const auto rangedUnit : rangedUnits)
	{
		if (!ShouldUnitHeal(rangedUnit))
			validUnits.push_back(rangedUnit);
	}
	const auto clusters = Util::GetUnitClusters(validUnits, isMarine ? typesToConsider : typesToIgnore, !isMarine, clusterQueryName, m_bot);
	const Util::UnitCluster* closestBiggestCluster = nullptr;
	float distance = 0.f;
	for(const auto & cluster : clusters)
	{
		if(!closestBiggestCluster || cluster.m_units.size() > closestBiggestCluster->m_units.size())
		{
			closestBiggestCluster = &cluster;
			distance = Util::Dist(supportUnit->pos, cluster.m_center) + Util::Dist(cluster.m_center, m_order.getPosition());
			continue;
		}
		if (cluster.m_units.size() < closestBiggestCluster->m_units.size())
		{
			//break;	//could break if the clusters were sorted
			continue;
		}
		const float dist = Util::Dist(supportUnit->pos, cluster.m_center) + Util::Dist(cluster.m_center, m_order.getPosition());
		if(dist < distance)
		{
			closestBiggestCluster = &cluster;
			distance = dist;
		}
	}
	if(closestBiggestCluster)
	{
		return closestBiggestCluster->m_center;
	}
	if (m_order.getType() == SquadOrderTypes::Defend || m_bot.GetCurrentSupply() == 200)
	{
		return m_order.getPosition();
	}
	return m_bot.GetStartLocation();
}

bool RangedManager::ExecuteVikingMorphLogic(const sc2::Unit * viking, CCPosition goal, const sc2::Unit* target, sc2::Units & threats, sc2::Units & targets, bool unitShouldHeal, bool isCycloneHelper)
{
	bool morph = false;
	sc2::AbilityID morphAbility = 0;
	//const auto airInfluence = Util::PathFinding::GetTotalInfluenceOnTiles(viking->pos, true, 2.f, m_bot);
	//const auto groundInfluence = Util::PathFinding::GetTotalInfluenceOnTiles(viking->pos, false, 2.f, m_bot);
	const auto airInfluence = Util::PathFinding::GetTotalInfluenceOnTile(Util::GetTilePosition(viking->pos), true, m_bot);
	const auto groundInfluence = Util::PathFinding::GetTotalInfluenceOnTile(Util::GetTilePosition(viking->pos), false, m_bot);
	if(unitShouldHeal && airInfluence <= groundInfluence)
	{
		if (viking->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
		{
			morphAbility = sc2::ABILITY_ID::MORPH_VIKINGFIGHTERMODE;
			morph = true;
		}
	}
	else
	{
		if (viking->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
		{
			if (!target && threats.empty() && groundInfluence <= 0.f && !isCycloneHelper)
			{
				const auto vikingAssaultRange = 6.f;
				bool potentialTarget = false; 
				for (const auto enemyUnit : targets)
				{
					const auto distanceToEnemy = Util::Dist(viking->pos, enemyUnit->pos);
					const auto enemyGroundDps = Util::GetGroundDps(enemyUnit, m_bot);
					if (enemyGroundDps >= 5.f)
					{
						const auto enemyThreatRange = Util::getThreatRange(false, viking->pos, viking->radius, enemyUnit, m_bot);
						const auto enemyThreatSpeed = Util::getSpeedOfUnit(enemyUnit, m_bot);
						if (distanceToEnemy <= enemyThreatRange + enemyThreatSpeed * 2.03f)	// 2.03 is maximum duration for landing
						{
							potentialTarget = false;
							break;	// We do not want to land near a dangerous unit
						}
						continue;	// Do not land to attack dangerous ground units
					}
					if (enemyUnit->is_flying)
						continue;	// We want to find a ground target
					const auto realVikingRange = vikingAssaultRange + viking->radius + enemyUnit->radius;
					const bool closeToEnemy = distanceToEnemy <= realVikingRange;
					if (!closeToEnemy)
						continue;	// Wait to be close to target until landing
					potentialTarget = true;
				}
				if (potentialTarget)
				{
					morphAbility = sc2::ABILITY_ID::MORPH_VIKINGASSAULTMODE;
					morph = true;
				}
			}
		}
		else
		{
			bool strongThreat = false;
			if (target)
			{
				for (const auto threat : threats)
				{
					const auto threatDps = Util::GetDpsForTarget(threat, viking, m_bot);
					if (threatDps >= 5.f)
					{
						strongThreat = true;
						break;
					}
				}
			}
			if (!target || strongThreat)
			{
				if (airInfluence <= groundInfluence)
				{
					morphAbility = sc2::ABILITY_ID::MORPH_VIKINGFIGHTERMODE;
					morph = true;
				}
			}
		}
	}
	if(morph)
	{
		const auto action = RangedUnitAction(MicroActionType::Ability, morphAbility, true, VIKING_MORPH_FRAME_COUNT);
		morph = PlanAction(viking, action);
	}
	return morph;
}

bool RangedManager::ExecuteThorMorphLogic(const sc2::Unit * thor)
{
	bool morph = false;
	sc2::AbilityID morphAbility = 0;
	if (thor->unit_type == sc2::UNIT_TYPEID::TERRAN_THOR)
	{
		morphAbility = sc2::ABILITY_ID::MORPH_THORHIGHIMPACTMODE;
		morph = true;
	}
	if (morph)
	{
		const auto action = RangedUnitAction(MicroActionType::Ability, morphAbility, true, THOR_MORPH_FRAME_COUNT);
		morph = PlanAction(thor, action);
	}
	return morph;
}

bool RangedManager::MoveToGoal(const sc2::Unit * rangedUnit, sc2::Units & threats, const sc2::Unit * target, CCPosition goal, bool unitShouldHeal, bool force)
{
	if (force ||
		(threats.empty()
		&& (unitShouldHeal || !target || Util::DistSq(rangedUnit->pos, target->pos) > m_order.getRadius() * m_order.getRadius())))
	{
#ifndef PUBLIC_RELEASE
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(rangedUnit->pos, goal, sc2::Colors::Blue);
#endif

		const float squaredDistanceToGoal = Util::DistSq(rangedUnit->pos, goal);
		const bool moveWithoutAttack = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER || (squaredDistanceToGoal > 10.f * 10.f && !m_bot.Strategy().shouldFocusBuildings()) || m_bot.Data(rangedUnit->unit_type).isBuilding;
		const int actionDuration = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? REAPER_MOVE_FRAME_COUNT : 0;
		const auto action = RangedUnitAction(moveWithoutAttack ? MicroActionType::Move : MicroActionType::AttackMove, goal, unitShouldHeal, actionDuration);
		PlanAction(rangedUnit, action);
		return true;
	}
	return false;
}

bool RangedManager::ShouldAttackTarget(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats) const
{
	if (rangedUnit->weapon_cooldown > 0.f)
		return false;

	if (IsInRangeOfSlowerUnit(rangedUnit, target))
		return false;

	for (auto & threat : threats)
	{
		if (IsInRangeOfSlowerUnit(rangedUnit, threat))
			return false;
	}

	return true;
}

bool RangedManager::IsInRangeOfSlowerUnit(const sc2::Unit * rangedUnit, const sc2::Unit * target) const
{
	const float targetSpeed = Util::getSpeedOfUnit(target, m_bot);
	const float rangedUnitSpeed = Util::getSpeedOfUnit(rangedUnit, m_bot);
	const float speedDiff = targetSpeed - rangedUnitSpeed;
	if (speedDiff >= 0.f)
		return false;	// Target is faster (or equal)
	const float targetRange = Util::GetAttackRangeForTarget(target, rangedUnit, m_bot);
	if (targetRange == 0.f)
		return false;	// Target cannot attack our unit
	const float targetRangeWithBuffer = targetRange + std::max(HARASS_THREAT_RANGE_BUFFER, speedDiff);
	const float squareDistance = Util::DistSq(rangedUnit->pos, target->pos);
	if (squareDistance > targetRangeWithBuffer * targetRangeWithBuffer)
		return false;	// Target is far
	if (squareDistance > targetRange * targetRange && targetSpeed == 0.f)
		return false;	// Target is somewhat close but cannot move
	return true;
}

CCPosition RangedManager::GetDirectionVectorTowardsGoal(const sc2::Unit * rangedUnit, const sc2::Unit * target, CCPosition goal, bool targetInAttackRange, bool unitShouldHeal) const
{
	CCPosition dirVec(0.f, 0.f);
	if (target && !unitShouldHeal)
	{
		// if not in range of target, add normalized vector towards target
		if (!targetInAttackRange)
		{
			dirVec = target->pos - rangedUnit->pos;
		}
	}
	else
	{
		// add normalized vector towards objective
		dirVec = goal - rangedUnit->pos;

#ifndef PUBLIC_RELEASE
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(rangedUnit->pos, goal, sc2::Colors::Blue);
#endif
	}
	Util::Normalize(dirVec);
	return dirVec;
}

const sc2::Unit * RangedManager::ExecuteLockOnLogic(const sc2::Unit * cyclone, bool shouldHeal, bool & shouldAttack, bool & shouldUseLockOn, bool & lockOnAvailable, const sc2::Units & rangedUnits, const sc2::Units & threats, const sc2::Unit * target, sc2::AvailableAbilities & abilities)
{
	const uint32_t currentFrame = m_bot.GetCurrentFrame();
	auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
	//const bool queryAbilityAvailable = QueryIsAbilityAvailable(cyclone, sc2::ABILITY_ID::EFFECT_LOCKON);
	const bool abilityAvailable = isAbilityAvailable(sc2::ABILITY_ID::EFFECT_LOCKON, cyclone);
	if (abilityAvailable)
	{
		lockOnAvailable = true;
		// Lock-On ability is not on cooldown
		const auto it = lockOnTargets.find(cyclone);
		if (it != lockOnTargets.end())
		{
			if (IsCycloneLockOnCanceled(cyclone, true, abilities))
			{
				lockOnTargets.erase(cyclone);
				setNextFrameAbilityAvailable(sc2::ABILITY_ID::EFFECT_LOCKON, cyclone, currentFrame + CYCLONE_LOCKON_COOLDOWN_FRAME_COUNT);
			}
			else
			{
				target = it->second.first;
				if (m_bot.Config().DrawHarassInfo)
					m_bot.Map().drawLine(cyclone->pos, target->pos, sc2::Colors::Green);
				// Attacking would cancel our lock-on
				shouldAttack = false;
			}
		}
		else
		{
			// The Cyclone could attack, but it should use its lock-on ability
			shouldAttack = false;
			shouldUseLockOn = true;
		}
	}
	else
	{
		lockOnAvailable = false;
		if (m_bot.Config().DrawHarassInfo)
		{
			auto & nextAvailableAbility = m_bot.Commander().Combat().getNextAvailableAbility();
			m_bot.Map().drawCircle(cyclone->pos, float(nextAvailableAbility[sc2::ABILITY_ID::EFFECT_LOCKON][cyclone] - currentFrame) / CYCLONE_LOCKON_COOLDOWN_FRAME_COUNT, sc2::Colors::Red);
		}
	}

	// Check if the Cyclone would have a better Lock-On target
	if (shouldUseLockOn)
	{
		if (!threats.empty())
		{
			const auto cycloneHeight = m_bot.Map().terrainHeight(cyclone->pos);
			auto & abilityCastingRanges = m_bot.Commander().Combat().getAbilityCastingRanges();
			const auto partialLockOnRange = abilityCastingRanges[sc2::ABILITY_ID::EFFECT_LOCKON] + cyclone->radius;
			std::map<const sc2::Unit *, int> lockedOnTargets;
			for (const auto & lockOnTarget : lockOnTargets)
			{
				auto lockedOnTarget = lockOnTarget.second.first;
				const auto it = lockedOnTargets.find(lockedOnTarget);
				if (it == lockedOnTargets.end())
					lockedOnTargets[lockedOnTarget] = 1;
				else
					lockedOnTargets[lockedOnTarget] += 1;
			}
			const sc2::Unit * bestTarget = nullptr;
			float bestScore = 0.f;
			for(const auto threat : threats)
			{
				// Do not Lock On on workers
				if (UnitType(threat->unit_type, m_bot).isWorker())
					continue;
				// Do not Lock On on units that are already Locked On unless they have a lot of hp
				// Will target the unit only if it can absorb more than 3 missiles (20 damage each) per Cyclone Locked On to it
				const auto it = lockedOnTargets.find(threat);
				if (it != lockedOnTargets.end() && threat->health + threat->shield <= it->second * 60)
					continue;
				const float threatHeight = m_bot.Map().terrainHeight(threat->pos);
				if (threatHeight > cycloneHeight)
				{
					bool hasGoodViewOfUnit = false;
					for (const auto rangedUnit : rangedUnits)
					{
						if (rangedUnit == cyclone)
							continue;
						const float distSq = Util::DistSq(rangedUnit->pos, threat->pos);
						if(distSq <= 11.f * 11.f && (rangedUnit->is_flying || m_bot.Map().terrainHeight(rangedUnit->pos) >= threatHeight))
						{
							hasGoodViewOfUnit = true;
							break;
						}
					}
					if(!hasGoodViewOfUnit)
						continue;
				}
				const float dist = Util::Dist(cyclone->pos, threat->pos);
				if (shouldHeal && dist > partialLockOnRange + threat->radius)
					continue;
				// The lower the better
				const auto distanceScore = std::pow(std::max(0.f, dist - 2), 2.5f);
				const auto healthScore = 0.25f * (threat->health + threat->shield * 1.5f);
				// The higher the better
				const auto energyScore = 0.25f * threat->energy;
				const auto detectorScore = 15 * (threat->detect_range > 0.f);
				const auto threatRange = Util::GetAttackRangeForTarget(threat, cyclone, m_bot);
				const auto threatDps = Util::GetDpsForTarget(threat, cyclone, m_bot);
				const float powerScore = threatRange * threatDps * 3.f;
				const float speedScore = Util::getSpeedOfUnit(threat, m_bot) * 6.f;
				auto armoredScore = 0.f;
				if(m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::CYCLONELOCKONDAMAGEUPGRADE))
				{
					const sc2::UnitTypeData unitTypeData = Util::GetUnitTypeDataFromUnitTypeId(threat->unit_type, m_bot);
					armoredScore = 15 * Util::Contains(sc2::Attribute::Armored, unitTypeData.attributes);
				}
				const float score = energyScore + detectorScore + armoredScore + powerScore + speedScore - healthScore - distanceScore;
				if(!bestTarget || score > bestScore)
				{
					bestTarget = threat;
					bestScore = score;
				}
			}
			if (bestTarget)
				target = bestTarget;
			if (!target)
			{
				shouldUseLockOn = false;
				shouldAttack = true;
			}
		}

		if(target)
		{
			const auto type = UnitType(target->unit_type, m_bot);
			// Prevent the use of Lock-On on passive buildings
			if (type.isWorker() || (type.isBuilding() && !type.isAttackingBuilding()))
			{
				shouldUseLockOn = false;
				shouldAttack = true;
			}
		}
	}

	return target;
}

void RangedManager::LockOnTarget(const sc2::Unit * cyclone, const sc2::Unit * target)
{
	const auto action = RangedUnitAction(MicroActionType::AbilityTarget, sc2::ABILITY_ID::EFFECT_LOCKON, target, true, 0);
	PlanAction(cyclone, action);
	const auto pair = std::pair<const sc2::Unit *, uint32_t>(target, m_bot.GetGameLoop());
	auto & lockOnCastedFrame = m_bot.Commander().Combat().getLockOnCastedFrame();
	auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
	lockOnCastedFrame[cyclone] = pair;
	lockOnTargets[cyclone] = pair;
}

bool RangedManager::CycloneHasTarget(const sc2::Unit * cyclone) const
{
	const auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
	return lockOnTargets.find(cyclone) != lockOnTargets.end();
}

bool RangedManager::ExecuteThreatFightingLogic(const sc2::Unit * rangedUnit, bool unitShouldHeal, sc2::Units & rangedUnits, sc2::Units & threats)
{
	if (threats.empty())
		return false;

	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_CYCLONE)
		return false;

	float unitsPower = 0.f;
	float targetsPower = 0.f;
	sc2::Units closeUnits;
	std::map<const sc2::Unit*, const sc2::Unit*> closeUnitsTarget;

	// The harass mode deactivation is a hack to not ignore range targets
	m_harassMode = false;
	const sc2::Unit* target = getTarget(rangedUnit, threats);
	const float range = Util::GetAttackRangeForTarget(rangedUnit, target, m_bot);
	const bool closeToEnemyTempest = target && target->unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST && Util::DistSq(rangedUnit->pos, target->pos) <= range * range;
	if (!target || !isTargetRanged(target) || (unitShouldHeal && !closeToEnemyTempest))
	{
		m_harassMode = true;
		return false;
	}

	// Check if unit can fight cloaked
	if(rangedUnit->energy >= 5 && (rangedUnit->cloak == sc2::Unit::CloakedAllied || (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE && ShouldBansheeCloak(rangedUnit, false))))
	{
		// If the unit is at an undetected position
		if (!Util::IsPositionUnderDetection(rangedUnit->pos, m_bot))
		{
			bool canFightCloaked = true;
			const float targetDist = Util::Dist(rangedUnit->pos, target->pos);
			if (targetDist > range)
			{
				const CCPosition closestAttackPosition = rangedUnit->pos + Util::Normalized(target->pos - rangedUnit->pos) * (targetDist - range);
				canFightCloaked = !Util::IsPositionUnderDetection(closestAttackPosition, m_bot);
			}
			if(canFightCloaked)
			{
				if (Util::PathFinding::HasCombatInfluenceOnTile(Util::GetTilePosition(rangedUnit->pos), rangedUnit, m_bot) && ExecuteBansheeCloakLogic(rangedUnit, false))
				{
					m_harassMode = true;
					return true;
				}

				// If the unit is standing on effect influence, get it out of there before fighting
				if (Util::PathFinding::GetEffectInfluenceOnTile(Util::GetTilePosition(rangedUnit->pos), rangedUnit, m_bot) > 0.f)
				{
					CCPosition movePosition = Util::PathFinding::FindOptimalPathToDodgeEffectAwayFromGoal(rangedUnit, target->pos, range, m_bot);
					if (movePosition != CCPosition())
					{
						const auto action = RangedUnitAction(MicroActionType::Move, movePosition, true, 0);
						// Move away from the effect
						PlanAction(rangedUnit, action);
						m_harassMode = true;
						return true;
					}
					else
					{
						Util::DisplayError("Could not find an escape path towards target", "", m_bot);
					}
				}

				const bool canAttackNow = range <= targetDist && rangedUnit->weapon_cooldown <= 0.f;
				const int attackDuration = canAttackNow ? getAttackDuration(rangedUnit, target) : 0;
				const auto action = RangedUnitAction(MicroActionType::AttackUnit, target, false, attackDuration);
				// Attack the target
				PlanAction(rangedUnit, action);
				m_harassMode = true;
				return true;
			}
		}
	}

	// Calculate ally power
	for (auto unit : rangedUnits)
	{
		// Ignore units that are too far to help
		if (Util::DistSq(unit->pos, rangedUnit->pos) > HARASS_FRIENDLY_SUPPORT_MIN_DISTANCE * HARASS_FRIENDLY_SUPPORT_MIN_DISTANCE)
		{
			continue;
		}
		auto & unitAction = unitActions[unit];
		// Ignore units that are executing a prioritized action
		if(unitAction.prioritized)
		{
			continue;
		}
		//TODO decide if we want only synchronized units or also desynchronized ones
		// Ignore units that are currently executing an action
		/*if(unitAction.executed && !unitAction.finished)
		{
			continue;
		}*/
		// Ignore units that are not ready to perform an action
		if(ShouldSkipFrame(unit))
		{
			continue;
		}
		const bool canAttackTarget = target->is_flying ? Util::CanUnitAttackAir(unit, m_bot) : Util::CanUnitAttackGround(unit, m_bot);
		const bool canAttackTempest = target->unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST && canAttackTarget;
		// Ignore units that should heal to not consider them in the power calculation, unless the target is a Tempest and our unit can attack it
		if (unit != rangedUnit && ShouldUnitHeal(unit) && !canAttackTempest)
		{
			continue;
		}
		// Ignore Cyclones because their health is too valuable to trade
		if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_CYCLONE)// && CycloneHasTarget(unit))
		{
			continue;
		}
		// We don't want flying helpers to take damage so Cyclones can keep vision for longer periods, unless it is a Viking because fuck Tempests
		if (m_cycloneFlyingHelpers.find(unit) != m_cycloneFlyingHelpers.end() && unit->unit_type != sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
		{
			continue;
		}

		// Check if it can attack the selected target
		const sc2::Unit* unitTarget = canAttackTarget ? target : nullptr;
		if (!unitTarget)
		{
			// If it cannot, check if there is a better target among the threats
			const sc2::Unit* otherTarget = getTarget(unit, threats);
			if(otherTarget && isTargetRanged(otherTarget))
			{
				unitTarget = otherTarget;
			}
		}

		// If the unit has a target, add it to the close units and calculate its power 
		if(unitTarget)
		{
			closeUnits.push_back(unit);
			closeUnitsTarget[unit] = unitTarget;
			unitsPower += Util::GetUnitPower(unit, unitTarget, m_bot);
		}
	}

	float maxThreatSpeed = 0.f;
	float maxThreatRange = 0.f;
	// Calculate enemy power
	for (auto threat : threats)
	{
		if(threat->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
		{
			continue;
		}
		const float threatSpeed = Util::getSpeedOfUnit(threat, m_bot);
		if (threatSpeed > maxThreatSpeed)
			maxThreatSpeed = threatSpeed;
		const sc2::Unit* threatTarget = getTarget(threat, closeUnits);
		const float threatRange = Util::GetAttackRangeForTarget(threat, threatTarget, m_bot);
		if (threatRange > maxThreatRange)
			maxThreatRange = threatRange;
		targetsPower += Util::GetUnitPower(threat, threatTarget, m_bot);
	}

	m_harassMode = true;

	sc2::Units vikings;
	sc2::Units tempests;
	for (const auto ally : closeUnits)
	{
		if (ally->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
			vikings.push_back(ally);
	}
	for (const auto threat : threats)
	{
		if (threat->unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST)
			tempests.push_back(threat);
	}

	bool currentUnitHasACommand = false;
	// If we can beat the enemy
	m_bot.StartProfiling("0.10.4.1.5.1.5.1          SimulateCombat");
	bool winSimulation = Util::SimulateCombat(closeUnits, threats, m_bot);
	m_bot.StopProfiling("0.10.4.1.5.1.5.1          SimulateCombat");
	const bool formulaWin = unitsPower >= targetsPower;
	bool shouldFight = winSimulation && formulaWin;

	if(!vikings.empty() && !tempests.empty())
	{
		const auto otherEnemies = threats.size() - tempests.size();
		if (!winSimulation && otherEnemies > 0)
		{
			winSimulation = Util::SimulateCombat(vikings, tempests, m_bot);
		}
		shouldFight = winSimulation;
	}
	else if(maxThreatRange >= 10)
	{
		shouldFight = winSimulation;	// We consider only the simulation for long range enemies because our formula is shit
	}

	// For each of our close units
	for (auto & unitAndTarget : closeUnitsTarget)
	{
		const auto unit = unitAndTarget.first;
		const auto unitTarget = unitAndTarget.second;

		if (shouldFight && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE && ExecuteBansheeCloakLogic(unit, false))
		{
			if (unit == rangedUnit)
				currentUnitHasACommand = true;
			continue;
		}

		const float unitRange = Util::GetAttackRangeForTarget(unit, unitTarget, m_bot);
		const bool canAttackNow = unitRange * unitRange >= Util::DistSq(unit->pos, unitTarget->pos) && unit->weapon_cooldown <= 0.f;

		// Even if the fight would be lost, should still attack if it can, but only if it is slower than the fastest enemy
		if (!shouldFight && (!canAttackNow || Util::getSpeedOfUnit(unit, m_bot) > maxThreatSpeed))
		{
			continue;
		}

		// If the unit is standing on effect influence, get it out of it before fighting
		if (Util::PathFinding::GetEffectInfluenceOnTile(Util::GetTilePosition(unit->pos), unit, m_bot) > 0.f)
		{
			CCPosition movePosition = Util::PathFinding::FindOptimalPathToDodgeEffectAwayFromGoal(unit, unitTarget->pos, unitRange, m_bot);
			if (movePosition != CCPosition())
			{
				const int actionDuration = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? REAPER_MOVE_FRAME_COUNT : 0;
				const auto action = RangedUnitAction(MicroActionType::Move, movePosition, true, actionDuration);
				PlanAction(unit, action);
				if (unit == rangedUnit)
					currentUnitHasACommand = true;
				continue;
			}
			else
			{
				Util::DisplayError("Could not find an escape path", "", m_bot);
			}
		}

		auto movePosition = CCPosition();
		const bool injured = unit->health / unit->health_max < 0.5f;
		const auto enemyRange = Util::GetAttackRangeForTarget(unitTarget, unit, m_bot);
		const auto unitSpeed = Util::getSpeedOfUnit(unit, m_bot);
		const auto enemySpeed = Util::getSpeedOfUnit(unitTarget, m_bot);
		const bool shouldKite = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER || (unitRange > enemyRange && unitSpeed > enemySpeed);
		const bool shouldChase = unitRange < enemyRange && unitSpeed >= enemySpeed;
		if (!canAttackNow && AllowUnitToPathFind(unit, false))
		{
			if (injured || shouldKite)
			{
				movePosition = Util::PathFinding::FindOptimalPathToSaferRange(unit, unitTarget, unitRange, m_bot);
			}
			else if(shouldChase)
			{
				auto path = Util::PathFinding::FindOptimalPath(unit, unitTarget->pos, {}, unitRange, false, false, true, true, false, m_bot);
				movePosition = Util::PathFinding::GetCommandPositionFromPath(path, unit, m_bot);
			}
		}
		if (movePosition != CCPosition())
		{
			// Flee but stay in range
			const int actionDuration = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? REAPER_MOVE_FRAME_COUNT : 0;
			const auto action = RangedUnitAction(MicroActionType::Move, movePosition, false, actionDuration);
			PlanAction(unit, action);
		}
		else
		{
			// Attack the target
			const int attackDuration = canAttackNow ? getAttackDuration(unit, unitTarget) : 0;
			const auto action = RangedUnitAction(MicroActionType::AttackUnit, unitTarget, true, attackDuration);
			PlanAction(unit, action);
		}
		if (unit == rangedUnit)
		{
			const float damageDealth = Util::GetDpsForTarget(rangedUnit, unitTarget, m_bot) / 22.4f;
			m_bot.Analyzer().increaseTotalDamage(damageDealth, rangedUnit->unit_type);
			currentUnitHasACommand = true;
		}
	}
	return currentUnitHasACommand;
}

void RangedManager::ExecuteCycloneLogic(const sc2::Unit * rangedUnit, bool & unitShouldHeal, bool & shouldAttack, bool & cycloneShouldUseLockOn, bool & cycloneShouldStayCloseToTarget, const sc2::Units & rangedUnits, const sc2::Units & threats, const sc2::Unit * & target, CCPosition & goal, sc2::AvailableAbilities & abilities)
{
	bool lockOnAvailable;
	target = ExecuteLockOnLogic(rangedUnit, unitShouldHeal, shouldAttack, cycloneShouldUseLockOn, lockOnAvailable, rangedUnits, threats, target, abilities);

	// If the Cyclone has a its Lock-On on a target with a big range (like a Tempest or Tank)
	if (!shouldAttack && !cycloneShouldUseLockOn && m_order.getType() != SquadOrderTypes::Defend)
	{
		const auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
		const auto it = lockOnTargets.find(rangedUnit);
		if (it != lockOnTargets.end())
		{
			const auto lockOnTarget = it->second.first;
			const auto enemyRange = Util::GetAttackRangeForTarget(lockOnTarget, rangedUnit, m_bot);
			if (enemyRange >= 10.f)
			{
				// We check if we have another unit that is close to it, but if not, the Cyclone should stay close to it
				bool closeAlly = false;
				for (const auto ally : rangedUnits)
				{
					if (ally == rangedUnit)
						continue;
					if (Util::DistSq(ally->pos, lockOnTarget->pos) > 7.f * 7.f)
						continue;
					// If the ally could survive at least 2 seconds close to the target
					if (ally->health >= Util::PathFinding::GetTotalInfluenceOnTile(Util::GetTilePosition(ally->pos), ally, m_bot) * 2.f)
					{
						closeAlly = true;
						break;
					}
				}
				if (!closeAlly)
				{
					cycloneShouldStayCloseToTarget = true;
					unitShouldHeal = false;
					target = lockOnTarget;
				}
			}
		}
	}

	const auto cycloneWithHelperIt = m_cyclonesWithHelper.find(rangedUnit);
	const bool hasFlyingHelper = cycloneWithHelperIt != m_cyclonesWithHelper.end();

	if (!unitShouldHeal && !cycloneShouldStayCloseToTarget && m_order.getType() != SquadOrderTypes::Defend)
	{
		// If the Cyclone wants to use its lock-on ability, we make sure it stays close to its flying helper to keep a good vision
		if (cycloneShouldUseLockOn && hasFlyingHelper)
		{
			const auto cycloneFlyingHelper = cycloneWithHelperIt->second;
			// If the flying helper is too far, go towards it
			if (Util::DistSq(rangedUnit->pos, cycloneFlyingHelper->pos) > CYCLONE_PREFERRED_MAX_DISTANCE_TO_HELPER * CYCLONE_PREFERRED_MAX_DISTANCE_TO_HELPER)
			{
				goal = cycloneFlyingHelper->pos;
			}
		}

		if (!hasFlyingHelper)
		{
			// If the target is too far, we don't want to chase it, we just leave
			if (target)
			{
				const float lockOnRange = m_bot.Commander().Combat().getAbilityCastingRanges().at(sc2::ABILITY_ID::EFFECT_LOCKON) + rangedUnit->radius + target->radius;
				if (Util::DistSq(rangedUnit->pos, target->pos) > lockOnRange * lockOnRange)
				{
					// TODO remove when the flying helper bug is fixed
					bool closeFlyingUnit = false;
					for (const auto allyUnit : rangedUnits)
					{
						if (!allyUnit->is_flying)
							continue;
						if (Util::DistSq(rangedUnit->pos, allyUnit->pos) < 7.f * 7.f)
						{
							closeFlyingUnit = true;
							break;
						}
					}
					if (!closeFlyingUnit)
						target = nullptr;
				}
			}
			// Find the closest flying helper on the map
			float minDistSq = 0.f;
			const sc2::Unit* closestHelper = nullptr;
			for (const auto & flyingHelper : m_cycloneFlyingHelpers)
			{
				const float distSq = Util::DistSq(rangedUnit->pos, flyingHelper.first->pos);
				if (!closestHelper || distSq < minDistSq)
				{
					minDistSq = distSq;
					closestHelper = flyingHelper.first;
				}
			}
			// If there is one
			if (closestHelper)
			{
				goal = closestHelper->pos;
			}
			else
			{
				// Otherwise go back to base, it's too dangerous to go alone
				goal = m_bot.GetStartLocation();
				unitShouldHeal = true;
			}
		}
	}

	if (!lockOnAvailable)
	{
		goal = m_bot.GetStartLocation();
	}
}

bool RangedManager::ExecutePrioritizedUnitAbilitiesLogic(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats, sc2::Units & targets, CCPosition goal, bool unitShouldHeal, bool isCycloneHelper)
{
	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
	{
		if (ExecuteVikingMorphLogic(rangedUnit, goal, target, threats, targets, unitShouldHeal, isCycloneHelper))
			return true;
	}

	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE)
	{
		if (ExecuteBansheeUncloakLogic(rangedUnit, goal, threats, unitShouldHeal))
			return true;
	}

	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER)
	{
		if (ExecuteOffensiveTeleportLogic(rangedUnit, threats, goal))
			return true;

		if (ExecuteYamatoCannonLogic(rangedUnit, targets))
			return true;
	}

	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_THOR || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_THORAP)
	{
		if (ExecuteThorMorphLogic(rangedUnit))
			return true;
	}

	return false;
}

bool RangedManager::ExecuteUnitAbilitiesLogic(const sc2::Unit * rangedUnit, sc2::Units & threats)
{
	if (ExecuteKD8ChargeLogic(rangedUnit, threats))
	{
		return true;
	}

	if (ExecuteAutoTurretLogic(rangedUnit, threats))
	{
		return true;
	}

	return false;
}

bool RangedManager::ExecuteOffensiveTeleportLogic(const sc2::Unit * battlecruiser, const sc2::Units & threats, CCPosition goal)
{
	if (!threats.empty())
		return false;

	// TODO Instead of not teleporting, teleport on the Tempests or Carriers
	if (m_bot.Strategy().enemyHasProtossHighTechAir())
		return false;

	/*const auto distSq = Util::DistSq(battlecruiser->pos, goal);
	if (distSq >= 50 * 50)
		return TeleportBattlecruiser(battlecruiser, goal);*/

	return false;
}

bool RangedManager::ExecuteYamatoCannonLogic(const sc2::Unit * battlecruiser, const sc2::Units & targets)
{
	const size_t currentFrame = m_bot.GetCurrentFrame();
	auto & queryYamatoAvailability = m_bot.Commander().Combat().getQueryYamatoAvailability();
	if(queryYamatoAvailability.find(battlecruiser) != queryYamatoAvailability.end())
	{
		queryYamatoAvailability.erase(battlecruiser);
		if(!QueryIsAbilityAvailable(battlecruiser, sc2::ABILITY_ID::EFFECT_YAMATOGUN))
		{
			setNextFrameAbilityAvailable(sc2::ABILITY_ID::EFFECT_YAMATOGUN, battlecruiser, currentFrame + BATTLECRUISER_YAMATO_CANNON_COOLDOWN_FRAME_COUNT);
			m_bot.Analyzer().increaseTotalDamage(200.f, battlecruiser->unit_type);
			return false;
		}
	}

	if (!isAbilityAvailable(sc2::ABILITY_ID::EFFECT_YAMATOGUN, battlecruiser))
		return false;

	const sc2::Unit* target = nullptr;
	float maxDps = 0.1f; // This will prevent Yamato to be shot on a unit that can't attack the BC
	auto & abilityCastingRanges = m_bot.Commander().Combat().getAbilityCastingRanges();
	auto & yamatoTargets = m_bot.Commander().Combat().getYamatoTargets();
	const float yamatoRange = abilityCastingRanges.at(sc2::ABILITY_ID::EFFECT_YAMATOGUN) + battlecruiser->radius;
	for (const auto potentialTarget : targets)
	{
		const auto type = UnitType(potentialTarget->unit_type, m_bot);
		if (type.isBuilding() && !type.isAttackingBuilding())
			continue;
		const float curentYamatoRange = yamatoRange + potentialTarget->radius;
		const float targetDistance = Util::DistSq(battlecruiser->pos, potentialTarget->pos);
		const float targetHp = potentialTarget->health + potentialTarget->shield;
		unsigned yamatos = 0;
		const auto & it = yamatoTargets.find(potentialTarget->tag);
		if (it != yamatoTargets.end())
			yamatos = it->second.size();
		// TODO find a way of targetting multiple yamato onto the same target if it has a lot of HP
		if (targetDistance <= curentYamatoRange * curentYamatoRange && yamatos == 0)
		{
			const auto isInfestor = potentialTarget->unit_type == sc2::UNIT_TYPEID::ZERG_INFESTOR || potentialTarget->unit_type == sc2::UNIT_TYPEID::ZERG_INFESTORBURROWED;
			if (isInfestor || (targetHp >= 120.f && targetHp <= 240.f))	//+ 240.f * yamatos)
			{
				const float dps = isInfestor ? 1000 : Util::GetDpsForTarget(potentialTarget, battlecruiser, m_bot);
				if (!target || dps > maxDps || (dps == maxDps && targetHp > target->health + target->shield))
				{
					maxDps = dps;
					target = potentialTarget;
				}
			}
		}
	}
	
	if(target)
	{
		const auto action = RangedUnitAction(MicroActionType::AbilityTarget, sc2::ABILITY_ID::EFFECT_YAMATOGUN, target, true, BATTLECRUISER_YAMATO_CANNON_FRAME_COUNT);
		PlanAction(battlecruiser, action);
		queryYamatoAvailability.insert(battlecruiser);
		yamatoTargets[target->tag][battlecruiser->tag] = currentFrame + BATTLECRUISER_YAMATO_CANNON_FRAME_COUNT + 20;
		return true;
	}

	return false;
}

bool RangedManager::QueryIsAbilityAvailable(const sc2::Unit* unit, sc2::ABILITY_ID abilityId) const
{
	const auto & availableAbilities = m_bot.Query()->GetAbilitiesForUnit(unit);
	for (const auto & ability : availableAbilities.abilities)
	{
		if (ability.ability_id.ToType() == abilityId)
			return true;
	}
	return false;
}

bool RangedManager::CanUseKD8Charge(const sc2::Unit * reaper)
{
	return reaper->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER && isAbilityAvailable(sc2::ABILITY_ID::EFFECT_KD8CHARGE, reaper);
}

bool RangedManager::ExecuteKD8ChargeLogic(const sc2::Unit * reaper, const sc2::Units & threats)
{
	// Check if the Reaper can use its KD8Charge ability
	if (!CanUseKD8Charge(reaper))
	{
		return false;
	}

	auto & abilityCastingRanges = m_bot.Commander().Combat().getAbilityCastingRanges();
	const float kd8Range = abilityCastingRanges.at(sc2::ABILITY_ID::EFFECT_KD8CHARGE);
	for (const auto threat : threats)
	{
		if (threat->is_flying)
			continue;

		if (!UnitType::isTargetable(threat->unit_type))
			continue;

		const auto it = m_bot.GetPreviousFrameEnemyPos().find(threat->tag);
		if (it == m_bot.GetPreviousFrameEnemyPos().end())
			continue;

		// The expected threat position will be used to decide where to throw the mine
		const auto previousFrameEnemyPos = it->second;
		const auto threatDirectionVector = Util::Normalized(threat->pos - previousFrameEnemyPos);
		const float threatSpeed = Util::getSpeedOfUnit(threat, m_bot);
		CCPosition expectedThreatPosition = threat->pos + threatDirectionVector * threatSpeed * HARASS_THREAT_SPEED_MULTIPLIER_FOR_KD8CHARGE;
		Unit threatUnit = Unit(threat, m_bot);
		if (threatUnit.getType().isBuilding())	//because some buildings speed > 0
			expectedThreatPosition = threat->pos;
		if (!m_bot.Map().isWalkable(expectedThreatPosition))
			continue;
		const float distToExpectedPosition = Util::DistSq(reaper->pos, expectedThreatPosition);
		// Check if we have enough reach to throw at the threat
		if (distToExpectedPosition <= kd8Range * kd8Range)
		{
			const auto action = RangedUnitAction(MicroActionType::AbilityPosition, sc2::ABILITY_ID::EFFECT_KD8CHARGE, expectedThreatPosition, true, REAPER_KD8_CHARGE_FRAME_COUNT);
			PlanAction(reaper, action);
			setNextFrameAbilityAvailable(sc2::ABILITY_ID::EFFECT_KD8CHARGE, reaper, m_bot.GetGameLoop() + REAPER_KD8_CHARGE_COOLDOWN);
			return true;
		}
	}

	return false;
}

bool RangedManager::ShouldBuildAutoTurret(const sc2::Unit * raven, const sc2::Units & threats) const
{
	if (raven->unit_type == sc2::UNIT_TYPEID::TERRAN_RAVEN && raven->energy >= 50)
	{
		const float maxDistance = 6.f + (m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::HISECAUTOTRACKING) ? 1.f : 0.f);
		for(const auto threat : threats)
		{
			if (Util::DistSq(raven->pos, threat->pos) < maxDistance * maxDistance)
				return true;
		}
	}
	return false;
}

bool RangedManager::ExecuteAutoTurretLogic(const sc2::Unit * raven, const sc2::Units & threats)
{
	if(!ShouldBuildAutoTurret(raven, threats))
	{
		return false;
	}

	const auto turretBuilding = Building(UnitType(sc2::UNIT_TYPEID::TERRAN_AUTOTURRET, m_bot), Util::GetTilePosition(raven->pos));
	const CCPosition turretPosition = Util::GetPosition(m_bot.Buildings().getBuildingPlacer().getBuildLocationNear(turretBuilding, 0, true, false, false));

	if(Util::DistSq(turretPosition, raven->pos) < 2.75f * 2.75)
	{
		//check all the enemy units to see if there is none blocking that location
		for(auto & enemy : m_bot.GetKnownEnemyUnits())
		{
			if (enemy.isFlying() || enemy.getType().isBuilding())
				continue;	// flying units do not block and buildings are already managed in getBuildLocationNear
			const float minDist = enemy.getUnitPtr()->radius + 1.5f;
			if (Util::DistSq(enemy, turretPosition) < minDist * minDist)
				return false;
		}
		const auto action = RangedUnitAction(MicroActionType::AbilityPosition, sc2::ABILITY_ID::EFFECT_AUTOTURRET, turretPosition, true, 0);
		PlanAction(raven, action);
		return true;
	}

	return false;
}

CCPosition RangedManager::GetFleeVectorFromThreat(const sc2::Unit * rangedUnit, const sc2::Unit * threat, CCPosition fleeVec, float distance, float threatRange) const
{
	float threatDps = Util::GetDpsForTarget(threat, rangedUnit, m_bot);
	float totalRange = Util::getThreatRange(rangedUnit, threat, m_bot);
	// The intensity is linearly interpolated in the buffer zone (between 0 and 1) * dps
	float bufferSize = totalRange - threatRange;
	if (bufferSize <= 0.f)
		bufferSize = 1.f;
	float intensity = threatDps * std::max(0.f, std::min(1.f, (totalRange - distance) / bufferSize));

#ifndef PUBLIC_RELEASE
	if (m_bot.Config().DrawHarassInfo)
	{
		m_bot.Map().drawCircle(threat->pos, threatRange, sc2::Colors::Red);
		m_bot.Map().drawCircle(threat->pos, totalRange, CCColor(128, 0, 0));
		m_bot.Map().drawLine(rangedUnit->pos, threat->pos, sc2::Colors::Red);
	}
#endif

	return fleeVec * intensity;
}

void RangedManager::AdjustSummedFleeVec(CCPosition & summedFleeVec) const
{
	// We normalize the threats vector only of if is higher than the max repulsion intensity
	float vecSquareLen = std::pow(summedFleeVec.x, 2) + std::pow(summedFleeVec.y, 2);
	if (vecSquareLen > std::pow(HARASS_THREAT_MAX_REPULSION_INTENSITY, 2))
	{
		Util::Normalize(summedFleeVec);
		summedFleeVec *= HARASS_THREAT_MAX_REPULSION_INTENSITY;
	}
}

CCPosition RangedManager::GetRepulsionVectorFromFriendlyReapers(const sc2::Unit * reaper, sc2::Units & rangedUnits) const
{
	// Check if there is a friendly harass unit close to this one
	float distToClosestFriendlyUnit = HARASS_FRIENDLY_REPULSION_MIN_DISTANCE * HARASS_FRIENDLY_REPULSION_MIN_DISTANCE;
	CCPosition closestFriendlyUnitPosition;
	for (auto friendlyRangedUnit : rangedUnits)
	{
		if (friendlyRangedUnit->tag != reaper->tag)
		{
			const float dist = Util::DistSq(reaper->pos, friendlyRangedUnit->pos);
			if (dist < distToClosestFriendlyUnit)
			{
				distToClosestFriendlyUnit = dist;
				closestFriendlyUnitPosition = friendlyRangedUnit->pos;
			}
		}
	}
	// Add repulsion vector if there is a friendly harass unit close enough
	if (distToClosestFriendlyUnit != HARASS_FRIENDLY_REPULSION_MIN_DISTANCE * HARASS_FRIENDLY_REPULSION_MIN_DISTANCE)
	{
#ifndef PUBLIC_RELEASE
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(reaper->pos, closestFriendlyUnitPosition, sc2::Colors::Red);
#endif

		CCPosition fleeVec = reaper->pos - closestFriendlyUnitPosition;
		Util::Normalize(fleeVec);
		// The repulsion intensity is linearly interpolated (the closer the friendly Reaper is, the stronger is the repulsion)
		const float intensity = HARASS_FRIENDLY_REPULSION_INTENSITY * (HARASS_FRIENDLY_REPULSION_MIN_DISTANCE - std::sqrt(distToClosestFriendlyUnit)) / HARASS_FRIENDLY_REPULSION_MIN_DISTANCE;
		return fleeVec * intensity;
	}
	return CCPosition(0, 0);
}

CCPosition RangedManager::GetAttractionVectorToFriendlyHellions(const sc2::Unit * hellion, sc2::Units & rangedUnits) const
{
	// Check if there is a friendly harass unit close to this one
	std::vector<const sc2::Unit*> closeAllies;
	for (auto friendlyRangedUnit : rangedUnits)
	{
		if (friendlyRangedUnit->tag != hellion->tag && friendlyRangedUnit->unit_type == hellion->unit_type)
		{
			const float dist = Util::DistSq(hellion->pos, friendlyRangedUnit->pos);
			if (dist < HARASS_FRIENDLY_ATTRACTION_MIN_DISTANCE * HARASS_FRIENDLY_ATTRACTION_MIN_DISTANCE)
				closeAllies.push_back(friendlyRangedUnit);
		}
	}
	// Add attraction vector if there is a friendly harass unit close enough
	if (!closeAllies.empty())
	{
		const CCPosition closeAlliesCenter = Util::CalcCenter(closeAllies);

#ifndef PUBLIC_RELEASE
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(hellion->pos, closeAlliesCenter, sc2::Colors::Green);
#endif

		const float distToCloseAlliesCenter = Util::Dist(hellion->pos, closeAlliesCenter);
		CCPosition attractionVector = closeAlliesCenter - hellion->pos;
		Util::Normalize(attractionVector);
		// The repulsion intensity is linearly interpolated (stronger the farthest to lower the closest)
		const float intensity = HARASS_FRIENDLY_ATTRACTION_INTENSITY * distToCloseAlliesCenter / HARASS_FRIENDLY_ATTRACTION_MIN_DISTANCE;
		return attractionVector * intensity;
	}
	return CCPosition(0, 0);
}

bool RangedManager::MoveUnitWithDirectionVector(const sc2::Unit * rangedUnit, CCPosition & directionVector, CCPosition & outPathableTile) const
{
	// Average estimate of the distance between a ledge and the other side, in increment of mineral chunck size, also based on interloperLE.
	const int initialMoveDistance = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? 9 : 5;
	int moveDistance = initialMoveDistance;
	CCPosition moveTo = rangedUnit->pos + directionVector * moveDistance;

	if (rangedUnit->is_flying)
	{
		outPathableTile = moveTo;
		return true;
	}
	
	const CCPosition mapMin = m_bot.Map().mapMin();
	const CCPosition mapMax = m_bot.Map().mapMax();
	bool canMoveAtInitialDistanceOrFarther = true;

	// Check if we can move in the direction of the vector
	// We check if we are moving towards and close to an unpathable position
	while (!m_bot.Observation()->IsPathable(moveTo))
	{
		++moveDistance;
		moveTo = rangedUnit->pos + directionVector * moveDistance;
		// If moveTo is out of the map, stop checking farther and switch to influence map navigation
		if (moveTo.x >= mapMax.x || moveTo.x < mapMin.x || moveTo.y >= mapMax.y || moveTo.y < mapMin.y)
		{
			canMoveAtInitialDistanceOrFarther = false;
			break;
		}
	}
	if (canMoveAtInitialDistanceOrFarther)
	{
		outPathableTile = moveTo;
		return true;
	}

	// If we did not found a pathable tile far enough, we check closer (will force the unit to go near a wall)
	moveDistance = 3;
	while (moveDistance <= initialMoveDistance)
	{
		++moveDistance;
		moveTo = rangedUnit->pos + directionVector * moveDistance;
		if (m_bot.Observation()->IsPathable(moveTo))
		{
			outPathableTile = moveTo;
			return true;
		}
	}
	return false;
}

CCPosition RangedManager::AttenuateZigzag(const sc2::Unit* rangedUnit, std::vector<const sc2::Unit*>& threats, CCPosition safeTile, CCPosition summedFleeVec) const
{
	float variance;
	const CCPosition threatsCenter = Util::CalcCenter(threats, variance);
	if (variance < 15)
	{
		const CCPosition vectorTowardsSafeTile = safeTile - rangedUnit->pos;
		summedFleeVec = Util::Normalized(summedFleeVec) * Util::GetNorm(vectorTowardsSafeTile);
		const CCPosition newFleeVector = (vectorTowardsSafeTile + summedFleeVec) / 2.f;
		const CCPosition newFleePosition = rangedUnit->pos + newFleeVector;
		if (m_bot.Observation()->IsPathable(newFleePosition))
		{
#ifndef PUBLIC_RELEASE
			if (m_bot.Config().DrawHarassInfo)
				m_bot.Map().drawCircle(safeTile, 0.2f, sc2::Colors::Purple);
#endif
			return newFleePosition;
		}
	}
	return safeTile;
}

// get a target for the ranged unit to attack
const sc2::Unit * RangedManager::getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets, bool filterHigherUnits) const
{
    BOT_ASSERT(rangedUnit, "null ranged unit in getTarget");

	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_RAVEN)
	{
		return nullptr;
	}

	std::multiset<std::pair<float, const sc2::Unit *>> targetPriorities;

    // for each possible target
    for (auto target : targets)
    {
        BOT_ASSERT(target, "null target unit in getTarget");

		// We don't want to target an enemy in the fog other than a worker (sometimes it could be good, but often it isn't)
		if (!UnitType(target->unit_type, m_bot).isWorker() && target->last_seen_game_loop != m_bot.GetGameLoop())
			continue;

		if (filterHigherUnits && m_bot.Map().terrainHeight(target->pos) > m_bot.Map().terrainHeight(rangedUnit->pos))
			continue;

		float priority = getAttackPriority(rangedUnit, target, m_harassMode);
		if(priority > 0.f)
			targetPriorities.insert(std::pair<float, const sc2::Unit*>(priority, target));
    }

	if (m_harassMode && rangedUnit->unit_type != sc2::UNIT_TYPEID::TERRAN_CYCLONE)
	{
		for(auto it = targetPriorities.rbegin(); it != targetPriorities.rend(); ++it)
		{
			const sc2::Unit* target = it->second;
			const float unitRange = Util::GetAttackRangeForTarget(rangedUnit, target, m_bot);
			// We do not want to target units surrounded by ranged units with our harass units 
			// TODO change this so we can check if there is a safe spot around the unit with the influence map
			bool threatTooClose = false;
			for (auto otherTarget : targets)
			{
				if (otherTarget == target)
					continue;

				const float otherTargetRange = Util::GetAttackRangeForTarget(otherTarget, rangedUnit, m_bot);
				if (otherTargetRange + HARASS_MIN_RANGE_DIFFERENCE_FOR_TARGET > unitRange && Util::DistSq(otherTarget->pos, target->pos) < HARASS_THREAT_MIN_DISTANCE_TO_TARGET * HARASS_THREAT_MIN_DISTANCE_TO_TARGET)
				{
					threatTooClose = true;
					break;
				}
			}
			if (!threatTooClose)
				return target;
		}
		return nullptr;
	}

	if (targetPriorities.empty())
		return nullptr;
	return (*targetPriorities.rbegin()).second;		//return last target because it's the one with the highest priority
}

// according to http://wiki.teamliquid.net/starcraft2/Range
// the maximum range of a melee unit is 1 (ultralisk)
bool RangedManager::isTargetRanged(const sc2::Unit * target)
{
    BOT_ASSERT(target, "target is null");
    const float maxRange = Util::GetMaxAttackRange(target, m_bot);
    return maxRange > 2.5f;
}

bool RangedManager::PlanAction(const sc2::Unit* rangedUnit, RangedUnitAction action)
{
	auto & currentAction = unitActions[rangedUnit];
	// If the unit is already performing the same action, we do nothing
	if (currentAction == action)
	{
		// Just reset the priority
		currentAction.prioritized = action.prioritized;
		return false;
	}

	// If the unit is performing a priorized action
	if(currentAction.prioritized && !action.prioritized)
	{
		return false;
	}

	// The current action is not yet finished and the new one is not prioritized
	if(currentAction.executed && !currentAction.finished && !action.prioritized)
	{
		return false;
	}

	unitActions[rangedUnit] = action;
	return true;
}

void RangedManager::CleanActions(sc2::Units &rangedUnits)
{
	sc2::Units unitsToClear;
	for (auto & unitAction : unitActions)
	{
		const auto rangedUnit = unitAction.first;
		auto & action = unitAction.second;

		// If the unit is dead, we will need to remove it from the map
		if(!rangedUnit->is_alive)
		{
			unitsToClear.push_back(rangedUnit);
			continue;
		}

		// If the unit is no longer in this squad
		if(!Util::Contains(rangedUnit, rangedUnits))
		{
			unitsToClear.push_back(rangedUnit);
			continue;
		}

		// Sometimes want to give an action only every few frames to allow slow attacks to occur and cliff jumps
		if (ShouldSkipFrame(rangedUnit))
			continue;
		// Reset the priority of the action because it is finished
		action.prioritized = false;
		action.finished = true;
	}

	for(auto unit : unitsToClear)
	{
		unitActions.erase(unit);
	}
}

void RangedManager::ExecuteActions()
{
	for (auto & unitAction : unitActions)
	{
		const auto rangedUnit = unitAction.first;
		auto & action = unitAction.second;

#ifndef PUBLIC_RELEASE
		if(m_bot.Config().DrawRangedUnitActions)
		{
			const std::string actionString = MicroActionTypeAccronyms[action.microActionType] + (action.prioritized ? "!" : "");
			m_bot.Map().drawText(rangedUnit->pos, actionString, sc2::Colors::Teal);
		}
#endif

		// If the action has already been executed and it is not time to reexecute it
		if (action.executed && (action.duration >= ACTION_REEXECUTION_FREQUENCY || m_bot.GetGameLoop() - action.executionFrame < ACTION_REEXECUTION_FREQUENCY))
			continue;

		m_bot.GetCommandMutex().lock();
		switch (action.microActionType)
		{
		case MicroActionType::AttackMove:
			Micro::SmartAttackMove(rangedUnit, action.position, m_bot);
			break;
		case MicroActionType::AttackUnit:
			Micro::SmartAttackUnit(rangedUnit, action.target, m_bot);
			break;
		case MicroActionType::Move:
			Micro::SmartMove(rangedUnit, action.position, m_bot);
			break;
		case MicroActionType::Ability:
			Micro::SmartAbility(rangedUnit, action.abilityID, m_bot);
			break;
		case MicroActionType::AbilityPosition:
			Micro::SmartAbility(rangedUnit, action.abilityID, action.position, m_bot);
			break;
		case MicroActionType::AbilityTarget:
			Micro::SmartAbility(rangedUnit, action.abilityID, action.target, m_bot);
			break;
		case MicroActionType::Stop:
			Micro::SmartStop(rangedUnit, m_bot);
			break;
		case MicroActionType::RightClick:
			Micro::SmartRightClick(rangedUnit, action.target, m_bot);
			break;
		case MicroActionType::ToggleAbility:
			Micro::SmartToggleAutoCast(rangedUnit, action.abilityID, m_bot);
			break;
		default:
			const int type = action.microActionType;
			Util::Log(__FUNCTION__, "Unknown MicroActionType: " + std::to_string(type), m_bot);
			break;
		}
		m_bot.GetCommandMutex().unlock();

		action.executed = true;
		action.executionFrame = m_bot.GetGameLoop();
		if (action.duration > 0)
		{
			nextCommandFrameForUnit[rangedUnit] = m_bot.GetGameLoop() + action.duration;
		}
	}
}

void RangedManager::CleanLockOnTargets() const
{
	auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
	for (auto it = lockOnTargets.cbegin(), next_it = it; it != lockOnTargets.cend(); it = next_it)
	{
		++next_it;
		if (!it->first->is_alive)
		{
			lockOnTargets.erase(it);
		}
	}
}

void RangedManager::CalcBestFlyingCycloneHelpers()
{
	m_cycloneFlyingHelpers.clear();
	m_cyclonesWithHelper.clear();
	CleanLockOnTargets();

	// Get the Cyclones and their potential flying helpers in the squad
	std::set<const sc2::Unit *> cyclones;
	std::set<const sc2::Unit *> potentialFlyingCycloneHelpers;
	for (const auto unit : m_units)
	{
		const auto unitPtr = unit.getUnitPtr();
		const auto type = unitPtr->unit_type;

		if (ShouldUnitHeal(unitPtr) || Util::isUnitLockedOn(unitPtr))
			continue;

		if(type == sc2::UNIT_TYPEID::TERRAN_CYCLONE)
		{
			cyclones.insert(unitPtr);
		}
		else if (unit.isFlying())
		{
			potentialFlyingCycloneHelpers.insert(unitPtr);
		}
	}

	if (cyclones.empty())
		return;

	// Gather Cyclones' targets
	const auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
	std::set<const sc2::Unit *> targets;
	for(const auto & cyclone : lockOnTargets)
	{
		// The Cyclone's target will need to be followed
		targets.insert(cyclone.second.first);
		// Cyclone does not need to be followed anymore because it has a target
		cyclones.erase(cyclone.first);
	}

	// Find clusters of targets to use less potential helpers
	sc2::Units targetsVector;
	for (const auto target : targets)
		targetsVector.push_back(target);
	const auto targetClusters = Util::GetUnitClusters(targetsVector, {}, true, m_bot);

	// Choose the best air unit to keep vision of Cyclone's targets
	for (const auto & targetCluster : targetClusters)
	{
		const sc2::Unit * closestHelper = nullptr;
		float smallestDistSq = 0.f;
		for(const auto potentialHelper : potentialFlyingCycloneHelpers)
		{
			const float distSq = Util::DistSq(targetCluster.m_center, potentialHelper->pos);
			if(!closestHelper || distSq < smallestDistSq)
			{
				closestHelper = potentialHelper;
				smallestDistSq = distSq;
			}
		}
		if (closestHelper)
		{
			// Remove the helper from the set because it is now taken
			potentialFlyingCycloneHelpers.erase(closestHelper);
			// Save the helper
			m_cycloneFlyingHelpers[closestHelper] = FlyingHelperMission(TRACK, targetCluster.m_center);
			// Associate the helper with every Cyclone that had a target in that target cluster
			for(const auto target : targetCluster.m_units)
			{
				for(const auto & cyclone : lockOnTargets)
				{
					if(target == cyclone.second.first)
					{
						m_cyclonesWithHelper[cyclone.first] = closestHelper;
					}
				}
			}
		}
	}

	// If there are Cyclones without target, follow them to give them vision
	if(!cyclones.empty() && !potentialFlyingCycloneHelpers.empty())
	{
		// Do not consider Cyclones without target that are already near a helper
		sc2::Units cyclonesVector;
		for (const auto cyclone : cyclones)
		{
			bool covered = false;
			for (const auto & helper : m_cycloneFlyingHelpers)
			{
				if (Util::DistSq(helper.first->pos, cyclone->pos) < CYCLONE_PREFERRED_MAX_DISTANCE_TO_HELPER * CYCLONE_PREFERRED_MAX_DISTANCE_TO_HELPER)
				{
					// that cyclone doesn't need help from another flying unit
					covered = true;
					m_cyclonesWithHelper[cyclone] = helper.first;
					break;
				}
			}
			if (!covered)
				cyclonesVector.push_back(cyclone);
		}

		// Find clusters of Cyclones without target to use less potential helpers
		const auto cycloneClustersVector = Util::GetUnitClusters(cyclonesVector, { sc2::UNIT_TYPEID::TERRAN_CYCLONE }, false, m_bot);
		std::list<Util::UnitCluster> cycloneClusters(cycloneClustersVector.begin(), cycloneClustersVector.end());

		if (potentialFlyingCycloneHelpers.size() <= cycloneClusters.size())
		{
			// Choose the best air unit to give vision to Cyclones without target
			for (const auto potentialHelper : potentialFlyingCycloneHelpers)
			{
				Util::UnitCluster closestCluster;
				float smallestDistSq = 0.f;
				for (const auto & cycloneCluster : cycloneClusters)
				{
					const float distSq = Util::DistSq(potentialHelper->pos, cycloneCluster.m_center);
					if (closestCluster.m_units.empty() || distSq < smallestDistSq)
					{
						closestCluster = cycloneCluster;
						smallestDistSq = distSq;
					}
				}
				if (!closestCluster.m_units.empty())
				{
					// Remove the helper from the set because it is now taken
					cycloneClusters.remove(closestCluster);
					// Save the helper
					m_cycloneFlyingHelpers[potentialHelper] = FlyingHelperMission(ESCORT, closestCluster.m_center);
					// Associate the helper with every Cyclone in that Cyclone cluster
					for (const auto cyclone : closestCluster.m_units)
					{
						m_cyclonesWithHelper[cyclone] = potentialHelper;
					}
				}
			}
		}
		else
		{
			// Choose the best air unit to give vision to Cyclones without target
			for (const auto & cycloneCluster : cycloneClusters)
			{
				const sc2::Unit * closestHelper = nullptr;
				float smallestDistSq = 0.f;
				for (const auto potentialHelper : potentialFlyingCycloneHelpers)
				{
					const float distSq = Util::DistSq(potentialHelper->pos, cycloneCluster.m_center);
					if (!closestHelper || distSq < smallestDistSq)
					{
						closestHelper = potentialHelper;
						smallestDistSq = distSq;
					}
				}
				if (closestHelper)
				{
					// Remove the helper from the set because it is now taken
					potentialFlyingCycloneHelpers.erase(closestHelper);
					// Save the helper
					m_cycloneFlyingHelpers[closestHelper] = FlyingHelperMission(ESCORT, cycloneCluster.m_center);
					// Associate the helper with every Cyclone in that Cyclone cluster
					for (const auto cyclone : cycloneCluster.m_units)
					{
						m_cyclonesWithHelper[cyclone] = closestHelper;
					}
				}
			}
		}
	}
}