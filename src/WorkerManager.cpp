#include <algorithm>
#include "WorkerManager.h"
#include "CCBot.h"
#include "Util.h"
#include "Building.h"

WorkerManager::WorkerManager(CCBot & bot)
    : m_bot         (bot)
    , m_workerData  (bot)
{

}

void WorkerManager::onStart()
{

}

void WorkerManager::onFrame()
{
    m_workerData.updateAllWorkerData();
	handleMineralWorkers();
    handleGasWorkers();
    handleIdleWorkers();

    drawResourceDebugInfo();
    drawWorkerInformation();

    m_workerData.drawDepotDebugInfo();

    handleRepairWorkers();
}

void WorkerManager::setRepairWorker(Unit worker, const Unit & unitToRepair)
{
    m_workerData.setWorkerJob(worker, WorkerJobs::Repair, unitToRepair);
}

void WorkerManager::stopRepairing(Unit worker)
{
    m_workerData.WorkerStoppedRepairing(worker);
    finishedWithWorker(worker);
}

void WorkerManager::handleMineralWorkers()
{
	auto workers = getWorkers();
	if (workers.empty())
	{
		return;
	}

	std::list<Unit> mineralWorkers;
	for (auto worker : workers)
	{
		int workerJob = m_workerData.getWorkerJob(worker);
		if (workerJob == WorkerJobs::Minerals && !isReturningCargo(worker))
		{
			mineralWorkers.push_back(worker);
		}
	}
	if (mineralWorkers.empty())
	{
		return;
	}
	
	if (!m_isFirstFrame)
	{//Worker split between bases
		if (m_bot.Bases().getBaseCount(Players::Self, true) <= 1)
		{//No point trying to split workers
			return;
		}
		WorkerData workerData = m_bot.Workers().getWorkerData();
		auto & bases = m_bot.Bases().getBaseLocations();
		std::vector<Unit> dispatchedWorkers;
		for (auto & base : bases)
		{
			if (base->isOccupiedByPlayer(Players::Self))
			{
				int workerCount = getWorkerCountAtBasePosition(base->getPosition());
				int optimalWorkers = base->getMinerals().size() * 2;
				if (workerCount > optimalWorkers)
				{
					int needed = workerCount - optimalWorkers;
					for (auto & worker : workers)//TODO order by closest to the target base location
					{
						if (m_bot.Workers().isFree(worker) && !worker.getType().isMule() && base->containsPosition(worker.getPosition()))
						{
							dispatchedWorkers.push_back(worker);
							needed--;
							if (needed <= 0)
							{
								break;
							}
						}
					}
				}
			}
		}
		for (auto & base : bases)//Dispatch workers to bases missing some
		{
			if (base->isOccupiedByPlayer(Players::Self))
			{
				int workerCount = getWorkerCountAtBasePosition(base->getPosition());
				int optimalWorkers = base->getMinerals().size() * 2;
				if (workerCount < optimalWorkers)
				{
					int needed = optimalWorkers - workerCount;
					for (auto & worker : dispatchedWorkers)//TODO order by closest again
					{
						m_workerData.setWorkerJob(worker, WorkerJobs::Minerals, getDepotAtBasePosition(base->getPosition()), true);
						needed--;
						if (needed <= 0)
						{
							break;
						}
					}
					if (needed <= 0)
					{
						break;
					}
				}
			}
		}
		return;
	}

	//split workers on first frame
	m_isFirstFrame = false;

	std::list<Unit> minerals;
	std::list<std::pair<Unit, int>> mineralsUsage;
	for (auto base : m_bot.Bases().getBaseLocations())
	{
		if (base->isOccupiedByPlayer(Players::Self))
		{
			auto mineralsVector = base->getMinerals();
			for (auto t : mineralsVector)
			{
				minerals.push_back(t);
				mineralsUsage.push_back(std::pair<Unit, int>(t, 0));
			}
		}
	}

	std::list<std::pair<Unit, float>> temp;
	for (auto mineralWorker : mineralWorkers)
	{
		auto closestMineral = getClosest(mineralWorker, minerals);
		const float dist = Util::Dist(mineralWorker.getPosition(), closestMineral.getPosition());
		temp.push_back(std::pair<Unit, float>(mineralWorker, dist));
	}

	std::list<Unit> orderedMineralWorkers;//Replaces workers
	for (auto mineralWorker : workers)
	{
		if (!mineralWorker.isValid())
		{
			continue;
		}

		int lowest = 200;
		std::list<Unit> lowestMinerals;
		for (auto mineral : mineralsUsage)
		{
			if (mineral.second < lowest)
			{
				lowest = mineral.second;
				lowestMinerals.clear();
			}
			if (mineral.second == lowest)
			{
				lowestMinerals.push_back(mineral.first);
			}
		}

		auto closestMineral = getClosest(mineralWorker, lowestMinerals);
		for (auto & mineral : mineralsUsage)
		{
			if (mineral.first.getID() == closestMineral.getID())
			{
				mineral.second++;
				break;
			}
		}

		mineralWorker.rightClick(closestMineral);
		//m_workerData.setWorkerJob(mineralWorker, WorkerJobs::Minerals, closestMineral);
		//workers.erase(mineralWorker);
	}
}

void WorkerManager::handleGasWorkers()
{
    // for each unit we have
    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        // if that unit is a refinery
        if (unit.getType().isRefinery() && unit.isCompleted())
        {
            // get the number of workers currently assigned to it
            int numAssigned = m_workerData.getNumAssignedWorkers(unit);

            // if it's less than we want it to be, fill 'er up
            for (int i=0; i<(3-numAssigned); ++i)
            {
                auto gasWorker = getGasWorker(unit);
                if (gasWorker.isValid())
                {
                    m_workerData.setWorkerJob(gasWorker, WorkerJobs::Gas, unit);
                }
            }
        }
    }
}

void WorkerManager::handleIdleWorkers()
{
    // for each of our workers
    for (auto & worker : m_workerData.getWorkers())
    {
        if (!worker.isValid()) { continue; }

		int workerJob = m_workerData.getWorkerJob(worker);
        if (worker.isIdle() &&
            // We need to consider building worker because of builder finishing the job of another worker is not consider idle.
			//(m_workerData.getWorkerJob(worker) != WorkerJobs::Build) && 
            (workerJob != WorkerJobs::Move) &&
            (workerJob != WorkerJobs::Repair) &&
			(workerJob != WorkerJobs::Scout) &&
			(workerJob != WorkerJobs::Build))//Prevent premoved builder from going Idle if they lack the ressources, also prevents refinery builder from going Idle
		{
			m_workerData.setWorkerJob(worker, WorkerJobs::Idle);
			workerJob = WorkerJobs::Idle;
		}

		if (workerJob == WorkerJobs::Idle)
		{
			if (!worker.isAlive())
			{
				worker.stop();
			}
			else
			{
				setMineralWorker(worker);
			}
		}
    }
}

void WorkerManager::handleRepairWorkers()
{
    // Only terran worker can repair
    if (!Util::IsTerran(m_bot.GetPlayerRace(Players::Self)))
        return;

    for (auto & worker : m_workerData.getWorkers())
    {
        if (!worker.isValid()) { continue; }

        if (m_workerData.getWorkerJob(worker) == WorkerJobs::Repair)
        {
            Unit repairedUnit = m_workerData.getWorkerRepairTarget(worker);
            if (!worker.isAlive())
            {
                // We inform the manager that we are no longer repairing
                stopRepairing(worker);
            }
            // We do not try to repair dead units
            else if (!repairedUnit.isAlive() || repairedUnit.getHitPoints() + std::numeric_limits<float>::epsilon() >= repairedUnit.getUnitPtr()->health_max)
            {
                stopRepairing(worker);
            }
            else if (worker.isIdle())
            {
                // Get back to repairing...
                worker.repair(repairedUnit);
            }
        }

        if (worker.isAlive() && worker.getHitPoints() < worker.getUnitPtr()->health_max)
        {
            const std::set<Unit> & repairedBy = m_workerData.getWorkerRepairingThatTargetC(worker);
            if (repairedBy.empty())
            {
                auto repairGuy = getClosestMineralWorkerTo(worker.getPosition(), worker.getID(), MIN_HP_PERCENTAGE_TO_FIGHT);
                 
                if (repairGuy.isValid() && Util::Dist(worker.getPosition(), repairGuy.getPosition()) <= m_bot.Config().MaxWorkerRepairDistance)
                {
                    setRepairWorker(repairGuy, worker);
                }
            }
        }
    }
}

Unit WorkerManager::getClosestMineralWorkerTo(const CCPosition & pos, CCUnitID workerToIgnore, float minHpPercentage) const
{
    Unit closestMineralWorker;
    double closestDist = std::numeric_limits<double>::max();

    // for each of our workers
    for (auto & worker : m_workerData.getWorkers())
    {
        if (!worker.isValid() || worker.getID() == workerToIgnore) { continue; }
		const sc2::Unit* workerPtr = worker.getUnitPtr();
		if (workerPtr->health < minHpPercentage * workerPtr->health_max)
		{
			continue;
		}

        // if it is a mineral worker, Idle or None
        if(isFree(worker))
        {
			if (!isReturningCargo(worker))
			{
				double dist = Util::DistSq(worker.getPosition(), pos);
				if (!closestMineralWorker.isValid() || dist < closestDist)
				{
					closestMineralWorker = worker;
					closestDist = dist;
				}
			}
        }
    }
	return closestMineralWorker;
}

Unit WorkerManager::getClosestMineralWorkerTo(const CCPosition & pos, float minHpPercentage) const
{
    return getClosestMineralWorkerTo(pos, CCUnitID{}, minHpPercentage);
}

Unit WorkerManager::getClosest(const Unit unit, const std::list<Unit> units) const
{
	Unit currentClosest;
	float minDist = -1;
	for (auto potentialClosest : units)
	{
		if (!potentialClosest.isValid())
		{
			continue;
		}
		const float dist = Util::Dist(potentialClosest.getUnitPtr()->pos, unit.getPosition());
		if (minDist == -1) {
			currentClosest = potentialClosest;
			minDist = dist;
		}
		else if (dist < minDist) {
			currentClosest = potentialClosest;
			minDist = dist;
		}
	}
	return currentClosest;
}

/*std::list<Unit> WorkerManager::orderByDistance(const std::list<Unit> units, CCPosition pos, bool closestFirst)
{
	struct UnitDistance
	{
		Unit unit;
		float distance;

		UnitDistance(Unit unit, float distance) : unit(unit), distance(distance) {}

		bool operator < (const UnitDistance& str) const
		{
			return (distance < str.distance);
		}

		bool operator - (const UnitDistance& str) const
		{
			return (distance - str.distance);
		}

		bool operator == (const UnitDistance& str) const
		{
			return (distance == str.distance);
		}
	};

	std::list<UnitDistance> unorderedUnitsDistance;
	std::list<UnitDistance> orderedUnitsDistance;

	for (auto unit : units)
	{
		auto a = unit.getUnitPtr();
		unorderedUnitsDistance.push_back(UnitDistance(unit, Util::Dist(unit.getUnitPtr()->pos, pos)));
	}
	
	//sort
	UnitDistance bestDistance = UnitDistance(Unit(), 1000000);
	while (!unorderedUnitsDistance.empty())
	{
		for (auto distance : unorderedUnitsDistance)
		{
			if (distance < bestDistance)
			{
				bestDistance = distance;
			}
		}
		if (bestDistance.distance == 1000000)
		{
			break;
		}
		orderedUnitsDistance.push_back(bestDistance);
		unorderedUnitsDistance.remove(bestDistance);
		bestDistance = UnitDistance(Unit(), 1000000);
	}

	std::list<Unit> orderedUnits;
	for (auto unitDistance : orderedUnitsDistance)
	{
		if (closestFirst)
		{
			orderedUnits.push_back(unitDistance.unit);
		}
		else
		{
			orderedUnits.push_front(unitDistance.unit);
		}
	}
	return orderedUnits;
}*/

// set a worker to mine minerals
void WorkerManager::setMineralWorker(const Unit & unit)
{
    // check if there is a mineral available to send the worker to
    auto depot = getClosestDepot(unit);

    // if there is a valid mineral
    if (depot.isValid())
    {
        // update m_workerData with the new job
        m_workerData.setWorkerJob(unit, WorkerJobs::Minerals, depot);
    }
}

Unit WorkerManager::getClosestDepot(Unit worker) const
{
    Unit closestDepot;
    double closestDistance = std::numeric_limits<double>::max();

    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        if (!unit.isValid()) { continue; }

        if (unit.getType().isResourceDepot())
        {
			if (!unit.isCompleted())
			{
				//Cannont allow unfinished bases, unless the base is morphing (orbital, lair and hive).
				switch ((sc2::UNIT_TYPEID)unit.getAPIUnitType())
				{
					case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER://Ignores flying depot
					case sc2::UNIT_TYPEID::PROTOSS_NEXUS:
					case sc2::UNIT_TYPEID::ZERG_HATCHERY:
						continue;
				}
			}
            double distance = Util::Dist(unit, worker);
            if (!closestDepot.isValid() || distance < closestDistance)
            {
                closestDepot = unit;
                closestDistance = distance;
            }
        }
    }

    return closestDepot;
}

// other managers that need workers call this when they're done with a unit
void WorkerManager::finishedWithWorker(const Unit & unit)
{
    if (m_workerData.getWorkerJob(unit) != WorkerJobs::Scout)
    {
        m_workerData.setWorkerJob(unit, WorkerJobs::Idle);
    }
}

Unit WorkerManager::getGasWorker(Unit refinery) const
{
    return getClosestMineralWorkerTo(refinery.getPosition());
}

Unit WorkerManager::getDepotAtBasePosition(CCPosition basePosition) const
{
	for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
	{
		// we only care about buildings on the ground
		if (!unit.getType().isBuilding() || unit.isFlying() || !unit.getType().isResourceDepot())
		{
			continue;
		}
		BaseLocation * baseLocation = m_bot.Bases().getBaseLocation(unit.getPosition());
		if (baseLocation->getPosition().x == basePosition.x && baseLocation->getPosition().y == basePosition.y)
		{
			return unit;
		}
	}
	return {};
}

int WorkerManager::getWorkerCountAtBasePosition(CCPosition basePosition) const
{
	// for each unit on the map, update which base location it may be occupying
	for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
	{
		// we only care about buildings on the ground
		if (!unit.getType().isBuilding() || unit.isFlying() || !unit.getType().isResourceDepot())
		{
			continue;
		}
		BaseLocation * baseLocation = m_bot.Bases().getBaseLocation(unit.getPosition());
		if (baseLocation->getPosition().x == basePosition.x && baseLocation->getPosition().y == basePosition.y)
		{
			Unit depot = unit;
			return m_bot.Workers().getWorkerData().getCountWorkerAtDepot(depot);
		}
	}
	return -1;
}

void WorkerManager::setBuildingWorker(Unit worker)
{
	m_workerData.setWorkerJob(worker, WorkerJobs::Build);
}

void WorkerManager::setBuildingWorker(Unit worker, Building & b)
{
    m_workerData.setWorkerJob(worker, WorkerJobs::Build, b.buildingUnit);
}

// gets a builder for BuildingManager to use
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
Unit WorkerManager::getBuilder(Building & b, bool setJobAsBuilder) const
{
    Unit builderWorker = getClosestMineralWorkerTo(Util::GetPosition(b.finalPosition));

    // if the worker exists (one may not have been found in rare cases)
    if (builderWorker.isValid() && setJobAsBuilder)
    {
        m_workerData.setWorkerJob(builderWorker, WorkerJobs::Build, b.builderUnit);
    }

    return builderWorker;
}

// sets a worker as a scout
void WorkerManager::setScoutWorker(Unit workerTag)
{
    m_workerData.setWorkerJob(workerTag, WorkerJobs::Scout);
}

void WorkerManager::setCombatWorker(Unit workerTag)
{
    m_workerData.setWorkerJob(workerTag, WorkerJobs::Combat);
}

void WorkerManager::drawResourceDebugInfo()
{
    if (!m_bot.Config().DrawResourceInfo)
    {
        return;
    }

    for (auto & worker : m_workerData.getWorkers())
    {
        if (!worker.isValid()) { continue; }

        if (worker.isIdle())
        {
            m_bot.Map().drawText(worker.getPosition(), m_workerData.getJobCode(worker));
        }

        auto depot = m_workerData.getWorkerDepot(worker);
        if (depot.isValid())
        {
            m_bot.Map().drawLine(worker.getPosition(), depot.getPosition());
        }
    }

	for (auto & unit : m_bot.UnitInfo().getUnits(Players::Neutral))
	{
		if (unit.getType().isMineral())
		{
			switch ((sc2::UNIT_TYPEID)unit.getAPIUnitType())
			{
				case sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD:
				case sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750:
				case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD:
				case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD750:
					m_bot.Map().drawText(unit.getPosition(), "Rich mineral");
					break;
				case sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD:
				case sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD750:
				case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD:
				case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD750:
				case sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD:
				case sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD750:
				case sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD:
				case sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD750:
					m_bot.Map().drawText(unit.getPosition(), "Mineral");
					break;
				default:
					m_bot.Map().drawText(unit.getPosition(), "Not mineral");
			}
		}
	}
}

void WorkerManager::drawWorkerInformation()
{
    if (!m_bot.Config().DrawWorkerInfo)
    {
        return;
    }

    std::stringstream ss;
    ss << "Workers: " << m_workerData.getWorkers().size() << "\n";

    int yspace = 0;

    for (auto & worker : m_workerData.getWorkers())
    {
        ss << m_workerData.getJobCode(worker) << " " << worker.getID() << "\n";

        m_bot.Map().drawText(worker.getPosition(), m_workerData.getJobCode(worker));
    }

    m_bot.Map().drawTextScreen(0.75f, 0.2f, ss.str());
}

bool WorkerManager::isFree(Unit worker) const
{
	if (worker.getType().isMule())
		return false;
	int job = m_workerData.getWorkerJob(worker);
    return job == WorkerJobs::Minerals || job == WorkerJobs::Idle || job == WorkerJobs::None;
}

bool WorkerManager::isWorkerScout(Unit worker) const
{
    return (m_workerData.getWorkerJob(worker) == WorkerJobs::Scout);
}

bool WorkerManager::isBuilder(Unit worker) const
{
    return (m_workerData.getWorkerJob(worker) == WorkerJobs::Build);
}

bool WorkerManager::isReturningCargo(Unit worker) const
{
	sc2::AvailableAbilities available_abilities = m_bot.Query()->GetAbilitiesForUnit(worker.getUnitPtr());
	for (const sc2::AvailableAbility & available_ability : available_abilities.abilities)
	{
		if (available_ability.ability_id == sc2::ABILITY_ID::HARVEST_RETURN)
		{
			return true;
			break;
		}
	}
	return false;
}

int WorkerManager::getNumMineralWorkers()
{
    return m_workerData.getWorkerJobCount(WorkerJobs::Minerals);
}

int WorkerManager::getNumGasWorkers()
{
    return m_workerData.getWorkerJobCount(WorkerJobs::Gas);

}

int WorkerManager::getNumWorkers()
{
    int count = 0;
    for (auto worker : m_workerData.getWorkers())
    {
        if (worker.isValid() && worker.isAlive())
        {
            ++count;
        }
    }
    return count;
}

std::set<Unit> WorkerManager::getWorkers() const
{
	return m_workerData.getWorkers();
}

WorkerData WorkerManager::getWorkerData() const
{
	return m_workerData;
}