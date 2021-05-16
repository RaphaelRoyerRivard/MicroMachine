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
	if (!m_stackingMineral.isValid())
	{
		m_bot.StartProfiling("0.10.4.1.4.1        identifyStackingMinerals");
		identifyStackingMinerals();
		m_bot.StopProfiling("0.10.4.1.4.1        identifyStackingMinerals");
	}

	const bool workerRushed = m_bot.Strategy().isWorkerRushed();
	if (workerRushed && m_order.getType() == SquadOrderTypes::Defend)
	{
		// Not working very well, the stacked workers push each other out of range of their target when they attack
		m_bot.StartProfiling("0.10.4.1.4.2        areUnitsStackedUp");
		bool stacked = areUnitsStackedUp();
		m_bot.StopProfiling("0.10.4.1.4.2        areUnitsStackedUp");
		if (!stacked)
		{
			m_bot.StartProfiling("0.10.4.1.4.3        stackUnits");
			stackUnits();
			m_bot.StopProfiling("0.10.4.1.4.3        stackUnits");
		}
		else
		{
			m_bot.StartProfiling("0.10.4.1.4.4        microStack");
			microStack();
			m_bot.StopProfiling("0.10.4.1.4.4        microStack");
		}

		// Not working well enough, backstabers get killed way too fast
		//if (m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Protoss)
		//	updateBackstabbers();
	}
    const std::vector<Unit> & meleeUnits = getUnits();
	const bool workerRushStrat = m_bot.Strategy().getStartingStrategy() == WORKER_RUSH;
	if (workerRushStrat)
	{
		m_bot.StartProfiling("0.10.4.1.4.5        waitForProbesHealed");
		for (auto it = m_healingProbes.begin(); it != m_healingProbes.end();)
		{
			auto unit = m_bot.GetUnit(*it);
			if (!unit.isValid() || !unit.isAlive())
				it = m_healingProbes.erase(it);
			else
				it++;
		}
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
		m_bot.StopProfiling("0.10.4.1.4.5        waitForProbesHealed");
	}

	m_bot.StartProfiling("0.10.4.1.4.6        microUnit");
    // for each meleeUnit
    for (auto & meleeUnit : meleeUnits)
    {
		microUnit(meleeUnit);
    }
	m_bot.StopProfiling("0.10.4.1.4.6        microUnit");
}

void MeleeManager::microUnit(const Unit & meleeUnit)
{
	// We do not want to micro further a unit in the stack, it is microed elsewhere
	if (m_stackedUnits.find(meleeUnit) != m_stackedUnits.end())
		return;

	const std::vector<Unit> & meleeUnits = getUnits();
	const bool workerRushed = m_bot.Strategy().isWorkerRushed();
	const bool hasBarracks = !m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKS).empty();
	const bool hasRefinery = !m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_REFINERY).empty();
	const bool workerRushStrat = m_bot.Strategy().getStartingStrategy() == WORKER_RUSH;
	bool isProbe = meleeUnit.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_PROBE;
	bool isHealing = false;
	if (isProbe)
	{
		m_bot.StartProfiling("0.10.4.1.4.6.1         probeChecks");
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
		m_bot.StopProfiling("0.10.4.1.4.6.1         probeChecks");
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
			m_bot.StartProfiling("0.10.4.1.4.6.2         getTarget");
			// find the best target for this meleeUnit
			Unit target = getTarget(meleeUnit, m_targets);
			m_bot.StopProfiling("0.10.4.1.4.6.2         getTarget");
			if (!target.isValid())
			{
				noTarget = true;
			}
			else
			{
				auto & backstabbers = m_bot.Commander().Combat().getBackstabbers();
				bool isBackstabber = backstabbers.find(meleeUnit) != backstabbers.end();
				bool injured = false;
				bool injuredUnitInDanger = false;
				if (!isBackstabber && (isProbe ? isHealing : (meleeUnit.getHitPointsPercentage() <= 25)))
				{
					m_bot.StartProfiling("0.10.4.1.4.6.3         injured");
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
					m_bot.StopProfiling("0.10.4.1.4.6.3         injured");
				}

				bool closeToTarget = Util::Dist(meleeUnit, target) <= Util::GetAttackRangeForTarget(meleeUnit.getUnitPtr(), target.getUnitPtr(), m_bot) + 0.5f;
				float minWeaponCooldownToMineralWalk = closeToTarget ? (isBackstabber ? 0.f : 5.f) : 10.f;
				// If it is a worker that just attacked a non building unit, we want it to mineral walk back (or forward when it's a backstabber)
				m_bot.StartProfiling("0.10.4.1.4.6.4         shouldMineralWalk");
				bool shouldMineralWalk = meleeUnit.getType().isWorker() && (meleeUnit.getUnitPtr()->weapon_cooldown > minWeaponCooldownToMineralWalk || injuredUnitInDanger) && Util::getSpeedOfUnit(target.getUnitPtr(), m_bot) > 0.f && m_squad->getName() != "ScoutDefense";
				m_bot.StopProfiling("0.10.4.1.4.6.4         shouldMineralWalk");
				if (shouldMineralWalk)
				{
					m_bot.StartProfiling("0.10.4.1.4.6.5         mineralWalk");
					auto & mineral = isBackstabber ? m_enemyMineral : m_stackingMineral;
					if (mineral.isValid())
					{
						const auto action = UnitAction(MicroActionType::RightClick, mineral.getUnitPtr(), false, 0, "mineral walk", m_squad->getName());
						m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
					}
					m_bot.StopProfiling("0.10.4.1.4.6.5         mineralWalk");
				}
				else
				{
					m_bot.StartProfiling("0.10.4.1.4.6.6         getRepairTarget");
					const sc2::Unit* repairTarget = nullptr;
					const sc2::Unit* closestRepairTarget = nullptr;
					float distanceToClosestRepairTarget = 0;
					// If the melee unit is a slightly injured worker
					if (meleeUnit.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_SCV && meleeUnit.getHitPointsPercentage() <= 50 && m_bot.GetMinerals() > (!hasRefinery ? 80 : hasBarracks ? 55 : 0) && (workerRushed || workerRushStrat))
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
					m_bot.StopProfiling("0.10.4.1.4.6.6         getRepairTarget");

					if (repairTarget || (injured && closestRepairTarget))
					{
						m_bot.StartProfiling("0.10.4.1.4.6.7         repair");
						if (!repairTarget)
							repairTarget = closestRepairTarget;
						const auto action = UnitAction(MicroActionType::RightClick, repairTarget, false, 0, "repair", m_squad->getName());
						m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
						m_bot.StopProfiling("0.10.4.1.4.6.7         repair");
					}
					else if (!injured || workerRushStrat)
					{
						// attack the target if we can see it, otherwise move towards it
						if (target.getUnitPtr()->last_seen_game_loop == m_bot.GetCurrentFrame())
						{
							m_bot.StartProfiling("0.10.4.1.4.6.8         attack");
							const auto action = UnitAction(MicroActionType::AttackUnit, target.getUnitPtr(), false, 0, "attack target", m_squad->getName());
							m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
							m_bot.StopProfiling("0.10.4.1.4.6.8         attack");
						}
						else
						{
							m_bot.StartProfiling("0.10.4.1.4.6.9         move");
							auto movePosition = target.getPosition();
							// If there is an enemy worker hidding in our base, explore the tiles of the base
							if (m_bot.Strategy().enemyHasWorkerHiddingInOurMain())
							{
								m_bot.StartProfiling("0.10.4.1.4.6.9.1          exploreBaseTiles");
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
								m_bot.StopProfiling("0.10.4.1.4.6.9.1          exploreBaseTiles");
							}
							const auto action = UnitAction(MicroActionType::Move, movePosition, false, 0, "move towards target", m_squad->getName());
							m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
							m_bot.StopProfiling("0.10.4.1.4.6.9         move");
						}
					}
				}
			}
		}
	}

	if (flee || noTarget)
	{
		if (flee && workerRushed && m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_REAPER) > 0)
		{
			auto ccs = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER);
			if (!ccs.empty() && ccs[0].getUnitPtr()->passengers.size() < 5)
			{
				const auto action = UnitAction(MicroActionType::RightClick, ccs[0].getUnitPtr(), true, 0, "hide", m_squad->getName());
				m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
				return;
			}
		}
		if (Util::PathFinding::GetTotalInfluenceOnTile(Util::GetTilePosition(meleeUnit.getPosition()), meleeUnit.getUnitPtr(), m_bot) > 0)
		{
			m_bot.StartProfiling("0.10.4.1.4.6.10         flee");
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
			m_bot.StopProfiling("0.10.4.1.4.6.10         flee");
		}
		auto enemyBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
		if (noTarget && workerRushStrat && enemyBase && enemyBase->containsPositionApproximative(m_order.getPosition()))
		{
			m_bot.StartProfiling("0.10.4.1.4.6.11         mineralWalkEnemyBase");
			auto enemyMineral = enemyBase->getMinerals()[0].getUnitPtr();
			const auto action = UnitAction(MicroActionType::RightClick, enemyMineral, false, 0, "mineral walk", m_squad->getName());
			m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
			m_bot.StopProfiling("0.10.4.1.4.6.11         mineralWalkEnemyBase");
		}
		else if (flee || noTarget)
		{
			m_bot.StartProfiling("0.10.4.1.4.6.12         moveBack");
			const auto action = UnitAction(MicroActionType::Move, m_order.getPosition(), false, 0, "flee", m_squad->getName());
			m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
			m_bot.StopProfiling("0.10.4.1.4.6.12         moveBack");
		}
	}

	/*if (m_bot.Config().DrawUnitTargetInfo)
	{
		// TODO: draw the line to the unit's target
	}*/
}

bool MeleeManager::areUnitsStackedUp()
{
	if (m_bot.Commander().Combat().haveWorkersStacked())
		return true;
	const std::vector<Unit> & meleeUnits = getUnits();
	if (meleeUnits.size() < 10)
		return false;
	for (auto & meleeUnit : meleeUnits)
	{
		for (auto & otherMeleeUnit : meleeUnits)
		{
			float distSq = Util::DistSq(meleeUnit, otherMeleeUnit);
			if (distSq > (m_mineralPocket ? 0.4f * 0.4f : 2 * 2))
				return false;
		}
	}
	m_bot.Commander().Combat().setWorkersHaveStacked(true);
	for (auto & meleeUnit : meleeUnits)
	{
		m_stackedUnits.insert(meleeUnit);
	}
	return true;
}

void MeleeManager::stackUnits()
{
	const std::vector<Unit> & meleeUnits = getUnits();
	for (auto & meleeUnit : meleeUnits)
	{
		auto & unitAction = m_bot.Commander().Combat().GetUnitAction(meleeUnit.getUnitPtr());
		if (unitAction.prioritized && (unitAction.microActionType == MicroActionType::AttackUnit || unitAction.microActionType == MicroActionType::AttackMove) && meleeUnit.getUnitPtr()->weapon_cooldown == 0.f)
			continue;

		if (Util::DistSq(meleeUnit, m_stackPosition) > 1.5f * 1.5f)
		{
			const auto action = UnitAction(MicroActionType::Move, m_stackPosition, true, 0, "stack", m_squad->getName());
			m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
		}
		else
		{
			// Not using the Action manager of the CombatCommander because we need to spam the action
			meleeUnit.rightClick(m_stackingMineral);
		}
	}
}

void MeleeManager::identifyStackingMinerals()
{
	for (int i = 0; i < 2; ++i)
	{
		auto mainBase = m_bot.Bases().getPlayerStartingBaseLocation(i == 0 ? Players::Self : Players::Enemy);
		if (mainBase && !mainBase->getFarMinerals().empty())
		{
			for (auto & farMineral : mainBase->getFarMinerals())
			{
				int closeMineralCount = 0;
				for (auto & closeMineral : mainBase->getCloseMinerals())
				{
					float dist = Util::DistSq(farMineral, closeMineral);
					if (dist < 2.5f)
					{
						if (++closeMineralCount == 2)
							break;
					}
				}
				if (closeMineralCount == 2)
				{
					(i == 0 ? m_stackingMineral : m_enemyMineral) = farMineral;
					if (i == 0)
						m_mineralPocket = true;
					break;
				}
			}
			if (!(i == 0 ? m_stackingMineral : m_enemyMineral).isValid())
				(i == 0 ? m_stackingMineral : m_enemyMineral) = mainBase->getFarMinerals()[0];
		}
		else
		{
			for (auto & neutralUnit : m_bot.GetNeutralUnits())
			{
				if (neutralUnit.second.getType().isMineral())
				{
					if (Util::DistSq(neutralUnit.second, i == 0 ? m_bot.GetStartLocation() : m_bot.GetEnemyStartLocations()[0]) < 10 * 10)
					{
						(i == 0 ? m_stackingMineral : m_enemyMineral) = neutralUnit.second;
					}
				}
			}
		}
	}
	if (m_stackPosition == CCPosition())
	{
		auto towardsBase = Util::Normalized(m_bot.GetStartLocation() - m_stackingMineral.getPosition());
		m_stackPosition = m_stackingMineral.getPosition() + towardsBase * 1.5f;
	}
}

/*
This function doesn't work to one shot enemy workers.
*/
void MeleeManager::microStack()
{
	float closestEnemyDistanceToBase = 0.f;
	for (auto & target : m_targets)
	{
		auto dist = Util::DistSq(target, m_bot.GetStartLocation());
		if (closestEnemyDistanceToBase == 0.f || dist < closestEnemyDistanceToBase)
			closestEnemyDistanceToBase = dist;
	}
	/*int canAttackCount = 0;
	for (int i = 0; i < 2; i++)
	{
		bool shouldAttack = canAttackCount >= std::min(int(m_stackedUnits.size()), 9);*/
		for (auto it = m_stackedUnits.begin(); it != m_stackedUnits.end(); )
		{
			auto & meleeUnit = *it;
			/*if (meleeUnit.getUnitPtr()->weapon_cooldown != 0.f)
			{
				it = m_stackedUnits.erase(it);
				continue;
			}*/
			bool canAttack = false;
			auto dist = Util::DistSq(meleeUnit, m_bot.GetStartLocation());
			if (dist > closestEnemyDistanceToBase)
				canAttack = true;
			else
			{
				Unit target = getTarget(meleeUnit, m_targets);
				if (target.isValid())
				{
					auto range = Util::GetAttackRangeForTarget(meleeUnit.getUnitPtr(), target.getUnitPtr(), m_bot) / 3;
					auto dist = Util::DistSq(meleeUnit, target);
					if (dist <= range * range)
					{
						canAttack = true;
					}
				}
			}
			bool incrementIterator = true;
			/*if (i == 0) {
				canAttackCount += canAttack ? 1 : 0;
			}
			else*/
			{
				auto & unitAction = m_bot.Commander().Combat().GetUnitAction(meleeUnit.getUnitPtr());
				// Ignore units that are executing a prioritized action
				if (!unitAction.prioritized && !m_bot.Commander().Combat().ShouldSkipFrame(meleeUnit.getUnitPtr()))
				{
					if (canAttack)// && shouldAttack)
					{
						//const auto action = UnitAction(MicroActionType::AttackUnit, target.getUnitPtr(), true, 10, "attack target", m_squad->getName());
						const auto action = UnitAction(MicroActionType::AttackMove, m_stackPosition, false, 0, "stack attack", m_squad->getName());
						m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
						it = m_stackedUnits.erase(it);
						incrementIterator = false;
					}
					else
					{
						const auto action = UnitAction(MicroActionType::RightClick, m_enemyMineral.getUnitPtr(), false, 0, "move stack", m_squad->getName());
						m_bot.Commander().Combat().PlanAction(meleeUnit.getUnitPtr(), action);
					}
				}
			}
			if (incrementIterator)
				++it;
		}
	//}
	//if (m_stackedUnits.empty())
	//	m_stacked = false;
}

void MeleeManager::updateBackstabbers()
{
	const std::vector<Unit> & meleeUnits = getUnits();
	int minBackstabbersCount = int(std::floor(meleeUnits.size() / 2));
	auto & backstabbers = m_bot.Commander().Combat().getBackstabbers();
	for (auto it = backstabbers.begin(); it != backstabbers.end();)
	{
		if (!it->isValid() || !it->isAlive())
			it = backstabbers.erase(it);
		else
			it++;
	}
	if (backstabbers.size() >= minBackstabbersCount)
		return;
	auto enemyBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
	if (!enemyBase)
		return;
	std::vector<std::pair<int, const Unit*>> orderedWorkers;
	for (auto & meleeUnit : meleeUnits)
	{
		if (backstabbers.find(meleeUnit) == backstabbers.end())
		{
			int groundDistance = enemyBase->getGroundDistance(meleeUnit.getPosition());
			const Unit* unitPointer = &meleeUnit;
			orderedWorkers.push_back(std::make_pair(groundDistance, unitPointer));
		}
	}
	std::sort(orderedWorkers.begin(), orderedWorkers.end());
	int backstabbersToAdd = minBackstabbersCount - backstabbers.size();
	for (int i = 0; i < backstabbersToAdd; i++)
	{
		backstabbers.insert(*orderedWorkers[i].second);
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