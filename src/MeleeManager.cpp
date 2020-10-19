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

    // for each meleeUnit
    for (auto & meleeUnit : meleeUnits)
    {
        BOT_ASSERT(meleeUnit.isValid(), "melee unit is null");

		bool flee = false;
		if (m_order.getType() == SquadOrderTypes::Retreat)
		{
			flee = true;
		}
        else if (m_order.getType() == SquadOrderTypes::Attack || m_order.getType() == SquadOrderTypes::Defend)
        {
			if (m_targets.empty())
			{
				flee = true;
			}
			else
			{
				// find the best target for this meleeUnit
				Unit target = getTarget(meleeUnit, m_targets);
				if (!target.isValid())
				{
					flee = true;
				}
				else
				{
					bool injured = false;
					bool injuredUnitInDanger = false;
					if (meleeUnit.getHitPointsPercentage() <= 25)
					{
						injured = true;
						for (const auto & threat : m_targets)
						{
							const auto enemyRange = Util::GetAttackRangeForTarget(threat.getUnitPtr(), meleeUnit.getUnitPtr(), m_bot);
							if (Util::Dist(threat, meleeUnit) < enemyRange + 0.75f)
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
							Micro::SmartRightClick(meleeUnit.getUnitPtr(), mineral[0].getUnitPtr(), m_bot);
						}
						else
						{
							Micro::SmartMove(meleeUnit.getUnitPtr(), Util::GetPosition(base->getCenterOfMinerals()), m_bot);
						}
					}
					else
					{
						const sc2::Unit* repairTarget = nullptr;
						const sc2::Unit* closestRepairTarget = nullptr;
						float distanceToClosestRepairTarget = 0;
						// If the melee unit is a worker
						if (meleeUnit.getType().isWorker() && m_bot.GetMinerals() > (hasBarracks ? 55 : 0) && (m_bot.Strategy().isWorkerRushed() || m_bot.Strategy().getStartingStrategy() == WORKER_RUSH))
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
							Micro::SmartRepair(meleeUnit.getUnitPtr(), repairTarget, m_bot);
						}
						else if (!injured)
						{
							// attack the target if we can see it, otherwise move towards it
							if (target.getUnitPtr()->last_seen_game_loop == m_bot.GetCurrentFrame())
								Micro::SmartAttackUnit(meleeUnit.getUnitPtr(), target.getUnitPtr(), m_bot);
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
								Micro::SmartMove(meleeUnit.getUnitPtr(), movePosition, m_bot);
							}
						}
					}
				}
            }
        }
		
		if(flee)
		{
			if (Util::PathFinding::GetTotalInfluenceOnTile(Util::GetTilePosition(meleeUnit.getPosition()), meleeUnit.getUnitPtr(), m_bot) > 0)
			{
				CCPosition goal = m_order.getPosition();
				if (m_bot.Workers().getWorkerData().isProxyWorker(meleeUnit))
					goal = m_bot.GetEnemyStartLocations().empty() ? m_bot.Map().center() : m_bot.GetEnemyStartLocations()[0];
				CCPosition fleePosition = Util::PathFinding::FindOptimalPathToSafety(meleeUnit.getUnitPtr(), goal, true, m_bot);
				if (fleePosition != CCPosition())
				{
					Micro::SmartMove(meleeUnit.getUnitPtr(), fleePosition, m_bot);
					flee = false;
				}
			}
			if (flee)
			{
				Micro::SmartMove(meleeUnit.getUnitPtr(), m_order.getPosition(), m_bot);
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

    // for each target possiblity
    for (auto & targetUnit : targets)
    {
        BOT_ASSERT(targetUnit.isValid(), "null target unit in getTarget");

    	// We don't want workers to leave base to fight
		if (meleeUnit.getType().isWorker() && m_squad->getName() != "ScoutDefense" && Util::TerrainHeight(targetUnit.getPosition()) != unitHeight && unitBaseLocation != m_bot.Bases().getBaseContainingPosition(targetUnit.getPosition()))
			continue;
    	
        const float priority = getAttackPriority(meleeUnit.getUnitPtr(), targetUnit.getUnitPtr(), false, false, true);

        if (!bestTarget.isValid() || priority > highestPriority)
        {
			highestPriority = priority;
            bestTarget = targetUnit;
        }
    }

    return bestTarget;
}