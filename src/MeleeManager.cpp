#include "MeleeManager.h"
#include "Util.h"
#include "CCBot.h"

MeleeManager::MeleeManager(CCBot & bot)
    : MicroManager(bot)
{

}

void MeleeManager::setTargets(const std::vector<Unit> & targets)
{
    std::vector<Unit> meleeUnitTargets;
    for (auto & target : targets)
    {
        if (!target.isValid()) { continue; }
        if (target.isFlying()) { continue; }
        if (target.getType().isEgg()) { continue; }
        if (target.getType().isLarva()) { continue; }

        meleeUnitTargets.push_back(target);
    }
    m_targets = meleeUnitTargets;
}

void MeleeManager::executeMicro()
{
    const std::vector<Unit> & meleeUnits = getUnits();
	const bool hasBarracks = !m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKS).empty();
	const bool hasRefinery = !m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_REFINERY).empty();
	const bool workerRushStrat = m_bot.Strategy().getStartingStrategy() == WORKER_RUSH;
	if (workerRushStrat)
	{
		if (!m_waitForProbesHealed)
		{
			if (m_healingProbes.size() >= meleeUnits.size() - 1)
			{
				m_waitForProbesHealed = true;
			}
		}
		else if (m_healingProbes.size() <= 1)
		{
			m_waitForProbesHealed = false;
		}
	}

    // for each meleeUnit
    for (auto & meleeUnit : meleeUnits)
    {
        BOT_ASSERT(meleeUnit.isValid(), "melee unit is null");

		bool isProbe = meleeUnit.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_PROBE;
		bool isHealing = false;
		if (isProbe)
		{
			if (meleeUnit.getShields() <= 5)
			{
				m_healingProbes.emplace(meleeUnit.getTag());
				isHealing = true;
			}
			else if (meleeUnit.getShields() == meleeUnit.getUnitPtr()->shield_max)
			{
				m_healingProbes.erase(meleeUnit.getTag());
			}
			else if (m_healingProbes.find(meleeUnit.getTag()) != m_healingProbes.end())
			{
				isHealing = true;
			}
			if (!isHealing && m_waitForProbesHealed)
			{
				isHealing = true;
			}
		}

		bool flee = false;
		bool noTarget = false;
		if (m_order.getType() == SquadOrderTypes::Retreat)
		{
			flee = true;
		}
        else if (m_order.getType() == SquadOrderTypes::Attack || m_order.getType() == SquadOrderTypes::Defend)
        {
			if (m_targets.empty())
			{
				noTarget = true;
			}
			else
			{
				// find the best target for this meleeUnit
				Unit target = getTarget(meleeUnit, m_targets);
				if (!target.isValid())
				{
					noTarget = true;
				}
				else
				{
					bool injured = false;
					bool injuredUnitInDanger = false;
					if (isProbe ? isHealing : (meleeUnit.getHitPointsPercentage() <= 25))
					{
						injured = true;
						for (const auto & threat : m_targets)
						{
							const auto enemyRange = Util::GetAttackRangeForTarget(threat.getUnitPtr(), meleeUnit.getUnitPtr(), m_bot);
							if (Util::Dist(threat, meleeUnit) < enemyRange + (isProbe ? 2.f : 0.75f))
							{
								injuredUnitInDanger = true;
								break;
							}
						}
					}

					// If it is a worker that just attacked a non building unit, we want it to mineral walk back
					if (meleeUnit.getType().isWorker() && (meleeUnit.getUnitPtr()->weapon_cooldown > 10.f || injuredUnitInDanger) && Util::getSpeedOfUnit(target.getUnitPtr(), m_bot) > 0.f && m_squad->getName() != "ScoutDefense")
					{
						auto base = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
						auto & mineral = base->getMinerals();
						if (mineral.size() > 0)
						{
							const auto action = UnitAction(MicroActionType::RightClick, mineral[0].getUnitPtr(), false, 0, "mineral walk", m_squad->getName());
							m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
						}
					}
					else
					{
						const sc2::Unit* repairTarget = nullptr;
						const sc2::Unit* closestRepairTarget = nullptr;
						float distanceToClosestRepairTarget = 0;
						// If the melee unit is a slightly injured worker
						if (meleeUnit.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_SCV && meleeUnit.getHitPointsPercentage() <= 50 && m_bot.GetMinerals() > (!hasRefinery ? 80 : hasBarracks ? 55 : 0) && (m_bot.Strategy().isWorkerRushed() || workerRushStrat))
						{
							const float range = Util::GetAttackRangeForTarget(meleeUnit.getUnitPtr(), target.getUnitPtr(), m_bot);
							const float distSq = Util::DistSq(meleeUnit, target);
							// If the worker is too far from its target to attack it
							if (meleeUnit.getUnitPtr()->weapon_cooldown > 0.f || distSq > range * range || injured)
							{
								// Check all other workers to check if there is one injured close enough to repair it
								for (auto & otherWorker : meleeUnits)
								{
									if (otherWorker == meleeUnit)
										continue;
									if (!otherWorker.getType().isWorker())
										continue;
									if (otherWorker.getHitPointsPercentage() > 99.f)
										continue;

									const float otherWorkerDistSq = Util::DistSq(meleeUnit, otherWorker);
									if (otherWorkerDistSq <= range * range)
									{
										repairTarget = otherWorker.getUnitPtr();
										break;
									}
									if (!closestRepairTarget || otherWorkerDistSq < distanceToClosestRepairTarget)
									{
										closestRepairTarget = otherWorker.getUnitPtr();
										distanceToClosestRepairTarget = otherWorkerDistSq;
									}
								}
							}
						}

						if (repairTarget || (injured && closestRepairTarget))
						{
							if (!repairTarget)
								repairTarget = closestRepairTarget;
							const auto action = UnitAction(MicroActionType::RightClick, repairTarget, false, 0, "repair", m_squad->getName());
							m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
						}
						else if (!injured || workerRushStrat)
						{
							// attack the target if we can see it, otherwise move towards it
							if (target.getUnitPtr()->last_seen_game_loop == m_bot.GetCurrentFrame())
							{
								const auto action = UnitAction(MicroActionType::AttackUnit, target.getUnitPtr(), false, 0, "attack target", m_squad->getName());
								m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
							}
							else
							{
								auto movePosition = target.getPosition();
								// If there is an enemy worker hidding in our base, explore the tiles of the base
								if (m_bot.Strategy().enemyHasWorkerHiddingInOurMain())
								{
									CCTilePosition closestUnexploredTile;
									float closestDistance = -1;
									const auto & baseTiles = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getBaseTiles();
									for (const auto & baseTile : baseTiles)
									{
										if (!m_bot.Map().isExplored(baseTile))
										{
											auto baseTilePosition = Util::GetPosition(baseTile);
											float distance = Util::DistSq(meleeUnit, baseTilePosition) + Util::DistSq(target, baseTilePosition);
											if (closestDistance < 0 || distance < closestDistance)
											{
												closestUnexploredTile = baseTile;
												closestDistance = distance;
											}
										}
									}
									if (closestDistance >= 0)
									{
										movePosition = Util::GetPosition(closestUnexploredTile);
									}
								}
								const auto action = UnitAction(MicroActionType::Move, movePosition, false, 0, "move towards target", m_squad->getName());
								m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
							}
						}
					}
				}
            }
        }
		
		if(flee || noTarget)
		{
			if (Util::PathFinding::GetTotalInfluenceOnTile(Util::GetTilePosition(meleeUnit.getPosition()), meleeUnit.getUnitPtr(), m_bot) > 0)
			{
				CCPosition goal = m_order.getPosition();
				if (m_bot.Workers().getWorkerData().isProxyWorker(meleeUnit))
					goal = m_bot.GetEnemyStartLocations().empty() ? m_bot.Map().center() : m_bot.GetEnemyStartLocations()[0];
				CCPosition fleePosition = Util::PathFinding::FindOptimalPathToSafety(meleeUnit.getUnitPtr(), goal, true, m_bot);
				if (fleePosition != CCPosition())
				{
					const auto action = UnitAction(MicroActionType::Move, fleePosition, false, 0, "flee", m_squad->getName());
					m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
					flee = false;
				}
			}
			auto enemyBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
			if (noTarget && workerRushStrat && enemyBase && enemyBase->containsPositionApproximative(m_order.getPosition()))
			{
				auto enemyMineral = enemyBase->getMinerals()[0].getUnitPtr();
				const auto action = UnitAction(MicroActionType::RightClick, enemyMineral, false, 0, "mineral walk", m_squad->getName());
				m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
			}
			else if (flee || noTarget)
			{
				const auto action = UnitAction(MicroActionType::Move, m_order.getPosition(), false, 0, "flee", m_squad->getName());
				m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
			}
		}

        /*if (m_bot.Config().DrawUnitTargetInfo)
        {
            // TODO: draw the line to the unit's target
        }*/
    }
}

// get a target for the meleeUnit to attack
Unit MeleeManager::getTarget(Unit meleeUnit, const std::vector<Unit> & targets) const
{
    BOT_ASSERT(meleeUnit.isValid(), "null melee unit in getTarget");

    float highestPriority = 0.f;
    Unit bestTarget;
	const auto unitHeight = Util::TerrainHeight(meleeUnit.getPosition());
	const auto unitBaseLocation = m_bot.Bases().getBaseContainingPosition(meleeUnit.getPosition());
	sc2::Units allTargets;	// Only needed for splash computation
	if (UnitType::hasSplashingAttack(meleeUnit.getUnitPtr()->unit_type, false) || UnitType::hasSplashingAttack(meleeUnit.getUnitPtr()->unit_type, true))
	{
		Util::CCUnitsToSc2Units(targets, allTargets);
	}

    // for each target possiblity
    for (auto & targetUnit : targets)
    {
        BOT_ASSERT(targetUnit.isValid(), "null target unit in getTarget");

    	// We don't want workers to leave base to fight
		if (meleeUnit.getType().isWorker() && m_squad->getName() != "ScoutDefense" && m_squad->getName() != "MainAttack" && Util::TerrainHeight(targetUnit.getPosition()) != unitHeight && unitBaseLocation != m_bot.Bases().getBaseContainingPosition(targetUnit.getPosition()))
			continue;
    	
		const float priority = getAttackPriority(meleeUnit.getUnitPtr(), targetUnit.getUnitPtr(), allTargets, false, false, true);

        if (!bestTarget.isValid() || priority > highestPriority)
        {
			highestPriority = priority;
            bestTarget = targetUnit;
        }
    }

    return bestTarget;
}