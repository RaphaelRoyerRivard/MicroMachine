#include "MicroManager.h"
#include "CCBot.h"

const float MIN_RANGE_DIFFERENCE_FOR_TARGET = 2.f;

MicroManager::MicroManager(CCBot & bot)
    : m_bot(bot)
{
}

void MicroManager::setUnits(const std::vector<Unit> & u)
{
    m_units = u;
}

const std::vector<Unit> & MicroManager::getUnits() const
{
    return m_units;
}

void MicroManager::regroup(const CCPosition & regroupPosition) const
{
    // for each of the units we have
    for (auto & unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit in MicroManager regroup");

        if (Util::DistSq(unit, regroupPosition) > 4 * 4)
        {
            // regroup it
			Micro::SmartMove(unit.getUnitPtr(), regroupPosition, m_bot);
        }
        else
        {
            //defend itself near the regroupPosition
			Micro::SmartAttackMove(unit.getUnitPtr(), regroupPosition, m_bot);
        }
    }
}

float MicroManager::getAverageSquadSpeed() const
{
	return Util::getAverageSpeedOfUnits(m_units, m_bot);
}

float MicroManager::getAverageTargetsSpeed() const
{
	return Util::getAverageSpeedOfUnits(m_targets, m_bot);
}

float MicroManager::getSquadPower() const
{
	return Util::GetUnitsPower(m_units, m_targets, m_bot);
}

float MicroManager::getTargetsPower(const std::vector<Unit>& units) const
{
	return Util::GetUnitsPower(m_targets, units, m_bot);
}

float MicroManager::getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * target, const sc2::Units & allTargets, bool filterHighRangeUnits, bool considerOnlyUnitsInRange, bool reducePriorityOfUnpowered) const
{
	BOT_ASSERT(target, "null unit in getAttackPriority");

	if (!UnitType::isTargetable(target->unit_type))
		return 0.f;

	if (m_bot.IsParasited(target))
		return 0.f;

	float notYetRevealedModifier = 1.f;
	if (target->display_type == sc2::Unit::Hidden)
	{
		if (!m_bot.Buildings().hasEnergyForScan())
			return 0.f;
		notYetRevealedModifier = 0.001f;
	}

	// Ignoring invisible creep tumors
	const uint32_t lastGameLoop = m_bot.GetGameLoop() - 1;
	if (target->unit_type == sc2::UNIT_TYPEID::ZERG_CREEPTUMOR ||
		target->unit_type == sc2::UNIT_TYPEID::ZERG_CREEPTUMORBURROWED||
		target->unit_type == sc2::UNIT_TYPEID::ZERG_CREEPTUMORQUEEN)
	{
		if (target->last_seen_game_loop < lastGameLoop)
			return 0.f;
	}
	
	// Check for close melee unit bonus
	const bool ignoreSpells = filterHighRangeUnits;	// ATM only the Cyclone Lock-On is considered and we want to ignore it only in harass mode
	const float attackerRange = Util::GetAttackRangeForTarget(attacker, target, m_bot, ignoreSpells);
	const float targetRange = Util::GetAttackRangeForTarget(target, attacker, m_bot);
	const float distance = Util::Dist(attacker->pos, target->pos);
	float closeMeleeUnitBonus = 1.f;
	if (targetRange > 0.f && targetRange < 2.f)
	{
		if (distance <= targetRange)
			closeMeleeUnitBonus = 2.f;
		else
		{
			const float targetThreatRange = Util::getThreatRange(attacker, target, m_bot);
			const float threatBuffer = targetThreatRange - targetRange;
			const float damageDistance = distance - targetRange;
			const float progression = std::max(0.f, std::min(1.f, damageDistance / threatBuffer));	// Between 0 and 1. The closer the enemy is, the closer to 0 it gets
			closeMeleeUnitBonus = 2 - progression;
		}
	}

	if (filterHighRangeUnits)
	{
		if (targetRange + MIN_RANGE_DIFFERENCE_FOR_TARGET > attackerRange)
			return 0.f;
	}

	const float unitDps = Util::GetDpsForTarget(attacker, target, m_bot);
	if (unitDps == 0.f)	//Do not attack targets on which we would do no damage
		return 0.f;

	if (UnitType::hasSplashingAttack(attacker->unit_type, target->is_flying))
	{
		if (attacker->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED && Util::Dist(attacker->pos, target->pos) < attacker->radius + target->radius + 2)
			return 0.f;	// Sieged up tanks cannot attack units that are too close
		float splashPriority =  getPriorityOfTargetConsideringSplash(attacker, target, allTargets);
		if (splashPriority > 0.f)
			return splashPriority;
	}

	const float healthValue = pow(target->health + target->shield, 0.5f);		//the more health a unit has, the less it is prioritized
	float proximityValue = 1.f;
	if (distance > attackerRange)
	{
		if (considerOnlyUnitsInRange)
			return 0.f;
		float attackerSpeed = Util::getSpeedOfUnit(attacker, m_bot);
		if (attackerSpeed == 0.f)
			proximityValue = 0.001f;
		else
		{
			proximityValue = std::pow(0.9f, distance - attackerRange);	//the more far a unit is, the less it is prioritized
			float targetSpeed = Util::getSpeedOfUnit(target, m_bot);
			if (attacker->weapon_cooldown == 0 && targetSpeed > attackerSpeed)	// if the unit can attack and is slower than its target, we reduce the priority
				proximityValue *= 0.25f;
		}
	}
	if (considerOnlyUnitsInRange && target->display_type == sc2::Unit::Snapshot)	// Sometimes we see enemy units attacking our units but we don't have vision so we can't attack it
		return 0.f;
	if (target->last_seen_game_loop < m_bot.GetGameLoop())
		proximityValue *= 0.25f;

	float invisModifier = 1.f;
	if (target->cloak == sc2::Unit::CloakedDetected)
		invisModifier = 2.f;
	else if (target->is_burrowed && target->unit_type != sc2::UNIT_TYPEID::ZERG_ZERGLINGBURROWED)
		invisModifier = 3.f;

	float hallucinationModifier = 1.f;
	if (target->is_hallucination)
		hallucinationModifier = 0.001f;

	const Unit attackerUnit(attacker, m_bot);
	const Unit targetUnit(target, m_bot);

	if (attackerUnit.getType().isWorker() && targetUnit.getType().isWorker() && !m_bot.Strategy().isWorkerRushed() && m_squad->getName() != "ScoutDefense")
		return 0.f;
	
	const float antiBuildingModifier = attackerUnit.getType().isWorker() && targetUnit.getType().isBuilding() && !m_bot.Strategy().isWorkerRushed() ? 10.f : 1.f;

	const float unpoweredModifier = reducePriorityOfUnpowered && (targetUnit.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON || targetUnit.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY) && !targetUnit.getUnitPtr()->is_powered && targetUnit.getUnitPtr()->build_progress == 1.f ? 0.05f : 1.f;		// is_powered is always false when the building is not finished

	if (targetUnit.getType().isCombatUnit() || targetUnit.getType().isWorker())
	{
		const float targetDps = Util::GetDpsForTarget(target, attacker, m_bot);
		float workerBonus = 1.f;
		if (targetUnit.getType().isWorker() && m_order.getType() != SquadOrderTypes::Defend)
		{
			/*if (attacker->unit_type != sc2::UNIT_TYPEID::TERRAN_HELLION)
			{
				workerBonus = 2.f;
			}*/

			// Reduce priority for workers that are going in a refinery
			if (Util::Contains(targetUnit.getUnitPtr(), m_bot.GetEnemyWorkersGoingInRefinery()))
			{
				workerBonus *= 0.5f;
				if (m_bot.Config().DrawHarassInfo)
					m_bot.Map().drawCircle(targetUnit.getPosition(), 0.5f, sc2::Colors::Red);
			}
		}
		if (targetUnit.getUnitPtr()->unit_type == sc2::UNIT_TYPEID::TERRAN_SCV && attacker->unit_type != sc2::UNIT_TYPEID::TERRAN_SCV)
		{
			// Add a bonus if the SCV is building
			if (Util::Contains(targetUnit.getUnitPtr(), m_bot.GetEnemySCVBuilders()))
			{
				workerBonus *= 2.f;
				if (m_bot.Config().DrawHarassInfo)
					m_bot.Map().drawCircle(targetUnit.getPosition(), 0.5f, sc2::Colors::Green);
			}

			// Add a bonus if the SCV is repairing
			else if (Util::Contains(targetUnit.getUnitPtr(), m_bot.GetEnemyRepairingSCVs()))
			{
				workerBonus *= 2.f;
				if (m_bot.Config().DrawHarassInfo)
					m_bot.Map().drawCircle(targetUnit.getPosition(), 0.5f, sc2::Colors::Green);
			}
		}

		float nonThreateningModifier = targetDps == 0.f ? 0.5f : 1.f;								//targets that cannot hit our unit are less prioritized
		if (targetUnit.getType().isAttackingBuilding())
		{
			nonThreateningModifier = targetDps == 0.f ? 1.f : 0.5f;		//for buildings, we prefer targetting them with units that will not get attacked back
		}
		const float flyingDetectorModifier = target->is_flying && UnitType::isDetector(target->unit_type) ? 2.f : 1.f;
		const float minionModifier = UnitType::isSpawnedUnit(target->unit_type) ? 0.1f : 1.f;	//units that are temporary or can be cheaply respawned should be less prioritized
		const auto & yamatoTargets = m_bot.Commander().Combat().getYamatoTargets();
		const float yamatoTargetModifier = yamatoTargets.find(target->tag) != yamatoTargets.end() ? 0.1f : 1.f;
		const float shieldUnitModifier = target->unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER ? 0.1f : 1.f;
		//const float nydusModifier = target->unit_type == sc2::UNIT_TYPEID::ZERG_NYDUSCANAL && target->build_progress < 1.f ? 100.f : 1.f;
		const float nydusModifier = 1.f;	// It seems like attacking it does close to nothing since it has 3 armor
		return std::max(0.1f, (targetDps + unitDps - healthValue + proximityValue * 50) * closeMeleeUnitBonus * workerBonus * nonThreateningModifier * minionModifier * invisModifier * hallucinationModifier * flyingDetectorModifier * yamatoTargetModifier * shieldUnitModifier * nydusModifier * antiBuildingModifier * unpoweredModifier * notYetRevealedModifier);
	}

	if (antiBuildingModifier > 1.f)
		return std::max(0.1f, (proximityValue * 50 - healthValue) * invisModifier * hallucinationModifier * antiBuildingModifier * unpoweredModifier);	// Our workers should clear buildings instead of enemy units
	
	return std::max(0.1f, (proximityValue * 50 - healthValue) * invisModifier * unpoweredModifier * hallucinationModifier / 100.f);		//we do not want non combat buildings to have a higher priority than other units
}

float MicroManager::getPriorityOfTargetConsideringSplash(const sc2::Unit * attacker, const sc2::Unit * target, const sc2::Units & allTargets) const
{
	//consider unit distance (bool as parameter to activate) to give a minor buff to unit closer
	//consider building splash, should be a very minor buff, as to avoid splash on buildings instead of units

	bool isHandled = false;//TEMPORARY

	bool canSplash = true;
	bool canSplashFriendlies = false;
	bool hitAir = false;
	bool hitGround = false;

	bool isCircle = false;
	bool isHalfCircle = false;
	bool isLine = false;
	bool isHLine = false;

	//if 0 range, is only main target
	float zone1Range = 0;
	float zone2Range = 0;
	float zone3Range = 0;

	//if 0 percent, does not hit
	float zone1Percentage = 1;
	float zone2Percentage = 0;
	float zone3Percentage = 0;

	switch ((sc2::UNIT_TYPEID)attacker->unit_type)
	{
		//line
		case sc2::UNIT_TYPEID::TERRAN_HELLION://line
			isLine = true;
			hitGround = true;
			canSplashFriendlies = false;
			break;
		case sc2::UNIT_TYPEID::ZERG_LURKERMPBURROWED://line
			isLine = true;
			hitGround = true;
			canSplashFriendlies = false;
			break;

		//horizontal line
		case sc2::UNIT_TYPEID::PROTOSS_COLOSSUS://horizontal line
			isHLine = true;
			hitGround = true;
			canSplashFriendlies = false;
			break;

		//half circle
		case sc2::UNIT_TYPEID::TERRAN_HELLIONTANK://half circle, melee
			isHalfCircle = true;
			hitGround = true;
			canSplashFriendlies = false;
			break;
		case sc2::UNIT_TYPEID::ZERG_ULTRALISK://half circle, melee, has main target
			isHalfCircle = true;
			hitGround = true;
			canSplashFriendlies = false;
			break;

		//circle
		case sc2::UNIT_TYPEID::TERRAN_NUKE://circle
			zone1Range = 4;
			zone2Range = 6;
			zone3Range = 8;

			zone2Percentage = 0.5f;
			zone3Percentage = 0.25f;

			isCircle = true;
			hitAir = true;
			hitGround = true;
			canSplashFriendlies = true;
			isHandled = true;
			break;
		case sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS://circle
			zone1Range = 0.5f;
			zone2Range = 0.8f;
			zone3Range = 1.25f;

			zone2Percentage = 0.75f;
			zone3Percentage = 0.375f;

			isCircle = true;
			hitGround = true;
			canSplashFriendlies = false;
			isHandled = true;
			break;
		case sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED://circle
			zone1Range = 0.4687f;
			zone2Range = 0.7812f;
			zone3Range = 1.25f;

			zone2Percentage = 0.5f;
			zone3Percentage = 0.25f;

			isCircle = true;
			hitGround = true;
			canSplashFriendlies = true;
			isHandled = true;
			break;
		case sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED://circle, has main target
			///TODO NOT EXACT, THIS IS AN APPROXIMATION
			zone1Range = 0;
			zone2Range = 1.75;

			zone2Percentage = 0.33f;

			isCircle = true;
			hitAir = true;
			hitGround = true;
			canSplashFriendlies = true;
			isHandled = true;
			break;
		case sc2::UNIT_TYPEID::TERRAN_LIBERATOR://circle
			zone1Range = 1.5;

			isCircle = true;
			hitGround = true;
			canSplashFriendlies = false;
			isHandled = true;
			break;
		case sc2::UNIT_TYPEID::PROTOSS_ARCHON://circle
			zone1Range = 0.25f;
			zone2Range = 0.5f;
			zone3Range = 1.f;

			zone2Percentage = 0.5f;
			zone3Percentage = 0.25f;

			isCircle = true;
			hitAir = true;
			hitGround = true;
			canSplashFriendlies = false;
			isHandled = true;
			break;
		case sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR://circle
		case sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED://circle
			zone1Range = 1.5f;

			isCircle = true;
			hitGround = true;
			canSplashFriendlies = false;
			isHandled = true;
			break;
		case sc2::UNIT_TYPEID::PROTOSS_HIGHTEMPLAR://circle
			isCircle = true;
			hitAir = true;
			hitGround = true;
			canSplashFriendlies = true;
			isHandled = false;
			break;
		case sc2::UNIT_TYPEID::ZERG_BANELING://circle
			zone1Range = 2.2f;

			isCircle = true;
			hitGround = true;
			canSplashFriendlies = false;
			isHandled = true;
			break;
		case sc2::UNIT_TYPEID::ZERG_BANELINGBURROWED://circle
			zone1Range = 2.2f;

			isCircle = true;
			hitGround = true;
			canSplashFriendlies = false;
			isHandled = true;
			break;
		case sc2::UNIT_TYPEID::TERRAN_THOR://circle
			zone1Range = 0.5f;

			isCircle = true;
			hitAir = true;
			canSplashFriendlies = false;
			isHandled = true;
			break;
		default:
			canSplash = false;
			break;
	}

	float damageScore = 0.f;//0 if not splash, will be handled by the target priority elsewhere
	if (canSplash && isHandled)
	{
		for (auto & splashTarget : allTargets)
		{
			if ((hitAir && hitGround) || (splashTarget->is_flying && hitAir) || (!splashTarget->is_flying && hitGround))
			{
				float distSq = Util::DistSq(target->pos, splashTarget->pos);
				if (zone1Range == 0 || distSq < pow(zone1Range + splashTarget->radius, 2))
				{
					float damage = Util::GetDamageForTarget(attacker, target, m_bot) * zone1Percentage;
					if (damage >= target->health + target->shield)
					{
						float buildP = splashTarget->build_progress;
						damage = target->health + (target->shield * 0.5) + target->health_max + (target->shield_max * 0.5);
					}
					if (UnitType(splashTarget->unit_type, m_bot).isBuilding())
					{
						damage *= 0.10f;
					}
					damageScore += damage;
				}
				else  if (zone2Range != 0 && distSq < pow(zone2Range + splashTarget->radius, 2))
				{
					float damage = Util::GetDamageForTarget(attacker, target, m_bot) * zone2Percentage;
					if (damage >= target->health)
					{
						damage = target->health + (target->shield * 0.5) + target->health_max + (target->shield_max * 0.5);
					}
					if (UnitType(splashTarget->unit_type, m_bot).isBuilding())
					{
						damage *= 0.10f;
					}
					damageScore += damage;
				}
				else  if (zone3Range != 0 && distSq < pow(zone3Range + splashTarget->radius, 2))
				{
					float damage = Util::GetDamageForTarget(attacker, target, m_bot) * zone3Percentage;
					if (damage >= target->health)
					{
						damage = target->health + (target->shield * 0.5) + target->health_max + (target->shield_max * 0.5);
					}
					if (UnitType(splashTarget->unit_type, m_bot).isBuilding())
					{
						damage *= 0.10f;
					}
					damageScore += damage;
				}
			}
		}

		if (canSplashFriendlies)
		{
			//for all friendly units
			//if type is same, switch to see if it splashes on allies of same type or not
			auto & allies = m_bot.GetAllyUnits();
			for (auto & ally : allies)
			{
				auto allyPtr = ally.second.getUnitPtr();
				if ((hitAir && hitGround) || (allyPtr->is_flying && hitAir) || (!allyPtr->is_flying && hitGround))
				{
					switch ((sc2::UNIT_TYPEID)attacker->unit_type)
					{
						case sc2::UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
						case sc2::UNIT_TYPEID::TERRAN_NUKE:
						case sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED:
							break;
						case sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR:
						case sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED:
							if (ally.second.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR || ally.second.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED)
								continue;//Skip these units, they cannot splash on one another
							else
								break;
						case sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED:
							if (ally.second.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED || ally.second.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_WIDOWMINE)
								continue;//Skip these units, they cannot splash on one another
							else
								break;
						default:
							Util::DisplayError("Unit is marked has able to hit allies, but isn't supposed to be able.", "0x00000014", m_bot, false);
							break;
					}

					float distSq = Util::DistSq(target->pos, allyPtr->pos);
					if (zone1Range == 0 || distSq < pow(zone1Range + allyPtr->radius, 2))
					{
						float damage = Util::GetDamageForTarget(attacker, target, m_bot) * zone1Percentage;
						if (damage >= target->health + target->shield)
						{
							damage = target->health + (target->shield * 0.5) + target->health_max + (target->shield_max * 0.5);
						}
						if (UnitType(allyPtr->unit_type, m_bot).isBuilding())
						{
							damage *= 0.10f;
						}
						damageScore -= damage * 2;
					}
					else  if (zone2Range != 0 && distSq < pow(zone2Range + allyPtr->radius, 2))
					{
						float damage = Util::GetDamageForTarget(attacker, target, m_bot) * zone2Percentage;
						if (damage >= target->health)
						{
							damage = target->health + (target->shield * 0.5) + target->health_max + (target->shield_max * 0.5);
						}
						if (UnitType(allyPtr->unit_type, m_bot).isBuilding())
						{
							damage *= 0.10f;
						}
						damageScore -= damage * 2;
					}
					else  if (zone3Range != 0 && distSq < pow(zone3Range + allyPtr->radius, 2))
					{
						float damage = Util::GetDamageForTarget(attacker, target, m_bot) * zone3Percentage;
						if (damage >= target->health)
						{
							damage = target->health + (target->shield * 0.5) + target->health_max + (target->shield_max * 0.5);
						}
						if (UnitType(allyPtr->unit_type, m_bot).isBuilding())
						{
							damage *= 0.10f;
						}
						damageScore -= damage * 2;
					}
				}
			}
		}
	}
						
	return damageScore;
}