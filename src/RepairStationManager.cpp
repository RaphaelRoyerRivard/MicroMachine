#include "RepairStationManager.h"
#include "CCBot.h"


RepairStationManager::RepairStationManager(CCBot& bot) :
m_bot(bot)
{
}

void RepairStationManager::onStart()
{
	
}

void RepairStationManager::onFrame()
{
	// Clean repair stations (remove the ones that are not valid anymore)
	for (auto it = m_repairStations.cbegin(), next_it = it; it != m_repairStations.cend(); it = next_it)
	{
		++next_it;
		if (!isRepairStationValidForBaseLocation(it->first))
		{
			for(auto unit : it->second)
			{
				m_destinations.erase(unit);
			}
			m_repairStations.erase(it);
		}
	}

	// Add the valid repair stations that are missing from the map
	const std::vector<const BaseLocation *> & bases = m_bot.Bases().getBaseLocations();
	for(auto & base : bases)
	{
		if (!isRepairStationValidForBaseLocation(base))
			continue;

		if(m_repairStations.find(base) == m_repairStations.end())
		{
			m_repairStations.insert_or_assign(base, std::list<const sc2::Unit*>());
		}
	}

	// Clean destinations (remove the ones that are not valid anymore)
	for (auto it = m_destinations.cbegin(), next_it = it; it != m_destinations.cend(); it = next_it)
	{
		++next_it;
		const sc2::Unit* unit = it->first;
		if (!unit->is_alive || unit->health == unit->health_max)
		{
			m_repairStations[it->second].remove(unit);
			m_destinations.erase(it);
		}
	}

	if(m_bot.Config().DrawRepairStation)
	{
		drawRepairStations();
	}
}

CCPosition RepairStationManager::getBestRepairStationForUnit(const sc2::Unit* unit)
{
	const auto it = m_destinations.find(unit);
	if (it != m_destinations.end() && it->second != nullptr)
	{
		return it->second->getPosition();
	}

	reservePlaceForUnit(unit);

	const BaseLocation* base = m_destinations[unit];
	if (base == nullptr)
	{
		return {};
	}

	return base->getPosition();
}

bool RepairStationManager::isRepairStationValidForBaseLocation(const BaseLocation * baseLocation, bool ignoreUnderAttack)
{
	const int MINIMUM_WORKER_COUNT = 3;

	if (!baseLocation)
		return false;

	if (!baseLocation->isOccupiedByPlayer(Players::Self))
		return false;

	if (!ignoreUnderAttack && baseLocation->isUnderAttack())
		return false;

	const Unit& resourceDepot = baseLocation->getResourceDepot();
	if (!resourceDepot.isValid())
		return false;

	if (!resourceDepot.isCompleted() && !resourceDepot.getType().isMorphedBuilding())
		return false;
	
	auto workerData = m_bot.Workers().getWorkerData();
	int workerCount = workerData.getNumAssignedWorkers(resourceDepot);
	int repairWorkerCount = workerData.getRepairStationWorkers()[baseLocation].size();
	if (workerCount + repairWorkerCount < MINIMUM_WORKER_COUNT)
		return false;

	return true;
}

void RepairStationManager::reservePlaceForUnit(const sc2::Unit* unit)
{
	const BaseLocation* baseLocation = nullptr;
	int lowestCount = 0;
	float lowestDistance = 0.f;
	// We check twice the repair stations, first for bases that are not under attack and if none is found, then check of bases that are also under attack
	for (int i = 0; i < 2; ++i)
	{
		if (i == 1 && baseLocation != nullptr)
			break;

		for (const auto & repairStation : m_repairStations)
		{
			const bool ignoreUnderAttack = i == 1;
			if (!isRepairStationValidForBaseLocation(repairStation.first, ignoreUnderAttack))
				continue;

			const int count = repairStation.second.size();
			const float distance = Util::DistSq(unit->pos, repairStation.first->getPosition());
			if (baseLocation == nullptr || count < lowestCount || (count == lowestCount && distance < lowestDistance))
			{
				baseLocation = repairStation.first;
				lowestCount = count;
				lowestDistance = distance;
			}
		}
	}
	if (baseLocation != nullptr)
	{
		m_repairStations[baseLocation].push_back(unit);
		m_destinations[unit] = baseLocation;
	}
}

void RepairStationManager::drawRepairStations()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
	for(const auto & repairStation : m_repairStations)
	{
		m_bot.Map().drawText(repairStation.first->getPosition(), "Units to repair: " + std::to_string(repairStation.second.size()));
	}
}