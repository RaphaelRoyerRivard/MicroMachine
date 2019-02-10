#include "WorkerData.h"
#include "Util.h"
#include "CCBot.h"
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
	if(m_bot.Config().DrawWorkerInfo)
	{
		for (auto & unit : m_workerRefineryMap)
		{
			m_bot.Map().drawText(unit.first.getPosition(), "  Affected to refinery");
		}
	}

    // check all our units and add new workers if we find them
    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        if (unit.getType().isWorker() && unit.isCompleted())
        {
            updateWorker(unit);
        }
    }

    // for each of our Workers
    for (auto worker : getWorkers())
    {
        // if it's idle
		auto job = getWorkerJob(worker);
        if (job == WorkerJobs::None)
        {
            setWorkerJob(worker, WorkerJobs::Idle);
        }
		else if (job == WorkerJobs::Build)//Handle refinery builder
		{
			auto orders = worker.getUnitPtr()->orders;
			if (!orders.empty() && !m_bot.Workers().isReturningCargo(worker) && orders[0].ability_id == 3666)//3666 = HARVEST_GATHER
			{
				worker.stop();
				setWorkerJob(worker, WorkerJobs::Idle);
			}
		}

        // TODO: If it's a gas worker whose refinery has been destroyed, set to minerals
    }

    // remove any worker units which no longer exist in the game
    std::vector<Unit> workersDestroyed;
    for (auto worker : getWorkers())
    {
		if (!worker.isValid() || !worker.isAlive())
        {
            workersDestroyed.push_back(worker);
        }
    }

    for (auto worker : workersDestroyed)
    {
        workerDestroyed(worker);
    }
}

void WorkerData::workerDestroyed(const Unit & unit)
{
    clearPreviousJob(unit);
    m_workers.erase(unit);
}

void WorkerData::updateWorker(const Unit & unit)
{
    if (m_workers.find(unit) == m_workers.end())
    {
        m_workers.insert(unit);
        m_workerJobMap[unit] = WorkerJobs::None;
    }
}

void WorkerData::setWorkerJob(const Unit & unit, int job, Unit jobUnit, bool mineralWorkerTargetJobUnit)
{
	const int spamOrderDuring = 75;//1 second is 24.4 frames
	if (getWorkerJob(unit) == WorkerJobs::Gas)
	{
		m_reorderedGasWorker[unit] = std::pair<Unit, int>(jobUnit, spamOrderDuring);
	}

	clearPreviousJob(unit);
    m_workerJobMap[unit] = job;
    m_workerJobCount[job]++;

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
		if (unit.getAPIUnitType() != sc2::UNIT_TYPEID::TERRAN_MULE)
		{
			m_workerDepotMap[unit] = jobUnit;
			m_depotWorkerCount[jobUnit]++;
		}

        // find the mineral to mine and mine it
		Unit mineralToMine;
		if (!mineralWorkerTargetJobUnit)
		{
			mineralToMine = getMineralToMine(unit);
		}
		else
		{
			mineralToMine = getMineralToMine(jobUnit);
		}

        unit.rightClick(mineralToMine);
    }
    else if (job == WorkerJobs::Gas)
    {
		BOT_ASSERT(jobUnit.getType().isRefinery(), "JobUnit should be refinery");
		BOT_ASSERT(unit.getType().isWorker(), "Unit should be worker");
        // if we haven't assigned any workers to this refinery yet set count to 0
        if (m_refineryWorkerCount.find(jobUnit) == m_refineryWorkerCount.end())
        {
            m_refineryWorkerCount[jobUnit] = 0;
        }

        // increase the count of workers assigned to this refinery
        m_refineryWorkerCount[jobUnit] += 1;
        m_workerRefineryMap[unit] = jobUnit;
		m_refineryWorkerMap[jobUnit].push_back(unit);
		
        // right click the refinery to start harvesting
        unit.rightClick(jobUnit);
    }
    else if (job == WorkerJobs::Repair)
    {
        unit.repair(jobUnit);
        m_workerRepairing[jobUnit].insert(unit);
        m_workerRepairTarget[unit] = jobUnit;
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

int WorkerData::getCountWorkerAtDepot(Unit & depot) const
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
			if(m_bot.Config().DrawWorkerInfo)
				m_bot.Map().drawText(unit.getPosition(), "Workers affected: " + std::to_string(it->second));
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

Unit WorkerData::getWorkerRepairTarget(const Unit & unit) const
{
           
    auto it = m_workerRepairTarget.find(unit);

    // if there is an entry, return it
    if (it != m_workerRepairTarget.end())
    {
        return it->second;
    }
    else
    {
        return {};
    }
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

void WorkerData::WorkerStoppedRepairing(const Unit & unit)
{
    auto target = getWorkerRepairTarget(unit);
    if (target.isValid())
    {
        getWorkerRepairingThatTarget(target).erase(unit);
        m_workerRepairTarget.erase(unit);
    }
}