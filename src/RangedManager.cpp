#include "RangedManager.h"
#include "Util.h"
#include "CCBot.h"
#include "BehaviorTreeBuilder.h"
#include <algorithm>
#include <string>
#include <thread>
#include <list>

const float HARASS_FRIENDLY_SUPPORT_MAX_DISTANCE = 7.f;
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
const int CYCLONE_MAX_INFLUENCE_FOR_LOCKON = 105;
const int MAX_INFLUENCE_FOR_OFFENSIVE_KITING = 35;
const int HELLION_ATTACK_FRAME_COUNT = 9;
const int REAPER_KD8_CHARGE_FRAME_COUNT = 3;
const int REAPER_KD8_CHARGE_COOLDOWN = 314 + REAPER_KD8_CHARGE_FRAME_COUNT + 7;
const int REAPER_MOVE_FRAME_COUNT = 3;
const int VIKING_MORPH_FRAME_COUNT = 40;
const int THOR_GROUND_ATTACK_FRAME_COUNT = 21;
const int THOR_MORPH_FRAME_COUNT = 40;
const std::string ACTION_DESCRIPTION_THREAT_FIGHT_ATTACK = "ThreatFightAttack";
const std::string ACTION_DESCRIPTION_THREAT_FIGHT_BC_MOVE_ATTACK = "ThreatFightBCMoveAttack";
const std::string ACTION_DESCRIPTION_THREAT_FIGHT_MOVE = "ThreatFightMove";
const std::string ACTION_DESCRIPTION_THREAT_FIGHT_DODGE_EFFECT = "ThreatFightDodgeEffect";
const std::string ACTION_DESCRIPTION_THREAT_FIGHT_MORPH = "ThreatFightMorph";
const std::string ACTION_DESCRIPTION_THREAT_FIGHT_MOVE_CLOSER_BEFORE_MORPH = "ThreatFightMoveCloserBeforeMorph";
const std::string ACTION_DESCRIPTION_THREAT_FIGHT_MOVE_FARTHER_BEFORE_MORPH = "ThreatFightMoveFartherBeforeMorph";
const std::vector<std::string> THREAT_FIGHTING_ACTION_DESCRIPTIONS = {
	ACTION_DESCRIPTION_THREAT_FIGHT_ATTACK,
	ACTION_DESCRIPTION_THREAT_FIGHT_MOVE,
	ACTION_DESCRIPTION_THREAT_FIGHT_DODGE_EFFECT,
	ACTION_DESCRIPTION_THREAT_FIGHT_MORPH
};

RangedManager::RangedManager(CCBot & bot) : MicroManager(bot)
{
}

void RangedManager::setTargets(const std::vector<Unit> & targets)
{
    std::vector<Unit> rangedUnitTargets;
	// We don't want to attack buildings (like a wall or proxy) if we never reached the enemy base
	const BaseLocation* enemyStartingBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
    for (auto & target : targets)
    {
        const auto targetPtr = target.getUnitPtr();
        if (!targetPtr) { continue; }
        if (targetPtr->unit_type == sc2::UNIT_TYPEID::ZERG_EGG) { continue; }
        if (targetPtr->unit_type == sc2::UNIT_TYPEID::ZERG_LARVA) { continue; }

        rangedUnitTargets.push_back(target);
    }
    m_targets = rangedUnitTargets;
}

void RangedManager::executeMicro()
{
    const std::vector<Unit> &units = getUnits();
	if (units.empty())
		return;

	sc2::Units rangedUnits;
	for (auto & unit : units)
	{
		const sc2::Unit * rangedUnit = unit.getUnitPtr();
		rangedUnits.push_back(rangedUnit);
	}

	sc2::Units otherSquadsUnits;
	for (auto & unit : m_bot.Commander().Combat().GetCombatUnits())
	{
		const sc2::Unit * unitPtr = unit.getUnitPtr();
		if (!Util::Contains(unitPtr, rangedUnits))
			otherSquadsUnits.push_back(unitPtr);
	}

    sc2::Units rangedUnitTargets;
    for (auto target : m_targets)
    {
        rangedUnitTargets.push_back(target.getUnitPtr());
    }

    HarassLogic(rangedUnits, rangedUnitTargets, otherSquadsUnits);
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
	else if (!target->is_flying && (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_THOR || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_THORAP))
		attackFrameCount = THOR_GROUND_ATTACK_FRAME_COUNT;
	const CCPosition targetDirection = Util::Normalized(target->pos - unit->pos);
	const CCPosition facingVector = Util::getFacingVector(unit);
	const float dot = sc2::Dot2D(targetDirection, facingVector);
	const float value = 1 - dot;
	const auto slowRotationUnits = { sc2::UNIT_TYPEID::TERRAN_CYCLONE, sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, sc2::UNIT_TYPEID::TERRAN_THOR, sc2::UNIT_TYPEID::TERRAN_THORAP };
	const int rotationMultiplier = Util::Contains(unit->unit_type, slowRotationUnits) ? 4 : 2;
	attackFrameCount += value * rotationMultiplier;
	return attackFrameCount;
}

void RangedManager::HarassLogic(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets, sc2::Units &otherSquadsUnits)
{
#ifdef NO_MICRO
	return;
#endif

	m_combatSimulationResults.clear();
	m_threatsForUnit.clear();
	m_threatTargetForUnit.clear();
	m_dummyAssaultVikings.clear();
	m_dummyFighterVikings.clear();
	m_dummyStimedUnits.clear();

	m_bot.StartProfiling("0.10.4.1.5.1        HarassLogicForUnit");
	if (m_bot.Config().EnableMultiThreading)
	{
		std::list<std::thread*> threads;
		for (int i = 0; i < rangedUnits.size(); ++i)
		{
			auto rangedUnit = rangedUnits[i];
			sc2::AvailableAbilities unitAbilities;
			m_bot.Commander().Combat().GetUnitAbilities(rangedUnit, unitAbilities);
			std::thread* t = new std::thread(&RangedManager::HarassLogicForUnit, this, rangedUnit, std::ref(rangedUnits), std::ref(rangedUnitTargets), std::ref(unitAbilities), std::ref(otherSquadsUnits));
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
		for (int i = 0; i < rangedUnits.size(); ++i)
		{
			auto rangedUnit = rangedUnits[i];
			sc2::AvailableAbilities unitAbilities;
			m_bot.Commander().Combat().GetUnitAbilities(rangedUnit, unitAbilities);
			HarassLogicForUnit(rangedUnit, rangedUnits, rangedUnitTargets, unitAbilities, otherSquadsUnits);
		}
	}
	m_bot.StopProfiling("0.10.4.1.5.1        HarassLogicForUnit");
}

void RangedManager::HarassLogicForUnit(const sc2::Unit* rangedUnit, sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets, sc2::AvailableAbilities &rangedUnitAbilities, sc2::Units &otherSquadsUnits)
{
	if (!rangedUnit)
		return;

	const bool isMarine = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE;
	const bool isMarauder = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARAUDER;
	const bool isMedivac = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC;
	const bool isReaper = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER;
	const bool isHellion = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_HELLION;
	const bool isBanshee = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE;
	const bool isRaven = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_RAVEN;
	const bool isViking = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT;
	const bool isCyclone = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_CYCLONE;
	const bool isBattlecruiser = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER;
	const bool isFlyingBarracks = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING;

	auto & unitAction = m_bot.Commander().Combat().GetRangedUnitAction(rangedUnit);
	// Ignore units that are executing a prioritized action
	if (unitAction.prioritized)
		return;

	bool isUnitDisabled = Util::isUnitDisabled(rangedUnit);
	// Sometimes want to give an action only every few frames to allow slow attacks to occur and cliff jumps, but not for disabled BCs (they might have a canceled Yamato)
	if (m_bot.Commander().Combat().ShouldSkipFrame(rangedUnit) && !(isBattlecruiser && isUnitDisabled))
		return;

	if (rangedUnit->is_selected)
		int a = 0;

	if (isCyclone && MonitorCyclone(rangedUnit, rangedUnitAbilities))
		return;

	sc2::Units allCombatAllies(rangedUnits);
	allCombatAllies.insert(allCombatAllies.end(), otherSquadsUnits.begin(), otherSquadsUnits.end());
	
	m_bot.StartProfiling("0.10.4.1.5.1.0          getTarget");
	//TODO Find if filtering higher units would solve problems without creating new ones
	const sc2::Unit * target = getTarget(rangedUnit, rangedUnitTargets, true, true);
	if (!target && m_order.getType() != SquadOrderTypes::Harass)	// If no standard target is found, we check for a building that is not out of vision on higher ground
		target = getTarget(rangedUnit, rangedUnitTargets, true, true, false, false);
	m_bot.StopProfiling("0.10.4.1.5.1.0          getTarget");
	m_bot.StartProfiling("0.10.4.1.5.1.1          getThreats");
	sc2::Units & threats = getThreats(rangedUnit, rangedUnitTargets);
	m_bot.StopProfiling("0.10.4.1.5.1.1          getThreats");

	CCPosition goal = m_order.getPosition();
	std::string goalDescription = "Order";

	const auto & cycloneFlyingHelpers = m_bot.Commander().Combat().getCycloneFlyingHelpers();
	// Check if unit is chosen to be a Cyclone helper
	const auto cycloneFlyingHelperIt = cycloneFlyingHelpers.find(rangedUnit);
	const bool isCycloneHelper = cycloneFlyingHelperIt != cycloneFlyingHelpers.end();
	if (isCycloneHelper)
	{
		const auto helperGoalPosition = cycloneFlyingHelperIt->second.position;
		if (isFlyingBarracks || Util::DistSq(rangedUnit->pos, helperGoalPosition) < 20 * 20)
		{
			goal = helperGoalPosition;
			std::stringstream ss;
			ss << "Helper" << (cycloneFlyingHelperIt->second.goal == ESCORT ? "Escort" : "Track");
			goalDescription = ss.str();
		}
		if (m_bot.Config().DrawHarassInfo)
		{
			m_bot.Map().drawCircle(rangedUnit->pos, 0.75f, sc2::Colors::Purple);
			m_bot.Map().drawLine(rangedUnit->pos, goal, sc2::Colors::Purple);
			m_bot.Map().drawCircle(goal, 0.5f, sc2::Colors::Purple);
		}
	}

	m_bot.StartProfiling("0.10.4.1.5.1.2          ShouldUnitHeal");
	bool unitShouldHeal = m_bot.Commander().Combat().ShouldUnitHeal(rangedUnit);
	if (unitShouldHeal)
	{
		CCPosition healGoal = isReaper ? m_bot.Map().center() : m_bot.RepairStations().getBestRepairStationForUnit(rangedUnit);
		if (healGoal != CCPosition())
		{
			goal = healGoal;
			goalDescription = "Heal";
		}
		else
			Util::DisplayError("RangedManager healGoal is (0, 0)", "", m_bot, false);
		if (isBattlecruiser && Util::DistSq(rangedUnit->pos, goal) > 15.f * 15.f && TeleportBattlecruiser(rangedUnit, goal))
			return;
	}
	else
	{
		if (!isCycloneHelper && (isMedivac || isRaven || ((isMarine || isViking || isHellion) && m_order.getType() != SquadOrderTypes::Defend)))
		{
			goal = GetBestSupportPosition(rangedUnit, allCombatAllies);
			goalDescription = "Support";
		}
		else if (isFlyingBarracks && (m_flyingBarracksShouldReachEnemyRamp || !isCycloneHelper))
		{
			goal = m_bot.Buildings().getEnemyMainRamp();
			goalDescription = "Ramp";
			if (Util::DistSq(rangedUnit->pos, goal) < 5 * 5 || !threats.empty() || (isCycloneHelper && cycloneFlyingHelperIt->second.goal == TRACK))
				m_flyingBarracksShouldReachEnemyRamp = false;
		}
		else if (isViking && !isCycloneHelper && !m_bot.Commander().Combat().hasEnoughVikingsAgainstTempests())
		{
			goal = m_bot.GetStartLocation();
			goalDescription = "VikingStart";
		}
		else if (isMarauder && m_bot.Strategy().getStartingStrategy() == PROXY_MARAUDERS && !m_marauderAttackInitiated)
		{
			goal = m_bot.Bases().getBaseLocations()[1]->getPosition();
			goalDescription = "EnemyNat";
			int groupedMarauders = 0;
			const auto & marauders = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_MARAUDER);
			for (const auto marauder : marauders)
			{
				if (Util::DistSq(marauder, goal) <= 3 * 3)
					++groupedMarauders;
			}
			if (groupedMarauders >= 3)
				m_marauderAttackInitiated = true;
		}
		else if (isBanshee && m_order.getType() == SquadOrderTypes::Harass)
		{
			GetInfiltrationGoalPosition(rangedUnit, goal, goalDescription);
		}
	}
	m_bot.StopProfiling("0.10.4.1.5.1.2          ShouldUnitHeal");

	// If our unit is affected by an Interference Matrix, it should back until the effect wears off
	if (isUnitDisabled)
	{
		goal = m_bot.GetStartLocation();
		goalDescription = "DisabledStart";
		unitShouldHeal = true;
	}
	// If our unit is targeted by an enemy Cyclone's Lock-On ability, it should back until the effect wears off
	else if (Util::isUnitLockedOn(rangedUnit))
	{
		// Banshee in danger should cloak itself
		if (isBanshee && ExecuteBansheeCloakLogic(rangedUnit, true))
		{
			return;
		}

		goal = m_bot.GetStartLocation();
		goalDescription = "LockedOnStart";
		unitShouldHeal = true;
	}

	bool shouldAttack = !isUnitDisabled;
	bool cycloneShouldUseLockOn = false;
	bool cycloneShouldStayCloseToTarget = false;
	if (isCyclone)
	{
		ExecuteCycloneLogic(rangedUnit, isUnitDisabled, unitShouldHeal, shouldAttack, cycloneShouldUseLockOn, cycloneShouldStayCloseToTarget, rangedUnits, threats, rangedUnitTargets, target, goal, goalDescription, rangedUnitAbilities);
	}

	const auto distSqToTarget = target ? Util::DistSq(rangedUnit->pos, target->pos) : 0.f;
	if (isReaper && !unitShouldHeal && !m_bot.Strategy().shouldFocusBuildings() && (!target || distSqToTarget > 10.f * 10.f) && (m_order.getType() == SquadOrderTypes::Harass || m_order.getType() == SquadOrderTypes::Attack))
	{
		const auto enemyStartingBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
		if (enemyStartingBase)
		{
			goal = enemyStartingBase->getPosition();
			goalDescription = "EnemyStart";
		}
	}

	if (!isUnitDisabled && ExecutePrioritizedUnitAbilitiesLogic(rangedUnit, threats, rangedUnitTargets, goal, unitShouldHeal, isCycloneHelper))
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
			bool allyUnitSeesTarget = Util::AllyUnitSeesEnemyUnit(rangedUnit, target, m_bot);
			unitAttackRange = (allyUnitSeesTarget ? 14.f : 10.f) + rangedUnit->radius + target->radius;
		}
		else
			unitAttackRange = Util::GetAttackRangeForTarget(rangedUnit, target, m_bot, true);
		targetInAttackRange = distSqToTarget <= unitAttackRange * unitAttackRange && target->last_seen_game_loop == m_bot.GetGameLoop();

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
	if (shouldAttack && ExecuteThreatFightingLogic(rangedUnit, unitShouldHeal, rangedUnits, rangedUnitTargets, otherSquadsUnits))
	{
		m_bot.StopProfiling("0.10.4.1.5.1.5          ThreatFighting");
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.1.5          ThreatFighting");

	m_bot.StartProfiling("0.10.4.1.5.1.4          ShouldAttackTarget");
	if (shouldAttack && targetInAttackRange && ShouldAttackTarget(rangedUnit, target, threats))
	{
		RangedUnitAction action = RangedUnitAction(MicroActionType::AttackUnit, target, unitShouldHeal, getAttackDuration(rangedUnit, target), "AttackTarget");
		m_bot.Commander().Combat().PlanAction(rangedUnit, action);
		m_bot.StopProfiling("0.10.4.1.5.1.4          ShouldAttackTarget");
		const float damageDealt = isBattlecruiser ? Util::GetDpsForTarget(rangedUnit, target, m_bot) / 22.4f : Util::GetDamageForTarget(rangedUnit, target, m_bot);
		m_bot.Analyzer().increaseTotalDamage(damageDealt, rangedUnit->unit_type);
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.1.4          ShouldAttackTarget");

	m_bot.StartProfiling("0.10.4.1.5.1.6          UnitAbilities");
	// Check if unit can use one of its abilities
	if(!isUnitDisabled && ExecuteUnitAbilitiesLogic(rangedUnit, target, threats, rangedUnitTargets, allCombatAllies, goal, unitShouldHeal, isCycloneHelper, rangedUnitAbilities))
	{
		m_bot.StopProfiling("0.10.4.1.5.1.6          UnitAbilities");
		return;
	}
	m_bot.StopProfiling("0.10.4.1.5.1.6          UnitAbilities");

	bool enemyThreatIsClose = false;
	bool fasterEnemyThreat = false;
	float unitSpeed = Util::getSpeedOfUnit(rangedUnit, m_bot);
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
			enemyThreatIsClose = true;
			if (!fasterEnemyThreat && Util::getSpeedOfUnit(threat, m_bot) > unitSpeed)
				fasterEnemyThreat = true;
		}
		summedFleeVec += GetFleeVectorFromThreat(rangedUnit, threat, fleeVec, dist, threatRange);
	}

	// Banshee is about to get hit, it should cloak itself
	if (isBanshee && enemyThreatIsClose && ExecuteBansheeCloakLogic(rangedUnit, unitShouldHeal))
	{
		return;
	}

	// Opportunistic attack (often on buildings)
	if ((shouldAttack || cycloneShouldUseLockOn) && !fasterEnemyThreat)
	{
		const auto closeTarget = getTarget(rangedUnit, rangedUnitTargets, true, true, true, false);
		if (closeTarget && closeTarget->last_seen_game_loop == m_bot.GetGameLoop() && ShouldAttackTarget(rangedUnit, closeTarget, threats))
		{
			const auto distToCloseTarget = Util::DistSq(rangedUnit->pos, closeTarget->pos);
			const auto range = Util::GetAttackRangeForTarget(rangedUnit, closeTarget, m_bot, true);	// We want to ignore spells so Cyclones won't think they have over 7 range
			if (distToCloseTarget > range * range)
				std::cout << "Opportunistic attack should not be done" << std::endl;
			const auto action = RangedUnitAction(MicroActionType::AttackUnit, closeTarget, false, getAttackDuration(rangedUnit, closeTarget), "OpportunisticAttack");
			m_bot.Commander().Combat().PlanAction(rangedUnit, action);
			const float damageDealt = isBattlecruiser ? Util::GetDpsForTarget(rangedUnit, closeTarget, m_bot) / 22.4f : Util::GetDamageForTarget(rangedUnit, closeTarget, m_bot);
			m_bot.Analyzer().increaseTotalDamage(damageDealt, rangedUnit->unit_type);
			return;
		}
	}

	if (!unitShouldHeal && distSqToTarget < m_order.getRadius() * m_order.getRadius() && (target || !threats.empty()))
	{
		m_bot.StartProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
		m_bot.StartProfiling("0.10.4.1.5.1.7          OffensivePathFinding " + rangedUnit->unit_type.to_string());
		const bool checkInfluence = isCyclone || rangedUnit->weapon_cooldown > 0;	// We might want to get enter the enemy influence a bit to attack our target, but not with Cyclones
		if (cycloneShouldUseLockOn || cycloneShouldStayCloseToTarget || AllowUnitToPathFind(rangedUnit, checkInfluence))
		{
			const CCPosition pathFindEndPos = target && !unitShouldHeal && !isCycloneHelper ? target->pos : goal;
			const bool ignoreInfluence = (cycloneShouldUseLockOn && target) || cycloneShouldStayCloseToTarget;
			const auto targetRange = target ? Util::GetAttackRangeForTarget(target, rangedUnit, m_bot) : 0.f;
			const bool tolerateInfluenceToAttackTarget = targetRange > 0.f && unitAttackRange - targetRange >= 2 && ShouldAttackTarget(rangedUnit, target, threats);
			const auto maxInfluence = (cycloneShouldUseLockOn && target) ? CYCLONE_MAX_INFLUENCE_FOR_LOCKON : tolerateInfluenceToAttackTarget ? MAX_INFLUENCE_FOR_OFFENSIVE_KITING : 0.f;
			const CCPosition secondaryGoal = (!cycloneShouldUseLockOn && !shouldAttack && !ignoreInfluence) ? m_bot.GetStartLocation() : CCPosition();	// Only set for Cyclones with lock-on target (other than Tempest)
			const float maxRange = target ? unitAttackRange : 3.f;
			CCPosition closePositionInPath = Util::PathFinding::FindOptimalPathToTarget(rangedUnit, pathFindEndPos, secondaryGoal, target, maxRange, ignoreInfluence, maxInfluence, m_bot);
			if (closePositionInPath != CCPosition())
			{
				const int actionDuration = rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? REAPER_MOVE_FRAME_COUNT : 0;
				const auto action = RangedUnitAction(MicroActionType::Move, closePositionInPath, unitShouldHeal, actionDuration, "PathfindOffensively");
				m_bot.Commander().Combat().PlanAction(rangedUnit, action);
				m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
				m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding " + rangedUnit->unit_type.to_string());
				return;
			}
			else
			{
				const int delay = target && rangedUnit->weapon_cooldown > 0 ? std::ceil(rangedUnit->weapon_cooldown) : HARASS_PATHFINDING_COOLDOWN_AFTER_FAIL;
				nextPathFindingFrameForUnit[rangedUnit] = m_bot.GetGameLoop() + delay;
			}
		}
		else if (isCyclone && !shouldAttack && !cycloneShouldUseLockOn && target)
		{
			CCPosition movePosition = Util::PathFinding::FindOptimalPathToSaferRange(rangedUnit, target, unitAttackRange, true, m_bot);
			if (movePosition != CCPosition())
			{
				const auto action = RangedUnitAction(MicroActionType::Move, movePosition, unitShouldHeal, 0, "StayInRange");
				m_bot.Commander().Combat().PlanAction(rangedUnit, action);
				m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
				m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding " + rangedUnit->unit_type.to_string());
				return;
			}
		}
		m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding");
		m_bot.StopProfiling("0.10.4.1.5.1.7          OffensivePathFinding " + rangedUnit->unit_type.to_string());
	}

	// if there is no potential target or threat, move to objective
	const bool forceMoveToGoal = isCycloneHelper && isFlyingBarracks && cycloneFlyingHelperIt->second.goal == TRACK;
	if (MoveToGoal(rangedUnit, threats, target, goal, goalDescription, unitShouldHeal, forceMoveToGoal))
	{
		return;
	}

	// Dodge enemy influence
	if (enemyThreatIsClose && Util::PathFinding::GetTotalInfluenceOnTile(Util::GetTilePosition(rangedUnit->pos), rangedUnit, m_bot) > 0.f)
	{
		// Only if our unit is not cloaked and safe
		if (!Util::IsUnitCloakedAndSafe(rangedUnit, m_bot))
		{
			m_bot.StartProfiling("0.10.4.1.5.1.9          DefensivePathfinding");
			// If close to an unpathable position or in danger, use influence map to find safest path
			CCPosition safeTile = Util::PathFinding::FindOptimalPathToSafety(rangedUnit, goal, unitShouldHeal, m_bot);
			if (safeTile != CCPosition())
			{
				const auto action = RangedUnitAction(MicroActionType::Move, safeTile, unitShouldHeal, isReaper ? REAPER_MOVE_FRAME_COUNT : 0, "PathfindFlee");
				m_bot.Commander().Combat().PlanAction(rangedUnit, action);
				m_bot.StopProfiling("0.10.4.1.5.1.9          DefensivePathfinding");
				return;
			}
		}
	}

	m_bot.StartProfiling("0.10.4.1.5.1.8          PotentialFields");
	const bool unitShouldBack = unitShouldHeal || !m_bot.Commander().Combat().winAttackSimulation();
	CCPosition dirVec = GetDirectionVectorTowardsGoal(rangedUnit, target, goal, targetInAttackRange, unitShouldBack);

	// Sum up the threats vector with the direction vector
	if (!threats.empty())
	{
		// Only if our unit is not cloaked and safe
		if (!Util::IsUnitCloakedAndSafe(rangedUnit, m_bot))
		{
			AdjustSummedFleeVec(summedFleeVec);
			dirVec += summedFleeVec;
		}
	}

	// We repulse the Reaper from our closest harass unit
	if (isReaper)
	{
		dirVec += GetRepulsionVectorFromFriendlyReapers(rangedUnit, rangedUnits);
	}
	// We attract the ranged unit towards our other close ranged units of same type
	else if (isHellion || isViking || isMarauder)
	{
		dirVec += GetAttractionVectorToFriendlyUnits(rangedUnit, rangedUnits);
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

		const auto action = RangedUnitAction(Move, pathableTile, unitShouldHeal, isReaper ? REAPER_MOVE_FRAME_COUNT : 0, "PotentialFields");
		m_bot.Commander().Combat().PlanAction(rangedUnit, action);
		m_bot.StopProfiling("0.10.4.1.5.1.8          PotentialFields");
		return;
	}
	
	// Unit cannot flee, we let it attack any close unit
	/*std::stringstream ss;
	ss << "Unit " << sc2::UnitTypeToName(rangedUnit->unit_type) << " cannot flee";
	Util::Log(__FUNCTION__, ss.str(), m_bot);*/
	const auto actionType = m_bot.Data(rangedUnit->unit_type).isBuilding ? Move : AttackMove;
	const auto action = RangedUnitAction(actionType, rangedUnit->pos, false, 0, "LastResort");
	m_bot.Commander().Combat().PlanAction(rangedUnit, action);
	m_bot.StopProfiling("0.10.4.1.5.1.8          PotentialFields");
}

void RangedManager::GetInfiltrationGoalPosition(const sc2::Unit * rangedUnit, CCPosition & goal, std::string & goalDescription) const
{
	const auto topY = m_bot.Map().mapMax().y;
	const auto rightX = m_bot.Map().mapMax().x;
	const auto bottomY = m_bot.Map().mapMin().y;
	const auto leftX = m_bot.Map().mapMin().x;
	const float unitToGoalDist = Util::DistSq(rangedUnit->pos, goal);
	// Compute distance from unit to closest edge
	std::list<std::pair<float, int>> unitDistanceToEdges = {
		std::make_pair(Util::DistSq(rangedUnit->pos, CCPosition(rangedUnit->pos.x, topY)), 0),	// top
		std::make_pair(Util::DistSq(rangedUnit->pos, CCPosition(rightX, rangedUnit->pos.y)), 1),	// right
		std::make_pair(Util::DistSq(rangedUnit->pos, CCPosition(rangedUnit->pos.x, bottomY)), 2),	// bottom
		std::make_pair(Util::DistSq(rangedUnit->pos, CCPosition(leftX, rangedUnit->pos.y)), 3)	// left
	};
	unitDistanceToEdges.sort();
	// Compute distance from goal to closest edge
	std::list<std::pair<float, int>> goalDistanceToEdges = {
		std::make_pair(Util::DistSq(goal, CCPosition(goal.x, topY)), 0),	// top
		std::make_pair(Util::DistSq(goal, CCPosition(rightX, goal.y)), 1),	// right
		std::make_pair(Util::DistSq(goal, CCPosition(goal.x, bottomY)), 2),	// bottom
		std::make_pair(Util::DistSq(goal, CCPosition(leftX, goal.y)), 3)	// left
	};
	goalDistanceToEdges.sort();

	int i = 0;
	int closeEdge = -1;
	float goalDistanceToCloseEdge = 0;
	for (const auto & goalDistanceToEdge : goalDistanceToEdges)
	{
		if (i++ == 2 || closeEdge >= 0)
			break;
		for (const auto & unitDistanceToEdge : unitDistanceToEdges)
		{
			if (unitDistanceToEdge.second == goalDistanceToEdge.second)
			{
				if (unitDistanceToEdge.first <= goalDistanceToEdge.first)
				{
					closeEdge = goalDistanceToEdge.second;
					goalDistanceToCloseEdge = goalDistanceToEdge.first;
				}
				break;
			}
		}
	}
	
	if (closeEdge >= 0)
	{
		if (unitToGoalDist <= goalDistanceToCloseEdge * 2)
		{
			// Our unit is getting close to the goal, it should go directly towards the goal
		}
		else
		{
			// Our unit is close to an edge that is also close to the goal, so it should continue on that edge while getting closer to the goal
			if (closeEdge == 0)
				goal = CCPosition(goal.x, topY);
			else if (closeEdge == 1)
				goal = CCPosition(rightX, goal.y);
			else if (closeEdge == 2)
				goal = CCPosition(goal.x, bottomY);
			else
				goal = CCPosition(leftX, goal.y);
		}
	}
	else if (unitDistanceToEdges.front().first <= 5 * 5)
	{
		// Our unit is close to an edge, it should keep close to the edge until it is close enough to the goal
		const auto topLeft = CCPosition(leftX, topY);
		const auto topRight = CCPosition(rightX, topY);
		const auto bottomLeft = CCPosition(leftX, bottomY);
		const auto bottomRight = CCPosition(rightX, bottomY);
		std::list<std::pair<float, int>> combinedDistanceToCorners = {
			std::make_pair(Util::DistSq(rangedUnit->pos, topLeft) + Util::DistSq(goal, topLeft), 0),
			std::make_pair(Util::DistSq(rangedUnit->pos, topRight) + Util::DistSq(goal, topRight), 1),
			std::make_pair(Util::DistSq(rangedUnit->pos, bottomLeft) + Util::DistSq(goal, bottomLeft), 2),
			std::make_pair(Util::DistSq(rangedUnit->pos, bottomRight) + Util::DistSq(goal, bottomRight), 3)
		};
		combinedDistanceToCorners.sort();
		const int closestCornerId = combinedDistanceToCorners.front().second;
		const CCPosition closestCorner = closestCornerId == 0 ? topLeft : closestCornerId == 1 ? topRight : closestCornerId == 2 ? bottomLeft : bottomRight;
		goal = closestCorner;
	}
	else
	{
		// Our unit is far from the goal and far from an edge, so it should get closer to an edge
		const auto towardsGoal = goal - rangedUnit->pos;
		CCPosition clockwiseOrthogonal, counterClockwiseOrthogonal;
		Util::GetOrthogonalVectors(towardsGoal, clockwiseOrthogonal, counterClockwiseOrthogonal);
		const auto slope = clockwiseOrthogonal.x == 0 ? std::numeric_limits<float>::max() : clockwiseOrthogonal.y / clockwiseOrthogonal.x;	// m = delta_y / delta_x
		const auto intercept = rangedUnit->pos.y - slope * rangedUnit->pos.x;	// b = y - mx
		float minDist = -1;
		CCPosition closestMapBorderPosition;
		// Calculate the interceptions with map borders
		if (slope != 0)
		{
			// x = (y - b) / m
			const auto bottomX = (bottomY - intercept) / slope;
			const auto bottomPosition = CCPosition(bottomX, bottomY);
			const auto distToBottom = Util::DistSq(rangedUnit->pos, bottomPosition);
			minDist = distToBottom;
			closestMapBorderPosition = bottomPosition;
			const auto topX = (topY - intercept) / slope;
			const auto topPosition = CCPosition(topX, topY);
			const auto distToTop = Util::DistSq(rangedUnit->pos, topPosition);
			if (distToTop < minDist)
			{
				minDist = distToTop;
				closestMapBorderPosition = topPosition;
			}
		}
		if (slope != std::numeric_limits<float>::max())
		{
			// y = mx + b
			const auto leftY = slope * leftX + intercept;
			const auto leftPosition = CCPosition(leftX, leftY);
			const auto distToLeft = Util::DistSq(rangedUnit->pos, leftPosition);
			if (distToLeft < minDist)
			{
				minDist = distToLeft;
				closestMapBorderPosition = leftPosition;
			}
			const auto rightY = slope * rightX + intercept;
			const auto rightPosition = CCPosition(rightX, rightY);
			const auto distToRight = Util::DistSq(rangedUnit->pos, rightPosition);
			if (distToRight < minDist)
			{
				minDist = distToRight;
				closestMapBorderPosition = rightPosition;
			}
		}
		goal = closestMapBorderPosition;
	}
	goalDescription = "Infiltrate";
	m_bot.Map().drawLine(rangedUnit->pos, goal);
	m_bot.Map().drawCircle(goal, 0.5);
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
		const auto action = RangedUnitAction(MicroActionType::ToggleAbility, sc2::ABILITY_ID::EFFECT_LOCKON, true, 0, "ToggleLockOn");
		m_bot.Commander().Combat().PlanAction(cyclone, action);
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
				//if (QueryIsAbilityAvailable(cyclone, sc2::ABILITY_ID::EFFECT_LOCKON))
				if (Util::IsAbilityAvailable(sc2::ABILITY_ID::EFFECT_LOCKON, abilities))
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

bool RangedManager::IsCycloneLockOnCanceled(const sc2::Unit * cyclone, bool started, const sc2::AvailableAbilities & abilities) const
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

	// The target is disabled by an Interference Matrix
	if (Util::isUnitDisabled(cyclone))
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
	if (checkInfluence && Util::PathFinding::HasInfluenceOnTile(Util::GetTilePosition(rangedUnit->pos), rangedUnit->is_flying, m_bot))
		return false;
	const uint32_t availableFrame = nextPathFindingFrameForUnit.find(rangedUnit) != nextPathFindingFrameForUnit.end() ? nextPathFindingFrameForUnit.at(rangedUnit) : m_bot.GetGameLoop();
	return m_bot.GetGameLoop() >= availableFrame;
}

bool RangedManager::ShouldBansheeCloak(const sc2::Unit * banshee, bool inDanger) const
{
	if (!m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::BANSHEECLOAK))
		return false;

	if (Util::isUnitDisabled(banshee))
		return false;

	// Cloak if the amount of energy is rather high or HP is low
	return banshee->cloak == sc2::Unit::NotCloaked && (banshee->energy > 50.f || inDanger && banshee->energy > 25.f) && !Util::IsPositionUnderDetection(banshee->pos, m_bot);
}

bool RangedManager::ExecuteBansheeCloakLogic(const sc2::Unit * banshee, bool inDanger)
{
	if (ShouldBansheeCloak(banshee, inDanger))
	{
		const auto action = RangedUnitAction(MicroActionType::Ability, sc2::ABILITY_ID::BEHAVIOR_CLOAKON, true, 0, "CloakOn");
		m_bot.Commander().Combat().PlanAction(banshee, action);
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
		const auto action = RangedUnitAction(MicroActionType::Ability, sc2::ABILITY_ID::BEHAVIOR_CLOAKOFF, true, 0, "CloakOff");
		m_bot.Commander().Combat().PlanAction(banshee, action);
		return true;
	}
	return false;
}

bool RangedManager::TeleportBattlecruiser(const sc2::Unit * battlecruiser, CCPosition location)
{
	// If the teleport ability is not on cooldown
	if (isAbilityAvailable(sc2::ABILITY_ID::EFFECT_TACTICALJUMP, battlecruiser))
	{
		const auto action = RangedUnitAction(MicroActionType::AbilityPosition, sc2::ABILITY_ID::EFFECT_TACTICALJUMP, location, true, BATTLECRUISER_TELEPORT_FRAME_COUNT, "TacticalJump");
		m_bot.Commander().Combat().PlanAction(battlecruiser, action);
		setNextFrameAbilityAvailable(sc2::ABILITY_ID::EFFECT_TACTICALJUMP, battlecruiser, m_bot.GetCurrentFrame() + BATTLECRUISER_TELEPORT_COOLDOWN_FRAME_COUNT);
		m_bot.StopProfiling("0.10.4.1.5.1.2          ShouldUnitHeal");
		return true;
	}

	return false;
}

CCPosition RangedManager::GetBestSupportPosition(const sc2::Unit* supportUnit, const sc2::Units & rangedUnits) const
{
	const bool isMarine = supportUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE;
	const bool isMedivac = supportUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC;
	const bool isRaven = supportUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_RAVEN;
	const std::vector<sc2::UNIT_TYPEID> typesToIgnore = supportTypes;
	std::vector<sc2::UNIT_TYPEID> typesToConsider;
	if (isMarine)
	{
		typesToConsider = { sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER };
	}
	else if (isMedivac)
	{
		typesToConsider = {
			sc2::UNIT_TYPEID::TERRAN_MARINE,
			sc2::UNIT_TYPEID::TERRAN_MARAUDER,
			sc2::UNIT_TYPEID::TERRAN_GHOST,
			sc2::UNIT_TYPEID::TERRAN_HELLIONTANK
		};
	}
	const auto clusterQueryName = "";// isRaven ? "Raven" : "";
	sc2::Units validUnits;
	for (const auto rangedUnit : rangedUnits)
	{
		if (m_bot.Commander().Combat().ShouldUnitHeal(rangedUnit))
			continue;
		if (!typesToConsider.empty() && !Util::Contains(rangedUnit->unit_type, typesToConsider))
			continue;
		validUnits.push_back(rangedUnit);
	}
	const auto clusters = Util::GetUnitClusters(validUnits, typesToIgnore, true, clusterQueryName, m_bot);
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
	if (m_order.getType() == SquadOrderTypes::Defend || m_bot.GetCurrentSupply() >= 195)
	{
		return m_order.getPosition();
	}
	return m_bot.GetStartLocation();
}

bool RangedManager::ExecuteVikingMorphLogic(const sc2::Unit * viking, CCPosition goal, const sc2::Unit* target, sc2::Units & threats, sc2::Units & targets, bool unitShouldHeal, bool isCycloneHelper)
{
	bool morph = false;
	sc2::AbilityID morphAbility = 0;
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
			const auto vikingFighter = GetSimulatedUnit(viking);
			const auto flyingTarget = getTarget(vikingFighter, targets, true);
			if (flyingTarget || (!target && airInfluence <= groundInfluence))
			{
				morphAbility = sc2::ABILITY_ID::MORPH_VIKINGFIGHTERMODE;
				morph = true;
			}
		}
	}
	if(morph)
	{
		const auto action = RangedUnitAction(MicroActionType::Ability, morphAbility, true, VIKING_MORPH_FRAME_COUNT, "Morph");
		morph = m_bot.Commander().Combat().PlanAction(viking, action);
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
		const auto action = RangedUnitAction(MicroActionType::Ability, morphAbility, true, THOR_MORPH_FRAME_COUNT, "Morph");
		morph = m_bot.Commander().Combat().PlanAction(thor, action);
	}
	return morph;
}

bool RangedManager::MoveToGoal(const sc2::Unit * rangedUnit, sc2::Units & threats, const sc2::Unit * target, CCPosition goal, std::string goalDescription, bool unitShouldHeal, bool force)
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
		std::stringstream actionDescription;
		actionDescription << "MoveToGoal" << goalDescription;
		const auto action = RangedUnitAction(moveWithoutAttack ? MicroActionType::Move : MicroActionType::AttackMove, goal, unitShouldHeal, actionDuration, actionDescription.str());
		m_bot.Commander().Combat().PlanAction(rangedUnit, action);
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

	if (Util::IsEnemyHiddenOnHighGround(rangedUnit, target, m_bot))
		return false;

	return true;
}

bool RangedManager::IsInRangeOfSlowerUnit(const sc2::Unit * rangedUnit, const sc2::Unit * target) const
{
	const float targetSpeed = Util::getSpeedOfUnit(target, m_bot);
	const float rangedUnitSpeed = Util::getSpeedOfUnit(rangedUnit, m_bot);
	const float speedDiff = targetSpeed - rangedUnitSpeed;
	if (speedDiff >= 0.f)
	{
		// We don't want to attack closeby ground enemies if our unit can flee over cliffs
		if (target->is_flying || (!rangedUnit->is_flying && rangedUnit->unit_type != sc2::UNIT_TYPEID::TERRAN_REAPER))
			return false;	// Target is faster (or equal)
	}
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

CCPosition RangedManager::GetDirectionVectorTowardsGoal(const sc2::Unit * rangedUnit, const sc2::Unit * target, CCPosition goal, bool targetInAttackRange, bool unitShouldBack) const
{
	CCPosition dirVec(0.f, 0.f);
	if (target && !unitShouldBack)
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

const sc2::Unit * RangedManager::ExecuteLockOnLogic(const sc2::Unit * cyclone, bool shouldHeal, bool & shouldAttack, bool & shouldUseLockOn, bool & lockOnAvailable, const sc2::Units & rangedUnits, const sc2::Units & threats, const sc2::Units & rangedUnitTargets, const sc2::Unit * target, sc2::AvailableAbilities & abilities)
{
	const uint32_t currentFrame = m_bot.GetCurrentFrame();
	auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
	//lockOnAvailable = QueryIsAbilityAvailable(cyclone, sc2::ABILITY_ID::EFFECT_LOCKON);
	//lockOnAvailable = isAbilityAvailable(sc2::ABILITY_ID::EFFECT_LOCKON, cyclone);
	lockOnAvailable = Util::IsAbilityAvailable(sc2::ABILITY_ID::EFFECT_LOCKON, abilities);
	if (lockOnAvailable)
	{
		// The Cyclone could attack, but it should use its lock-on ability
		shouldAttack = false;
		shouldUseLockOn = true;
	}
	else
	{
		// Lock-On ability is not not available (maybe in use, maybe in cooldown)
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
		else if (m_bot.Config().DrawHarassInfo)
		{
			auto & nextAvailableAbility = m_bot.Commander().Combat().getNextAvailableAbility();
			m_bot.Map().drawCircle(cyclone->pos, float(nextAvailableAbility[sc2::ABILITY_ID::EFFECT_LOCKON][cyclone] - currentFrame) / CYCLONE_LOCKON_COOLDOWN_FRAME_COUNT, sc2::Colors::Red);
		}
	}

	// Check if the Cyclone would have a better Lock-On target
	if (shouldUseLockOn)
	{
		if (!rangedUnitTargets.empty())
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
			for(const auto potentialTarget : rangedUnitTargets)
			{
				// Do not Lock On on workers
				if (UnitType(potentialTarget->unit_type, m_bot).isWorker())
					continue;
				// Do not Lock On on units that are already Locked On unless they have a lot of hp
				// Will target the unit only if it can absorb more than 3 missiles (20 damage each) per Cyclone Locked On to it
				const auto it = lockedOnTargets.find(potentialTarget);
				if (it != lockedOnTargets.end() && potentialTarget->health + potentialTarget->shield <= it->second * 60)
					continue;
				const float threatHeight = m_bot.Map().terrainHeight(potentialTarget->pos);
				if (threatHeight > cycloneHeight)
				{
					bool hasGoodViewOfUnit = false;
					for (const auto allyUnitPair : m_bot.GetAllyUnits())
					{
						const auto allyUnit = allyUnitPair.second.getUnitPtr();
						if (allyUnit == cyclone)
							continue;
						const auto canSeeEnemy = Util::AllyUnitSeesEnemyUnit(allyUnit, potentialTarget, m_bot);
						if(canSeeEnemy)
						{
							hasGoodViewOfUnit = true;
							break;
						}
					}
					if(!hasGoodViewOfUnit)
						continue;
				}
				const float dist = Util::Dist(cyclone->pos, potentialTarget->pos) - potentialTarget->radius;
				if (shouldHeal && dist > partialLockOnRange + potentialTarget->radius)
					continue;
				if (potentialTarget->display_type == sc2::Unit::Hidden)
					continue;
				// The lower the better
				const auto distanceScore = std::pow(std::max(0.f, dist - 2), 3.f);
				const auto healthScore = 0.25f * (potentialTarget->health + potentialTarget->shield * 1.5f);
				// The higher the better
				const auto energyScore = 0.25f * potentialTarget->energy;
				const auto energyLessSpellcasterScore = 15 * (potentialTarget->unit_type == sc2::UNIT_TYPEID::TERRAN_LIBERATORAG || potentialTarget->unit_type == sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR);
				const auto detectorScore = 15 * (potentialTarget->detect_range > 0.f);
				const auto threatRange = Util::GetAttackRangeForTarget(potentialTarget, cyclone, m_bot);
				const auto threatDps = Util::GetDpsForTarget(potentialTarget, cyclone, m_bot);
				const float powerScore = threatRange * threatDps * 1.5f;
				const float speedScore = Util::getSpeedOfUnit(potentialTarget, m_bot) * 6.f;
				auto armoredScore = 0.f;
				if(m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::CYCLONELOCKONDAMAGEUPGRADE))
				{
					const sc2::UnitTypeData unitTypeData = Util::GetUnitTypeDataFromUnitTypeId(potentialTarget->unit_type, m_bot);
					armoredScore = 15 * Util::Contains(sc2::Attribute::Armored, unitTypeData.attributes);
				}
				const float nydusBonus = potentialTarget->unit_type == sc2::UNIT_TYPEID::ZERG_NYDUSCANAL && potentialTarget->build_progress < 1.f ? 10000.f : 0.f;
				const float score = energyScore + energyLessSpellcasterScore + detectorScore + armoredScore + powerScore + speedScore + nydusBonus - healthScore - distanceScore;
				if(!bestTarget || score > bestScore)
				{
					bestTarget = potentialTarget;
					bestScore = score;
				}
			}
			if (bestTarget)
			{
				target = bestTarget;
			}
			else
			{
				shouldUseLockOn = false;
				shouldAttack = true;
			}
		}

		if(target)
		{
			const auto type = UnitType(target->unit_type, m_bot);
			// Prevent the use of Lock-On on passive buildings
			if (type.isWorker() || (type.isBuilding() && !type.isCombatUnit()))
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
	const auto action = RangedUnitAction(MicroActionType::AbilityTarget, sc2::ABILITY_ID::EFFECT_LOCKON, target, true, 0, "LockOn");
	m_bot.Commander().Combat().PlanAction(cyclone, action);
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

bool RangedManager::ExecuteThreatFightingLogic(const sc2::Unit * rangedUnit, bool unitShouldHeal, sc2::Units & rangedUnits, sc2::Units & rangedUnitTargets, sc2::Units & otherSquadsUnits)
{
	if (rangedUnitTargets.empty())
		return false;

	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_CYCLONE)
		return false;

	float unitsPower = 0.f;
	float stimedUnitsPowerDifference = 0.f;
	float targetsPower = 0.f;
	bool morphFlyingVikings = false;
	bool morphLandedVikings = false;
	bool useStim = false;
	std::map<const sc2::Unit *, const sc2::Unit*> simulatedStimedUnits;
	std::map<const sc2::Unit*, const sc2::Unit*> closeUnitsTarget;

	// The harass mode deactivation is to not ignore ranged targets
	const sc2::Unit* target = getTarget(rangedUnit, rangedUnitTargets, false);
	const auto & cycloneFlyingHelpers = m_bot.Commander().Combat().getCycloneFlyingHelpers();
	const auto vikingCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Viking.getUnitType(), false, true);
	// If the Viking that is not a flying helper has no target, we try to see if it would have one if it was landed
	if (!target && rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
	{
		if (!m_bot.Analyzer().enemyHasCombatAirUnit() || vikingCount >= 40)
		{
			if (cycloneFlyingHelpers.find(rangedUnit) == cycloneFlyingHelpers.end())
			{
				rangedUnit = GetSimulatedUnit(rangedUnit);
				target = getTarget(rangedUnit, rangedUnitTargets, false);
				if (target)
				{
					morphFlyingVikings = true;
				}
				/*else if (vikingCount >= 20)
					Util::Log(__FUNCTION__, "Vikings won't land - no target", m_bot);*/
			}
			/*else if (vikingCount >= 20)
				Util::Log(__FUNCTION__, "Vikings won't land - flying helper", m_bot);*/
		}
		/*else if (vikingCount >= 20)
			Util::Log(__FUNCTION__, "Vikings won't land - enemy air combat units", m_bot);*/
	}
	else if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
	{
		const auto simulatedUnit = GetSimulatedUnit(rangedUnit);
		const auto flyingTarget = getTarget(simulatedUnit, rangedUnitTargets, false);
		if (flyingTarget)
		{
			rangedUnit = simulatedUnit;
			target = flyingTarget;
			morphLandedVikings = true;
		}
	}
	const float range = Util::GetAttackRangeForTarget(rangedUnit, target, m_bot);
	const bool closeToEnemyTempest = target && target->unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST && Util::DistSq(rangedUnit->pos, target->pos) <= range * range;
	if (!target || (!isTargetRanged(target) && !morphFlyingVikings))
	{
		return false;
	}

	const float targetDist = Util::Dist(rangedUnit->pos, target->pos);
	if (Util::IsEnemyHiddenOnHighGround(rangedUnit, target, m_bot))
	{
		// Check if the unit cannot just walk the ramp to reach the enemy
		bool easilyWalkable = false;
		if (targetDist <= 16 && AllowUnitToPathFind(rangedUnit, false))
		{
			const auto pathDistance = Util::PathFinding::FindOptimalPathDistance(rangedUnit, target->pos, true, m_bot);
			if (pathDistance >= 0 && pathDistance <= 15)
				easilyWalkable = true;
		}
		if (!easilyWalkable)
			return false;
	}

	// Check if unit can fight cloaked
	if(rangedUnit->energy >= 5 && (rangedUnit->cloak == sc2::Unit::CloakedAllied || (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE && ShouldBansheeCloak(rangedUnit, false))))
	{
		// If the unit is at an undetected position
		if (!Util::IsPositionUnderDetection(rangedUnit->pos, m_bot))
		{
			bool canFightCloaked = true;
			if (targetDist > range)
			{
				const CCPosition closestAttackPosition = rangedUnit->pos + Util::Normalized(target->pos - rangedUnit->pos) * (targetDist - range);
				canFightCloaked = !Util::IsPositionUnderDetection(closestAttackPosition, m_bot);
			}
			if(canFightCloaked)
			{
				if (Util::PathFinding::HasCombatInfluenceOnTile(Util::GetTilePosition(rangedUnit->pos), rangedUnit, m_bot) && ExecuteBansheeCloakLogic(rangedUnit, false))
				{
					return true;
				}

				// If the unit is standing on effect influence, get it out of there before fighting
				if (Util::PathFinding::GetEffectInfluenceOnTile(Util::GetTilePosition(rangedUnit->pos), rangedUnit, m_bot) > 0.f)
				{
					CCPosition movePosition = Util::PathFinding::FindOptimalPathToDodgeEffectAwayFromGoal(rangedUnit, target->pos, range, m_bot);
					if (movePosition != CCPosition())
					{
						const auto action = RangedUnitAction(MicroActionType::Move, movePosition, true, 0, "DodgeEffect");
						// Move away from the effect
						m_bot.Commander().Combat().PlanAction(rangedUnit, action);
						return true;
					}
				}

				const bool canAttackNow = range - 0.1f >= targetDist && rangedUnit->weapon_cooldown <= 0.f;	// Added a small buffer on the attack to allow Banshees to hit fleeing units
				bool skipAction = false;
				RangedUnitAction action;
				if (canAttackNow)
				{
					// Attack the target
					const int attackDuration = getAttackDuration(rangedUnit, target);
					action = RangedUnitAction(MicroActionType::AttackUnit, target, false, attackDuration, "AttackThreatCloaked");
				}
				else
				{
					const CCPosition closeAttackPosition = rangedUnit->pos + Util::Normalized(target->pos - rangedUnit->pos) * 0.5f;
					if (Util::IsPositionUnderDetection(closeAttackPosition, m_bot))
					{
						// Our unit would move into a detection zone if it continues moving towards its target, so we skip the action
						skipAction = true;
					}
					else
					{
						// Move towards the target
						action = RangedUnitAction(MicroActionType::Move, target->pos, false, 0, "MoveTowardsThreatCloaked");
					}
				}
				if (!skipAction)
				{
					m_bot.Commander().Combat().PlanAction(rangedUnit, action);
					return true;
				}
			}
		}
	}

	// Check for saved result
	for (const auto & combatSimulationResultPair : m_combatSimulationResults)
	{
		const auto & allyUnits = combatSimulationResultPair.first;
		if (Util::Contains(rangedUnit, allyUnits))
		//if (allyUnits == closeUnitsSet)
		{
			bool savedResult = combatSimulationResultPair.second;
			if (savedResult)
			{
				auto action = m_bot.Commander().Combat().GetRangedUnitAction(rangedUnit);
				std::stringstream ss;
				ss << "ThreatFightingLogic was called again when all close units should have been given a prioritized action... Current unit of type " << sc2::UnitTypeToName(rangedUnit->unit_type) << " had a " << action.description << " action and is " << (Util::Contains(rangedUnit, allyUnits) ? "" : "not") << " part of the set";
				Util::Log(__FUNCTION__, ss.str(), m_bot);
			}
			return savedResult;
		}
	}
	
	m_bot.StartProfiling("0.10.4.1.5.1.5.1          CalcCloseUnits");
	float minUnitRange = -1;
	// We create a set because we need an ordered data structure for accurate and efficient comparison with data in memory
	std::set<const sc2::Unit *> closeUnitsSet;
	sc2::Units allyCombatUnits;
	allyCombatUnits.insert(allyCombatUnits.end(), rangedUnits.begin(), rangedUnits.end());
	allyCombatUnits.insert(allyCombatUnits.end(), otherSquadsUnits.begin(), otherSquadsUnits.end());
	CalcCloseUnits(rangedUnit, target, allyCombatUnits, rangedUnitTargets, true, closeUnitsSet, morphFlyingVikings, morphLandedVikings, simulatedStimedUnits, stimedUnitsPowerDifference, closeUnitsTarget, unitsPower, minUnitRange);
	m_bot.StopProfiling("0.10.4.1.5.1.5.1          CalcCloseUnits");

	if (closeUnitsSet.empty() || !Util::Contains(rangedUnit, closeUnitsSet))
	{
		return false;
	}

	if (closeUnitsSet.size() > 1 && unitShouldHeal && !closeToEnemyTempest)
	{
		return false;
	}

	sc2::Units closeUnits;
	for (const auto closeUnit : closeUnitsSet)
		closeUnits.push_back(closeUnit);

	m_bot.StartProfiling("0.10.4.1.5.1.5.2          CalcThreats");
	// Calculate all the threats of all the ally units participating in the fight
	std::set<const sc2::Unit *> allThreatsSet;
	for (const auto allyUnit : closeUnits)
	{
		const auto & allyUnitThreats = getThreats(allyUnit, rangedUnitTargets);
		for (const auto threat : allyUnitThreats)
			allThreatsSet.insert(threat);
	}
	// Add enemies that are close to the threats
	for (const auto enemy : rangedUnitTargets)
	{
		if (Util::Contains(enemy, allThreatsSet))
			continue;
		bool closeToThreat = false;
		bool isShieldBattery = enemy->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY;
		if (isShieldBattery && (!enemy->is_powered || enemy->build_progress < 1.f))
			continue;
		for (const auto threat : allThreatsSet)
		{
			if (isShieldBattery)
			{
				if (threat->unit_type != sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY && Util::Dist(threat->pos, enemy->pos) <= 6.f + enemy->radius + threat->radius)
				{
					allThreatsSet.insert(enemy);
					break;
				}
			}
			else if (Util::DistSq(threat->pos, enemy->pos) <= 3.f * 3.f)
			{
				closeToThreat = true;
				break;
			}
		}
		if (!closeToThreat || isShieldBattery)
			continue;
		const auto enemyTarget = getTarget(enemy, allyCombatUnits, false);
		if (!enemyTarget)
			continue;
		const auto threatRange = Util::getThreatRange(enemyTarget, enemy, m_bot) + 3.f;
		if (Util::DistSq(enemy->pos, enemyTarget->pos) <= threatRange * threatRange)
			allThreatsSet.insert(enemy);
	}
	sc2::Units allThreats;
	for (const auto threat : allThreatsSet)
	{
		allThreats.push_back(threat);
	}
	m_bot.StopProfiling("0.10.4.1.5.1.5.2          CalcThreats");

	m_bot.StartProfiling("0.10.4.1.5.1.5.3          CalcThreatsPower");
	float maxThreatSpeed = 0.f;
	float maxThreatRange = 0.f;
	sc2::Units threatsToKeep;
	// Calculate enemy power
	for (auto threat : allThreats)
	{
		if(threat->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
		{
			continue;
		}
		const float threatSpeed = Util::getSpeedOfUnit(threat, m_bot);
		if (threatSpeed > maxThreatSpeed)
			maxThreatSpeed = threatSpeed;
		const sc2::Unit* threatTarget = getTarget(threat, closeUnits, false);
		if (!threatTarget)
			continue;
		const float threatRange = Util::GetAttackRangeForTarget(threat, threatTarget, m_bot);
		const float threatDistance = threatTarget ? Util::Dist(threat->pos, threatTarget->pos) : 0.f;
		// If the building threat is too far from its target to attack it (with a very small buffer)
		if (Unit(threat, m_bot).getType().isBuilding() && threatTarget && threatDistance > threatRange + 0.01f)
		{
			bool unitWillGetCloseEnough = false;
			// Simulate the future position of our units to check if they would be in range of the building
			for (const auto unitTargetPair : closeUnitsTarget)
			{
				const auto unit = unitTargetPair.first;
				const auto unitTarget = unitTargetPair.second;
				if (Util::IsEnemyHiddenOnHighGround(unit, unitTarget, m_bot))
				{
					unitWillGetCloseEnough = true;
					break;
				}
				const float unitRange = Util::GetAttackRangeForTarget(unit, unitTarget, m_bot);
				const CCPosition futurePosition = unitTarget->pos + Util::Normalized(unit->pos - unitTarget->pos) * unitRange;
				if (Util::Dist(threat->pos, futurePosition) <= threatRange)
				{
					unitWillGetCloseEnough = true;
					break;
				}
			}
			if (!unitWillGetCloseEnough)
				continue;
		}
		if (threatRange > maxThreatRange)
			maxThreatRange = threatRange;
		targetsPower += Util::GetUnitPower(threat, threatTarget, m_bot);
		threatsToKeep.push_back(threat);
	}
	m_bot.StopProfiling("0.10.4.1.5.1.5.3          CalcThreatsPower");

	// If our units have 2 more range, they should kite, not trade
	if (!morphFlyingVikings && minUnitRange - maxThreatRange >= 2.f)
	{
		m_combatSimulationResults[closeUnitsSet] = false;
		return false;
	}

	const bool enemyHasLongRangeUnits = maxThreatRange >= 10;

	sc2::Units vikings;
	sc2::Units tempests;
	int injuredVikings = 0;
	int injuredTempests = 0;
	for (const auto ally : closeUnits)
	{
		if (ally->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
		{
			vikings.push_back(ally);
			if (ally->health < ally->health_max)
				++injuredVikings;
		}
	}
	for (const auto threat : threatsToKeep)
	{
		if (threat->unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST)
		{
			tempests.push_back(threat);
			if (threat->health < threat->health_max || threat->shield < threat->shield_max)
				++injuredTempests;
		}
	}

	// If we can beat the enemy
	m_bot.StartProfiling("0.10.4.1.5.1.5.4          SimulateCombat");
	float simulationResult = Util::SimulateCombat(closeUnits, threatsToKeep, m_bot);
	float minDesiredOutcome = 0.f;
	if (m_order.getType() == SquadOrderTypes::Harass)
	{
		minDesiredOutcome = 0.75f;
		for (const auto closeUnit : closeUnits)
		{
			if (Util::Contains(closeUnit, otherSquadsUnits))
			{
				minDesiredOutcome = 0.f;
				break;
			}
		}
	}
	bool winSimulation = simulationResult > minDesiredOutcome;
	bool formulaWin = unitsPower >= targetsPower;
	bool shouldFight = winSimulation && formulaWin;

	if (!simulatedStimedUnits.empty())
	{
		sc2::Units units;
		for (const auto closeUnit : closeUnitsSet)
		{
			const auto it = simulatedStimedUnits.find(closeUnit);
			if (it != simulatedStimedUnits.end())
				units.push_back(it->second);
			else
				units.push_back(closeUnit);
		}
		const float stimedSimulationResult = Util::SimulateCombat(closeUnits, units, threatsToKeep, m_bot);
		if (stimedSimulationResult > simulationResult && (enemyHasLongRangeUnits || unitsPower + stimedUnitsPowerDifference >= targetsPower))
		{
			winSimulation = true;
			formulaWin = true;
			shouldFight = true;
			useStim = true;
		}
	}

	if (!shouldFight)
	{
		if (!winSimulation && !vikings.empty() && !tempests.empty())
		{
			const auto otherEnemies = threatsToKeep.size() - tempests.size();
			if (otherEnemies > 0)
			{
				winSimulation = Util::SimulateCombat(vikings, tempests, m_bot) > 0.f;
			}
			shouldFight = winSimulation;
			/*std::stringstream ss;
			ss << getSquad()->getName() << ": " << vikings.size() << " Vikings (" << injuredVikings << " injured) vs " << tempests.size() << " Tempests (" << injuredTempests << " injured): " << (winSimulation ? "win" : "LOSE");
			Util::Log(__FUNCTION__, ss.str(), m_bot);
			m_bot.Commander().Combat().SetLogVikingActions(true);*/
		}
		else if (enemyHasLongRangeUnits)
		{
			shouldFight = winSimulation;	// We consider only the simulation for long range enemies because our formula is shit
		}
	}
	m_bot.StopProfiling("0.10.4.1.5.1.5.4          SimulateCombat");

	// Save result
	m_combatSimulationResults[closeUnitsSet] = shouldFight;

	// For each of our close units
	for (auto & unitAndTarget : closeUnitsTarget)
	{
		auto unit = unitAndTarget.first;
		const sc2::Unit* simulatedUnit = nullptr;
		const auto unitTarget = unitAndTarget.second;

		// Cloak Banshee if threatened
		if (shouldFight && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE && Util::PathFinding::HasCombatInfluenceOnTile(Util::GetTilePosition(unit->pos), unit->is_flying, m_bot) && ExecuteBansheeCloakLogic(unit, false))
		{
			continue;
		}

		// Make sure the unit pointer is the right one for the Vikings
		if ((morphFlyingVikings && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT) || (morphLandedVikings && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER))
		{
			simulatedUnit = unit;
			unit = m_bot.GetUnitPtr(unit->tag);
		}

		const float unitRange = Util::GetAttackRangeForTarget(unit, unitTarget, m_bot);
		bool canAttackNow = unit->weapon_cooldown <= 0.f && unitRange > 0;
		if (canAttackNow)
		{
			if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER)
				canAttackNow = unitRange > Util::Dist(unit->pos, unitTarget->pos) + 1;	// Otherwise the BC would stop moving at the limit of its range, so it gets kited too much
			else
				canAttackNow = unitRange * unitRange >= Util::DistSq(unit->pos, unitTarget->pos);
		}

		// Even if the fight would be lost, should still attack if it can, but only if it is slower than the fastest enemy and its target is not on high ground
		if (!shouldFight && (!canAttackNow || Util::getSpeedOfUnit(unit, m_bot) > maxThreatSpeed || Util::IsEnemyHiddenOnHighGround(unit, unitTarget, m_bot)))
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
				const auto action = RangedUnitAction(MicroActionType::Move, movePosition, true, actionDuration, ACTION_DESCRIPTION_THREAT_FIGHT_DODGE_EFFECT);
				m_bot.Commander().Combat().PlanAction(unit, action);
				continue;
			}
			else
			{
				Util::DisplayError("Could not find an escape path", "", m_bot);
			}
		}
		
		if (shouldFight)
		{
			// Morph the flying Viking
			if (morphFlyingVikings && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
			{
				auto action = RangedUnitAction();
				const auto distSq = Util::DistSq(unit->pos, unitTarget->pos);
				if (distSq > 8 * 8)
				{
					action = RangedUnitAction(MicroActionType::Move, unitTarget->pos, true, 0, ACTION_DESCRIPTION_THREAT_FIGHT_MOVE_CLOSER_BEFORE_MORPH);
				}
				else if (distSq < 4 * 4 && Util::GetGroundAttackRange(unitTarget, m_bot) <= 2.f)
				{
					const float simulatedUnitRange = Util::GetAttackRangeForTarget(simulatedUnit, unitTarget, m_bot);
					auto saferPosition = Util::PathFinding::FindOptimalPathToSaferRange(simulatedUnit, unitTarget, simulatedUnitRange, true, m_bot);
					if (saferPosition == CCPosition())
					{
						saferPosition = unit->pos + Util::Normalized(unit->pos - unitTarget->pos) * 10;
					}
					action = RangedUnitAction(MicroActionType::Move, saferPosition, true, 0, ACTION_DESCRIPTION_THREAT_FIGHT_MOVE_CLOSER_BEFORE_MORPH);
				}
				else
				{
					action = RangedUnitAction(MicroActionType::Ability, sc2::ABILITY_ID::MORPH_VIKINGASSAULTMODE, true, VIKING_MORPH_FRAME_COUNT, ACTION_DESCRIPTION_THREAT_FIGHT_MORPH);
				}
				m_bot.Commander().Combat().PlanAction(unit, action);
				continue;
			}

			// Morph the landed Viking
			if (morphLandedVikings && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
			{
				const auto action = RangedUnitAction(MicroActionType::Ability, sc2::ABILITY_ID::MORPH_VIKINGFIGHTERMODE, true, VIKING_MORPH_FRAME_COUNT, ACTION_DESCRIPTION_THREAT_FIGHT_MORPH);
				m_bot.Commander().Combat().PlanAction(unit, action);
				continue;
			}

			// Micro the Medivac
			if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
			{
				ExecuteHealLogic(unit, allyCombatUnits, false);
				continue;
			}

			// Stim the Marine or Marauder if it is close enough to its target (to prevent using it from very far away)
			if (useStim && Util::DistSq(unit->pos, unitTarget->pos) <= 10 * 10 && ExecuteStimLogic(unit))
				continue;
		}

		auto movePosition = CCPosition();
		const bool injured = unit->health / unit->health_max < 0.5f;
		const auto enemyRange = Util::GetAttackRangeForTarget(unitTarget, unit, m_bot);
		const auto unitSpeed = Util::getSpeedOfUnit(unit, m_bot);
		const auto enemySpeed = Util::getSpeedOfUnit(unitTarget, m_bot);
		const bool shouldKite = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER || (unitRange > enemyRange && unitSpeed > enemySpeed);
		const bool shouldChase = unitRange < enemyRange && enemySpeed > 0;	//unitSpeed >= enemySpeed;
		if (!canAttackNow && AllowUnitToPathFind(unit, false))
		{
			if (shouldKite || (injured && enemyRange - unitRange < 2 && (enemySpeed == 0 || unitSpeed / enemySpeed >= 0.85f)))
			{
				movePosition = Util::PathFinding::FindOptimalPathToSaferRange(unit, unitTarget, unitRange, true, m_bot);
			}
			if(movePosition == CCPosition() && shouldChase)
			{
				auto path = Util::PathFinding::FindOptimalPath(unit, unitTarget->pos, {}, unitRange, false, false, true, true, 0, false, m_bot);
				movePosition = Util::PathFinding::GetCommandPositionFromPath(path, unit, true, m_bot);
			}
		}
		if (movePosition == CCPosition() && unitTarget->last_seen_game_loop < m_bot.GetCurrentFrame())
		{
			// The target is not visible, so we should move towards until it is visible so we can attack it
			movePosition = unitTarget->pos;
		}
		if (movePosition != CCPosition())
		{
			// Flee but stay in range
			const int actionDuration = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER ? REAPER_MOVE_FRAME_COUNT : 0;
			const auto action = RangedUnitAction(MicroActionType::Move, movePosition, true, actionDuration, ACTION_DESCRIPTION_THREAT_FIGHT_MOVE);
			m_bot.Commander().Combat().PlanAction(unit, action);
		}
		else
		{
			// Attack the target
			if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER && unitRange < Util::Dist(unit->pos, unitTarget->pos) + 1)
			{
				// BCs will stop attacking if they are too far from their prioritized target, but if they get a move command, they will keep attacking other units
				// Also, we want them to get a bit closer to their target than their maximum range, otherwise they might get kited
				const auto action = RangedUnitAction(MicroActionType::Move, unitTarget->pos, true, 0, ACTION_DESCRIPTION_THREAT_FIGHT_ATTACK);
				m_bot.Commander().Combat().PlanAction(unit, action);
			}
			else
			{
				const int attackDuration = canAttackNow ? getAttackDuration(unit, unitTarget) : 0;
				const auto action = RangedUnitAction(MicroActionType::AttackUnit, unitTarget, true, attackDuration, ACTION_DESCRIPTION_THREAT_FIGHT_ATTACK);
				m_bot.Commander().Combat().PlanAction(unit, action);
			}
			// Keep track of damage dealt
			const float damageDealt = Util::GetDpsForTarget(unit, unitTarget, m_bot) / 22.4f;
			m_bot.Analyzer().increaseTotalDamage(damageDealt, unit->unit_type);
		}
	}
	return shouldFight;
}

const sc2::Unit* RangedManager::GetSimulatedUnit(const sc2::Unit * rangedUnit)
{
	const sc2::Unit * simulatedUnit = nullptr;
	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARAUDER)
	{
		auto & dummyMap = GetDummyMap(rangedUnit->unit_type);
		auto it = dummyMap.find(rangedUnit->tag);
		if (it != dummyMap.end())
		{
			simulatedUnit = &it->second;
		}
		if (!simulatedUnit)
		{
			dummyMap[rangedUnit->tag] = Util::CreateDummyFromUnit(rangedUnit);
			simulatedUnit = &dummyMap[rangedUnit->tag];
		}
	}
	return simulatedUnit;
}

std::map<sc2::Tag, sc2::Unit> & RangedManager::GetDummyMap(sc2::UNIT_TYPEID type)
{
	if (type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
		return m_dummyAssaultVikings;
	if (type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
		return m_dummyFighterVikings;
	return m_dummyStimedUnits;
}

/*
 * Calculates the ally units to be considered in a combat simulation.
 * The result is stored in the closeUnitsSet.
 * We use a set because we need an ordered data structure for accurate and efficient comparison with data in memory.
 */
void RangedManager::CalcCloseUnits(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & allyCombatUnits, sc2::Units & rangedUnitTargets, std::set<const sc2::Unit *> & closeUnitsSet)
{
	bool morphFlyingVikings = false;
	bool morphLandedVikings = false;
	std::map<const sc2::Unit *, const sc2::Unit *> simulatedStimedUnits;
	float stimedUnitsPowerDifference;
	std::map<const sc2::Unit*, const sc2::Unit*> closeUnitsTarget;
	float unitsPower;
	float minUnitRange;
	const bool ignoreCyclones = false;
	CalcCloseUnits(rangedUnit, target, allyCombatUnits, rangedUnitTargets, ignoreCyclones, closeUnitsSet, morphFlyingVikings, morphLandedVikings, simulatedStimedUnits, stimedUnitsPowerDifference, closeUnitsTarget, unitsPower, minUnitRange);
}
/*
 * Calculates the ally units to be considered in a combat simulation.
 * The result is stored in the closeUnitsSet.
 * We use a set because we need an ordered data structure for accurate and efficient comparison with data in memory.
 */
void RangedManager::CalcCloseUnits(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & allyCombatUnits, sc2::Units & rangedUnitTargets, bool ignoreCyclones, std::set<const sc2::Unit *> & closeUnitsSet, bool & morphFlyingVikings, bool & morphLandedVikings, std::map<const sc2::Unit *, const sc2::Unit *> & simulatedStimedUnits, float & stimedUnitsPowerDifference, std::map<const sc2::Unit*, const sc2::Unit*> & closeUnitsTarget, float & unitsPower, float & minUnitRange)
{
	bool checkedForFlyingTarget = false;
	sc2::Units farAllyUnits;
	// Calculate ally power
	for (int i = 0; i < 2; ++i)
	{
		const auto & unitsToLoopOver = i == 0 ? allyCombatUnits : farAllyUnits;
		for (const auto unit : unitsToLoopOver)
		{
			// Distance check (first time with rangedUnit, second time with all fighting units and also the threat)
			if (i == 0)
			{
				// Ignore units that are too far to help
				if (Util::DistSq(unit->pos, rangedUnit->pos) > HARASS_FRIENDLY_SUPPORT_MAX_DISTANCE * HARASS_FRIENDLY_SUPPORT_MAX_DISTANCE)
				{
					farAllyUnits.push_back(unit);
					continue;
				}
			}
			else
			{
				bool tooFar = true;
				for (auto allyUnit : closeUnitsSet)
				{
					if (Util::DistSq(unit->pos, allyUnit->pos) <= HARASS_FRIENDLY_SUPPORT_MAX_DISTANCE * HARASS_FRIENDLY_SUPPORT_MAX_DISTANCE)
					{
						tooFar = false;
						break;
					}
				}
				if (tooFar)
				{
					// Also check if close enough to the threat
					float unitRange = Util::GetAttackRangeForTarget(unit, target, m_bot);
					if (unitRange == 0.f)
						unitRange = Util::GetMaxAttackRange(unit, m_bot);
					if (unitRange == 0.f || Util::Dist(unit->pos, target->pos) > unitRange + HARASS_FRIENDLY_SUPPORT_MAX_DISTANCE)
						continue;
				}
			}
			auto & unitAction = m_bot.Commander().Combat().GetRangedUnitAction(unit);
			// Ignore units that are executing a prioritized action other than a threat fighting one
			if (unitAction.prioritized && !Util::Contains(unitAction.description, THREAT_FIGHTING_ACTION_DESCRIPTIONS))
			{
				continue;
			}
			const bool canAttackTarget = target->is_flying ? Util::CanUnitAttackAir(unit, m_bot) : Util::CanUnitAttackGround(unit, m_bot);
			const bool canAttackTempest = target->unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST && canAttackTarget;
			// Ignore units that should heal to not consider them in the power calculation, unless the target is a Tempest and our unit can attack it
			if (unit != rangedUnit && m_bot.Commander().Combat().ShouldUnitHeal(unit) && !canAttackTempest)
			{
				continue;
			}
			// Ignore Cyclones because their health is too valuable to trade
			if (ignoreCyclones && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_CYCLONE)
			{
				continue;
			}
			// Ignore units that are disabled
			if (Util::isUnitDisabled(unit))
			{
				continue;
			}

			const sc2::Unit* unitTarget = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC ? GetHealTarget(unit, allyCombatUnits, false) : getTarget(unit, rangedUnitTargets, false);
			const sc2::Unit* unitToSave = unit;

			const auto & cycloneFlyingHelpers = m_bot.Commander().Combat().getCycloneFlyingHelpers();
			// If the flying Viking doesn't have a target, we check if it would have one as a landed Viking (unless it is a flying helper)
			if (!unitTarget && !m_bot.Analyzer().enemyHasCombatAirUnit() && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER && cycloneFlyingHelpers.find(unit) == cycloneFlyingHelpers.end())
			{
				const sc2::Unit* vikingAssault = GetSimulatedUnit(unit);
				unitTarget = getTarget(vikingAssault, rangedUnitTargets, false);
				if (unitTarget)
				{
					unitToSave = vikingAssault;
					morphFlyingVikings = true;
				}
			}
			else if(unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
			{
				if (!checkedForFlyingTarget || morphLandedVikings)
				{
					checkedForFlyingTarget = true;
					const sc2::Unit* vikingFighter = GetSimulatedUnit(unit);
					const auto flyingUnitTarget = getTarget(vikingFighter, rangedUnitTargets, false);
					if (flyingUnitTarget)
					{
						unitToSave = vikingFighter;
						unitTarget = flyingUnitTarget;
						morphLandedVikings = true;
					}
				}
			}

			// If the unit has a target, add it to the close units and calculate its power
			if (unitTarget)
			{
				const auto unitPower = Util::GetUnitPower(unitToSave, unitTarget, m_bot);
				// If the unit can use Stim we simulate it to compare and find if it is worth using it
				if (CanUseStim(unitToSave))
				{
					simulatedStimedUnits[unitToSave] = GetSimulatedUnit(unitToSave);
					stimedUnitsPowerDifference += Util::GetUnitPower(simulatedStimedUnits[unitToSave], unitTarget, m_bot) - unitPower;
				}
				closeUnitsSet.insert(unitToSave);
				closeUnitsTarget[unitToSave] = unitTarget;
				unitsPower += unitPower;
				const float unitRange = Util::GetAttackRangeForTarget(unitToSave, unitTarget, m_bot);
				if (minUnitRange < 0 || unitRange < minUnitRange)
					minUnitRange = unitRange;
			}
		}
	}
}

void RangedManager::ExecuteCycloneLogic(const sc2::Unit * cyclone, bool isUnitDisabled, bool & unitShouldHeal, bool & shouldAttack, bool & cycloneShouldUseLockOn, bool & cycloneShouldStayCloseToTarget, const sc2::Units & rangedUnits, const sc2::Units & threats, const sc2::Units & rangedUnitTargets, const sc2::Unit * & target, CCPosition & goal, std::string & goalDescription, sc2::AvailableAbilities & abilities)
{
	bool lockOnAvailable;
	target = ExecuteLockOnLogic(cyclone, unitShouldHeal, shouldAttack, cycloneShouldUseLockOn, lockOnAvailable, rangedUnits, threats, rangedUnitTargets, target, abilities);

	// If the Cyclone has a its Lock-On on a target with a big range (like a Tempest or Tank)
	if (!shouldAttack && !cycloneShouldUseLockOn && !isUnitDisabled)
	{
		const auto & lockOnTargets = m_bot.Commander().Combat().getLockOnTargets();
		const auto it = lockOnTargets.find(cyclone);
		if (it != lockOnTargets.end())
		{
			const auto lockOnTarget = it->second.first;
			const auto enemyRange = Util::GetAttackRangeForTarget(lockOnTarget, cyclone, m_bot);
			const auto enemyDps = Util::GetDpsForTarget(lockOnTarget, cyclone, m_bot);
			if (enemyRange >= 10.f && enemyDps > 0.f)
			{
				// We check if we have another unit that is close to it, but if not, the Cyclone should stay close to it
				bool closeAlly = Util::AllyUnitSeesEnemyUnit(cyclone, lockOnTarget, m_bot);;
				/*for (const auto ally : rangedUnits)
				{
					if (ally == cyclone)
						continue;
					if (Util::DistSq(ally->pos, lockOnTarget->pos) > 7.f * 7.f)
						continue;
					// If the ally could survive at least 2 seconds close to the target
					if (ally->health >= Util::PathFinding::GetTotalInfluenceOnTile(Util::GetTilePosition(ally->pos), ally, m_bot) * 2.f)
					{
						closeAlly = true;
						break;
					}
				}*/
				if (!closeAlly)
				{
					cycloneShouldStayCloseToTarget = true;
					unitShouldHeal = false;
					target = lockOnTarget;
				}
			}
		}
	}

	const auto cyclonesWithHelper = m_bot.Commander().Combat().getCyclonesWithHelper();
	const auto cycloneWithHelperIt = cyclonesWithHelper.find(cyclone);
	const bool hasFlyingHelper = cycloneWithHelperIt != cyclonesWithHelper.end();

	if (!unitShouldHeal && !cycloneShouldStayCloseToTarget && m_order.getType() != SquadOrderTypes::Defend)
	{
		// If the Cyclone wants to use its lock-on ability, we make sure it stays close to its flying helper to keep a good vision
		if (cycloneShouldUseLockOn && hasFlyingHelper)
		{
			const auto cycloneFlyingHelper = cycloneWithHelperIt->second;
			// If the flying helper is too far, go towards it
			if (Util::DistSq(cyclone->pos, cycloneFlyingHelper->pos) > CYCLONE_PREFERRED_MAX_DISTANCE_TO_HELPER * CYCLONE_PREFERRED_MAX_DISTANCE_TO_HELPER)
			{
				goal = cycloneFlyingHelper->pos;
				goalDescription = "StayCloseToHelper";
			}
		}

		if (!hasFlyingHelper && m_bot.Strategy().getStartingStrategy() != PROXY_MARAUDERS)
		{
			// If the target is too far, we don't want to chase it, we just leave
			if (target && cycloneShouldUseLockOn)
			{
				const float lockOnRange = m_bot.Commander().Combat().getAbilityCastingRanges().at(sc2::ABILITY_ID::EFFECT_LOCKON) + cyclone->radius + target->radius;
				if (Util::DistSq(cyclone->pos, target->pos) > lockOnRange * lockOnRange)
				{
					// TODO remove when the flying helper bug is fixed
					bool closeFlyingUnit = false;
					for (const auto allyUnit : rangedUnits)
					{
						if (!allyUnit->is_flying)
							continue;
						if (Util::DistSq(cyclone->pos, allyUnit->pos) < 7.f * 7.f)
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
			const auto & cycloneFlyingHelpers = m_bot.Commander().Combat().getCycloneFlyingHelpers();
			for (const auto & flyingHelper : cycloneFlyingHelpers)
			{
				const float distSq = Util::DistSq(cyclone->pos, flyingHelper.first->pos);
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
				goalDescription = "ReachHelper";
			}
			else if (m_bot.Strategy().getStartingStrategy() != PROXY_MARAUDERS)
			{
				// Otherwise go back to base, it's too dangerous to go alone
				goal = m_bot.GetStartLocation();
				goalDescription = "NoHelperStart";
				unitShouldHeal = true;
			}
		}
	}

	if (!lockOnAvailable)
	{
		auto & nextAvailableAbility = m_bot.Commander().Combat().getNextAvailableAbility();
		const auto currentFrame = m_bot.GetCurrentFrame();
		if (nextAvailableAbility[sc2::ABILITY_ID::EFFECT_LOCKON][cyclone] - currentFrame > CYCLONE_LOCKON_COOLDOWN_FRAME_COUNT / 2)
		{
			goal = m_bot.GetStartLocation();
			goalDescription = "CooldownStart";
		}
	}
}

bool RangedManager::ExecutePrioritizedUnitAbilitiesLogic(const sc2::Unit * rangedUnit, sc2::Units & threats, sc2::Units & targets, CCPosition goal, bool unitShouldHeal, bool isCycloneHelper)
{
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

bool RangedManager::ExecuteUnitAbilitiesLogic(const sc2::Unit * rangedUnit, const sc2::Unit * target, sc2::Units & threats, sc2::Units & targets, sc2::Units & allyUnits, CCPosition goal, bool unitShouldHeal, bool isCycloneHelper, sc2::AvailableAbilities & abilities)
{
	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER || rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
	{
		if (ExecuteVikingMorphLogic(rangedUnit, goal, target, threats, targets, unitShouldHeal, isCycloneHelper))
			return true;
	}
	
	if (ExecuteKD8ChargeLogic(rangedUnit, threats))
	{
		return true;
	}

	if (ExecuteAutoTurretLogic(rangedUnit, threats))
	{
		return true;
	}

	if (ExecuteHealLogic(rangedUnit, allyUnits, unitShouldHeal))
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
	if (!m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::BATTLECRUISERENABLESPECIALIZATIONS))
		return false;
	
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
		if (potentialTarget->display_type == sc2::Unit::Hidden)
			continue;
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
		const auto action = RangedUnitAction(MicroActionType::AbilityTarget, sc2::ABILITY_ID::EFFECT_YAMATOGUN, target, true, BATTLECRUISER_YAMATO_CANNON_FRAME_COUNT, "Yamato");
		m_bot.Commander().Combat().PlanAction(battlecruiser, action);
		queryYamatoAvailability.insert(battlecruiser);
		yamatoTargets[target->tag][battlecruiser->tag] = currentFrame + BATTLECRUISER_YAMATO_CANNON_FRAME_COUNT + 20;
		return true;
	}

	return false;
}

bool RangedManager::ExecuteHealLogic(const sc2::Unit * medivac, const sc2::Units & allyUnits, bool shouldHeal) const
{
	if (medivac->unit_type != sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
		return false;
	
	if (shouldHeal)
		return false;
	
	const sc2::Unit * target = GetHealTarget(medivac, allyUnits, true);
	
	return ExecuteHealCommand(medivac, target);
}

const sc2::Unit * RangedManager::GetHealTarget(const sc2::Unit * medivac, const sc2::Units & allyUnits, bool filterFullHealthUnits) const
{
	// "The minimum required energy to start the healing process is 5" according to https://liquipedia.net/starcraft2/Medivac_(Legacy_of_the_Void)
	if (medivac->energy < 5)
		return nullptr;

	const auto & abilityRanges = m_bot.Commander().Combat().getAbilityCastingRanges();
	const float healRange = abilityRanges.at(sc2::ABILITY_ID::EFFECT_HEAL) + medivac->radius;

	const sc2::Unit * target = nullptr;
	float bestScore = 0.f;	// The lower the best
	for (const auto ally : allyUnits)
	{
		if (filterFullHealthUnits && ally->health >= ally->health_max)
			continue;
		const Unit allyUnit(ally, m_bot);
		if (!allyUnit.getType().isCombatUnit() || !allyUnit.hasAttribute(sc2::Attribute::Biological))
			continue;
		const float distance = Util::DistSq(medivac->pos, ally->pos);
		const float score = ally->health + std::max(healRange + ally->radius, distance);
		if (!target || score < bestScore)
		{
			target = ally;
			bestScore = score;
		}
	}

	return target;
}

bool RangedManager::ExecuteHealCommand(const sc2::Unit * medivac, const sc2::Unit * target) const
{
	if (target)
	{
		const auto & abilityRanges = m_bot.Commander().Combat().getAbilityCastingRanges();
		const float healRange = abilityRanges.at(sc2::ABILITY_ID::EFFECT_HEAL) + medivac->radius + target->radius;
		if (Util::PathFinding::HasInfluenceOnTile(Util::GetTilePosition(medivac->pos), medivac->is_flying, m_bot))
		{
			CCPosition movePosition = Util::PathFinding::FindOptimalPathToSaferRange(medivac, target, healRange, false, m_bot);
			if (movePosition != CCPosition())
			{
				const float distSq = Util::DistSq(medivac->pos, movePosition);
				if (distSq > 0.25f)
				{
					const auto action = RangedUnitAction(MicroActionType::Move, movePosition, false, 0, "MoveToSaferRange");
					m_bot.Commander().Combat().PlanAction(medivac, action);
					return true;
				}
			}
			else
				return false;
		}
		if (Util::DistSq(medivac->pos, target->pos) <= healRange * healRange)
		{
			const auto action = RangedUnitAction(MicroActionType::AbilityTarget, sc2::ABILITY_ID::EFFECT_HEAL, target, false, 0, "Heal");
			m_bot.Commander().Combat().PlanAction(medivac, action);
		}
		else
		{
			const auto action = RangedUnitAction(MicroActionType::Move, target->pos, false, 0, "MoveToHealTarget");
			m_bot.Commander().Combat().PlanAction(medivac, action);
		}
		return true;
	}

	return false;
}

bool RangedManager::ExecuteStimLogic(const sc2::Unit * unit) const
{
	if (!CanUseStim(unit))
		return false;

	const auto action = RangedUnitAction(MicroActionType::Ability, sc2::ABILITY_ID::EFFECT_STIM, true, 0, "Stim");
	m_bot.Commander().Combat().PlanAction(unit, action);
	return true;
}

bool RangedManager::CanUseStim(const sc2::Unit * unit) const
{
	if (!m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::STIMPACK))
		return false;

	if (unit->unit_type != sc2::UNIT_TYPEID::TERRAN_MARINE && unit->unit_type != sc2::UNIT_TYPEID::TERRAN_MARAUDER)
		return false;

	const bool isMarine = unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE;
	if (unit->health <= (isMarine ? 10 : 20))
		return false;

	if (Util::unitHasBuff(unit, isMarine ? sc2::BUFF_ID::STIMPACK : sc2::BUFF_ID::STIMPACKMARAUDER))
		return false;

	return true;
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
			const auto action = RangedUnitAction(MicroActionType::AbilityPosition, sc2::ABILITY_ID::EFFECT_KD8CHARGE, expectedThreatPosition, true, REAPER_KD8_CHARGE_FRAME_COUNT, "KD8Charge");
			m_bot.Commander().Combat().PlanAction(reaper, action);
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
		const auto action = RangedUnitAction(MicroActionType::AbilityPosition, sc2::ABILITY_ID::EFFECT_AUTOTURRET, turretPosition, true, 0, "AutoTurret");
		m_bot.Commander().Combat().PlanAction(raven, action);
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

CCPosition RangedManager::GetAttractionVectorToFriendlyUnits(const sc2::Unit * rangedUnit, sc2::Units & rangedUnits) const
{
	// Check if there is a friendly harass unit close to this one
	std::vector<const sc2::Unit*> closeAllies;
	for (auto friendlyRangedUnit : rangedUnits)
	{
		if (friendlyRangedUnit->tag != rangedUnit->tag && friendlyRangedUnit->unit_type == rangedUnit->unit_type)
		{
			const float dist = Util::DistSq(rangedUnit->pos, friendlyRangedUnit->pos);
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
			m_bot.Map().drawLine(rangedUnit->pos, closeAlliesCenter, sc2::Colors::Green);
#endif

		const float distToCloseAlliesCenter = Util::Dist(rangedUnit->pos, closeAlliesCenter);
		CCPosition attractionVector = closeAlliesCenter - rangedUnit->pos;
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
const sc2::Unit * RangedManager::getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets, bool harass, bool filterHigherUnits, bool considerOnlyUnitsInRange, bool filterPassiveBuildings)
{
    BOT_ASSERT(rangedUnit, "null ranged unit in getTarget");

	if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_RAVEN)
	{
		return nullptr;
	}

	std::set<const sc2::Unit *> currentTargets;
	if (!harass)
	{
		currentTargets = std::set<const sc2::Unit *>(targets.begin(), targets.end());
		const auto it = m_threatTargetForUnit.find(rangedUnit);
		if (it != m_threatTargetForUnit.end())
		{
			const auto & savedTargets = it->second;
			const auto it2 = savedTargets.find(currentTargets);
			if (it2 != savedTargets.end())
			{
				return it2->second;
			}
		}
	}

	std::multiset<std::pair<float, const sc2::Unit *>> targetPriorities;

    // for each possible target
    for (auto target : targets)
    {
        BOT_ASSERT(target, "null target unit in getTarget");

		// We don't want to target an enemy in the fog other than a worker (sometimes it could be good, but often it isn't)
		/*if (!UnitType(target->unit_type, m_bot).isWorker() && target->last_seen_game_loop != m_bot.GetGameLoop())
			continue;*/ //*edit: commented because we couldn't engage against cannons on top of cliffs

		if (filterHigherUnits && Util::IsEnemyHiddenOnHighGround(rangedUnit, target, m_bot))
			continue;

    	// We don't want Reapers to lose their time attacking buildings unless they are defending
    	if (filterPassiveBuildings || (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER && m_order.getType() != SquadOrderTypes::Defend))
    	{
			auto targetUnit = Unit(target, m_bot);
			if (targetUnit.getType().isBuilding() && !targetUnit.getType().isCombatUnit())
				continue;
    	}

		float priority = getAttackPriority(rangedUnit, target, harass, considerOnlyUnitsInRange);
		if(priority > 0.f)
			targetPriorities.insert(std::pair<float, const sc2::Unit*>(priority, target));
    }

	if (harass && rangedUnit->unit_type != sc2::UNIT_TYPEID::TERRAN_CYCLONE)
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

	const sc2::Unit * target = targetPriorities.empty() ? nullptr : (*targetPriorities.rbegin()).second;	//last target because it's the one with the highest priority
	if (!harass)
		m_threatTargetForUnit[rangedUnit][currentTargets] = target;
	return target;		
}

sc2::Units & RangedManager::getThreats(const sc2::Unit * rangedUnit, const sc2::Units & targets)
{
	const auto it = m_threatsForUnit.find(rangedUnit);
	if (it != m_threatsForUnit.end())
		return it->second;
	sc2::Units threats;
	Util::getThreats(rangedUnit, targets, threats, m_bot);
	m_threatsForUnit[rangedUnit] = threats;
	return m_threatsForUnit[rangedUnit];
}

// according to http://wiki.teamliquid.net/starcraft2/Range
// the maximum range of a melee unit is 1 (ultralisk)
bool RangedManager::isTargetRanged(const sc2::Unit * target)
{
    BOT_ASSERT(target, "target is null");
    const float maxRange = Util::GetMaxAttackRange(target, m_bot);
    return maxRange > 2.5f;
}