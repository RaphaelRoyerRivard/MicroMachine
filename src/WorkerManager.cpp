#include <algorithm>
#include "WorkerManager.h"
#include "CCBot.h"
#include "Util.h"
#include "Building.h"
#include "Micro.h"

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
	m_bot.StartProfiling("0.7.1   m_workerData.updateAllWorkerData");
    m_workerData.updateAllWorkerData();
	m_bot.StopProfiling("0.7.1   m_workerData.updateAllWorkerData");
	m_bot.StartProfiling("0.7.2   handleMineralWorkers");
	handleMineralWorkers();
	m_bot.StopProfiling("0.7.2   handleMineralWorkers");
	m_bot.StartProfiling("0.7.3   handleGasWorkers");
    handleGasWorkers();
	m_bot.StopProfiling("0.7.3   handleGasWorkers");
	m_bot.StartProfiling("0.7.4   handleIdleWorkers");
	handleIdleWorkers();
	m_bot.StopProfiling("0.7.4   handleIdleWorkers");
	m_bot.StartProfiling("0.7.5   repairCombatBuildings");
	repairCombatBuildings();
	m_bot.StopProfiling("0.7.5   repairCombatBuildings");
	m_bot.StartProfiling("0.7.6   lowPriorityChecks");
	lowPriorityChecks();
	m_bot.StopProfiling("0.7.6   lowPriorityChecks");

    drawResourceDebugInfo();
    drawWorkerInformation();

    m_workerData.drawDepotDebugInfo();

	m_bot.StartProfiling("0.7.7   handleRepairWorkers");
    handleRepairWorkers();
	m_bot.StopProfiling("0.7.7   handleRepairWorkers");
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
	m_bot.StartProfiling("0.7.2.1     getWorkers");
	auto workers = getWorkers();
	m_bot.StopProfiling("0.7.2.1     getWorkers");
	if (workers.empty())
	{
		return;
	}

	/*if (!m_isFirstFrame || true)///TODO Disable split. might cause the long distance mining issue. Gotta test without this code.
	{
		return;
	}

	//split workers on first frame
	m_isFirstFrame = false;

	m_bot.StartProfiling("0.7.2.2     selectMinerals");
	std::list<Unit> minerals;
	std::list<std::pair<Unit, int>> mineralsUsage;
	for (auto& base : m_bot.Bases().getBaseLocations())
	{
		if (base->isOccupiedByPlayer(Players::Self))
		{
			auto mineralsVector = base->getMinerals();
			for (auto& t : mineralsVector)
			{
				minerals.push_back(t);
				mineralsUsage.push_back(std::pair<Unit, int>(t, 0));
			}
		}
	}
	m_bot.StopProfiling("0.7.2.2     selectMinerals");

	m_bot.StartProfiling("0.7.2.3     orderedMineralWorkers");
	std::list<Unit> orderedMineralWorkers;//Replaces workers
	for (auto& mineralWorker : workers)
	{
		if (!mineralWorker.isValid())
		{
			continue;
		}

		int lowest = 200;
		std::list<Unit> lowestMinerals;
		for (auto& mineral : mineralsUsage)
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
	}
	m_bot.StopProfiling("0.7.2.3     orderedMineralWorkers");*/
}

void WorkerManager::handleGasWorkers()
{
	int mineral = m_bot.GetMinerals();
	int gas = m_bot.GetGas();
	int numMineralWorker = getNumMineralWorkers();
	int numGasWorker = getNumGasWorkers();
	int numRefinery = m_bot.Buildings().getBuildingCountOfType(Util::GetRefinery(m_bot.GetSelfRace(), m_bot).getAPIUnitType(), true);
	const int ressourceTreshold = 300;

	switch (gasWorkersTarget)
	{
		case 0:
			if (mineral > gas)
			{
				gasWorkersTarget = 1;
			}
			break;
		case 1:
			/*if (numMineralWorker < numGasWorker + numRefinery)
			{
				gasWorkersTarget = 0;
			}
			else */if (gas > mineral * 5 && mineral + gas > ressourceTreshold)
			{
				gasWorkersTarget = 0;
			}
			else if (mineral > gas * 1.5f)
			{
				gasWorkersTarget = 2;
			}
			break;
		case 2:
			/*if (numMineralWorker < numGasWorker + numRefinery)
			{
				gasWorkersTarget = 1;
			}
			else */if (gas > mineral * 3 && mineral + gas > ressourceTreshold)
			{
				gasWorkersTarget = 1;
			}
			else if (mineral > gas * 2)
			{
				gasWorkersTarget = 3;
			}
			break;
		case 3:
			/*if (numMineralWorker + numRefinery < numGasWorker)
			{
				gasWorkersTarget = 2;
			}
			else */if (gas > mineral * 2 && mineral + gas > ressourceTreshold)
			{
				gasWorkersTarget = 2;
			}
			break;
		default:
			gasWorkersTarget = 3;
	}

	if (numMineralWorker < 6 + numRefinery)
	{
		gasWorkersTarget = 0;
	}
	if (m_bot.Strategy().isWorkerRushed())
	{
		gasWorkersTarget = 3;
	}

    // for each unit we have
    for (auto & building : m_bot.Buildings().getFinishedBuildings())
    {
        // if that unit is a refinery
        if (building.getType().isRefinery() && building.isCompleted())
        {
            // get the number of workers currently assigned to it
            int numAssigned = m_workerData.getNumAssignedWorkers(building);
			auto base = m_bot.Bases().getBaseContainingPosition(building.getPosition(), Players::Self);

			if (numAssigned < gasWorkersTarget)
			{
				// if it's less than we want it to be, fill 'er up
				for (int i = 0; i<(gasWorkersTarget - numAssigned); ++i)
				{
					auto mineralWorker = getMineralWorker(building);
					if (mineralWorker.isValid() && Util::PathFinding::IsPathToGoalSafe(mineralWorker.getUnitPtr(), building.getPosition(), m_bot))
					{
						m_workerData.setWorkerJob(mineralWorker, WorkerJobs::Gas, building);
					}
				}
			}
			else if (numAssigned > gasWorkersTarget)
			{
				int mineralWorkerRoom = 26;//Number of free spaces for mineral workers
				if (base != nullptr)
				{
					auto & depot = base->getResourceDepot();
					if (depot.isValid())
					{
						int mineralWorkersCount = m_workerData.getNumAssignedWorkers(depot);
						int optimalWorkersCount = base->getOptimalMineralWorkerCount();
						mineralWorkerRoom = optimalWorkersCount - mineralWorkersCount;
					}
				}

				// if it's more than we want it to be, empty it up
				for (int i = 0; i<(numAssigned - gasWorkersTarget); ++i)
				{
					//check if we have room for more mineral workers
					if (mineralWorkerRoom <= 0)
					{
						break;
					}

					auto gasWorker = getGasWorker(building, true);
					if (gasWorker.isValid())
					{
						if (m_workerData.getWorkerJob(gasWorker) != WorkerJobs::Gas)
						{
							Util::DisplayError(__FUNCTION__, "Worker assigned to a refinery is not a gas worker.", m_bot);
						}
 						m_workerData.setWorkerJob(gasWorker, WorkerJobs::Idle);
					}

					mineralWorkerRoom--;
				}
			}
        }
    }

	//Spam order in case worker is in Refinery
	auto & reorderedGasWorker = m_workerData.getReorderedGasWorkers();
	if (reorderedGasWorker.size() > 0)
	{
		for (auto & worker : m_bot.Workers().getWorkers())
		{
			auto it = reorderedGasWorker.find(worker);
			if (it != reorderedGasWorker.end())
			{
				if (reorderedGasWorker[worker].second > 0)//If order hasn't changed
				{
					auto frame = reorderedGasWorker[worker].second--;
					if (frame % 15)
					{
						continue;
					}

					auto target = it->first;
					if (target.isValid() && target.getPosition().x != 0 && target.getPosition().y != 0)
					{
						worker.rightClick(it->first);
					}
					else
					{//If no target unit, we stop
						auto orders = worker.getUnitPtr()->orders;
						if (orders.size() == 1 && orders[0].target_pos.x == 0 && orders[0].target_pos.y == 0)
						{
							worker.stop();
						}
					}
				}
				else
				{
					reorderedGasWorker.erase(it);
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
		else if (workerJob == WorkerJobs::Build && !worker.isConstructingAnything())
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
				bool isBuilder = false;
				for(const auto & building : m_bot.Buildings().getBuildings())
				{
					if(building.builderUnit == worker)
					{
						m_workerData.setWorkerJob(worker, WorkerJobs::Build, building.buildingUnit);
						if(building.buildingUnit.isValid() && building.buildingUnit.getBuildPercentage() < 1.f)
						{
							Micro::SmartRightClick(worker.getUnitPtr(), building.buildingUnit.getUnitPtr(), m_bot);
						}
						isBuilder = true;
						break;
					}
				}
				if (!isBuilder)
				{
					setMineralWorker(worker);
				}
			}
		}
    }
}

void WorkerManager::handleRepairWorkers()
{
	const int REPAIR_STATION_MIN_MINERAL = 25;//Also affects the automatic building repairing

    // Only terran worker can repair
    if (!Util::IsTerran(m_bot.GetSelfRace()))
        return;

	int mineral = m_bot.GetFreeMinerals();
	int gas = m_bot.GetFreeGas();

    for (auto & worker : m_workerData.getWorkers())
    {
        if (!worker.isValid()) { continue; }

        if (m_workerData.getWorkerJob(worker) == WorkerJobs::Repair)
        {
            Unit repairedUnit = m_workerData.getWorkerRepairTarget(worker);
			auto type = repairedUnit.getType();
            if (!worker.isAlive())
            {
                // We inform the manager that we are no longer repairing
                stopRepairing(worker);
            }
			// We do not try to repair if we don't have the required ressources
			else if ((type.mineralPrice() > 0 && mineral == 0) || (type.gasPrice() > 0 && gas == 0))
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

		// this has been commented out because it doesn't work well against worker rushes (less combat scvs and too much minerals are spent repairing)
        /*if (worker.isAlive() && worker.getHitPoints() < worker.getUnitPtr()->health_max)
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
        }*/
    }

	if (mineral < REPAIR_STATION_MIN_MINERAL)//Stop repairing if not enough minerals
	{
		return;
	}

	//Repair station (RepairStation)
	const int MIN_GAS_TO_REPAIR = 5;
	const int MAX_REPAIR_WORKER = 5;
	const int FULL_REPAIR_MINERAL = 100;
	const int FULL_REPAIR_GAS = 50;
	const int REPAIR_STATION_SIZE = 3;
	const int REPAIR_STATION_WORKER_ZONE_SIZE = 10;
	int currentMaxRepairWorker = std::min(MAX_REPAIR_WORKER, (int)floor(MAX_REPAIR_WORKER * (mineral / (float)FULL_REPAIR_MINERAL)));
	/* Chart of repair worker based on minerals
	0: 25 mineral-
	1: 25 mineral+
	2: 40 mineral+
	3: 60 mineral+
	4: 80 mineral+
	5: 100 mineral+
	*/

	if (m_bot.GetPlayerRace(Players::Self) == CCRace::Terran)
	{
		auto bases = m_bot.Bases().getOccupiedBaseLocations(Players::Self);
		for (auto & base : bases)
		{
			std::vector<Unit> unitsToRepair;

			//Get all units to repair
			for (auto & tagUnit : m_bot.GetAllyUnits())
			{
				Unit unit = tagUnit.second;
				if (unit.getUnitPtr()->build_progress != 1.0f)
				{
					continue;
				}

				float healthPercentage = unit.getHitPointsPercentage();
				if (healthPercentage < 100 && !unit.isBeingConstructed())
				{
					//Is repairable
					if (!unit.getType().isRepairable())
						continue;
					//Check if we have the required gas to repair it
					if (unit.getType().gasPrice() > 0 && gas <= MIN_GAS_TO_REPAIR)
						continue;

					const float distanceSquare = Util::DistSq(unit, base->getPosition());
					if (distanceSquare < REPAIR_STATION_SIZE * REPAIR_STATION_SIZE)
					{
						unitsToRepair.push_back(unit);
					}
				}
			}

			if (unitsToRepair.size() > 0)
			{
				//Send workers to repair
				int repairWorkers = 0;
				auto & workerData = getWorkerData();
				for (auto & worker : getWorkers())
				{
					auto it = unitsToRepair.begin();
					if (repairWorkers >= currentMaxRepairWorker)
					{
						break;
					}

					if (workerData.getWorkerJob(worker) == WorkerJobs::Minerals || workerData.getWorkerJob(worker) == WorkerJobs::Repair)
					{
						const float distanceSquare = Util::DistSq(worker, base->getPosition());
						if (distanceSquare < REPAIR_STATION_WORKER_ZONE_SIZE * REPAIR_STATION_WORKER_ZONE_SIZE)
						{
							setRepairWorker(worker, *it);
							repairWorkers++;
							++it;
							if (it == unitsToRepair.end())
							{
								it = unitsToRepair.begin();
							}
						}
					}
				}
			}
		}
	}
	
	//Automatically repair low health buildings, maximum 1 worker
	const float MIN_HEALTH = 30.f;
	const float MAX_HEALTH = 100.f;
	const int MAX_WORKER_PER_BUILDING = 1;
	for (auto & building : m_bot.Buildings().getBaseBuildings())
	{
		auto percentage = building.getHitPointsPercentage();
		//Skip building being repaired
		if (std::find(buildingAutomaticallyRepaired.begin(), buildingAutomaticallyRepaired.end(), building) != buildingAutomaticallyRepaired.end())
		{
			if (percentage >= MAX_HEALTH)
			{
				buildingAutomaticallyRepaired.remove(building);
			}
			continue;
		}

		if (building.isCompleted() && percentage <= MIN_HEALTH && m_workerData.getWorkerRepairingTargetCount(building) < MAX_WORKER_PER_BUILDING)
		{
			auto position = building.getPosition();
			auto worker = getClosestMineralWorkerTo(position);
			if (worker.isValid() && Util::PathFinding::IsPathToGoalSafe(worker.getUnitPtr(), position, m_bot))
			{
				setRepairWorker(worker, building);
				buildingAutomaticallyRepaired.push_back(building);
			}
		}
	}
}

void WorkerManager::repairCombatBuildings()
{
	const float repairAt = 0.9f; //90% health
	const int maxReparator = 5; //Turret and bunkers only

	if (m_bot.GetSelfRace() != CCRace::Terran)
	{
		return;
	}

	auto workers = getWorkers();
	auto buildings = m_bot.Buildings().getFinishedBuildings();
	for (auto building : buildings)
	{
		if (building.isBeingConstructed())
		{
			continue;
		}

		int alreadyRepairing = 0;
		switch ((sc2::UNIT_TYPEID)building.getAPIUnitType())
		{
			case sc2::UNIT_TYPEID::TERRAN_MISSILETURRET:
			case sc2::UNIT_TYPEID::TERRAN_BUNKER:
				if (building.getHitPoints() > repairAt * building.getUnitPtr()->health_max)
				{
					continue;
				}

				for (auto & worker : workers)
				{
					Unit repairedUnit = m_workerData.getWorkerRepairTarget(worker);
					if (repairedUnit.isValid() && repairedUnit.getID() == building.getID())
					{
						alreadyRepairing++;
						if (maxReparator == alreadyRepairing)
						{
							break;
						}
					}
				}
				for (int i = 0; i < maxReparator - alreadyRepairing; i++)
				{
					Unit worker = getClosestMineralWorkerTo(building.getPosition());
					if (worker.isValid())
						setRepairWorker(worker, building);
					else
						break;
				}
				break;
			case sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
				///TODO Doesn't use the gas workers
				if (building.getHitPoints() > repairAt * building.getUnitPtr()->health_max)
				{
					continue;
				}
				for (auto & worker : workers)///TODO order by closest to the target base location
				{
					auto depot = m_workerData.getWorkerDepot(worker);
					if (depot.isValid() && depot.getID() == building.getID())
					{
						setRepairWorker(worker, building);
					}
				}
				break;
		}
	}
}

void WorkerManager::lowPriorityChecks()
{
	int currentFrame = m_bot.GetCurrentFrame();
	if (currentFrame % 60)
	{
		return;
	}

	//Worker split between bases (transfer worker)
	if (m_bot.Bases().getBaseCount(Players::Self, true) <= 1)
	{//No point trying to split workers
		return;
	}
	bool needTransfer = false;
	auto workers = getWorkers();
	WorkerData workerData = m_bot.Workers().getWorkerData();
	auto & bases = m_bot.Bases().getOccupiedBaseLocations(Players::Self);
	std::list<Unit> dispatchedWorkers;
	for (auto & base : bases)
	{
		auto basePosition = base->getPosition();
		int optimalWorkers = base->getOptimalMineralWorkerCount();
		auto depot = getDepotAtBasePosition(basePosition);
		int workerCount = m_workerData.getNumAssignedWorkers(depot);
		if (workerCount > optimalWorkers)
		{
			int extra = workerCount - optimalWorkers;
			for (auto & worker : workers)///TODO order by closest to the target base location
			{
				if (m_bot.Workers().isFree(worker) && m_workerData.getWorkerDepot(worker).getID() == depot.getID())
				{
					dispatchedWorkers.push_back(worker);
					extra--;
					if (extra <= 0)
					{
						break;
					}
				}
			}
		}
		else
		{
			needTransfer = true;
		}
	}

	//Dispatch workers to bases missing some
	if (needTransfer)
	{
		for (auto & base : bases)
		{
			if (base->isUnderAttack())
			{
				continue;
			}

			auto depot = getDepotAtBasePosition(base->getPosition());
			if (depot.isBeingConstructed())
			{
				continue;
			}

			int workerCount = m_workerData.getNumAssignedWorkers(depot);
			int optimalWorkers = base->getMinerals().size() * 2;
			if (workerCount < optimalWorkers)
			{
				std::vector<Unit> toRemove;
				int needed = optimalWorkers - workerCount;
				int moved = 0;
				for (auto it = dispatchedWorkers.begin(); it != dispatchedWorkers.end(); it++)
				{
					//Dont move workers if its not safe
					if (!Util::PathFinding::IsPathToGoalSafe(it->getUnitPtr(), base->getPosition(), m_bot))
					{
						continue;
					}

					m_workerData.setWorkerJob(*it, WorkerJobs::Minerals, depot, true);
					toRemove.push_back(*it);
					moved++;
					if (moved >= needed)
					{
						break;
					}
				}

				//remove already dispatched workers
				for (auto worker : toRemove)
				{
					auto it = find(dispatchedWorkers.begin(), dispatchedWorkers.end(), worker);
					if (it != dispatchedWorkers.end())
					{
						dispatchedWorkers.erase(it);
					}
				}
				if (dispatchedWorkers.empty())
				{
					break;
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
				///TODO: Maybe it should by ground distance?
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

Unit WorkerManager::getClosestMineralWorkerTo(const CCPosition & pos, std::vector<CCUnitID> workerToIgnore, float minHpPercentage) const
{
	Unit closestMineralWorker;
	double closestDist = std::numeric_limits<double>::max();

	// for each of our workers
	for (auto & worker : m_workerData.getWorkers())
	{
		if (!worker.isValid() || std::find(workerToIgnore.begin(), workerToIgnore.end(), worker.getID()) != workerToIgnore.end()) { continue; }
		const sc2::Unit* workerPtr = worker.getUnitPtr();
		if (workerPtr->health < minHpPercentage * workerPtr->health_max)
		{
			continue;
		}

		// if it is a mineral worker, Idle or None
		if (isFree(worker))
		{
			if (!isReturningCargo(worker))
			{
				///TODO: Maybe it should by ground distance?
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

Unit WorkerManager::getClosestGasWorkerTo(const CCPosition & pos, CCUnitID workerToIgnore, float minHpPercentage) const
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
		if (m_workerData.getWorkerJob(worker) == WorkerJobs::Gas)
		{
			if (!isReturningCargo(worker))
			{
				///TODO: Maybe it should by ground distance?
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

Unit WorkerManager::getClosestGasWorkerTo(const CCPosition & pos, float minHpPercentage) const
{
	return getClosestGasWorkerTo(pos, CCUnitID{}, minHpPercentage);
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
		const float dist = Util::DistSq(potentialClosest.getUnitPtr()->pos, unit.getPosition());
		if (minDist == -1 || dist < minDist) {
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
            const double distance = Util::DistSq(unit, worker);
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

Unit WorkerManager::getMineralWorker(Unit refinery) const
{
    return getClosestMineralWorkerTo(refinery.getPosition());
}

Unit WorkerManager::getGasWorker(Unit refinery, bool checkReturningCargo) const
{
	auto workers = m_workerData.getAssignedWorkersRefinery(refinery);
	if (checkReturningCargo)
	{
		for (auto & worker : workers)
		{
			if (!isReturningCargo(worker))
			{
				return worker;
			}
		}
	}
	return workers[0];
}

int WorkerManager::getGasWorkersTarget() const
{
	return gasWorkersTarget;
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
	return m_bot.Workers().getWorkerData().getCountWorkerAtDepot(getDepotAtBasePosition(basePosition));
}

// gets a builder for BuildingManager to use
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
Unit WorkerManager::getBuilder(Building & b, bool setJobAsBuilder) const
{
	bool isValid;
	std::vector<CCUnitID> invalidWorkers;
	Unit builderWorker;
	
	do
	{
		isValid = true;
		builderWorker = getClosestMineralWorkerTo(Util::GetPosition(b.finalPosition), invalidWorkers, 0);
		if (!builderWorker.isValid())//If no worker left to check
		{
			break;
		}

		//Check if worker is already building something else
		//TODO This shouldn't be needed (shouldn't happen), right?
		for (auto & building : m_bot.Buildings().getBuildings())
		{
			if (building.builderUnit.isValid() && building.builderUnit.getID() == builderWorker.getID())
			{
				invalidWorkers.push_back(builderWorker.getID());
				isValid = false;
				break;
			}
		}
	} while (!isValid);

    // if the worker exists (one may not have been found in rare cases)
    if (builderWorker.isValid() && setJobAsBuilder && m_workerData.getWorkerJob(builderWorker) != WorkerJobs::Build)
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
		if (worker.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_MULE) { continue; }

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
{//(ReturnCargo)
	auto orders = worker.getUnitPtr()->orders;
	if (orders.size() > 0)
	{
		//Not checking the abilities HARVEST_RETURN_DRONE, HARVEST_RETURN_MULE, HARVEST_RETURN_PROBE and HARVEST_RETURN_SCV, because they seem to never be used.
		auto order = orders.at(0).ability_id;
		if (order == sc2::ABILITY_ID::HARVEST_RETURN)
		{
			return true;
		}
	}
	return false;
}

bool WorkerManager::canHandleMoreRefinery() const
{
	return gasWorkersTarget >= 3;
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

WorkerData & WorkerManager::getWorkerData() const
{
	return m_workerData;
}