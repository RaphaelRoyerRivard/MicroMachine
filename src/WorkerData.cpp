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

void WorkerData::workerDestroyed(const Unit & unit)
{
    clearPreviousJob(unit);
    m_workers.erase(unit);
    m_proxyWorkers.erase(unit);

	for (auto & building : m_workerRepairing)
	{
		if (building.second.find(unit) != building.second.end())
		{
			//worker is no longer repairing
			building.second.erase(unit);
		}
	}
}

void WorkerData::updateWorker(const Unit & unit)
{
    if (m_workers.find(unit) == m_workers.end())
    {
        m_workers.insert(unit);
        m_workerJobMap[unit] = WorkerJobs::None;
		m_workerReturningCargoMap[unit] = false;
    }
	else
	{
		auto orders = unit.getUnitPtr()->orders;

		if (!orders.empty())
		{
			if (m_workerReturningCargoMap[unit])
			{
				if (orders.at(0).ability_id == sc2::ABILITY_ID::HARVEST_GATHER)
				{
					m_workerReturningCargoMap[unit] = false;
				}
			}
			else
			{
				if (orders.at(0).ability_id == sc2::ABILITY_ID::HARVEST_RETURN)
				{
					m_workerReturningCargoMap[unit] = true;
				}
			}
		}
	}
}

void WorkerData::setWorkerJob(const Unit & worker, int job, Unit jobUnit, bool mineralWorkerTargetJobUnit)
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

        // add the depot to our set of depots
        m_depots.insert(jobUnit);

        // increase the worker count of this depot
		if (worker.getAPIUnitType() != sc2::UNIT_TYPEID::TERRAN_MULE)
		{
			m_workerDepotMap[worker] = jobUnit;
			m_depotWorkerCount[jobUnit]++;
		}

        // find the mineral to mine and mine it
		Unit mineralToMine;
		if (!mineralWorkerTargetJobUnit)
		{
			mineralToMine = getMineralToMine(worker);
		}
		else
		{
			mineralToMine = getMineralToMine(jobUnit);
		}

        worker.rightClick(mineralToMine);
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

    }
	else if (job == WorkerJobs::Idle)
	{//Must not call stop().

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
    else if (previousJob == WorkerJobs::Move)
    {

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
	auto it = m_workerReturningCargoMap.find(unit);

	if (it != m_workerReturningCargoMap.end())
	{
		return it->second;
	}
	return false;
}

Unit WorkerData::getMineralToMine(const Unit & unit) const
{
    Unit bestMineral;
    const double maxDistance = 100000;
    double bestDist = maxDistance;

    // We search only in our bases minerals (we do not want to long range mine)
    for (auto base : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
    {
        GetBestMineralInList(base->getMinerals(), unit, bestMineral, bestDist);
    }
    // If we did not found minerals, we fall back to all minerals
    if (bestDist == maxDistance)
    {
        GetBestMineralInList(m_bot.GetUnits(), unit, bestMineral, bestDist);
    }

    return bestMineral;
}


void WorkerData::GetBestMineralInList(const std::vector<Unit> & unitsToTest, const Unit & worker, Unit & bestMineral, double & bestDist) const
{
    for (auto & mineral : unitsToTest)
    {
		if (!mineral.getType().isMineral() || !mineral.isAlive() || mineral.getUnitPtr()->display_type != sc2::Unit::DisplayType::Visible)
			continue;

        double dist = Util::DistSq(mineral, worker);

        if (dist < bestDist)
        {
            bestMineral = mineral;
            bestDist = dist;
        }
    }
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