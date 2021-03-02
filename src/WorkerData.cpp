#include "WorkerData.h"
#include "Util.h"
#include "CCBot.h"
#include "BaseLocation.h"
#include <iostream>
#include <sstream>

WorkerData::WorkerData(CCBot & bot)
    : m_bot(bot)
{
    for (int i=0; i < WorkerJobs::Num; ++i)
    {
        m_workerJobCount[i] = 0;
    }
}

void WorkerData::updateAllWorkerData()
{
#ifndef PUBLIC_RELEASE
	if(m_bot.Config().DrawWorkerInfo)
	{
		for (auto & unit : m_workerRefineryMap)
		{
			m_bot.Map().drawText(unit.first.getPosition(), "  Affected to refinery");
		}
	}
#endif

    // check all our units and add new workers if we find them
    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        if (unit.getType().isWorker() && unit.isCompleted())
        {
            updateWorker(unit);
        }
    }

    // for each of our Workers
    for (auto & worker : getWorkers())
    {
        // if it's idle
		auto job = getWorkerJob(worker);
        if (job == WorkerJobs::None)
        {
            setWorkerJob(worker, WorkerJobs::Idle);
        }

    	// This caused a bug where the first proxy worker wouldn't stay a proxy worker
		/*auto it = m_proxyWorkers.find(worker);
    	if (it != m_proxyWorkers.end())
    	{
			if (job == WorkerJobs::Minerals || job == WorkerJobs::Gas)
				m_proxyWorkers.erase(it);
    	}*/

        // TODO: If it's a gas worker whose refinery has been destroyed, set to minerals
    }

    // remove any worker units which no longer exist in the game
    std::vector<Unit> workersDestroyed;
    for (auto & worker : getWorkers())
    {
		if (!worker.isValid() || !worker.isAlive())
        {
            workersDestroyed.push_back(worker);
        }
    }

    for (auto & worker : workersDestroyed)
    {
        workerDestroyed(worker);		
    }
}

void WorkerData::updateIdleMineralTarget()
{
	BaseLocation* closestBase = nullptr;
	// Choose our base in which we are building a resource depot
	auto & allyBases = m_bot.Bases().getOccupiedBaseLocations(Players::Self);
	for (auto base : allyBases)
	{
		if (base && !base->isUnderAttack() && (!base->getResourceDepot().isValid() || !base->getResourceDepot().isCompleted()))
		{
			if (m_bot.Strategy().wasProxyStartingStrategy())
			{
				auto groundDistance = base->getGroundDistance(m_bot.Buildings().getProxyLocation());
				if (groundDistance <= 15)
					continue;
			}
			closestBase = base;
			break;
		}
	}
	// If we have none, choose the next expansion
	if (!closestBase)
	{
		closestBase = m_bot.Bases().getNextExpansion(Players::Self, false, false, true);
	}
	// If that base is not occupied by the enemy and does not have a completed resource depot, we find the mineral patch to mine from
	if (closestBase && !closestBase->isUnderAttack() && !closestBase->isOccupiedByPlayer(Players::Enemy) && (!closestBase->getResourceDepot().isValid() || !closestBase->getResourceDepot().isCompleted()))
	{
		const BaseLocation * homeBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
		if (homeBase->getResourceDepot().isValid())
		{
			m_idleMineralTarget = GetBestMineralInList(closestBase->getMinerals(), homeBase->getResourceDepot(), false);

			if (m_idleMineralTarget.isValid() && m_idleMineralTarget.getUnitPtr()->display_type == sc2::Unit::Snapshot)
			{
				closestBase->updateMineral(m_idleMineralTarget);
			}
		}
	}
}

void WorkerData::workerDestroyed(const Unit & unit)//deadWorker
{
    clearPreviousJob(unit);
    m_workers.erase(unit);
    m_proxyWorkers.erase(unit);
	m_workerMineralMap.erase(unit);

	for (auto & building : m_workerRepairing)
	{
		if (building.second.find(unit) != building.second.end())
		{
			//worker is no longer repairing
			building.second.erase(unit);
		}
	}

	for (auto & mineralWorker : m_mineralWorkersMap)
	{
		mineralWorker.second.remove(unit.getTag());
	}	
}

void WorkerData::updateWorker(const Unit & unit)
{
    if (m_workers.find(unit) == m_workers.end())
    {
        m_workers.insert(unit);
        m_workerJobMap[unit] = WorkerJobs::None;
    }
}

void WorkerData::setWorkerJob(const Unit & worker, int job, Unit jobUnit)
{
	clearPreviousJob(worker);
    m_workerJobMap[worker] = job;
    m_workerJobCount[job]++;

	//Handle starting a job
    if (job == WorkerJobs::Minerals)
    {
        // if we haven't assigned anything to this depot yet, set its worker count to 0
        if (m_depotWorkerCount.find(jobUnit) == m_depotWorkerCount.end())
        {
            m_depotWorkerCount[jobUnit] = 0;
        }

        // add the worker to our set of depots
        m_depots.insert(jobUnit);

        // increase the worker count of this depot
		if (worker.getAPIUnitType() != sc2::UNIT_TYPEID::TERRAN_MULE)
		{
			m_workerDepotMap[worker] = jobUnit;
			m_depotWorkerCount[jobUnit]++;
		}

        // find the mineral to mine and mine it
		if (m_bot.GetCurrentFrame() == 0)
		{		
		}
		else
		{
			Unit mineralToMine = getMineralToMine(jobUnit, worker.getPosition());
			if (!mineralToMine.isValid())
			{
				//Try other bases
				for (auto base : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
				{
					const Unit & depot = base->getResourceDepot();
					if (jobUnit.getTag() == depot.getTag())
						continue;
					mineralToMine = getMineralToMine(depot, worker.getPosition());
					if (mineralToMine.isValid())
					{
						break;
					}
				}
			}

			if (mineralToMine.isValid())
			{
				worker.rightClick(mineralToMine);

				if (!worker.getType().isMule())
				{
					m_workerMineralMap[worker] = mineralToMine;
					m_mineralWorkersMap[mineralToMine].push_back(worker.getTag());
				}
			}
			else
			{
				//No minerals to mine on the map, A-move the enemy base
				worker.attackMove(m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy)->getPosition());
			}
		}

		m_mineralWorkers.insert(worker);
    }
    else if (job == WorkerJobs::Gas)
    {
		BOT_ASSERT(jobUnit.getType().isRefinery(), "JobUnit should be refinery");
		BOT_ASSERT(worker.getType().isWorker(), "Unit should be worker");
        // if we haven't assigned any workers to this refinery yet set count to 0
        if (m_refineryWorkerCount.find(jobUnit) == m_refineryWorkerCount.end())
        {
            m_refineryWorkerCount[jobUnit] = 0;
        }

        // increase the count of workers assigned to this refinery
        m_refineryWorkerCount[jobUnit] += 1;
        m_workerRefineryMap[worker] = jobUnit;
		m_refineryWorkerMap[jobUnit].push_back(worker);
		
        // right click the refinery to start harvesting
        worker.rightClick(jobUnit);
    }
    else if (job == WorkerJobs::Repair)
    {
        worker.repair(jobUnit);
        m_workerRepairing[jobUnit].insert(worker);
        m_workerRepairTarget[worker] = jobUnit;
    }
    else if (job == WorkerJobs::Scout)
    {

    }
    else if (job == WorkerJobs::Build)
    {
		if (jobUnit.isValid() && jobUnit.getBuildProgress() < 1.f)
			worker.rightClick(jobUnit);	// We want to send the SCV to continue the building
    }
	else if (job == WorkerJobs::Idle)
	{//Must not call stop().
		if (!isProxyWorker(worker))
		{
			m_idleWorkers.insert(worker);
			sendIdleWorkerToMiningSpot(worker, false);
		}
	}
}

void WorkerData::clearPreviousJob(const Unit & unit)
{
    const int previousJob = getWorkerJob(unit);
	m_workerJobCount[previousJob]--;

    if (previousJob == WorkerJobs::Minerals)
    {
        // remove one worker from the count of the depot this worker was assigned to
		// increase the worker count of this depot
		if (unit.getAPIUnitType() != sc2::UNIT_TYPEID::TERRAN_MULE)
		{
			m_depotWorkerCount[m_workerDepotMap[unit]]--;
			m_workerDepotMap.erase(unit);

			if (m_workerMineralMap.find(unit) != m_workerMineralMap.end())
			{
				m_mineralWorkersMap[m_workerMineralMap[unit]].remove(unit.getTag());
			}
			m_workerMineralMap.erase(unit);
			m_mineralWorkers.erase(unit);
		}
    }
    else if (previousJob == WorkerJobs::Gas)
    {
        m_refineryWorkerCount[m_workerRefineryMap[unit]]--;
        m_workerRefineryMap.erase(unit);
		for (auto & refinery : m_refineryWorkerMap)
		{
			auto it = std::find(refinery.second.begin(), refinery.second.end(), unit);
			if (it != refinery.second.end()) {
				refinery.second.erase(it);
			}
		}
    }
    else if (previousJob == WorkerJobs::Build)
    {
		Micro::SmartAbility(unit.getUnitPtr(), sc2::ABILITY_ID::HALT, m_bot);
    }
    else if (previousJob == WorkerJobs::Repair)
    {

    }
	else if (previousJob == WorkerJobs::Idle)
	{
		m_idleWorkers.erase(unit);
	}
    else if (previousJob == WorkerJobs::Move)
    {

    }
	else if (previousJob == WorkerJobs::None)//Only affects newly spawned workers
	{
		sendIdleWorkerToMiningSpot(unit, true);
	}

    m_workerJobMap.erase(unit);
}

size_t WorkerData::getNumWorkers() const
{
    return m_workers.size();
}

int WorkerData::getWorkerJobCount(int job) const
{
    return m_workerJobCount.at(job);
}

int WorkerData::getCountWorkerAtDepot(const Unit & depot) const
{
	auto it = m_depotWorkerCount.find(depot);
	if (it == m_depotWorkerCount.end())
	{
		return 0;
	}
	return it->second;
}

int WorkerData::getWorkerJob(const Unit & unit) const
{
    auto it = m_workerJobMap.find(unit);

    if (it != m_workerJobMap.end())
    {
        return it->second;
    }

    return WorkerJobs::None;
}

bool WorkerData::isReturningCargo(const Unit & unit) const
{
	return (sc2::IsCarryingMinerals(*unit.getUnitPtr()) || sc2::IsCarryingVespene(*unit.getUnitPtr()));
}

bool WorkerData::isActivelyReturningCargo(const Unit & unit) const
{
	return isReturningCargo(unit) && unit.getUnitPtr()->orders.size() > 0 && unit.getUnitPtr()->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_RETURN;
}

Unit WorkerData::getMineralToMine(const Unit & depot, const CCPosition location) const
{
    Unit bestMineral;

	auto base = m_bot.Bases().getBaseForDepot(depot);
	if (base)
		bestMineral = GetBestMineralWithLessWorkersInLists(base->getCloseMinerals(), base->getFarMinerals(), location);

    // If the base has no mineral (why was it assigned here in the first place?), we search in all our bases minerals (we do not want to long range mine)
	if (!bestMineral.isValid())
	{
		for (auto base : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
		{
			bestMineral = GetBestMineralWithLessWorkersInLists(base->getCloseMinerals(), base->getFarMinerals(), location);
			if (bestMineral.isValid())
			{
				break;
			}
		}
	}
    // If we did not found minerals, we fall back to all minerals
    if (!bestMineral.isValid())
    {
		bestMineral = GetBestMineralInList(m_bot.GetUnits(), depot, true);
    }

    return bestMineral;
}

Unit WorkerData::GetBestMineralInList(const std::vector<Unit> & unitsToTest, CCPosition depotPosition, bool checkVisibility) const
{
	double bestDist = 100000;
	Unit bestMineral = Unit();

	//Need to check the close and far patches, than determine where the worker should go. Definitely need to rename the function.
	for (auto & mineral : unitsToTest)
	{
		if (!mineral.getType().isMineral() || !mineral.isAlive())
			if (!checkVisibility || mineral.getUnitPtr()->display_type != sc2::Unit::DisplayType::Visible)
				continue;

		double dist = Util::DistSq(mineral.getPosition(), depotPosition);

		if (dist < bestDist)
		{
			bestMineral = mineral;
			bestDist = dist;
		}
	}

	return bestMineral;
}

Unit WorkerData::GetBestMineralInList(const std::vector<Unit> & unitsToTest, const Unit & depot, bool checkVisibility) const
{
	if (!depot.isValid())
		return Unit();
	return GetBestMineralInList(unitsToTest, depot.getPosition(), checkVisibility);
}

const Unit WorkerData::GetBestMineralWithLessWorkersInLists(const std::vector<Unit> & closeMinerals, const std::vector<Unit> & farMinerals, const CCPosition location) const
{
	std::vector<Unit> sortedCloseMinerals = closeMinerals;
	if (location.x != 0 && location.y != 0)
	{
		std::sort(sortedCloseMinerals.begin(), sortedCloseMinerals.end(), [location](Unit a, Unit b) {
			return Util::DistSq(a.getPosition(), location) > Util::DistSq(b.getPosition(), location);
		});
	}

	//Close
	for (auto & closeMineral : sortedCloseMinerals)
	{
		if (!closeMineral.isValid() || !closeMineral.isAlive() || closeMineral.getUnitPtr()->mineral_contents == 0)
		{
			continue;
		}
		auto it = m_mineralWorkersMap.find(closeMineral);
		if (it != m_mineralWorkersMap.end())
		{
			if (it->second.size() == 0)//Can be at 0 if a worker was removed
			{
				return closeMineral;
			}
		}
		else//Should have 0 workers
		{
			return closeMineral;
		}
	}
	for (auto & closeMineral : sortedCloseMinerals)
	{
		if (!closeMineral.isValid() || !closeMineral.isAlive() || closeMineral.getUnitPtr()->mineral_contents == 0)
		{
			continue;
		}
		auto it = m_mineralWorkersMap.find(closeMineral);
		if (it->second.size() == 1)
		{
			return closeMineral;
		}
	}

	std::vector<Unit> sortedFarMinerals = farMinerals;
	if (location.x != 0 && location.y != 0)
	{
		std::sort(sortedFarMinerals.begin(), sortedFarMinerals.end(), [location](Unit a, Unit b) {
			return Util::DistSq(a.getPosition(), location) > Util::DistSq(b.getPosition(), location);
		});
	}
	
	//Far
	for (auto & farMineral : sortedFarMinerals)
	{
		if (!farMineral.isValid() || !farMineral.isAlive() || farMineral.getUnitPtr()->mineral_contents == 0)
		{
			continue;
		}
		auto it = m_mineralWorkersMap.find(farMineral);
		if (it != m_mineralWorkersMap.end())
		{
			if (it->second.size() == 0)//Can be at 0 if a worker was removed
			{
				return farMineral;
			}
		}
		else//Should have 0 workers
		{
			return farMineral;
		}
	}
	for (auto & farMineral : sortedFarMinerals)
	{
		if (!farMineral.isValid() || !farMineral.isAlive() || farMineral.getUnitPtr()->mineral_contents == 0)
		{
			continue;
		}
		auto it = m_mineralWorkersMap.find(farMineral);
		if (it->second.size() == 1)
		{
			return farMineral;
		}
	}

	//No viable mineral found
	return Unit();
}

bool WorkerData::isAnyMineralAvailable(CCPosition workerCurrentPosition) const
{
	for (auto base : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
	{
		if (base->isUnderAttack() && !base->containsPositionApproximative(workerCurrentPosition))
			continue;
		auto & depot = base->getResourceDepot();
		if (!depot.isValid() || !depot.isAlive() || depot.getBuildProgress() < 0.99)
			continue;
		if (GetBestMineralWithLessWorkersInLists(base->getCloseMinerals(), base->getFarMinerals(), CCPosition()).isValid())
		{
			return true;
		}
	}
	return false;
}

Unit WorkerData::getWorkerDepot(const Unit & unit) const
{
    auto it = m_workerDepotMap.find(unit);

    if (it != m_workerDepotMap.end())
    {
        return it->second;
    }

    return Unit();
}

int WorkerData::getNumAssignedWorkers(const Unit & unit)
{
	if (!unit.isValid())
		return 0;
	
    if (unit.getType().isResourceDepot())
    {
        auto it = m_depotWorkerCount.find(unit);

        // if there is an entry, return it
        if (it != m_depotWorkerCount.end())
        {
            return it->second;
        }
    }
    else if (unit.getType().isRefinery())
    {
        auto it = m_refineryWorkerCount.find(unit);

        // if there is an entry, return it
        if (it != m_refineryWorkerCount.end())
        {
#ifndef PUBLIC_RELEASE
			if(m_bot.Config().DrawWorkerInfo)
				m_bot.Map().drawText(unit.getPosition(), "Workers affected: " + std::to_string(it->second));
#endif
            return it->second;
        }
        // otherwise, we are only calling this on completed refineries, so set it
        else
        {
            m_refineryWorkerCount[unit] = 0;
        }
    }

    // when all else fails, return 0
    return 0;
}

std::vector<Unit> WorkerData::getAssignedWorkersRefinery(const Unit & unit)
{
	if (unit.getType().isResourceDepot())
	{
		auto it = m_refineryWorkerMap.find(unit);
		// if there is an entry, return it
		if (it != m_refineryWorkerMap.end())
		{
			return it->second;
		}
	}
	else if (unit.getType().isRefinery())
	{
		auto it = m_refineryWorkerMap.find(unit);

		// if there is an entry, return it
		if (it != m_refineryWorkerMap.end())
		{
			return it->second;
		}
	}

	// when all else fails, return 0
	return std::vector<Unit>();
}

const char * WorkerData::getJobCode(const Unit & unit)
{
    const int j = getWorkerJob(unit);

    if (j == WorkerJobs::Build)     return "B";
    if (j == WorkerJobs::Combat)    return "C";
    if (j == WorkerJobs::None)      return "N";
    if (j == WorkerJobs::Gas)       return "G";
    if (j == WorkerJobs::Idle)      return "I";
    if (j == WorkerJobs::Minerals)  return "M";
    if (j == WorkerJobs::Repair)    return "R";
    if (j == WorkerJobs::Move)      return "O";
    if (j == WorkerJobs::Scout)     return "S";
    return "X";
}

void WorkerData::drawDepotDebugInfo()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
	if (!m_bot.Config().DrawWorkerInfo)
		return;

    for (auto depot: m_depots)
    {
        std::stringstream ss;
        ss << "Workers: " << getNumAssignedWorkers(depot);

        m_bot.Map().drawText(depot.getPosition(), ss.str());
    }
}

const std::set<Unit> & WorkerData::getWorkers() const
{
	return m_workers;
}

const std::set<Unit> & WorkerData::getProxyWorkers() const
{
	return m_proxyWorkers;
}

const std::set<Unit> WorkerData::getIdleWorkers() const
{
	return m_idleWorkers;
}

const std::set<Unit> WorkerData::getMineralWorkers() const
{
	return m_mineralWorkers;
}

void WorkerData::sendIdleWorkerToIdleSpot(const Unit & worker, bool force)
{
	if (m_bot.Bases().getBaseCount(Players::Self) > 0)
	{
		auto & base = *m_bot.Bases().getOccupiedBaseLocations(Players::Self).begin();
		if (force || Util::DistSq(worker.getPosition(), base->getDepotPosition()) > 40)
		{
			if (force || worker.getUnitPtr()->orders.size() == 0)
			{
				CCTilePosition mainPos = base->getDepotTilePosition();
				CCTilePosition ressourcesCenterPos = base->getTurretPosition();
				CCTilePosition idlePos = CCTilePosition(mainPos.x + (mainPos.x - ressourcesCenterPos.x), mainPos.y + (mainPos.y - ressourcesCenterPos.y));
				worker.move(idlePos);
				return;
			}
		}
	}
	else
	{
		//If all else fail, just cancel current order.
		worker.stop();
	}
}

void WorkerData::sendIdleWorkerToMiningSpot(const Unit & worker, bool force)
{
	if (worker.getType().isMule() || isActivelyReturningCargo(worker))
	{
		return;
	}
	if (!m_idleMineralTarget.isValid() || !Util::PathFinding::IsPathToGoalSafe(worker.getUnitPtr(), m_idleMineralTarget.getPosition(), false, m_bot))
	{
		sendIdleWorkerToIdleSpot(worker, force);
	}
	else
	{
		if (worker.getUnitPtr()->orders.empty() || worker.getUnitPtr()->orders[0].ability_id != sc2::ABILITY_ID::HARVEST_GATHER || Util::DistSq(worker.getPosition(), m_idleMineralTarget.getPosition()) > 75)
		{
			if (m_idleMineralTarget.getUnitPtr()->last_seen_game_loop == m_bot.GetCurrentFrame())//When the mineral is visible, just click it.
			{
				worker.rightClick(m_idleMineralTarget);
			}
			else if(worker.getUnitPtr()->orders.empty() || worker.getUnitPtr()->orders[0].ability_id != sc2::ABILITY_ID::MOVE || worker.getUnitPtr()->orders[0].target_pos != m_idleMineralTarget.getPosition())
			{
				worker.rightClick(m_idleMineralTarget.getPosition());
			}
		}
	}
}

bool WorkerData::isProxyWorker(const Unit & unit) const
{
	return m_proxyWorkers.find(unit) != m_proxyWorkers.end();
}

void WorkerData::setProxyWorker(const Unit & unit)
{
	m_proxyWorkers.insert(unit);
}

void WorkerData::removeProxyWorker(const Unit & unit)
{
	m_proxyWorkers.erase(unit);
}

void WorkerData::clearProxyWorkers()
{
	m_proxyWorkers.erase(m_proxyWorkers.begin(), m_proxyWorkers.end());
}

std::map<const BaseLocation*, std::list<Unit>>& WorkerData::getRepairStationWorkers()
{
	return m_repairStationWorkers;
}

void WorkerData::validateRepairStationWorkers()
{
	for (auto & station : m_repairStationWorkers)
	{
		//Could validate the depot is valid and is alive, else clear everything for that base location

		std::list<Unit> toRemove;
		for (auto & worker : station.second)
		{
 			if (!worker.isValid() || !worker.isAlive() || getWorkerJob(worker) != WorkerJobs::Repair || Util::DistSq(worker, station.first->getPosition()) > 10.f * 10.f)
			{
				toRemove.push_back(worker);
			}
		}
		for (auto remove : toRemove)
		{
			station.second.remove(remove);
		}
	}
}

Unit WorkerData::getWorkerRepairTarget(const Unit & unit) const
{
    auto it = m_workerRepairTarget.find(unit);

    // if there is an entry, return it
    if (it != m_workerRepairTarget.end())
    {
        return it->second;
    }
    return {};
}

const std::set<Unit> WorkerData::getWorkerRepairingThatTargetC(const Unit & unit) const
{
    auto it = m_workerRepairing.find(unit);

    // if there is an entry, return it
    if (it != m_workerRepairing.end())
    {
        return it->second;
    }
    else
    {
        auto emptySet = std::set<Unit>();
        return emptySet;
    }
}

std::set<Unit> WorkerData::getWorkerRepairingThatTarget(const Unit & unit)
{
    auto it = m_workerRepairing.find(unit);

    // if there is an entry, return it
    if (it != m_workerRepairing.end())
    {
        return it->second;
    }
    else
    {
        auto emptySet = std::set<Unit>();
        return emptySet;
    }
}

int WorkerData::getWorkerRepairingTargetCount(const Unit & unit)
{
	auto it = m_workerRepairing.find(unit);

	// if there is an entry, return it
	if (it != m_workerRepairing.end())
	{
		return it->second.size();
	}
	return 0;
}

void WorkerData::WorkerStoppedRepairing(const Unit & unit)
{
    auto target = getWorkerRepairTarget(unit);
    if (target.isValid())
    {
        getWorkerRepairingThatTarget(target).erase(unit);
        m_workerRepairTarget.erase(unit);
    }
}