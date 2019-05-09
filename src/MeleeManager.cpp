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

    // for each meleeUnit
    for (auto & meleeUnit : meleeUnits)
    {
        BOT_ASSERT(meleeUnit.isValid(), "melee unit is null");

        // if the order is to attack or defend
        if (m_order.getType() == SquadOrderTypes::Attack || m_order.getType() == SquadOrderTypes::Defend)
        {
            // run away if we meet the retreat critereon
            if (meleeUnitShouldRetreat(meleeUnit, m_targets))
            {
                CCPosition fleeTo(Util::GetPosition(m_bot.Bases().getClosestBasePosition(meleeUnit.getUnitPtr())));

                meleeUnit.move(fleeTo);
            }
            // if there are targets
            else if (!m_targets.empty())
            {
				// If it is a worker that just attacked, we want it to mineral walk back
				if (meleeUnit.getType().isWorker() && meleeUnit.getUnitPtr()->weapon_cooldown > 10.f)
				{
					Micro::SmartRightClick(meleeUnit.getUnitPtr(), m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getMinerals()[0].getUnitPtr(), m_bot);
				}
				else
				{
					// find the best target for this meleeUnit
					Unit target = getTarget(meleeUnit, m_targets);
					const sc2::Unit* repairTarget = nullptr;
					// If the melee unit is a worker
					if (meleeUnit.getType().isWorker() && m_bot.GetMinerals() > 0)
					{
						const float range = Util::GetAttackRangeForTarget(meleeUnit.getUnitPtr(), target.getUnitPtr(), m_bot);
						const float distSq = Util::DistSq(meleeUnit, target);
						// If the worker is too far from its target to attack it
						if (meleeUnit.getUnitPtr()->weapon_cooldown > 0.f || distSq > range * range)
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
							}
						}
					}

					if (repairTarget)
					{
						Micro::SmartRepair(meleeUnit.getUnitPtr(), repairTarget, m_bot);
					}
					else
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
				CCPosition fleePosition = Util::PathFinding::FindOptimalPathToSafety(meleeUnit.getUnitPtr(), closestBaseLocation->getPosition(), m_bot);
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

        const float priority = getAttackPriority(meleeUnit.getUnitPtr(), targetUnit.getUnitPtr());

        if (!bestTarget.isValid() || priority > highestPriority)
        {
			highestPriority = priority;
            bestTarget = targetUnit;
        }
    }

    return bestTarget;
}

// get the attack priority of a type in relation to a zergling
float MeleeManager::getAttackPriority(const sc2::Unit* attacker, const sc2::Unit* target) const
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
		if (targetUnit.getType().isWorker() && m_order.getType() != SquadOrderTypes::Defend)
		{
			if (attacker->unit_type != sc2::UNIT_TYPEID::TERRAN_HELLION)
			{
				workerBonus = 2.f;
			}

			// Reduce priority for workers that are going in a refinery
			const auto enemyRace = m_bot.GetPlayerRace(Players::Enemy);
			const auto enemyRefineryType = UnitType::getEnemyRefineryType(enemyRace);
			for (auto & refinery : m_bot.GetKnownEnemyUnits(enemyRefineryType))
			{
				const float refineryDist = Util::DistSq(refinery, target->pos);
				if (refineryDist < 2.5 * 2.5)
				{
					const CCPosition facingVector = Util::getFacingVector(target);
					if (Dot2D(facingVector, refinery.getPosition() - target->pos) > 0.99f)
					{
						workerBonus *= 0.5f;
					}
				}
			}
		}


		float nonThreateningModifier = targetDps == 0.f ? 0.5f : 1.f;								//targets that cannot hit our unit are less prioritized
		if (targetUnit.getType().isAttackingBuilding())
		{
			nonThreateningModifier = targetDps == 0.f ? 1.f : 0.5f;		//for buildings, we prefer targetting them with units that will not get attacked back
		}
		const float flyingDetectorModifier = target->is_flying && UnitType::isDetector(target->unit_type) ? 2.f : 1.f;
		const float minionModifier = target->unit_type == sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR ? 0.1f : 1.f;	//units that can be respawned should be less prioritized
		return (targetDps + unitDps - healthValue + distanceValue * 50) * workerBonus * nonThreateningModifier * minionModifier * invisModifier * flyingDetectorModifier;
	}

	return (distanceValue * 50 - healthValue) * invisModifier / 100.f;		//we do not want non combat buildings to have a higher priority than other units
}

bool MeleeManager::meleeUnitShouldRetreat(Unit meleeUnit, const std::vector<Unit> & targets)
{
    // TODO: should melee units ever retreat?
    return false;
}
