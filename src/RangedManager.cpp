#include "RangedManager.h"
#include "Util.h"
#include "CCBot.h"
#include "BehaviorTreeBuilder.h"
#include <algorithm>
#include <string>
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
const int HELLION_ATTACK_FRAME_COUNT = 9;
const int REAPER_KD8_CHARGE_COOLDOWN = 342;
const int REAPER_KD8_CHARGE_FRAME_COUNT = 3;
const int REAPER_MOVE_FRAME_COUNT = 3;
const int VIKING_MORPH_FRAME_COUNT = 40;
const float VIKING_LANDING_DISTANCE_FROM_GOAL = 10.f;
const int ACTION_REEXECUTION_FREQUENCY = 50;

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

int RangedManager::getAttackDuration(const sc2::Unit* unit) const
{
	int attackFrameCount = 2;
	if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_HELLION)
		attackFrameCount = HELLION_ATTACK_FRAME_COUNT;
	return attackFrameCount;
}

void RangedManager::HarassLogic(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets)
{
	m_bot.StartProfiling("0.10.4.1.5.0        CleanActions");
	CleanActions(rangedUnits);
	m_bot.StopProfiling("0.10.4.1.5.0        CleanActions");

	m_bot.StartProfiling("0.10.4.1.5.1        HarassLogicForUnit");
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
	m_bot.StopProfiling("0.10.4.1.5.1        HarassLogicForUnit");

	m_bot.StartProfiling("0.10.4.1.5.2        ExecuteActions");
	ExecuteActions();
	m_bot.StopProfiling("0.10.4.1.5.2        ExecuteActions");
}

void RangedManager::HarassLogicForUnit(const sc2::Unit* rangedUnit, sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets)
{
	if (!rangedUnit)
		return;

	const bool isReaper = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER;
	const bool isHellion = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_HELLION;
	const bool isBanshee = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE;
	const bool isRaven = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_RAVEN;
	const bool isViking = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT;

#ifndef PUBLIC_RELEASE
	if (m_bot.Config().DrawHarassInfo)
		m_bot.Map().drawText(rangedUnit->pos, std::to_string(rangedUnit->tag));
#endif

	// Sometimes want to give an action only every few frames to allow slow attacks to occur and cliff jumps
	if (ShouldSkipFrame(rangedUnit))
		return;

	m_bot.StartProfiling("0.10.4.1.5.1.0          getTarget");
	const sc2::Unit * target = getTarget(rangedUnit, rangedUnitTargets);
	m_bot.StopProfiling("0.10.4.1.5.1.0          getTarget");
	m_bot.StartProfiling("0.10.4.1.5.1.1          getThreats");
	sc2::Units threats = Util::getThreats(rangedUnit, rangedUnitTargets, m_bot);
	m_bot.StopProfiling("0.10.4.1.5.1.1          getThreats");

	CCPosition goal = m_order.getPosition();

	m_bot.StartProfiling("0.10.4.1.5.1.2          ShouldUnitHeal");
	bool unitShouldHeal = ShouldUnitHeal(rangedUnit);
	if (unitShouldHeal)
	{
		goal = isReaper ? m_bot.Map().center() : m_bot.RepairStations().getBestRepairStationForUnit(rangedUnit);
	}
	else if(isRaven)
	{
		goal = GetBestSupportPosition(rangedUnit, rangedUnits);
	}
	m_bot.StopProfiling("0.10.4.1.5.1.2          ShouldUnitHeal");

	m_bot.StartProfiling("0.10.4.1.5.1.3          CheckBuffs");
	// If our unit is targeted by an enemy Cyclone's Lock-On ability, it should back until the effect wears off
	for(const auto buff : rangedUnit->buffs)
	{
		if(buff == sc2::BUFF_ID::LOCKON)
		{
			// Banshee in danger should cloak itself
			if (isBanshee && ExecuteBansheeCloakLogic(rangedUnit, true))
			{
				m_bot.StopProfiling("0.10.4.1.5.1.3          CheckBuffs");
				return;
			}

			goal = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getPosition();
			unitShouldHeal = true;
			break;
		}
	}
	m_bot.StopProfiling("0.10.4.1.5.1.3          CheckBuffs");

	const float squaredDistanceToGoal = Util::DistSq(rangedUnit->pos, goal);

	// check if the viking should morph
	if(isViking && ExecuteVikingMorphLogic(rangedUnit, squaredDistanceToGoal, target, unitShouldHeal))
	{
		return;
	}

	// if there is no potential target or threat, move to objective (max distance is not considered when defending)
	if(MoveToGoal(rangedUnit, threats, target, goal, squaredDistanceToGoal, unitShouldHeal))
	{
		return;
	}

	bool targetInAttackRange = false;
	float unitAttackRange = 0.f;
	if (target)
	{
		unitAttackRange = Util::GetAttackRangeForTarget(rangedUnit, target, m_bot);
		targetInAttackRange = Util::DistSq(rangedUnit->pos, target->pos) <= unitAttackRange * unitAttackRange;

#ifndef PUBLIC_RELEASE
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(rangedUnit->pos, target->pos, targetInAttackRange ? sc2::Colors::Green : sc2::Colors::Yellow);
#endif
	}

	m_bot.StartProfiling("0.10.4.1.5.1.4          ShouldAttackTarget");
	if(targetInAttackRange && ShouldAttackTarget(rangedUnit, target, threats))
	{
		const auto action = RangedUnitAction(MicroActionType::AttackUnit, target, unitShouldHeal, getAttackDuration(rangedUnit));
		PlanAction(rangedUnit, action);
		m_bot.StopProfiling("0.10.4.1.5.1.4          ShouldAttackTarget");

		m_bot.CombatAnalyzer().increaseTotalDamage(Util::GetDpsForTarget(rangedUnit, target, m_bot), rangedUnit->unit_type);
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.1.4          ShouldAttackTarget");

	m_bot.StartProfiling("0.10.4.1.5.1.5          ThreatFighting");
	// Check if our units are powerful enough to exchange fire with the enemies
	if (ExecuteThreatFightingLogic(rangedUnit, rangedUnits, threats, unitShouldHeal))
	{
		m_bot.StopProfiling("0.10.4.1.5.1.5          ThreatFighting");
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.1.5          ThreatFighting");

	m_bot.StartProfiling("0.10.4.1.5.1.6          UnitAbilities");
	// Check if unit can use one of its abilities
	if(ExecuteUnitAbilitiesLogic(rangedUnit, threats))
	{
		m_bot.StopProfiling("0.10.4.1.5.1.6          UnitAbilities");
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.1.6          UnitAbilities");

	m_bot.StartProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
	if (AllowUnitToPathFind(rangedUnit))
	{
		const CCPosition pathFindEndPos = target && !unitShouldHeal ? target->pos : goal;
		CCPosition closePositionInPath = Util::PathFinding::FindOptimalPathToTarget(rangedUnit, pathFindEndPos, target, target ? unitAttackRange : 3.f, m_bot);
		if (closePositionInPath != CCPosition())
		{
			const int actionDuration = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? REAPER_MOVE_FRAME_COUNT : 0;
			const auto action = RangedUnitAction(MicroActionType::Move, closePositionInPath, unitShouldHeal, actionDuration);
			PlanAction(rangedUnit, action);
			m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
			return;
		}
		else
		{
			nextPathFindingFrameForUnit[rangedUnit] = m_bot.GetGameLoop() + HARASS_PATHFINDING_COOLDOWN_AFTER_FAIL;
		}
	}
	m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding");

	m_bot.StartProfiling("0.10.4.1.5.1.8          PotentialFields");
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

			const auto action = RangedUnitAction(MicroActionType::Move, pathableTile, unitShouldHeal, isReaper ? REAPER_MOVE_FRAME_COUNT : 0);
			PlanAction(rangedUnit, action);
			m_bot.StopProfiling("0.10.4.1.5.1.8          PotentialFields");
			return;
		}
	}
	m_bot.StopProfiling("0.10.4.1.5.1.8          PotentialFields");

	// Banshee in danger should cloak itself if low on hp
	if (isBanshee && ExecuteBansheeCloakLogic(rangedUnit, unitShouldHeal))
	{
		return;
	}

	m_bot.StartProfiling("0.10.4.1.5.1.9          DefensivePathfinding");
	// If close to an unpathable position or in danger
	// Use influence map to find safest path
	CCPosition safeTile = Util::PathFinding::FindOptimalPathToSafety(rangedUnit, goal, m_bot);
	//safeTile = AttenuateZigzag(rangedUnit, threats, safeTile, summedFleeVec);	//if we decomment this, we must not break in the threat check loop
	const auto action = RangedUnitAction(MicroActionType::Move, safeTile, unitShouldHeal, isReaper ? REAPER_MOVE_FRAME_COUNT : 0);
	PlanAction(rangedUnit, action);
	m_bot.StopProfiling("0.10.4.1.5.1.9          DefensivePathfinding");
}

bool RangedManager::ShouldSkipFrame(const sc2::Unit * rangedUnit) const
{
	const uint32_t availableFrame = nextCommandFrameForUnit.find(rangedUnit) != nextCommandFrameForUnit.end() ? nextCommandFrameForUnit.at(rangedUnit) : 0;
	return m_bot.GetGameLoop() < availableFrame;
}

bool RangedManager::AllowUnitToPathFind(const sc2::Unit * rangedUnit) const
{
	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_HELLION)
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
	/*else if(threats.empty() && banshee->cloak == sc2::Unit::Cloaked)
	Micro::SmartAbility(banshee, sc2::ABILITY_ID::BEHAVIOR_CLOAKOFF, m_bot);*/
	return false;
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

CCPosition RangedManager::GetBestSupportPosition(const sc2::Unit* supportUnit, const sc2::Units & rangedUnits) const
{
	const std::vector<sc2::UNIT_TYPEID> typesToIgnore = {sc2::UNIT_TYPEID::TERRAN_RAVEN};
	const auto clusters = Util::GetUnitClusters(rangedUnits, typesToIgnore, m_bot);
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
	return m_order.getPosition();
}

bool RangedManager::ExecuteVikingMorphLogic(const sc2::Unit * viking, float squaredDistanceToGoal, const sc2::Unit* target, bool unitShouldHeal)
{
	bool morph = false;
	sc2::AbilityID morphAbility = 0;
	if(unitShouldHeal)
	{
		if (viking->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
		{
			morphAbility = sc2::ABILITY_ID::MORPH_VIKINGFIGHTERMODE;
			morph = true;
		}
	}
	else if (squaredDistanceToGoal < VIKING_LANDING_DISTANCE_FROM_GOAL * VIKING_LANDING_DISTANCE_FROM_GOAL && !target)
	{
		if (viking->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER
			&& Util::PathFinding::GetCombatInfluenceOnTile(Util::GetTilePosition(viking->pos), false, m_bot) == 0.f
			&& Util::PathFinding::GetEffectInfluenceOnTile(Util::GetTilePosition(viking->pos), false, m_bot) == 0.f)
		{
			morphAbility = sc2::ABILITY_ID::MORPH_VIKINGASSAULTMODE;
			morph = true;
		}
		//TODO should morph to assault mode if there are close flying units
		if (viking->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT && !m_bot.Strategy().shouldFocusBuildings())
		{
			morphAbility = sc2::ABILITY_ID::MORPH_VIKINGFIGHTERMODE;
			morph = true;
		}
	}
	if(morph)
	{
		const auto action = RangedUnitAction(MicroActionType::Ability, morphAbility, true, VIKING_MORPH_FRAME_COUNT);
		morph = PlanAction(viking, action);
	}
	return morph;
}

bool RangedManager::MoveToGoal(const sc2::Unit * rangedUnit, sc2::Units & threats, const sc2::Unit * target, CCPosition & goal, float squaredDistanceToGoal, bool unitShouldHeal)
{
	if ((!target ||
		(m_order.getType() != SquadOrderTypes::Defend && Util::DistSq(rangedUnit->pos, target->pos) > m_order.getRadius() * m_order.getRadius()))
		&& threats.empty())
	{
#ifndef PUBLIC_RELEASE
		if (m_bot.Config().DrawHarassInfo)
			m_bot.Map().drawLine(rangedUnit->pos, goal, sc2::Colors::Blue);
#endif

		const bool moveWithoutAttack = squaredDistanceToGoal > 10.f * 10.f && !m_bot.Strategy().shouldFocusBuildings();
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

bool RangedManager::ExecuteThreatFightingLogic(const sc2::Unit * rangedUnit, sc2::Units & rangedUnits, sc2::Units & threats, bool unitShouldHeal)
{
	if (threats.empty())
		return false;

	float unitsPower = 0.f;
	float targetsPower = 0.f;
	sc2::Units closeUnits;
	std::map<const sc2::Unit*, const sc2::Unit*> closeUnitsTarget;

	// The harass mode deactivation is a hack to not ignore range targets
	m_harassMode = false;
	const sc2::Unit* target = getTarget(rangedUnit, threats);
	if (!target || !isTargetRanged(target))
	{
		m_harassMode = true;
		return false;
	}

	// Check if unit can fight cloaked
	if(!unitShouldHeal && rangedUnit->energy >= 5 && (rangedUnit->cloak == sc2::Unit::Cloaked || (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE && ShouldBansheeCloak(rangedUnit, false))))
	{
		// If the unit is at an undetected position
		if (!Util::IsPositionUnderDetection(rangedUnit->pos, m_bot))
		{
			bool canFightCloaked = true;
			const float range = Util::GetAttackRangeForTarget(rangedUnit, target, m_bot);
			const float targetDist = Util::Dist(rangedUnit->pos, target->pos);
			if (targetDist > range)
			{
				const CCPosition closestAttackPosition = rangedUnit->pos + Util::Normalized(target->pos - rangedUnit->pos) * (targetDist - range);
				canFightCloaked = !Util::IsPositionUnderDetection(closestAttackPosition, m_bot);
			}
			if(canFightCloaked)
			{
				if (ExecuteBansheeCloakLogic(rangedUnit, false))
				{
					m_harassMode = true;
					return true;
				}

				// If the unit is standing on effect influence, get it out of there before fighting
				if (Util::PathFinding::GetEffectInfluenceOnTile(Util::GetTilePosition(rangedUnit->pos), rangedUnit, m_bot) > 0.f)
				{
					CCPosition movePosition = Util::PathFinding::FindOptimalPathToDodgeEffectTowardsGoal(rangedUnit, target->pos, range, m_bot);
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
				const int attackDuration = canAttackNow ? getAttackDuration(rangedUnit) : 0;
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
		if(unitAction.executed && !unitAction.finished)
		{
			continue;
		}
		// Ignore units that are not ready to perform an action
		if(ShouldSkipFrame(unit))
		{
			continue;
		}
		// Ignore units that should heal to not consider them in the power calculation
		if (unit != rangedUnit && ShouldUnitHeal(unit))
		{
			continue;
		}

		// Check if it can attack the selected target
		const bool canAttackTarget = target->is_flying ? Util::CanUnitAttackAir(unit, m_bot) : Util::CanUnitAttackGround(unit, m_bot);
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
			// If the unit should heal and is not alone or it is alone against multiple enemies, don't fight and let it back to heal
			if (unitShouldHeal && (unit != rangedUnit || threats.size() > 1))
			{
				m_harassMode = true;
				return false;
			}

			closeUnits.push_back(unit);
			closeUnitsTarget.insert_or_assign(unit, unitTarget);
			unitsPower += Util::GetUnitPower(unit, unitTarget, m_bot);
		}
	}

	float maxThreatSpeed = 0.f;
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
		targetsPower += Util::GetUnitPower(threat, threatTarget, m_bot);
	}

	m_harassMode = true;

	bool currentUnitHasACommand = false;
	// If we can beat the enemy
	const bool shouldFight = unitsPower >= targetsPower;
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

		if (!shouldFight && (!canAttackNow || Util::getSpeedOfUnit(unit, m_bot) > maxThreatSpeed))
		{
			continue;
		}

		// If the unit is standing on effect influence, get it out of it before fighting
		if (Util::PathFinding::GetEffectInfluenceOnTile(Util::GetTilePosition(unit->pos), unit, m_bot) > 0.f)
		{
			CCPosition movePosition = Util::PathFinding::FindOptimalPathToDodgeEffectTowardsGoal(unit, unitTarget->pos, unitRange, m_bot);
			if (movePosition != CCPosition())
			{
				const int actionDuration = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? REAPER_MOVE_FRAME_COUNT : 0;
				const auto action = RangedUnitAction(MicroActionType::Move, movePosition, true, actionDuration);
				// Attack the target
				PlanAction(unit, action);
				if (unit == rangedUnit)
					currentUnitHasACommand = true;
				continue;
			}
			else
			{
				Util::DisplayError("Could not find an escape path towards target", "", m_bot);
			}
		}

		auto fleePosition = CCPosition();
		if (!canAttackNow && unit->health / unit->health_max < 0.5f)
		{
			fleePosition = Util::PathFinding::FindOptimalPathToSaferRange(unit, unitTarget, m_bot);
		}
		if (fleePosition != CCPosition())
		{
			// Flee but stay in range
			const int actionDuration = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? REAPER_MOVE_FRAME_COUNT : 0;
			const auto action = RangedUnitAction(MicroActionType::Move, fleePosition, false, actionDuration);
			PlanAction(unit, action);
		}
		else
		{
			// Attack the target
			const int attackDuration = canAttackNow ? getAttackDuration(unit) : 0;
			const auto action = RangedUnitAction(MicroActionType::AttackUnit, unitTarget, false, attackDuration);
			PlanAction(unit, action);
		}
		if (unit == rangedUnit)
			currentUnitHasACommand = true;
	}
	return currentUnitHasACommand;
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

bool RangedManager::CanUseKD8Charge(const sc2::Unit * reaper) const
{
	if (reaper->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER)
	{
		const uint32_t availableFrame = nextAvailableKD8ChargeFrameForReaper.find(reaper) != nextAvailableKD8ChargeFrameForReaper.end() ? nextAvailableKD8ChargeFrameForReaper.at(reaper) : 0;
		return m_bot.GetGameLoop() >= availableFrame;
	}
	return false;
}

bool RangedManager::ExecuteKD8ChargeLogic(const sc2::Unit * reaper, const sc2::Units & threats)
{
	// Check if the Reaper can use its KD8Charge ability
	if (!CanUseKD8Charge(reaper))
	{
		return false;
	}

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
		const float rangedUnitRange = Util::GetAttackRangeForTarget(reaper, threat, m_bot);
		// Check if we have enough reach to throw at the threat
		if (distToExpectedPosition <= rangedUnitRange * rangedUnitRange)
		{
			const auto action = RangedUnitAction(MicroActionType::AbilityPosition, sc2::ABILITY_ID::EFFECT_KD8CHARGE, expectedThreatPosition, true, REAPER_KD8_CHARGE_FRAME_COUNT);
			if (PlanAction(reaper, action))
			{
				nextAvailableKD8ChargeFrameForReaper[reaper] = m_bot.GetGameLoop() + REAPER_KD8_CHARGE_COOLDOWN;
			}
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
	const CCPosition turretPosition = Util::GetPosition(m_bot.Buildings().getBuildingPlacer().getBuildLocationNear(turretBuilding, 0, true, false));

	if(Util::DistSq(turretPosition, raven->pos) < 2.75f * 2.75)
	{
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
	const int initialMoveDistance = 9;
	int moveDistance = initialMoveDistance;
	CCPosition moveTo = rangedUnit->pos + directionVector * moveDistance;

	if (!rangedUnit->is_flying)
	{
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
const sc2::Unit * RangedManager::getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets)
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

	if (!UnitType::isTargetable(target->unit_type))
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
		float workerBonus = 1.f;
		if(targetUnit.getType().isWorker() && m_order.getType() != SquadOrderTypes::Defend)
		{
			if (attacker->unit_type != sc2::UNIT_TYPEID::TERRAN_HELLION)
			{
				workerBonus = 2.f;
			}

			// Reduce priority for workers that are going in a refinery
			const auto enemyRace = m_bot.GetPlayerRace(Players::Enemy);
			const auto enemyRefineryType = UnitType::getEnemyRefineryType(enemyRace);
			for(auto & refinery : m_bot.GetKnownEnemyUnits(enemyRefineryType))
			{
				const float refineryDist = Util::DistSq(refinery, target->pos);
				if(refineryDist < 2.5 * 2.5)
				{
					const CCPosition facingVector = CCPosition(cos(target->facing), sin(target->facing));
					if(Dot2D(facingVector, refinery.getPosition() - target->pos) > 0.99f)
					{
						workerBonus *= 0.5f;
					}
				}
			}
		}


		float nonThreateningModifier = targetDps == 0.f ? 0.5f : 1.f;								//targets that cannot hit our unit are less prioritized
		if(targetUnit.getType().isAttackingBuilding())
		{
			nonThreateningModifier = targetDps == 0.f ? 1.f : 0.5f;		//for buildings, we prefer targetting them with units that will not get attacked back
		}
		const float flyingDetectorModifier = target->is_flying && UnitType::isDetector(target->unit_type) ? 2.f : 1.f;
		const float minionModifier = target->unit_type == sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR ? 0.1f : 1.f;	//units that can be respawned should be less prioritized
        return (targetDps + unitDps - healthValue + distanceValue * 50) * workerBonus * nonThreateningModifier * minionModifier * invisModifier * flyingDetectorModifier;
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

	unitActions.insert_or_assign(rangedUnit, action);
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