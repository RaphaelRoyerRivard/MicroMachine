#include "RangedManager.h"
#include "Util.h"
#include "CCBot.h"
#include "BehaviorTreeBuilder.h"
#include "RangedActions.h"
#include <algorithm>
#include <string>
#include "AlphaBetaConsideringDurations.h"
#include "AlphaBetaUnit.h"
#include "AlphaBetaMove.h"
#include "AlphaBetaAction.h"
#include "UCTConsideringDurations.h"
#include "UCTCDUnit.h"
#include "UCTCDMove.h"
#include "UCTCDAction.h"
#include <thread>

const float HARASS_REPAIR_STATION_MAX_HEALTH_PERCENTAGE = 0.3f;
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
const int HARASS_PATHFINDING_MAX_EXPLORED_NODE = 500;
const float HARASS_PATHFINDING_TILE_BASE_COST = 0.1f;
const float HARASS_PATHFINDING_TILE_CREEP_COST = 0.5f;
const float HARASS_PATHFINDING_HEURISTIC_MULTIPLIER = 1.f;
const int HELLION_ATTACK_FRAME_COUNT = 9;
const int REAPER_KD8_CHARGE_COOLDOWN = 314;
const int REAPER_MOVE_FRAME_COUNT = 3;
const int VIKING_MORPH_FRAME_COUNT = 80;
const float CLIFF_MIN_HEIGHT_DIFFERENCE = 1.f;
const float CLIFF_MAX_HEIGHT_DIFFERENCE = 2.5f;

RangedManager::RangedManager(CCBot & bot) : MicroManager(bot)
{ }

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
		if (filterPassiveBuildings && target.getType().isBuilding() && !target.getType().isCombatUnit()) { continue; }

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

    // Use UCTCD
    if (m_bot.Config().UCTCD)
	{
        UCTCD(rangedUnits, rangedUnitTargets);
    }
    // Use alpha-beta (considering durations) for combat
    else if (m_bot.Config().AlphaBetaPruning)
	{
        AlphaBetaPruning(rangedUnits, rangedUnitTargets);
    }
	else if (m_harassMode)
	{
		HarassLogic(rangedUnits, rangedUnitTargets);
	}
    else 
	{
		RunBehaviorTree(rangedUnits, rangedUnitTargets);
    }
}

void RangedManager::RunBehaviorTree(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets)
{
	// use good-ol' BT
	for (auto rangedUnit : rangedUnits)
	{
		BOT_ASSERT(rangedUnit, "ranged unit is null");

		const sc2::Unit * target = getTarget(rangedUnit, rangedUnitTargets);
		const bool isEnemyInSightCondition = !rangedUnitTargets.empty() && target != nullptr &&
			Util::DistSq(rangedUnit->pos, target->pos) <= m_bot.Config().MaxTargetDistance * m_bot.Config().MaxTargetDistance;

		ConditionAction isEnemyInSight(isEnemyInSightCondition);
		ConditionAction isEnemyRanged(target != nullptr && isTargetRanged(target));

		FocusFireAction focusFireAction(rangedUnit, target, &rangedUnitTargets, m_bot, m_focusFireStates, &rangedUnits, m_unitHealth);
		KiteAction kiteAction(rangedUnit, target, m_bot, m_kittingStates);
		GoToObjectiveAction goToObjectiveAction(rangedUnit, m_order.getPosition(), m_bot);

		BehaviorTree* bt = BehaviorTreeBuilder()
			.selector()
			.sequence()
			.condition(&isEnemyInSight).end()
			.selector()
			.sequence()
			.condition(&isEnemyRanged).end()
			.action(&focusFireAction).end()
			.end()
			.action(&kiteAction).end()
			.end()
			.end()
			.action(&goToObjectiveAction).end()
			.end()
			.end();

		bt->tick();
	}
}

void RangedManager::setNextCommandFrameAfterAttack(const sc2::Unit* unit)
{
	int attackFrameCount = 2;
	if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_HELLION)
		attackFrameCount = HELLION_ATTACK_FRAME_COUNT;
	nextCommandFrameForUnit[unit] = m_bot.GetGameLoop() + attackFrameCount;
}

void RangedManager::HarassLogic(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets)
{
	if (m_bot.Config().EnableMultiThreading)
	{
		std::list<std::thread*> threads;
		for (auto rangedUnit : rangedUnits)
		{
			std::thread* t = new std::thread(&RangedManager::HarassLogicForUnit, this, rangedUnit, std::ref(rangedUnits), std::ref(rangedUnitTargets));
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
		for (auto rangedUnit : rangedUnits)
		{
			HarassLogicForUnit(rangedUnit, rangedUnits, rangedUnitTargets);
		}
	}
}

void RangedManager::HarassLogicForUnit(const sc2::Unit* rangedUnit, sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets)
{
	if (!rangedUnit)
		return;

	const bool isReaper = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER;
	const bool isHellion = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_HELLION;
	const bool isBanshee = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE;
	const bool isViking = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT;

	if (m_bot.Config().DrawHarassInfo)
		m_bot.Map().drawText(rangedUnit->pos, std::to_string(rangedUnit->tag));

	// Sometimes want to give an action only every few frames to allow slow attacks to occur and cliff jumps
	if (ShouldSkipFrame(rangedUnit))
		return;

	m_bot.StartProfiling("0.10.4.1.5.0        getTarget");
	const sc2::Unit * target = getTarget(rangedUnit, rangedUnitTargets);
	m_bot.StopProfiling("0.10.4.1.5.0        getTarget");
	m_bot.StartProfiling("0.10.4.1.5.1        getThreats");
	sc2::Units threats = Util::getThreats(rangedUnit, rangedUnitTargets, m_bot);
	m_bot.StopProfiling("0.10.4.1.5.1        getThreats");

	if(isBanshee && m_bot.Strategy().isBansheeCloakCompleted())
	{
		ExecuteBansheeCloakLogic(rangedUnit, threats);
	}

	CCPosition goal = m_order.getPosition();

	const bool unitShouldHeal = ShouldUnitHeal(rangedUnit);
	if (unitShouldHeal)
	{
		goal = isReaper ? CCPosition(m_bot.Map().width(), m_bot.Map().height()) * 0.5f : Util::GetPosition(m_bot.Bases().getClosestBasePosition(rangedUnit, Players::Self, false));
	}

	const float squaredDistanceToGoal = Util::DistSq(rangedUnit->pos, goal);

	// check if the viking should morph
	if(isViking && ExecuteVikingMorphLogic(rangedUnit, squaredDistanceToGoal, target, unitShouldHeal))
	{
		return;
	}

	// if there is no potential target or threat, move to objective (max distance is not considered when defending)
	if(MoveToGoal(rangedUnit, threats, target, goal, squaredDistanceToGoal))
	{
		return;
	}

	bool targetInAttackRange = false;
	float unitAttackRange = 0.f;
	if (target)
	{
		unitAttackRange = Util::GetAttackRangeForTarget(rangedUnit, target, m_bot);
		targetInAttackRange = Util::DistSq(rangedUnit->pos, target->pos) <= unitAttackRange * unitAttackRange;

		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(rangedUnit->pos, target->pos, targetInAttackRange ? sc2::Colors::Green : sc2::Colors::Yellow);
	}

	m_bot.StartProfiling("0.10.4.1.5.2        ShouldAttackTarget");
	if(targetInAttackRange && ShouldAttackTarget(rangedUnit, target, threats))
	{
		m_bot.GetCommandMutex().lock();
		Micro::SmartAttackUnit(rangedUnit, target, m_bot);
		m_bot.GetCommandMutex().unlock();
		setNextCommandFrameAfterAttack(rangedUnit);
		m_bot.StopProfiling("0.10.4.1.5.2        ShouldAttackTarget");

		m_bot.Commander().Combat().increaseTotalDamage(Util::GetDpsForTarget(rangedUnit, target, m_bot), rangedUnit->unit_type);
		m_bot.Commander().Combat().increaseTotalDamage(Util::GetDpsForTarget(rangedUnit, target, m_bot), (sc2::UNIT_TYPEID)0);
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.2        ShouldAttackTarget");

	m_bot.StartProfiling("0.10.4.1.5.3        ThreatFighting");
	// Check if our units are powerful enough to exchange fire with the enemies
	if (!unitShouldHeal && ExecuteThreatFightingLogic(rangedUnit, rangedUnits, threats))
	{
		m_bot.StopProfiling("0.10.4.1.5.3        ThreatFighting");
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.3        ThreatFighting");

	// Check if unit can use KD8Charge
	if(CanUseKD8Charge(rangedUnit))
	{
		bool usedKD8Charge = false;
		for (auto threat : threats)
		{
			// The expected threat position will be used to decide where to throw the mine
			// (between the unit and the enemy or on the enemy if it is a worker)
			if (ExecuteKD8ChargeLogic(rangedUnit, threat))
			{
				usedKD8Charge = true;
				break;
			}
		}
		if (usedKD8Charge)
			return;
	}

	m_bot.StartProfiling("0.10.4.1.5.4        OffensivePathFinding");
	if (AllowUnitToPathFind(rangedUnit))
	{
		const CCPosition pathFindEndPos = target && !unitShouldHeal ? target->pos : goal;
		CCPosition closePositionInPath = FindOptimalPathToTarget(rangedUnit, pathFindEndPos, target ? unitAttackRange : 3.f);
		if (closePositionInPath != CCPosition())
		{
			m_bot.GetCommandMutex().lock();
			Micro::SmartMove(rangedUnit, closePositionInPath, m_bot);
			m_bot.GetCommandMutex().unlock();
			if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER)
			{
				nextCommandFrameForUnit[rangedUnit] = m_bot.GetGameLoop() + REAPER_MOVE_FRAME_COUNT;
			}
			m_bot.StopProfiling("0.10.4.1.5.4        OffensivePathFinding");
			return;
		}
		else
		{
			nextPathFindingFrameForUnit[rangedUnit] = m_bot.GetGameLoop() + HARASS_PATHFINDING_COOLDOWN_AFTER_FAIL;
		}
	}
	m_bot.StopProfiling("0.10.4.1.5.4        OffensivePathFinding");

	bool useInfluenceMap = false;
	CCPosition summedFleeVec(0, 0);
	// add normalied * 1.5 vector of potential threats
	for (auto threat : threats)
	{
		const CCPosition fleeVec = Util::Normalized(rangedUnit->pos - threat->pos);

		// If our unit is almost in range of threat, use the influence map to find the best flee path
		const float dist = Util::Dist(rangedUnit->pos, threat->pos);
		const float threatRange = Util::GetAttackRangeForTarget(threat, rangedUnit, m_bot);
		if (dist < threatRange + 0.5f)
		{
			useInfluenceMap = true;
			break;
		}

		summedFleeVec += GetFleeVectorFromThreat(rangedUnit, threat, fleeVec, dist, threatRange);
	}

	if(!useInfluenceMap)
	{
		CCPosition dirVec = GetDirectionVectorTowardsGoal(rangedUnit, target, goal, targetInAttackRange);

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
			return;

		// Normalize the final direction vector so it's easier to work with
		Util::Normalize(dirVec);

		// If we find a pathable tile in the direction of the vector, we move at that tile
		CCPosition pathableTile(0, 0);
		if(MoveUnitWithDirectionVector(rangedUnit, dirVec, pathableTile))
		{
			if (m_bot.Config().DrawHarassInfo)
				m_bot.Map().drawLine(rangedUnit->pos, rangedUnit->pos+dirVec, sc2::Colors::Purple);

			m_bot.GetCommandMutex().lock();
			Micro::SmartAttackMove(rangedUnit, pathableTile, m_bot);
			m_bot.GetCommandMutex().unlock();
			if (isReaper)
			{
				nextCommandFrameForUnit[rangedUnit] = m_bot.GetGameLoop() + REAPER_MOVE_FRAME_COUNT;
			}
			return;
		}
	}

	// If close to an unpathable position or in danger
	// Use influence map to find safest path
	const CCPosition moveTo = FindOptimalPathToSafety(rangedUnit, goal);
	m_bot.GetCommandMutex().lock();
	Micro::SmartMove(rangedUnit, moveTo, m_bot);
	m_bot.GetCommandMutex().unlock();
	if (isReaper)
	{
		nextCommandFrameForUnit[rangedUnit] = m_bot.GetGameLoop() + REAPER_MOVE_FRAME_COUNT;
	}
}

bool RangedManager::ShouldSkipFrame(const sc2::Unit * rangedUnit) const
{
	const uint32_t availableFrame = nextCommandFrameForUnit.find(rangedUnit) != nextCommandFrameForUnit.end() ? nextCommandFrameForUnit.at(rangedUnit) : 0;
	return m_bot.GetGameLoop() < availableFrame;
}

bool RangedManager::AllowUnitToPathFind(const sc2::Unit * rangedUnit) const
{
	const uint32_t availableFrame = nextPathFindingFrameForUnit.find(rangedUnit) != nextPathFindingFrameForUnit.end() ? nextPathFindingFrameForUnit.at(rangedUnit) : m_bot.GetGameLoop();
	return m_bot.GetGameLoop() >= availableFrame;
}

void RangedManager::ExecuteBansheeCloakLogic(const sc2::Unit * banshee, sc2::Units & threats) const
{
	//TODO consider detectors
	if (!threats.empty() && banshee->cloak == sc2::Unit::NotCloaked && banshee->energy > 50.f)
	{
		m_bot.GetCommandMutex().lock();
		Micro::SmartAbility(banshee, sc2::ABILITY_ID::BEHAVIOR_CLOAKON, m_bot);
		m_bot.GetCommandMutex().unlock();
	}
	/*else if(threats.empty() && banshee->cloak == sc2::Unit::Cloaked)
	Micro::SmartAbility(banshee, sc2::ABILITY_ID::BEHAVIOR_CLOAKOFF, m_bot);*/
}

bool RangedManager::ShouldUnitHeal(const sc2::Unit * rangedUnit)
{
	UnitType unitType(rangedUnit->unit_type, m_bot);
	if (unitType.isRepairable())
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
		else if (rangedUnit->health / rangedUnit->health_max < HARASS_REPAIR_STATION_MAX_HEALTH_PERCENTAGE)
		{
			unitsBeingRepaired.insert(rangedUnit);
			return true;
		}
	}

	return rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER && rangedUnit->health / rangedUnit->health_max < 0.66f;
}

bool RangedManager::ExecuteVikingMorphLogic(const sc2::Unit * viking, float squaredDistanceToGoal, const sc2::Unit* target, bool unitShouldHeal)
{
	bool morph = false;
	if(unitShouldHeal && viking->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
	{
		m_bot.GetCommandMutex().lock();
		Micro::SmartAbility(viking, sc2::ABILITY_ID::MORPH_VIKINGFIGHTERMODE, m_bot);
		m_bot.GetCommandMutex().unlock();
		morph = true;
	}
	else if (squaredDistanceToGoal < 7.f * 7.f && !target)
	{
		if (viking->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
		{
			m_bot.GetCommandMutex().lock();
			Micro::SmartAbility(viking, sc2::ABILITY_ID::MORPH_VIKINGASSAULTMODE, m_bot);
			m_bot.GetCommandMutex().unlock();
			morph = true;
		}
		//TODO should morph to assault mode if there are close flying units
		if (viking->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT && !m_bot.Strategy().shouldFocusBuildings())
		{
			m_bot.GetCommandMutex().lock();
			Micro::SmartAbility(viking, sc2::ABILITY_ID::MORPH_VIKINGFIGHTERMODE, m_bot);
			m_bot.GetCommandMutex().unlock();
			morph = true;
		}
	}
	if(morph)
		nextCommandFrameForUnit[viking] = m_bot.GetGameLoop() + VIKING_MORPH_FRAME_COUNT;
	return morph;
}

bool RangedManager::MoveToGoal(const sc2::Unit * rangedUnit, sc2::Units & threats, const sc2::Unit * target, CCPosition & goal, float squaredDistanceToGoal)
{
	if ((!target ||
		(m_order.getType() != SquadOrderTypes::Defend && Util::DistSq(rangedUnit->pos, target->pos) > m_order.getRadius() * m_order.getRadius()))
		&& threats.empty())
	{
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(rangedUnit->pos, goal, sc2::Colors::Blue);

		m_bot.GetCommandMutex().lock();
		if (squaredDistanceToGoal > 10.f * 10.f)
		{
			if (m_bot.Strategy().shouldFocusBuildings())
				Micro::SmartAttackMove(rangedUnit, goal, m_bot);
			else
				Micro::SmartMove(rangedUnit, goal, m_bot);
		}
		else
		{
			Micro::SmartAttackMove(rangedUnit, goal, m_bot);
		}
		m_bot.GetCommandMutex().unlock();

		if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER)
		{
			nextCommandFrameForUnit[rangedUnit] = m_bot.GetGameLoop() + REAPER_MOVE_FRAME_COUNT;
		}
		return true;
	}
	return false;
}

bool RangedManager::ShouldAttackTarget(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats) const
{
	if (rangedUnit->weapon_cooldown > 0.f)
		return false;

	bool inRangeOfThreat = false;
	bool isCloseThreatFaster = false;
	for (auto & threat : threats)
	{
		const float threatRange = Util::GetAttackRangeForTarget(threat, rangedUnit, m_bot);
		const float threatSpeed = Util::getSpeedOfUnit(threat, m_bot);
		const float rangedUnitSpeed = Util::getSpeedOfUnit(rangedUnit, m_bot);
		const float speedDiff = threatSpeed - rangedUnitSpeed;
		const float threatRangeWithBuffer = threatRange + std::max(HARASS_THREAT_RANGE_BUFFER, speedDiff);
		const float squareDistance = Util::DistSq(rangedUnit->pos, threat->pos);
		if (squareDistance <= threatRangeWithBuffer * threatRangeWithBuffer)
		{
			if (squareDistance > threatRange * threatRange && threatSpeed == 0.f)
				continue;	// if our unit is just out of the range of a combat building, it's ok

			inRangeOfThreat = true;
			if (threatSpeed > rangedUnitSpeed)
			{
				isCloseThreatFaster = true;
				break;
			}
		}
	}

	return !inRangeOfThreat || isCloseThreatFaster;
}

CCPosition RangedManager::GetDirectionVectorTowardsGoal(const sc2::Unit * rangedUnit, const sc2::Unit * target, CCPosition goal, bool targetInAttackRange) const
{
	CCPosition dirVec(0.f, 0.f);
	const bool reaperShouldHeal = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER && rangedUnit->health / rangedUnit->health_max < 0.66f;
	if (target && !reaperShouldHeal)
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

		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(rangedUnit->pos, goal, sc2::Colors::Blue);
	}
	Util::Normalize(dirVec);
	return dirVec;
}

bool RangedManager::ExecuteThreatFightingLogic(const sc2::Unit * rangedUnit, sc2::Units & rangedUnits, sc2::Units & threats)
{
	if (threats.empty())
		return false;

	float unitsPower = 0.f;
	float targetsPower = 0.f;
	sc2::Units closeUnits;
	// The harass mode deactivation is a hack to not ignore range targets
	m_harassMode = false;
	for (auto unit : rangedUnits)
	{
		if (Util::DistSq(unit->pos, rangedUnit->pos) < HARASS_FRIENDLY_SUPPORT_MIN_DISTANCE * HARASS_FRIENDLY_SUPPORT_MIN_DISTANCE)
		{
			closeUnits.push_back(unit);
			const sc2::Unit* target = getTarget(rangedUnit, threats);
			unitsPower += Util::GetUnitPower(rangedUnit, target, m_bot);
		}
	}
	for (auto threat : threats)
	{
		const sc2::Unit* target = target = getTarget(threat, closeUnits);
		targetsPower += Util::GetUnitPower(threat, target, m_bot);
	}
	if (unitsPower > targetsPower)
	{
		const sc2::Unit* target = getTarget(rangedUnit, threats);
		if (target && isTargetRanged(target))
		{
			//RunBehaviorTree(units, threats);
			sc2::Units units;
			units.push_back(rangedUnit);
			sc2::Units targets;
			targets.push_back(target);
			RunBehaviorTree(units, targets);
			m_harassMode = true;
			//Micro::SmartAttackUnit(rangedUnit, target, m_bot);
			if (rangedUnit->weapon_cooldown <= 0.f)
			{
				const float unitRange = Util::GetAttackRangeForTarget(rangedUnit, target, m_bot);
				if (Util::DistSq(rangedUnit->pos, target->pos) <= unitRange * unitRange)
					setNextCommandFrameAfterAttack(rangedUnit);
			}
			return true;
		}
	}
	m_harassMode = true;
	return false;
}

bool RangedManager::ExecuteKD8ChargeLogic(const sc2::Unit * rangedUnit, const sc2::Unit * threat)
{
	const CCPosition fleeVec = Util::Normalized(rangedUnit->pos - threat->pos);
	const float threatSpeed = Util::getSpeedOfUnit(threat, m_bot);
	CCPosition expectedThreatPosition = threat->pos + fleeVec * threatSpeed * HARASS_THREAT_SPEED_MULTIPLIER_FOR_KD8CHARGE;
	Unit threatUnit = Unit(threat, m_bot);
	if (threatUnit.getType().isWorker() || threatUnit.getType().isBuilding())	//because some buildings speed > 0
		expectedThreatPosition = threat->pos;
	const float distToExpectedPosition = Util::DistSq(rangedUnit->pos, expectedThreatPosition);
	const float rangedUnitRange = Util::GetAttackRangeForTarget(rangedUnit, threat, m_bot);
	// Check if we have enough reach to throw at the threat
	if (distToExpectedPosition <= rangedUnitRange * rangedUnitRange)
	{
		//TODO find a group of threat
		m_bot.GetCommandMutex().lock();
		Micro::SmartAbility(rangedUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE, expectedThreatPosition, m_bot);
		m_bot.GetCommandMutex().unlock();
		nextAvailableKD8ChargeFrameForReaper[rangedUnit] = m_bot.GetGameLoop() + REAPER_KD8_CHARGE_COOLDOWN;
		setNextCommandFrameAfterAttack(rangedUnit);
		return true;
	}
	return false;
}

bool RangedManager::CanUseKD8Charge(const sc2::Unit * rangedUnit) const
{
	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER)
	{
		const uint32_t availableFrame = nextAvailableKD8ChargeFrameForReaper.find(rangedUnit) != nextAvailableKD8ChargeFrameForReaper.end() ? nextAvailableKD8ChargeFrameForReaper.at(rangedUnit) : 0;
		return m_bot.GetGameLoop() >= availableFrame;
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

	if (m_bot.Config().DrawHarassInfo)
	{
		m_bot.Map().drawCircle(threat->pos, threatRange, sc2::Colors::Red);
		m_bot.Map().drawCircle(threat->pos, totalRange, CCColor(128, 0, 0));
		m_bot.Map().drawLine(rangedUnit->pos, threat->pos, sc2::Colors::Red);
	}

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
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(reaper->pos, closestFriendlyUnitPosition, sc2::Colors::Red);

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
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(hellion->pos, closeAlliesCenter, sc2::Colors::Green);
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
	const int initialMoveDistance = 9;
	int moveDistance = initialMoveDistance;
	CCPosition moveTo = rangedUnit->pos + directionVector * moveDistance;

	if (!rangedUnit->is_flying)
	{
		const int mapWidth = m_bot.Map().width();
		const int mapHeight = m_bot.Map().height();
		bool canMoveAtInitialDistanceOrFarther = true;

		// Check if we can move in the direction of the vector
		// We check if we are moving towards and close to an unpathable position
		while (!m_bot.Observation()->IsPathable(moveTo))
		{
			++moveDistance;
			moveTo = rangedUnit->pos + directionVector * moveDistance;
			// If moveTo is out of the map, stop checking farther and switch to influence map navigation
			if (mapWidth < moveTo.x || moveTo.x < 0 || mapHeight < moveTo.y || moveTo.y < 0)
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
		moveDistance = initialMoveDistance;
		while (moveDistance > 2)
		{
			--moveDistance;
			moveTo = rangedUnit->pos + directionVector * moveDistance;
			if (m_bot.Observation()->IsPathable(moveTo))
			{
				outPathableTile = moveTo;
				return true;
			}
		}
		return false;
	}
	outPathableTile = moveTo;
	return true;
}

// Influence Map Node
struct IMNode {
	IMNode(CCTilePosition position) :
		position(position),
		parent(nullptr),
		cost(0.f),
		heuristic(0.f)
	{
	}
	IMNode(CCTilePosition position, IMNode* parent, float heuristic) :
		position(position),
		parent(parent),
		cost(0.f),
		heuristic(heuristic)
	{
	}
	IMNode(CCTilePosition position, IMNode* parent, float cost, float heuristic) :
		position(position),
		parent(parent),
		cost(cost),
		heuristic(heuristic)
	{
	}
	CCTilePosition position;
	IMNode* parent;
	float cost;
	float heuristic;

	float getTotalCost() const
	{
		return cost + heuristic;
	}

	bool operator<(const IMNode& rhs) const
	{
		return getTotalCost() < rhs.getTotalCost();
	}

	bool operator<=(const IMNode& rhs) const
	{
		return getTotalCost() <= rhs.getTotalCost();
	}

	bool operator==(const IMNode& rhs) const
	{
		return position == rhs.position;
	}
};

IMNode* getLowestCostNode(std::set<IMNode*> & set)
{
	IMNode* lowestCostNode = nullptr;
	for (const auto node : set)
	{
		if (!lowestCostNode || *node < *lowestCostNode)
			lowestCostNode = node;
	}
	return lowestCostNode;
}

bool SetContainsNode(const std::set<IMNode*> & set, IMNode* node, bool mustHaveLowerCost)
{
	for (auto n : set)
	{
		if (*n == *node)
		{
			return !mustHaveLowerCost || *n <= *node;
		}
	}
	return false;
}

CCPosition RangedManager::FindOptimalPathToTarget(const sc2::Unit * rangedUnit, CCPosition goal, float maxRange) const
{
	return FindOptimalPath(rangedUnit, goal, maxRange);
}

CCPosition RangedManager::FindOptimalPathToSafety(const sc2::Unit * rangedUnit, CCPosition goal) const
{
	return FindOptimalPath(rangedUnit, goal, 0.f);
}

CCPosition RangedManager::FindOptimalPath(const sc2::Unit * rangedUnit, CCPosition goal, float maxRange) const
{
	CCPosition returnPos;
	std::set<IMNode*> opened;
	std::set<IMNode*> closed;

	const CCTilePosition startPosition = Util::GetTilePosition(rangedUnit->pos);
	const bool flee = maxRange == 0.f;
	const CCTilePosition goalPosition = Util::GetTilePosition(goal);
	const auto start = new IMNode(startPosition);
	opened.insert(start);

	while (!opened.empty() && closed.size() < HARASS_PATHFINDING_MAX_EXPLORED_NODE)
	{
		IMNode* currentNode = getLowestCostNode(opened);
		opened.erase(currentNode);
		closed.insert(currentNode);

		const bool shouldTriggerExit = flee ? ShouldTriggerExit(currentNode, rangedUnit) : ShouldTriggerExit(currentNode, rangedUnit, goal, maxRange);
		if (shouldTriggerExit)
		{
			returnPos = GetCommandPositionFromPath(currentNode, rangedUnit);
			break;
		}

		// Find neighbors
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				if (!IsNeighborNodeValid(x, y, currentNode, rangedUnit))
					continue;

				const CCTilePosition neighborPosition(currentNode->position.x + x, currentNode->position.y + y);

				const bool isDiagonal = abs(x + y) != 1;
				const float creepCost = !rangedUnit->is_flying && m_bot.Observation()->HasCreep(Util::GetPosition(neighborPosition)) ? HARASS_PATHFINDING_TILE_CREEP_COST : 0.f;
				const float nodeCost = (GetInfluenceOnTile(neighborPosition, rangedUnit) + creepCost + HARASS_PATHFINDING_TILE_BASE_COST) * (isDiagonal ? sqrt(2) : 1);
				const float totalCost = currentNode->cost + nodeCost;
				const float heuristic = CalcEuclidianDistanceHeuristic(neighborPosition, goalPosition);
				auto neighbor = new IMNode(neighborPosition, currentNode, totalCost, heuristic);

				if (SetContainsNode(closed, neighbor, false))
					continue;	// already explored check

				if (SetContainsNode(opened, neighbor, true))
					continue;	// node already opened and of lower cost

				opened.insert(neighbor);
			}
		}
	}
	for (auto node : opened)
		delete node;
	for (auto node : closed)
		delete node;
	return returnPos;
}

bool RangedManager::IsNeighborNodeValid(int x, int y, IMNode* currentNode, const sc2::Unit * rangedUnit) const
{
	if (x == 0 && y == 0)
		return false;	// same tile check

	const CCTilePosition neighborPosition(currentNode->position.x + x, currentNode->position.y + y);

	if (currentNode->parent && neighborPosition == currentNode->parent->position)
		return false;	// parent tile check

	// TODO some tiles at the edges of the screen cannot be used
	if (neighborPosition.x < 0 || neighborPosition.y < 0 || neighborPosition.x >= m_bot.Map().width() || neighborPosition.y >= m_bot.Map().height())
		return false;	// out of bounds check

	if (!rangedUnit->is_flying)
	{
		if (m_bot.Commander().Combat().getBlockedTiles()[neighborPosition.x][neighborPosition.y])
			return false;	// tile is blocked

		// TODO check if the unit can pass between 2 blocked tiles (this will need a change in the blocked tiles map to have types of block)
		// All units can pass between 2 command structures, medium units and small units can pass between a command structure and another building 
		// while only small units can pass between non command buildings (where "between" means when 2 buildings have their corners touching diagonaly)

		if (!m_bot.Map().isWalkable(neighborPosition))
		{
			if (rangedUnit->unit_type != sc2::UNIT_TYPEID::TERRAN_REAPER)
				return false;	// unwalkable tile check

			if (!m_bot.Map().isWalkable(currentNode->position))
				return false;	// maybe the reaper is already on an unpathable tile

			const CCTilePosition furtherTile(neighborPosition.x + x, neighborPosition.y + y);
			if (!m_bot.Map().isWalkable(furtherTile))
				return false;	// unwalkable next tile check

			const float heightDiff = abs(m_bot.Map().terrainHeight(currentNode->position.x, currentNode->position.y) - m_bot.Map().terrainHeight(furtherTile.x, furtherTile.y));
			if (heightDiff < CLIFF_MIN_HEIGHT_DIFFERENCE || heightDiff > CLIFF_MAX_HEIGHT_DIFFERENCE)
				return false;	// unjumpable cliff check

			// TODO neighbor tile will need to have the furtherTile position, while also using cost of both tiles
		}
	}

	return true;
}

CCPosition RangedManager::GetCommandPositionFromPath(IMNode* currentNode, const sc2::Unit * rangedUnit) const
{
	CCPosition returnPos;
	do
	{
		const CCPosition currentPosition = Util::GetPosition(currentNode->position) + CCPosition(0.5f, 0.5f);
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawTile(Util::GetTilePosition(currentPosition), sc2::Colors::Teal, 0.2f);
		//we want to retun a node close to the current position
		if (Util::DistSq(currentPosition, rangedUnit->pos) <= 3.f * 3.f && returnPos == CCPosition(0, 0))
			returnPos = currentPosition;
		currentNode = currentNode->parent;
	} while (currentNode != nullptr);

	if (returnPos == CCPosition(0, 0))
		std::cout << "returnPos is null" << std::endl;
	else if(rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER)
	{
		//We need to click far enough to jump cliffs
		const float squareDistance = Util::DistSq(rangedUnit->pos, returnPos);
		const float terrainHeightDiff = abs(m_bot.Map().terrainHeight(rangedUnit->pos.x, rangedUnit->pos.y) - m_bot.Map().terrainHeight(returnPos.x, returnPos.y));
		if (squareDistance < 2.5f * 2.5f && terrainHeightDiff > CLIFF_MIN_HEIGHT_DIFFERENCE)
			returnPos = rangedUnit->pos + Util::Normalized(returnPos - rangedUnit->pos) * 3.f;
	}
	if (m_bot.Config().DrawHarassInfo)
		m_bot.Map().drawTile(Util::GetTilePosition(returnPos), sc2::Colors::Purple, 0.3f);
	return returnPos;
}

float RangedManager::CalcEuclidianDistanceHeuristic(CCTilePosition from, CCTilePosition to) const
{
	return Util::Dist(from, to) * HARASS_PATHFINDING_HEURISTIC_MULTIPLIER;
}

bool RangedManager::ShouldTriggerExit(const IMNode* node, const sc2::Unit * unit, CCPosition goal, float maxRange) const
{
	return GetInfluenceOnTile(node->position, unit) == 0.f && Util::Dist(Util::GetPosition(node->position) + CCPosition(0.5f, 0.5f), goal) < maxRange;
}

bool RangedManager::ShouldTriggerExit(const IMNode* node, const sc2::Unit * unit) const
{
	return GetInfluenceOnTile(node->position, unit) == 0.f;
}

float RangedManager::GetInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit) const
{
	const auto & influenceMap = unit->is_flying ? m_bot.Commander().Combat().getAirInfluenceMap() : m_bot.Commander().Combat().getGroundInfluenceMap();
	return influenceMap[tile.x][tile.y];
}

void RangedManager::AlphaBetaPruning(std::vector<const sc2::Unit *> rangedUnits, std::vector<const sc2::Unit *> rangedUnitTargets) {
    std::vector<std::shared_ptr<AlphaBetaUnit>> minUnits;
    std::vector<std::shared_ptr<AlphaBetaUnit>> maxUnits;

    if (lastUnitCommand.size() >= rangedUnits.size()) {
        lastUnitCommand.clear();
    }

    for (auto unit : rangedUnits) {
        bool has_played = false;

        if (m_bot.Config().UnitOwnAgent) {
            // Update has_played value
            for (auto unitC : lastUnitCommand) {
                if (unitC == unit) {
                    has_played = true;
                    break;
                }
            }
        }

        maxUnits.push_back(std::make_shared<AlphaBetaUnit>(unit, &m_bot, has_played));
    }
    for (auto unit : rangedUnitTargets) {
        minUnits.push_back(std::make_shared<AlphaBetaUnit>(unit, &m_bot));
    }

    AlphaBetaConsideringDurations alphaBeta = AlphaBetaConsideringDurations(std::chrono::milliseconds(m_bot.Config().AlphaBetaMaxMilli), m_bot.Config().AlphaBetaDepth, m_bot.Config().UnitOwnAgent, m_bot.Config().ClosestEnemy, m_bot.Config().WeakestEnemy, m_bot.Config().HighestPriority);
    AlphaBetaValue value = alphaBeta.doSearch(maxUnits, minUnits, &m_bot);
    size_t nodes = alphaBeta.nodes_evaluated;
    m_bot.Map().drawTextScreen(0.005f, 0.005f, std::string("Nodes explored : ") + std::to_string(nodes));
    m_bot.Map().drawTextScreen(0.005f, 0.020f, std::string("Max depth : ") + std::to_string(m_bot.Config().AlphaBetaDepth));
    m_bot.Map().drawTextScreen(0.005f, 0.035f, std::string("AB value : ") + std::to_string(value.score));
    if (value.move != NULL) {
        for (auto action : value.move->actions) {
            lastUnitCommand.push_back(action->unit->actual_unit);
            if (action->type == AlphaBetaActionType::ATTACK) {
                Micro::SmartAttackUnit(action->unit->actual_unit, action->target->actual_unit, m_bot);
            }
            else if (action->type == AlphaBetaActionType::MOVE_BACK) {
                Micro::SmartMove(action->unit->actual_unit, action->position, m_bot);
            }
            else if (action->type == AlphaBetaActionType::MOVE_FORWARD) {
                Micro::SmartMove(action->unit->actual_unit, action->position, m_bot);
            }
        }
    }
}

void RangedManager::UCTCD(std::vector<const sc2::Unit *> rangedUnits, std::vector<const sc2::Unit *> rangedUnitTargets) {
    std::vector<UCTCDUnit> minUnits;
    std::vector<UCTCDUnit> maxUnits;
    
    if (m_bot.Config().SkipOneFrame && isCommandDone) {
        isCommandDone = false;

        return;
    }

    if (lastUnitCommand.size() >= rangedUnits.size()) {
        lastUnitCommand.clear();
    }

    for (auto unit : rangedUnits) {
        bool has_played = false;

        if (m_bot.Config().UnitOwnAgent && std::find(lastUnitCommand.begin(), lastUnitCommand.end(), unit) != lastUnitCommand.end()) {
            // Update has_played value
            has_played = true;
        }

        maxUnits.push_back(UCTCDUnit(unit, &m_bot, has_played));
    }
    for (auto unit : rangedUnitTargets) {
        minUnits.push_back(UCTCDUnit(unit, &m_bot));
    }
    UCTConsideringDurations uctcd = UCTConsideringDurations(m_bot.Config().UCTCDK, m_bot.Config().UCTCDMaxTraversals, m_bot.Config().UCTCDMaxMilli, command_for_unit);
    UCTCDMove move = uctcd.doSearch(maxUnits, minUnits, m_bot.Config().ClosestEnemy, m_bot.Config().WeakestEnemy, m_bot.Config().HighestPriority, m_bot.Config().UCTCDConsiderDistance, m_bot.Config().UnitOwnAgent);

    size_t nodes = uctcd.nodes_explored;
    size_t traversals = uctcd.traversals;
    long long time_spent = uctcd.time_spent.count();
    int win_value = uctcd.win_value;
    m_bot.Map().drawTextScreen(0.005f, 0.005f, std::string("Nodes explored : ") + std::to_string(nodes));
    m_bot.Map().drawTextScreen(0.005f, 0.020f, std::string("Root traversed : ") + std::to_string(traversals) + std::string(" times"));
    m_bot.Map().drawTextScreen(0.005f, 0.035f, std::string("Time spent : ") + std::to_string(time_spent));
    m_bot.Map().drawTextScreen(0.005f, 0.050f, std::string("Most value : ") + std::to_string(win_value));

    for (auto action : move.actions) {
        if (action.unit.has_played) {
            // reset priority
            lastUnitCommand.clear();
        }
        lastUnitCommand.push_back(action.unit.actual_unit);
        command_for_unit[action.unit.actual_unit] = action;

        // Select unit (visual info only)
        const sc2::ObservationInterface* obs = m_bot.Observation();
        sc2::Point2DI target = Util::ConvertWorldToCamera(obs->GetGameInfo(), obs->GetCameraPos(), action.unit.actual_unit->pos);
        m_bot.ActionsFeatureLayer()->Select(target, sc2::PointSelectionType::PtSelect);

        if (action.type == UCTCDActionType::ATTACK) {
            Micro::SmartAttackUnit(action.unit.actual_unit, action.target.actual_unit, m_bot);
        }
        else if (action.type == UCTCDActionType::MOVE_BACK) {
            Micro::SmartMove(action.unit.actual_unit, action.position, m_bot);
        }
        else if (action.type == UCTCDActionType::MOVE_FORWARD) {
            Micro::SmartMove(action.unit.actual_unit, action.position, m_bot);
        }

        isCommandDone = true;
    }
}

// get a target for the ranged unit to attack
const sc2::Unit * RangedManager::getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets)
{
    BOT_ASSERT(rangedUnit, "null ranged unit in getTarget");

	std::multiset<std::pair<float, const sc2::Unit *>> targetPriorities;

    // for each possible target
    for (auto target : targets)
    {
        BOT_ASSERT(target, "null target unit in getTarget");

		// We don't want to target an enemy in the fog (sometimes it could be good, but often it isn't)
		if (target->last_seen_game_loop != m_bot.GetGameLoop())
			continue;

		float priority = getAttackPriority(rangedUnit, target);
		if(priority > 0.f)
			targetPriorities.insert(std::pair<float, const sc2::Unit*>(priority, target));
    }

	if (m_harassMode)
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

float RangedManager::getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * target) const
{
    BOT_ASSERT(target, "null unit in getAttackPriority");

	if (target->unit_type == sc2::UNIT_TYPEID::PROTOSS_ADEPTPHASESHIFT
		|| target->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
		return 0.f;

	// Ignoring invisible creep tumors
	const uint32_t lastGameLoop = m_bot.GetGameLoop() - 1;
	if ((target->unit_type == sc2::UNIT_TYPEID::ZERG_CREEPTUMOR
		|| target->unit_type == sc2::UNIT_TYPEID::ZERG_CREEPTUMORBURROWED
		|| target->unit_type == sc2::UNIT_TYPEID::ZERG_CREEPTUMORQUEEN)
		&& target->last_seen_game_loop < lastGameLoop)
	{
		return 0.f;
	}

	float attackerRange = Util::GetAttackRangeForTarget(attacker, target, m_bot);
	float targetRange = Util::GetAttackRangeForTarget(target, attacker, m_bot);

	if (m_harassMode)
	{
		if (targetRange + HARASS_MIN_RANGE_DIFFERENCE_FOR_TARGET > attackerRange)
			return 0.f;
	}

	float unitDps = Util::GetDpsForTarget(attacker, target, m_bot);
	if (unitDps == 0.f)	//Do not attack targets on which we would do no damage
		return 0.f;

	float healthValue = pow(target->health + target->shield, 0.5f);		//the more health a unit has, the less it is prioritized
	const float distance = Util::Dist(attacker->pos, target->pos) + attacker->radius + target->radius;
	float distanceValue = 1.f;
	if (distance > attackerRange)
		distanceValue = std::pow(0.9f, distance - attackerRange);	//the more far a unit is, the less it is prioritized

	float invisModifier = 1.f;
	if (target->cloak == sc2::Unit::CloakedDetected)
		invisModifier = 2.f;
	else if (target->is_burrowed && target->unit_type != sc2::UNIT_TYPEID::ZERG_ZERGLINGBURROWED)
		invisModifier = 2.f;
	
	Unit targetUnit(target, m_bot);
    if (targetUnit.getType().isCombatUnit() || targetUnit.getType().isWorker())
    {
        float targetDps = Util::GetDpsForTarget(target, attacker, m_bot);
        if (target->unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER)
        {
            //We manually reduce the dps of the bunker because it only serve as a shield, units will spawn out of it when destroyed
			targetDps = 5.f;
        }
        const float workerBonus = targetUnit.getType().isWorker() ? m_harassMode ? 2.f : 1.5f : 1.f;	//workers are important to kill
		float nonThreateningModifier = targetDps == 0.f ? 0.5f : 1.f;								//targets that cannot hit our unit are less prioritized
		if(targetUnit.getType().isAttackingBuilding())
		{
			nonThreateningModifier = targetDps == 0.f ? 1.f : 0.5f;		//for buildings, we prefer targetting them with units that will not get attacked back
		}
		const float minionModifier = target->unit_type == sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR ? 0.1f : 1.f;	//units that can be respawned should be less prioritized
        return (targetDps + unitDps - healthValue + distanceValue * 50) * workerBonus * nonThreateningModifier * minionModifier * invisModifier;
    }

	return (distanceValue * 50 - healthValue) * invisModifier / 100.f;		//we do not want non combat buildings to have a higher priority than other units
}

// according to http://wiki.teamliquid.net/starcraft2/Range
// the maximum range of a melee unit is 1 (ultralisk)
bool RangedManager::isTargetRanged(const sc2::Unit * target)
{
    BOT_ASSERT(target, "target is null");
    const float maxRange = Util::GetMaxAttackRange(target, m_bot);
    return maxRange > 2.5f;
}