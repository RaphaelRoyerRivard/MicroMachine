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
	bool hasBarracks = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKS).size() > 0;

    // for each meleeUnit
    for (auto & meleeUnit : meleeUnits)
    {
        BOT_ASSERT(meleeUnit.isValid(), "melee unit is null");

        // if the order is to attack or defend
        if (m_order.getType() == SquadOrderTypes::Attack || m_order.getType() == SquadOrderTypes::Defend)
        {
            if (!m_targets.empty())
            {
				bool injured = false;
				bool injuredUnitInDanger = false;
            	if (meleeUnit.getHitPointsPercentage() <= 25)
            	{
					injured = true;
            		for (const auto target : m_targets)
            		{
						const auto enemyRange = Util::GetAttackRangeForTarget(target.getUnitPtr(), meleeUnit.getUnitPtr(), m_bot);
            			if (Util::Dist(target, meleeUnit) < enemyRange + 0.75f)
            			{
							injuredUnitInDanger = true;
							break;
            			}
            		}
            	}
            	
				// find the best target for this meleeUnit
				Unit target = getTarget(meleeUnit, m_targets);
            	
				// If it is a worker that just attacked a non building unit, we want it to mineral walk back
				if (meleeUnit.getType().isWorker() && (meleeUnit.getUnitPtr()->weapon_cooldown > 10.f || injuredUnitInDanger) && Util::getSpeedOfUnit(target.getUnitPtr(), m_bot) > 0.f)
				{
					Micro::SmartRightClick(meleeUnit.getUnitPtr(), m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getMinerals()[0].getUnitPtr(), m_bot);
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
						// attack the target
						Micro::SmartAttackUnit(meleeUnit.getUnitPtr(), target.getUnitPtr(), m_bot);
					}
				}
            }
            // if there are no targets
            else
            {
                // if we're not near the order position
                if (Util::DistSq(meleeUnit, m_order.getPosition()) > 4 * 4)
                {
                    // move to it
                    meleeUnit.move(m_order.getPosition());
                }
            }
        }
		else if(m_order.getType() == SquadOrderTypes::Retreat)
		{
			const BaseLocation* closestBaseLocation = m_bot.Bases().getClosestOccupiedBaseLocationForUnit(meleeUnit);
			if (closestBaseLocation)
			{
				CCPosition fleePosition = Util::PathFinding::FindOptimalPathToSafety(meleeUnit.getUnitPtr(), closestBaseLocation->getPosition(), true, m_bot);
				if (fleePosition != CCPosition())
				{
					Micro::SmartMove(meleeUnit.getUnitPtr(), fleePosition, m_bot);
				}
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

    // for each target possiblity
    for (auto & targetUnit : targets)
    {
        BOT_ASSERT(targetUnit.isValid(), "null target unit in getTarget");

        const float priority = getAttackPriority(meleeUnit.getUnitPtr(), targetUnit.getUnitPtr(), false, false);

        if (!bestTarget.isValid() || priority > highestPriority)
        {
			highestPriority = priority;
            bestTarget = targetUnit;
        }
    }

    return bestTarget;
}