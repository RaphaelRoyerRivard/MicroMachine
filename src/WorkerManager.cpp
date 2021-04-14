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

void WorkerManager::onFrame(bool executeMacro)
{
	if (m_bot.Config().AllowDebug)
	{
		//const int minutes = 2;
		//const int frames = (int)(22.4 * minutes * 60);
		//if (m_bot.GetCurrentFrame() > 0)
		//{
		//	const float mineralRate = m_bot.Observation()->GetScore().score_details.collection_rate_minerals;
		//	const float gasRate = m_bot.Observation()->GetScore().score_details.collection_rate_vespene;
		//	Util::AddStatistic("Mineral GatherRate", mineralRate);
		//	Util::AddStatistic("Gas GatherRate", gasRate);
		//}
		//if (m_bot.GetCurrentFrame() > 0 && m_bot.GetCurrentFrame() % frames == 0)//after 5 minutes
		//{
		//	Util::DisplayStatistic(m_bot, "Mineral GatherRate");
		//	Util::DisplayStatistic(m_bot, "Gas GatherRate");
		//}
		//if (m_bot.GetCurrentFrame() > 0 && m_bot.GetCurrentFrame() % frames + 1 == 0)
		//{
		//	Util::ClearChat(m_bot);
		//}
	}

	m_bot.StartProfiling("0.7.1   m_workerData.updateAllWorkerData");
    m_workerData.updateAllWorkerData();
	m_bot.StopProfiling("0.7.1   m_workerData.updateAllWorkerData");

	m_bot.StartProfiling("0.7.1.1   m_workerData.updateIdleMineralTarget");
	m_workerData.updateIdleMineralTarget();
	m_bot.StopProfiling("0.7.1.1   m_workerData.updateIdleMineralTarget");
	if (executeMacro)
	{
		handleGeyserProtectWorkers();
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
		m_bot.StartProfiling("0.7.7   handleRepairWorkers");
		handleRepairWorkers();
		m_bot.StopProfiling("0.7.7   handleRepairWorkers");
		m_bot.StartProfiling("0.7.8   handleBuildWorkers");
		handleBuildWorkers();
		m_bot.StopProfiling("0.7.8   handleBuildWorkers");
	}
    drawResourceDebugInfo();
    drawWorkerInformation();

    m_workerData.drawDepotDebugInfo();
}

void WorkerManager::lowPriorityChecks()
{
	int currentFrame = m_bot.GetCurrentFrame();
	if (currentFrame - m_lastLowPriorityCheckFrame < 48)
	{
		return;
	}
	m_lastLowPriorityCheckFrame = currentFrame;

	m_bot.StartProfiling("0.7.6.1     SalvageDepletedGeysers");
	//Detect depleted geysers
	for (auto & geyser : m_bot.GetAllyGeyserUnits())
	{
		//if Depleted
		if (geyser.isValid() && geyser.getUnitPtr()->vespene_contents == 0)
		{
			//Salvage gas bunkers, if any
			auto base = m_bot.Bases().getBaseContainingPosition(geyser.getPosition(), Players::Self);
			auto & gasBunkers = base->getGasBunkers();
			for (auto & bunker : gasBunkers)
			{
				if (bunker.isValid())
				{
					Micro::SmartAbility(bunker.getUnitPtr(), sc2::ABILITY_ID::EFFECT_SALVAGE, m_bot);
				}
			}
		}
	}
	m_bot.StopProfiling("0.7.6.1     SalvageDepletedGeysers");

	m_bot.StartProfiling("0.7.6.2     HandleWorkerTransfer");
	//No longer need to transfer workers since we limit the number of workers per base. Still can be used to send workers to gold bases in advance.
	//HandleWorkerTransfer();
	m_bot.StopProfiling("0.7.6.2     HandleWorkerTransfer");

	m_bot.StartProfiling("0.7.6.3     validateRepairStationWorkers");
	m_bot.Workers().getWorkerData().validateRepairStationWorkers();
	m_bot.StopProfiling("0.7.6.3     validateRepairStationWorkers");

	m_bot.StartProfiling("0.7.6.4     clean mineral and workers association");
	std::vector<Unit> mineralWorkerToRemove;
	std::vector<Unit> workerMineralToRemove;
	auto & bases = m_bot.Bases().getOccupiedBaseLocations(Players::Self);
	for (auto & mineralWorkers : m_workerData.m_mineralWorkersMap)
	{
		bool remove = true;
		if (mineralWorkers.first.isValid() && mineralWorkers.first.isAlive() && mineralWorkers.first.getUnitPtr()->mineral_contents > 0)
		{
			for (auto & base : bases)
			{
				auto & depot = base->getResourceDepot();
				if (!depot.isValid() || !depot.isAlive())
				{
					continue;
				}
				for (auto & mineral : base->getMinerals())
				{
					if (mineral.isValid() && mineralWorkers.first.getTag() == mineral.getTag())
					{
						remove = false;
						break;
					}
				}
				if (!remove)
				{
					break;
				}
			}
		}
		if (remove)
		{
			mineralWorkerToRemove.push_back(mineralWorkers.first);
			for (auto & workerMineral : m_workerData.m_workerMineralMap)
			{
				if (workerMineral.second.isValid() && mineralWorkers.first.isValid() && workerMineral.second.getTag() == mineralWorkers.first.getTag())
				{
					workerMineralToRemove.push_back(workerMineral.first);
				}
			}
		}
	}
	for (auto & remove : mineralWorkerToRemove)
	{
		m_workerData.m_mineralWorkersMap.erase(remove);
	}
	for (auto & remove : workerMineralToRemove)
	{
		m_workerData.m_workerMineralMap.erase(remove);
		m_workerData.setWorkerJob(remove, WorkerJobs::Idle);
	}
	m_bot.StopProfiling("0.7.6.4     clean mineral and workers association");
}

//Worker split between bases (transfer worker)
void WorkerManager::HandleWorkerTransfer()
{
	if (m_bot.Bases().getBaseCount(Players::Self, false) <= 1)
	{//No point trying to split workers
		return;
	}

	auto & workers = getWorkers();
	auto & bases = m_bot.Bases().getOccupiedBaseLocations(Players::Self);
	std::set<Unit> idleWorkers = m_bot.Workers().getWorkerData().getIdleWorkers();
	std::list<std::pair<BaseLocation*, int>> unorderedBasesWithFewWorkers;
	std::list<std::pair<BaseLocation*, int>> unorderedBasesWithExtraWorkers;
	for (auto & base : bases)
	{
		if (base->isUnderAttack())//Do not transfer or receive workers while the base is underattack.
		{
			continue;
		}

		auto basePosition = base->getPosition();
		auto depot = getDepotAtBasePosition(basePosition);
		int workerCount = m_workerData.getNumAssignedWorkers(depot);
		int optimalWorkers = base->getOptimalMineralWorkerCount();
		if (workerCount > optimalWorkers)
		{
			unorderedBasesWithExtraWorkers.push_back(std::pair<BaseLocation*, int>(base, workerCount - optimalWorkers));
		}
		else if(workerCount < optimalWorkers)
		{
			unorderedBasesWithFewWorkers.push_back(std::pair<BaseLocation*, int>(base, optimalWorkers - workerCount));
		}
	}

	if (unorderedBasesWithFewWorkers.size() <= 0 || unorderedBasesWithExtraWorkers.size() <= 0)
	{
		return;
	}

	//Sorting the bases by the number of the wanted workers (highest first in the list) 
	//Defining a lambda function to compare two pairs. It will compare two pairs using the value
	WorkerTransferComparator compFunctor = [](std::pair<BaseLocation*, int> elem1, std::pair<BaseLocation*, int> elem2)
	{
		return elem1.second > elem2.second;
	};

	//Declaring a set that will store the pairs using above comparision logic
	std::set<std::pair<BaseLocation*, int>, WorkerTransferComparator> basesWithFewWorkers(unorderedBasesWithFewWorkers.begin(), unorderedBasesWithFewWorkers.end(), compFunctor);
	std::set<std::pair<BaseLocation*, int>, WorkerTransferComparator> basesWithExtraWorkers(unorderedBasesWithExtraWorkers.begin(), unorderedBasesWithExtraWorkers.end(), compFunctor);

	int totalRequiredWorkers = 0;
	for (auto baseWithFewWorkers : basesWithFewWorkers)
	{
		totalRequiredWorkers += baseWithFewWorkers.second;
	}

	for (auto & baseWithFewWorkers : basesWithFewWorkers)
	{
		auto & depot = baseWithFewWorkers.first->getResourceDepot();
		if (!depot.isValid())
			continue;
		auto closestWorker = this->getClosestAvailableWorkerTo(depot.getPosition());//Could be replaced by a path from a depot, once its possible.
		int fewWorkerNeeded = baseWithFewWorkers.second;

		//Transfer idle workers first then extra workers
		for (auto & idleWorker : idleWorkers)
		{
			if (fewWorkerNeeded <= 0)
			{
				break;
			}

			if (!Util::PathFinding::IsPathToGoalSafe(closestWorker.getUnitPtr(), idleWorker.getPosition(), true, m_bot))
			{//Path isn't safe
				continue;
			}

			m_workerData.setWorkerJob(idleWorker, WorkerJobs::Minerals, depot);
			fewWorkerNeeded--;
		}

		//TODO Is this even needed anymore? Can it still happen?
		for (auto & baseWithExtraWorkers : basesWithExtraWorkers)
		{
			if (fewWorkerNeeded <= 0)
			{
				break;
			}

			bool shouldSendWorkers = false;
			if (depot.isBeingConstructed())
			{
				int distance = baseWithFewWorkers.first->getGroundDistance(baseWithExtraWorkers.first->getDepotTilePosition());//Util::PathFinding::FindOptimalPathDistance(baseWithExtraWorkers.getUnitPtr(), depot.getPosition(), false, m_bot);

				const float speed = 2.8125f;//Always the same for workers, Util::getSpeedOfUnit(worker.getUnitPtr(), m_bot);

				const float speedPerFrame = speed / 16.f;
				auto travelFrame = distance / speedPerFrame;

				const float depotBuildTime = 71 * 22.4f * 1.07f;//71 seconds * 22.4 fps * small buffer
				float depotFinishedInXFrames = depotBuildTime * (1.f - depot.getBuildProgress());
				if (travelFrame >= depotFinishedInXFrames)
				{
					//Should send workers, its the right timing
					shouldSendWorkers = true;
				}
			}
			else
			{
				shouldSendWorkers = true;
			}

			if (shouldSendWorkers)
			{
				if (!Util::PathFinding::IsPathToGoalSafe(closestWorker.getUnitPtr(), Util::GetPosition(baseWithExtraWorkers.first->getCenterOfMinerals()), true, m_bot))
				{//Path isn't safe
					continue;
				}

				int extraWorkers = baseWithExtraWorkers.second;
				for (auto & worker : workers)///TODO order by closest to the target base location
				{
					if (m_bot.Workers().isFree(worker) && !worker.isReturningCargo())
					{
						const auto workerDepot = m_workerData.getWorkerDepot(worker);
						if (workerDepot.isValid() && workerDepot.getTag() == baseWithExtraWorkers.first->getResourceDepot().getTag())
						{
							m_workerData.setWorkerJob(worker, WorkerJobs::Minerals, depot);
							extraWorkers--;
							fewWorkerNeeded--;
							if (extraWorkers <= 0 || fewWorkerNeeded <= 0)
							{
								break;
							}
						}
					}
				}
			}
		}
	}
}

void WorkerManager::setRepairWorker(Unit worker, const Unit & unitToRepair)
{
    m_workerData.setWorkerJob(worker, WorkerJobs::Repair, unitToRepair);
}

void WorkerManager::stopRepairing(const Unit & worker)
{
    m_workerData.WorkerStoppedRepairing(worker);
	if (m_workerData.isProxyWorker(worker))
		return;
    finishedWithWorker(worker);
}

void WorkerManager::handleGeyserProtectWorkers()
{
	auto enemyRace = m_bot.GetPlayerRace(Players::Enemy);
	if (m_bot.Strategy().isWorkerRushed() || enemyRace == sc2::Race::Terran || enemyRace == sc2::Race::Random)
		return;
	auto mainBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
	if (!mainBase)
		return;
	if (m_bot.Bases().getOccupiedBaseLocations(Players::Self).size() > 1)
	{
		freeGeyserProtectors();
		return;
	}
	auto enemyWorkerType = enemyRace == sc2::Race::Protoss ? sc2::UNIT_TYPEID::PROTOSS_PROBE : sc2::UNIT_TYPEID::ZERG_DRONE;
	auto & gasBuildings = m_bot.GetAllyUnits(Util::GetRefineryType().getAPIUnitType());
	sc2::Units freeGeysers;
	for (auto & geyser : mainBase->getGeysers())
	{
		bool freeGeyser = true;
		for (auto & gasBuilding : gasBuildings)
		{
			if (Util::DistSq(gasBuilding, geyser) < 1)
			{
				freeGeyser = false;
				break;
			}
		}
		if (freeGeyser)
			freeGeysers.push_back(geyser.getUnitPtr());
	}
	if (freeGeysers.empty())
	{
		freeGeyserProtectors();
		return;
	}
	for (auto freeGeyser : freeGeysers)
	{
		bool shouldProtect = false;
		for (auto & enemyWorker : m_bot.GetEnemyUnits(enemyWorkerType))
		{
			if (!enemyWorker.isValid())
				continue;
			if (enemyWorker.getUnitPtr()->last_seen_game_loop != m_bot.GetCurrentFrame())
				continue;
			if (Util::DistSq(enemyWorker, freeGeyser->pos) < 10 * 10)
			{
				shouldProtect = true;
				break;
			}
		}
		auto it = geyserProtectors.find(freeGeyser);
		if (it == geyserProtectors.end())
		{
			if (shouldProtect)
			{
				auto worker = getClosestAvailableWorkerTo(freeGeyser->pos);
				if (worker.isValid())
				{
					geyserProtectors[freeGeyser] = worker;
					m_workerData.setWorkerJob(worker, WorkerJobs::GeyserProtect);
					Micro::SmartMove(worker.getUnitPtr(), freeGeyser->pos, m_bot);
					Micro::SmartHold(worker.getUnitPtr(), true, m_bot);
				}
			}
		}
		else
		{
			if (!shouldProtect)
			{
				m_workerData.setWorkerJob(it->second, WorkerJobs::Idle);
				geyserProtectors.erase(it);
			}
		}
	}
}

void WorkerManager::freeGeyserProtectors()
{
	for (auto geyserProtector : geyserProtectors)
	{
		m_workerData.setWorkerJob(geyserProtector.second, WorkerJobs::Idle);
	}
	geyserProtectors.clear();
}

void WorkerManager::handleMineralWorkers()
{
	handleMules();

	const auto & workers = getWorkers();

	Unit proxyWorker;
	// Send second proxy worker for proxy Marauders strategy
	if (m_bot.Strategy().getStartingStrategy() == PROXY_MARAUDERS 
		&& !m_secondProxyWorkerSent
		&& m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::SupplyDepot.getUnitType(), true, true, true) == 1)
	{
		float minDist = 0.f;
		const auto rampPosition = Util::GetPosition(m_bot.Buildings().getWallPosition());
		for (const auto & worker : workers)
		{
			if (isReturningCargo(worker) || !isFree(worker))
				continue;
			const auto dist = Util::DistSq(worker, rampPosition);
			if (!proxyWorker.isValid() || dist < minDist)
			{
				minDist = dist;
				proxyWorker = worker;
			}
		}
		if (proxyWorker.isValid())
		{
			proxyWorker.move(m_bot.Buildings().getProxyLocation());
			m_workerData.setProxyWorker(proxyWorker);
			m_workerData.setWorkerJob(proxyWorker, WorkerJobs::Idle);
			m_secondProxyWorkerSent = true;
		}
	}

	//TODO If no idle, move far patch worker to close patch?
	for (auto & worker : workers)
	{
		if (!worker.isValid() || !worker.isAlive() || worker.getType().isMule())
			continue;
		auto job = m_workerData.getWorkerJob(worker);

		//Correct the mining workers target if its not the right one.
		if (job == WorkerJobs::Minerals)
		{
			auto mineral = m_workerData.m_workerMineralMap.find(worker);
			if (mineral != m_workerData.m_workerMineralMap.end())
			{
				auto depot = m_workerData.updateWorkerDepot(worker, mineral->second);
				if (depot.isValid())
				{
					if (worker.isReturningCargo())
					{
						if (!m_bot.Config().IsRealTime)
						{
							auto distsq = Util::DistSq(worker.getPosition(), depot.getPosition());
							if (distsq > 3.5f * 3.5f && distsq < 5 * 5 && worker.getUnitPtr()->orders.size() < 2)
							{
								//3 is the distance with the center of the depot, its arbitrary
								worker.move(depot.getPosition() + Util::Normalized(worker.getPosition() - depot.getPosition()) * 3);
								worker.shiftRightClick(depot);
							}
						}
					}
					else
					{
						auto distToMineral = Util::DistSq(worker.getPosition(), mineral->second.getPosition());
						if (!m_bot.Config().IsRealTime && distToMineral > 2 * 2 && distToMineral < 2.5f * 2.5f && worker.getUnitPtr()->orders.size() < 2)//Distsq 3 is arbitrary but works great
						{
							//if (!Util::IsFarMineralPatch(mineral->second.getAPIUnitType()))
							{
								//1.3 is the distance with the mineral, its arbitrary
								worker.move(mineral->second.getPosition() + Util::Normalized(worker.getPosition() - mineral->second.getPosition()) * 1.3f);
								worker.shiftRightClick(mineral->second);
							}
						}
						else
						{
							sc2::Tag target;
							if (worker.getUnitPtr()->orders.size() > 0)
							{
								if (worker.getUnitPtr()->orders[0].ability_id == sc2::ABILITY_ID::MOVE)//If he has a move order, let it happen.
								{
									continue;
								}
								target = worker.getUnitPtr()->orders[0].target_unit_tag;
							}

							if (mineral->second.getTag() != target)
							{
								worker.rightClick(mineral->second);
							}
						}
					}
				}
				else // No depot for the mineral, the worker shouldn't be a mineral worker
				{
					m_workerData.setWorkerJob(worker, WorkerJobs::Idle);
				}
			}
			else//Unfound, not normal
			{
				m_workerData.setWorkerJob(worker, WorkerJobs::Idle);
			}
		}
	}

	//split workers on first frame and handle proxy
	if (!m_isFirstFrame)
	{
		return;
	}
	m_isFirstFrame = false;

	// Send the first worker at the proxy location if we have a proxy strategy, but not with proxy Marauders because we want to make the first Barracks at home
	if (m_bot.Strategy().isProxyStartingStrategy() && m_bot.Strategy().getStartingStrategy() != PROXY_MARAUDERS)
	{
		float minDist = 0.f;
		const auto & workers = getWorkers();
		const auto rampPosition = Util::GetPosition(m_bot.Buildings().getWallPosition());
		for (const auto & worker : workers)
		{
			const auto dist = Util::DistSq(worker, rampPosition);
			if (!proxyWorker.isValid() || dist < minDist)
			{
				minDist = dist;
				proxyWorker = worker;
			}
		}
		proxyWorker.move(m_bot.Buildings().getProxyLocation());
		m_workerData.setProxyWorker(proxyWorker);
	}

	m_bot.StartProfiling("0.7.2.2     frame1WorkerSplit");
	auto & main = m_bot.Bases().getOccupiedBaseLocations(Players::Self);
	auto & closePatch = (*main.begin())->getCloseMinerals();
	auto & farPatch = (*main.begin())->getFarMinerals();
	std::vector<CCUnitID> usedWorkers;
	for (auto & mineral : closePatch)
	{
		usedWorkers = dispatchWorkerToMineral(mineral, usedWorkers, (*main.begin())->getResourceDepot());
	}
	for (auto & mineral : farPatch)
	{
		usedWorkers = dispatchWorkerToMineral(mineral, usedWorkers, (*main.begin())->getResourceDepot());
	}
	for (auto & mineral : closePatch)
	{
		usedWorkers = dispatchWorkerToMineral(mineral, usedWorkers, (*main.begin())->getResourceDepot());
	}
	for (auto & mineral : farPatch)
	{
		usedWorkers = dispatchWorkerToMineral(mineral, usedWorkers, (*main.begin())->getResourceDepot());
	}
	m_bot.StopProfiling("0.7.2.2     frame1WorkerSplit");
}

std::vector<CCUnitID> WorkerManager::dispatchWorkerToMineral(const Unit & mineral, std::vector<CCUnitID> usedWorkers, const Unit & ressourceDepot)
{
	auto worker = getClosestAvailableWorkerTo(mineral.getPosition(), usedWorkers, 0);
	if (!worker.isValid())
	{
		return usedWorkers;
	}
	usedWorkers.push_back(worker.getTag());

	worker.rightClick(mineral);

	m_workerData.m_workerMineralMap[worker] = mineral;
	m_workerData.m_mineralWorkersMap[mineral].push_back(worker.getTag());

	m_workerData.setWorkerJob(worker, WorkerJobs::Minerals, ressourceDepot);
	return usedWorkers;
}

void WorkerManager::handleMules()
{
	//Clear mineral patch of mule that expired
	std::vector<CCUnitID> toRemove;
	int frame = m_bot.GetCurrentFrame();
	for (auto & i : mineralMuleDeathFrame)
	{
		if (frame >= i.second)
		{
			toRemove.push_back(i.first);
		}
	}
	for (auto & remove : toRemove)
	{
		mineralMuleDeathFrame.erase(remove);
	}

	for (auto & mule : m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_MULE))
	{
		if (!mule.isValid())
		{
			continue;
		}

		auto id = mule.getTag();
		if (muleHarvests.find(id) == muleHarvests.end())
		{
			muleHarvests[id] = mule_info();
			muleHarvests[id].deathFrame = m_bot.GetCurrentFrame() + 1344;
		}
		else
		{
			auto mineral = muleHarvests[id].mineral.getUnitPtr();
			auto abilityId = mule.getUnitPtr()->orders.empty() ? sc2::ABILITY_ID::INVALID : mule.getUnitPtr()->orders[0].ability_id.ToType();
			auto allowedAbilities = { sc2::ABILITY_ID::MOVE, sc2::ABILITY_ID::HARVEST_GATHER, sc2::ABILITY_ID::HARVEST_RETURN };
			if (abilityId == sc2::ABILITY_ID::INVALID || !Util::Contains(abilityId, allowedAbilities))
			{
				if (!mineral)
					mineral = m_bot.Buildings().getClosestMineral(mule.getPosition());
				if (mineral)
				{
					Micro::SmartRightClick(mule.getUnitPtr(), mineral, m_bot);//Cannot be done frame 1, thats why its in the 'else' clause
					muleHarvests[id].mineral = m_bot.GetNeutralUnits()[mineral->tag];
				}
			}
		}

		if (isReturningCargo(mule))
		{
			muleHarvests[id].isReturningCargo = true;
		}
		else
		{
			if (muleHarvests[id].isReturningCargo)//Cargo was returned
			{
				muleHarvests[id].isReturningCargo = false;
				muleHarvests[id].finishedHarvestCount++;

				//if (muleHarvests[id].harvestFramesRequired != 0 && m_bot.GetCurrentFrame() + muleHarvests[id].harvestFramesRequired > muleHarvests[id].deathFrame)//Maximum of 9 harvest per mule, the mules can't finish the 10th.
				if(m_bot.GetCurrentFrame() + muleHarvests[id].harvestFramesRequired > muleHarvests[id].deathFrame)
				{
					mule.move(m_bot.Map().center());
					muleHarvests.erase(id);
				}
				else
				{
					muleHarvests[id].harvestFramesRequired = (muleHarvests[id].lastCargoReturnFrame == 0 ? 0 : m_bot.GetCurrentFrame() - muleHarvests[id].lastCargoReturnFrame);
					muleHarvests[id].lastCargoReturnFrame = m_bot.GetCurrentFrame();

					//mule.move(muleHarvests[id].mineral.getPosition());

					//Micro mules, does not seem possible to get a 10th trip.... so its commented out. Missing just a handful of frames to succeed
					//mule.move(muleHarvests[id].mineral.getPosition());
					//auto a = mule.getUnitPtr()->radius;
					//auto b = muleHarvests[id].mineral.getUnitPtr()->radius;
					//mule.move(muleHarvests[id].mineral.getPosition() + Util::Normalized(mule.getPosition() - muleHarvests[id].mineral.getPosition()) * 2.9);//2.5? 2.7 (146)?
					//mule.shiftRightClick(muleHarvests[id].mineral);
				}
			}
		}
	}
}

void WorkerManager::handleGasWorkers()
{
	int mineral = m_bot.GetMinerals();
	int gas = m_bot.GetGas();
	const int ressourceTreshold = 300;
	int previousGasWorkersTarget = gasWorkersTarget;

	switch (gasWorkersTarget)
	{
		case 0:
			if (mineral > gas)
			{
				gasWorkersTarget = 1;
			}
			break;
		case 1:
			if (gas > mineral * 5 && mineral + gas > ressourceTreshold)
			{
				if (getNumMineralWorkers() <= 10)
				{
					gasWorkersTarget = 0;
				}
				else
				{
					gasWorkersTarget = 1;
				}
			}
			else if (mineral > gas * 1)
			{
				gasWorkersTarget = 2;
			}
			break;
		case 2:
			if (gas > mineral * 3 && mineral + gas > ressourceTreshold)
			{
				gasWorkersTarget = 1;
			}
			else if (mineral > gas * 1.5f)
			{
				gasWorkersTarget = 3;
			}
			break;
		case 3:
			if (gas > mineral * 2 && mineral + gas > ressourceTreshold)
			{
				gasWorkersTarget = 2;
			}
			break;
		default:
			gasWorkersTarget = 3;
	}

	if (m_bot.Strategy().isWorkerRushed())
	{
		//If we have enough gas to produce reaper or we are/have produced one, we don't need gas anymore.
		if (m_bot.GetGas() >= 50 || m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Reaper.getUnitType(), false, true, true) > 0)
		{
			gasWorkersTarget = 0;
		}
		else
		{
			gasWorkersTarget = 3;
		}
	}

	for (auto & geyser : m_bot.GetAllyGeyserUnits())
    {
        // if that unit is a refinery
		if (!geyser.isCompleted() || geyser.getUnitPtr()->vespene_contents <= 0)
			continue;

		// get the number of workers currently assigned to it
		int numAssigned = m_workerData.getNumAssignedWorkers(geyser);

		auto geyserPosition = geyser.getPosition();
		auto base = m_bot.Bases().getBaseContainingPosition(geyserPosition, Players::Self);
		if (base == nullptr)
		{
			m_bot.StartProfiling("0.7.3.1    setIdleWhenBaseIsDestroyed");
			//if the base is destroyed, remove the gas workers
			for (int i = 0; i < numAssigned; i++)
			{
				auto gasWorker = getGasWorker(geyser, false, false);
				m_workerData.setWorkerJob(gasWorker, WorkerJobs::Idle);
			}
			m_bot.StopProfiling("0.7.3.1    setIdleWhenBaseIsDestroyed");
			continue;
		}

		//TODO doesn't handle split geysers if only one of the geysers has a bunker.
		const auto & gasBunkers = base->getGasBunkers();

		//Bunker counts as a worker (for 2 and 3 only, we still want 1 worker at 1)
		int geyserGasWorkersTarget = (!gasBunkers.empty() && gasBunkers.size() > 0 && gasBunkers[0].isCompleted() && gasWorkersTarget > 1 ? gasWorkersTarget - 1 : gasWorkersTarget);

		auto & depot = base->getResourceDepot();
		if (!depot.isValid() || !depot.isCompleted())
			continue;

		if (numAssigned < geyserGasWorkersTarget)
		{
			m_bot.StartProfiling("0.7.3.2    assignNewGasWorkers");
			// if it's less than we want it to be, fill 'er up
			bool shouldAssignThisWorker = true;
			auto refineryWorkers = m_workerData.getAssignedWorkersRefinery(geyser);
			for (auto & worker : refineryWorkers)
			{
				if (!isInsideGeyser(worker) && !isReturningCargo(worker))
				{
					shouldAssignThisWorker = false;
					break;
				}
			}

			if (shouldAssignThisWorker)
			{
				auto mineralWorker = getMineralWorker(geyser);
				if (mineralWorker.isValid())
				{
					if (!base->isUnderAttack() && Util::PathFinding::IsPathToGoalSafe(mineralWorker.getUnitPtr(), geyserPosition, true, m_bot))
					{
						m_workerData.setWorkerJob(mineralWorker, WorkerJobs::Gas, geyser);
					}
				}
			}
			m_bot.StopProfiling("0.7.3.2    assignNewGasWorkers");
		}
		else if (numAssigned > geyserGasWorkersTarget)
		{
			m_bot.StartProfiling("0.7.3.3    unassignGasWorkers");
			int mineralWorkerRoom = 26;//Number of free spaces for mineral workers
			int mineralWorkersCount = m_workerData.getNumAssignedWorkers(depot);
			int optimalWorkersCount = base->getOptimalMineralWorkerCount();
			mineralWorkerRoom = optimalWorkersCount - mineralWorkersCount;

			// if it's more than we want it to be, empty it up
			for (int i = 0; i<(numAssigned - geyserGasWorkersTarget); ++i)
			{
				//check if we have room for more mineral workers
				if (mineralWorkerRoom <= 0 && numAssigned < 3)
				{//Do not remove gas workers if we don't have room for an additional mineral worker, except if we are at 3 gas workers (exception for bunkers).
					break;
				}

				auto gasWorker = getGasWorker(geyser, true, true);
				if (gasWorker.isValid())
				{
					if (m_workerData.getWorkerJob(gasWorker) != WorkerJobs::Gas)
					{
						Util::DisplayError(__FUNCTION__, "Worker assigned to a refinery is not a gas worker.", m_bot);
					}

					gasWorker.stop();
					getWorkerData().setWorkerJob(gasWorker, WorkerJobs::Idle);

					mineralWorkerRoom--;
				}
			}
			m_bot.StopProfiling("0.7.3.3    unassignGasWorkers");
		}
    }

	m_bot.StartProfiling("0.7.3.4    gasBunkerMicro");
#ifndef PUBLIC_RELEASE
	std::vector<sc2::Tag> bunkerHasLoaded;
#endif
	for (auto & geyser : m_bot.GetAllyGeyserUnits())
	{
		m_bot.StartProfiling("0.7.3.4.1     initialChecks");
		auto base = m_bot.Bases().getBaseContainingPosition(geyser.getPosition(), Players::Self);
		if (base == nullptr)
		{
			m_bot.StopProfiling("0.7.3.4.1     initialChecks");
			continue;
		}
		auto & depot = base->getResourceDepot();
		bool hasUsableDepot = true;
		if (!depot.isValid() || (depot.isValid() && depot.getUnitPtr()->build_progress < 1))//Do not using the gas bunker with an unfinished or inexistant depot.
		{
			hasUsableDepot = false;
		}

		auto workers = m_bot.Workers().m_workerData.getAssignedWorkersRefinery(geyser);
		m_bot.StopProfiling("0.7.3.4.1     initialChecks");
		for (auto & bunker : base->getGasBunkers())
		{
			if (!bunker.isCompleted())
			{
				continue;
			}

			if (!hasUsableDepot)//If there is no depot or if its not finished, empty the bunker.
			{
				Micro::SmartAbility(bunker.getUnitPtr(), sc2::ABILITY_ID::UNLOADALL, m_bot);
				continue;
			}

			auto hasReturningWorker = false;
			if (!base->isGeyserSplit())
			{
				m_bot.StartProfiling("0.7.3.4.2     handleWorkers");
				for (auto & worker : workers)//Handle workers inside
				{
					if (m_bot.Commander().isInside(worker.getTag()))
					{
						if (worker.isReturningCargo())//drop on CC side
						{
							bunker.rightClick(base->getDepotPosition());
						}
						else//drop on refinery side
						{
							bunker.rightClick(base->getGasBunkerUnloadTarget(geyser.getPosition()));
						}
#ifdef PUBLIC_RELEASE
						bunker.unloadUnit(worker.getTag());
#else
						Micro::SmartAbility(bunker.getUnitPtr(), sc2::ABILITY_ID::UNLOADALL, m_bot);
#endif
					}
					else
					{
						auto distWorkerDepot = Util::DistSq(worker.getPosition(), base->getDepotPosition());
						auto distDepotBunker = Util::DistSq(base->getDepotPosition(), bunker.getPosition());
						if (worker.isReturningCargo())
						{
#ifdef PUBLIC_RELEASE
							if (distWorkerDepot > distDepotBunker)//If the bunker is empty or if there is already a returning worker, click to enter bunker
#else
							if ((std::find(bunkerHasLoaded.begin(), bunkerHasLoaded.end(), bunker.getTag()) == bunkerHasLoaded.end() || hasReturningWorker) && distWorkerDepot > distDepotBunker)
#endif
							{
								if (worker.getUnitPtr()->orders.size() == 0 || worker.getUnitPtr()->orders[0].target_unit_tag != bunker.getTag())
								{
									worker.rightClick(bunker);
								}
#ifndef PUBLIC_RELEASE
								bunkerHasLoaded.push_back(bunker.getTag());
#endif
								hasReturningWorker = true;
							}
							else//Click to drop resource
							{
								if (worker.getUnitPtr()->orders.size() == 0 || worker.getUnitPtr()->orders[0].target_unit_tag != depot.getTag())
								{
									worker.rightClick(depot);
								}
							}
						}
						else
						{
							if (distWorkerDepot > distDepotBunker)//Click to enter refinery
							{
								if (worker.getUnitPtr()->orders.size() == 0 || worker.getUnitPtr()->orders[0].target_unit_tag != geyser.getTag())
								{
									worker.rightClick(geyser);
								}
							}
#ifdef PUBLIC_RELEASE
							else//Click to enter bunker
#else
							else if (std::find(bunkerHasLoaded.begin(), bunkerHasLoaded.end(), bunker.getTag()) == bunkerHasLoaded.end())
#endif
							{
								if (worker.getUnitPtr()->orders.size() == 0 || worker.getUnitPtr()->orders[0].target_unit_tag != bunker.getTag())
								{
									Micro::SmartAbility(bunker.getUnitPtr(), sc2::ABILITY_ID::LOAD, worker.getUnitPtr(), m_bot);//Causes the workers to do a full 180, instead of taking 2-3 frames to turn around.
								}
#ifndef PUBLIC_RELEASE
								bunkerHasLoaded.push_back(bunker.getTag());
#endif
							}
						}
					}
				}
				m_bot.StopProfiling("0.7.3.4.2     handleWorkers");
				m_bot.StartProfiling("0.7.3.4.3     unloadUnwantedPassengers");
				if (workers.size() == 0)//Empty bunkers if they have units inside that shouldn't be inside
				{
					auto passengers = bunker.getUnitPtr()->passengers;
					for (auto & passenger : passengers)
					{
						if(passenger.unit_type == sc2::UNIT_TYPEID::TERRAN_SCV)
						{
#ifdef PUBLIC_RELEASE
							bunker.unloadUnit(passenger.tag);
#else
							Micro::SmartAbility(bunker.getUnitPtr(), sc2::ABILITY_ID::UNLOADALL, m_bot);
							break;
#endif
						}
					}
				}
				m_bot.StopProfiling("0.7.3.4.3     unloadUnwantedPassengers");
			}
			else
			{
				//UNHANDLED SINGLE GEYSER
			}
			m_bot.StartProfiling("0.7.3.4.5     unloadOutOfPlaceWorkers");
			for (auto & unit : bunker.getUnitPtr()->passengers)
			{
				if (unit.unit_type != sc2::UNIT_TYPEID::TERRAN_SCV)
					continue;
				bool isOutOfPlace = true;
				for (auto & worker : workers)
				{
					if (unit.tag == worker.getTag())
					{
						if (getWorkerData().getWorkerJob(worker) != WorkerJobs::Gas)
						{
#ifdef PUBLIC_RELEASE
							bunker.unloadUnit(unit.tag);
#else
							//Technically illogical, it cannot happen if we use UNLOADALL above. But I'd rather have the code anyway in case anything changes.
							Micro::SmartAbility(bunker.getUnitPtr(), sc2::ABILITY_ID::UNLOADALL, m_bot);
#endif
							break;
						}
					}
				}
			}
			m_bot.StopProfiling("0.7.3.4.5     unloadOutOfPlaceWorkers");
		}
	}
	m_bot.StopProfiling("0.7.3.4    gasBunkerMicro");
}

void WorkerManager::handleIdleWorkers()
{
    // for each of our workers
    for (auto & worker : m_workerData.getWorkers())
    {
        if (!worker.isValid()) { continue; }
		if (worker.getType().isMule()) 
		{ continue; }

		int workerJob = m_workerData.getWorkerJob(worker);
		bool idle = worker.isIdle();
        if (idle &&
            // We need to consider building worker because of builder finishing the job of another worker is not consider idle.
			//(m_workerData.getWorkerJob(worker) != WorkerJobs::Build) && 
			(workerJob != WorkerJobs::Idle) &&
            (workerJob != WorkerJobs::Move) &&
            (workerJob != WorkerJobs::Repair) &&
			(workerJob != WorkerJobs::Scout) &&
			(workerJob != WorkerJobs::Combat) &&
			(workerJob != WorkerJobs::Build))//Prevent premoved builder from going Idle if they lack the ressources, also prevents refinery builder from going Idle
		{
			m_bot.StartProfiling("0.7.4.1    setIdleJob");
			m_workerData.setWorkerJob(worker, WorkerJobs::Idle);
			workerJob = WorkerJobs::Idle;
			m_bot.StopProfiling("0.7.4.1    setIdleJob");
		}
		else if (workerJob == WorkerJobs::Build)
		{
			if (!worker.isConstructingAnything())
			{
				m_bot.StartProfiling("0.7.4.2    checkHasBuilding");
				bool hasBuilding = false;
				bool isCloseToBuildingLocation = false;
				if (idle)
				{
					//Check if worker is waiting to build
					for (auto b : m_bot.Buildings().getBuildings())
					{
						if (!b.builderUnit.isValid()) continue;
						if (b.buildingUnit.isValid() && b.buildingUnit.isCompleted()) continue;
						if (b.builderUnit.getTag() == worker.getTag())
						{
							//Do not try to patrol on refinery, could slow down the building creation
							if (!b.type.isRefinery())
							{
								hasBuilding = true;
								if (Util::DistSq(b.builderUnit, Util::GetPosition(b.finalPosition)) < 1.f * 1.f)
								{
									isCloseToBuildingLocation = true;
								}
							}
							break;
						}
					}
				}
				m_bot.StopProfiling("0.7.4.2    checkHasBuilding");
				if (hasBuilding)
				{
					if (isCloseToBuildingLocation)
					{
						//Is waiting for resources, so we patrol
						//Patrol requires at least a 1 tile distance, 0.707 is exactly what we need to have a diagonal distance of 1.
						auto patrolTarget = worker.getPosition();
						patrolTarget.x += 0.71f;
						patrolTarget.y += 0.71f;

						worker.patrol(patrolTarget);
					}
				}
				else
				{
					auto orders = worker.getUnitPtr()->orders;
					if (!orders.empty() && orders[0].ability_id != sc2::ABILITY_ID::PATROL)
					{
						m_bot.StartProfiling("0.7.4.3    setIdleJobToBuilder");
						//return mining
						m_workerData.setWorkerJob(worker, WorkerJobs::Idle);
						workerJob = WorkerJobs::Idle;
						m_bot.StopProfiling("0.7.4.3    setIdleJobToBuilder");
					}
				}
			}
		}

		if (workerJob == WorkerJobs::Idle)
		{
			if (!worker.isAlive())
			{
				worker.stop();
			}
			else
			{
				m_bot.StartProfiling("0.7.4.4    setBuildJob");
				bool isBuilder = false;
				for(const auto & building : m_bot.Buildings().getBuildings())
				{
					if(building.builderUnit == worker)
					{
						m_workerData.setWorkerJob(worker, WorkerJobs::Build, building.buildingUnit);
						isBuilder = true;
						break;
					}
				}
				m_bot.StopProfiling("0.7.4.4    setBuildJob");
				
				if (!isBuilder && !m_workerData.isProxyWorker(worker))
				{
					m_bot.StartProfiling("0.7.4.5    isAnyMineralAvailable");
					const bool isAnyMineralAvailable = m_workerData.isAnyMineralAvailable(worker.getPosition());
					m_bot.StopProfiling("0.7.4.5    isAnyMineralAvailable");
					if (isAnyMineralAvailable)
					{
						m_bot.StartProfiling("0.7.4.6    setMineralWorker");
						setMineralWorker(worker);
						m_bot.StopProfiling("0.7.4.6    setMineralWorker");
					}
					else//Do not set as mineral worker if there is no place for it
					{
						m_bot.StartProfiling("0.7.4.7    sendIdleWorkerToMiningSpot");
						m_workerData.sendIdleWorkerToMiningSpot(worker, false);
						m_bot.StopProfiling("0.7.4.7    sendIdleWorkerToMiningSpot");
					}
				}
			}
		}
    }
}

void WorkerManager::handleRepairWorkers()
{
	const int REPAIR_STATION_MIN_MINERAL = 10;//Also affects the automatic building repairing

    // Only terran worker can repair
    if (!Util::IsTerran(m_bot.GetSelfRace()))
        return;

	int mineral = m_bot.GetMinerals();
	int gas = m_bot.GetGas();

	m_bot.StartProfiling("0.7.7.1    stopRepairing");
    for (auto & worker : m_workerData.getWorkers())
    {
        if (!worker.isValid()) { continue; }

        if (m_workerData.getWorkerJob(worker) == WorkerJobs::Repair)
        {
			if (!worker.isAlive())
			{
				// We inform the manager that we are no longer repairing
				stopRepairing(worker);
			}
            Unit& repairedUnit = m_workerData.getWorkerRepairTarget(worker);
			if (repairedUnit.isValid())
			{
				auto type = repairedUnit.getType();
				// We do not try to repair if we don't have the required ressources
				if ((type.mineralPrice() > 0 && mineral == 0) || (type.gasPrice() > 0 && gas == 0))
				{
					// We inform the manager that we are no longer repairing
					stopRepairing(worker);
				}
				// We do not try to repair dead units nor full health units
				else if (!repairedUnit.isAlive() || repairedUnit.getHitPoints() + std::numeric_limits<float>::epsilon() >= repairedUnit.getUnitPtr()->health_max)
				{
					stopRepairing(worker);
				}
				//Stop repairing units that are no longer wanting to be repaired
				else if (!repairedUnit.getType().isBuilding() && m_bot.Bases().getBaseContainingPosition(worker.getPosition()) != m_bot.Bases().getBaseContainingPosition(repairedUnit.getPosition()))
				{
					stopRepairing(worker);
				}
				else if (worker.isIdle() || worker.getUnitPtr()->orders[0].ability_id != sc2::ABILITY_ID::EFFECT_REPAIR)
				{
					// Do not spam repair action if the unit is a teleporting BC
					if (repairedUnit.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER)
					{
						auto & unitAction = m_bot.Commander().Combat().GetUnitAction(repairedUnit.getUnitPtr());
						if (unitAction.abilityID == sc2::ABILITY_ID::EFFECT_TACTICALJUMP && !unitAction.finished)
						{
							continue;
						}
					}
					// Get back to repairing...
					worker.repair(repairedUnit);
				}
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
	m_bot.StopProfiling("0.7.7.1    stopRepairing");

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

	/* Chart of repair worker based on minerals
	0: 10 mineral-
	1: 10 mineral+
	2: 40 mineral+
	3: 60 mineral+
	4: 80 mineral+
	5: 100 mineral+
	*/
	int currentMaxRepairWorker = std::min(MAX_REPAIR_WORKER, (int)floor(MAX_REPAIR_WORKER * (mineral / (float)FULL_REPAIR_MINERAL)));

	m_bot.StartProfiling("0.7.7.2    chooseRepairStationWorkers");
	auto & bases = m_bot.Bases().getOccupiedBaseLocations(Players::Self);
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

			if (getWorkerData().getWorkerRepairingTargetCount(unit) > std::min(3, 5 - int(std::floor(unit.getHitPointsPercentage() / 20.f))))//Max number of worker proportional to health. Same logic as lower in this method.
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

				const float distanceSquare = Util::DistSq(unit, base->getRepairStationTilePosition());
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

				if (workerData.getWorkerJob(worker) == WorkerJobs::Idle || workerData.getWorkerJob(worker) == WorkerJobs::Minerals || workerData.getWorkerJob(worker) == WorkerJobs::Repair)
				{
					const float distanceSquare = Util::DistSq(worker, base->getRepairStationTilePosition());
					if (distanceSquare < REPAIR_STATION_WORKER_ZONE_SIZE * REPAIR_STATION_WORKER_ZONE_SIZE)
					{
						//Add worker to the list of repair station worker for this base
						auto & baseRepairWorkers = workerData.getRepairStationWorkers()[base];
						if ((std::find(baseRepairWorkers.begin(), baseRepairWorkers.end(), worker) == baseRepairWorkers.end()))
						{
							baseRepairWorkers.push_back(worker);
							setRepairWorker(worker, *it);
						}
						 
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
	m_bot.StopProfiling("0.7.7.2    chooseRepairStationWorkers");

	m_bot.StartProfiling("0.7.7.3    repairBuildings");
	//Automatically repair low health buildings, maximum 1 worker
	const float MIN_HEALTH = 50.f;
	const float MAX_HEALTH = 100.f;
	const int MAX_WORKER_PER_BUILDING = 1;
	for (auto & building : m_bot.Buildings().getFinishedBuildings())
	{
		if (!building.isCompleted())
		{
			continue;
		}

		auto percentage = building.getHitPointsPercentage();
		auto repairCount = getWorkerData().getWorkerRepairingTargetCount(building);

		if (percentage <= MIN_HEALTH && repairCount < MAX_WORKER_PER_BUILDING)
		{
			auto position = building.getPosition();
			auto worker = getClosestAvailableWorkerTo(position);
			if (worker.isValid())
			{
				if (Util::DistSq(position, worker.getPosition()) < 30 * 30 && Util::PathFinding::IsPathToGoalSafe(worker.getUnitPtr(), position, true, m_bot))
				{
					setRepairWorker(worker, building);
					buildingAutomaticallyRepaired.push_back(building);
				}
			}
		}
		else if (percentage >= MAX_HEALTH)
		{
			if (std::find(buildingAutomaticallyRepaired.begin(), buildingAutomaticallyRepaired.end(), building) != buildingAutomaticallyRepaired.end())
			{
				buildingAutomaticallyRepaired.remove(building);
			}
		}
	}
	m_bot.StopProfiling("0.7.7.3    repairBuildings");

	m_bot.StartProfiling("0.7.7.4    repairSlowMechs");
	// Automatically repair slow mechs defending our base
	std::vector<sc2::UNIT_TYPEID> slowMechTypes = { sc2::UNIT_TYPEID::TERRAN_SIEGETANK, sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED, sc2::UNIT_TYPEID::TERRAN_THOR, sc2::UNIT_TYPEID::TERRAN_THORAP, sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER };
	std::map<std::string, Squad> & squads = m_bot.Commander().Combat().getSquadData().getSquads();
	for (auto & squadPair : squads)
	{
		auto & squad = squadPair.second;
		for (auto & unit : squad.getUnits())
		{
			// Only repair injured units
			if (unit.getHitPointsPercentage() == 100.f)
				continue;
			// Check if we have the required gas to repair it
			if (unit.getType().gasPrice() > 0 && gas <= MIN_GAS_TO_REPAIR)
				continue;
			// Check if the unit is a slow mech
			if (!Util::Contains(unit.getAPIUnitType(), slowMechTypes))
				continue;
			// Check in which base the slow mech is
			auto unitBase = m_bot.Bases().getBaseContainingPosition(unit.getPosition());
			if (!unitBase)
				continue;
			int repairerCount = m_workerData.getWorkerRepairingTargetCount(unit);
			int repairerCountTarget = std::min(3, 5 - int(std::floor(unit.getHitPointsPercentage() / 20.f)));//Max number of worker proportional to health. Same logic as higher in this method.
			while (repairerCount < repairerCountTarget)
			{
				Unit repairer = getClosestAvailableWorkerTo(unit.getPosition(), m_bot.Workers().MIN_HP_PERCENTAGE_TO_FIGHT, false, true);
				if (!repairer.isValid())
					break;
				auto repairerBase = m_bot.Bases().getBaseContainingPosition(repairer.getPosition());
				if (!repairerBase)
					break;
				if (unitBase != repairerBase)
					break;
				setRepairWorker(repairer, unit);
				++repairerCount;
			}
		}
	}
	m_bot.StopProfiling("0.7.7.4    repairSlowMechs");
}

void WorkerManager::handleBuildWorkers()
{
	for (auto & worker : getWorkers())
	{
		if (m_workerData.getWorkerJob(worker) != WorkerJobs::Build)
			continue;
		bool found = false;
		for (auto & building : m_bot.Buildings().getBuildings())
		{
			if (building.builderUnit.getTag() == worker.getTag())
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			finishedWithWorker(worker);
		}
	}
}

void WorkerManager::repairCombatBuildings()//Ignores if the path or the area around the building is safe or not.
{
	const float repairAt = 0.95f; //95% health
	const int maxReparator = 5; //Turret and bunkers only

	if (m_bot.GetSelfRace() != CCRace::Terran)
	{
		return;
	}

	bool completeWall = false;
	bool checkedCompleteWall = false;
	auto buildings = m_bot.Buildings().getFinishedBuildings();
	for (auto building : buildings)
	{
		if (building.isBeingConstructed())
		{
			continue;
		}

		if (building.getHitPoints() > repairAt * building.getUnitPtr()->health_max)
		{
			continue;
		}

		int maxReparator = 5;
		int alreadyRepairing = 0;
		bool shouldRepair = false;
		bool onlyBaseWorker = false;
		switch ((sc2::UNIT_TYPEID)building.getAPIUnitType())
		{
			case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER://Always repair command center and orbital
			case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
			case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
			case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
				maxReparator = 3;
				shouldRepair = true;
				onlyBaseWorker = true;
				break;
			case sc2::UNIT_TYPEID::TERRAN_MISSILETURRET://Always repair MISSILETURRET and BUNKER
			case sc2::UNIT_TYPEID::TERRAN_BUNKER:
				shouldRepair = true;
				break;
			case sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
				shouldRepair = true;
				{	// scope needed to declare new variables in the switch case
					// first SCV repairs the PF at 41.67 hp/s, the second at 20.83, the third at 10.42, etc.
					const auto recentDamageTaken = m_bot.Analyzer().getUnitState(building.getUnitPtr()).GetRecentDamageTaken();
					const auto baseRepairPerSecond = building.getUnitPtr()->health_max / 107.f;	// HP divided by construction time (CC + morph)
					
					maxReparator = 0;
					float repairPerSecond = 0.f;
					do
					{
						++maxReparator;
						repairPerSecond += baseRepairPerSecond / maxReparator;
					} while (repairPerSecond < recentDamageTaken && maxReparator < 20);
					++maxReparator;	// add a buffer of 1 SCV
				}
				break;
			default:	// Allows to repair buildings in wall (repair wall)
				//NOTE: commented out because we don't currently have a good way to check if the enemy has ranged units that could hit our workers.
				//		We don't want to suicide all of our workers trying to repair. Should also only repair if its a full wall.
				maxReparator = 3;
				const auto buildingTilePos = building.getTilePosition();
				const auto buildingPos = Util::GetPosition(buildingTilePos);
				const auto buildingDistSq = Util::DistSq(buildingTilePos, m_bot.Buildings().getWallPosition());
				if (buildingDistSq < 5 * 5)	// Within 5 tiles of the wall
				{
					if (!checkedCompleteWall)
					{
						std::vector<Unit> possibleWallBuildings;
						//TODO maybe consider all types of buildings
						const auto & barracks = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKS);
						const auto & supplyDepots = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT);
						const auto & supplyDepotsLowered = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED);
						possibleWallBuildings.insert(possibleWallBuildings.end(), barracks.begin(), barracks.end());
						possibleWallBuildings.insert(possibleWallBuildings.end(), supplyDepots.begin(), supplyDepots.end());
						possibleWallBuildings.insert(possibleWallBuildings.end(), supplyDepotsLowered.begin(), supplyDepotsLowered.end());
						int wallBuildings = 0;
						for (const auto & possibleWallBuilding : possibleWallBuildings)
						{
							const auto distSq = Util::DistSq(possibleWallBuilding, buildingPos);
							if (distSq < 5 * 5)
								wallBuildings += 1;
						}
						if (wallBuildings >= 3)
							completeWall = true;
						checkedCompleteWall = true;
					}
					if (!completeWall)
					{
						shouldRepair = false;
						break;
					}
					shouldRepair = true;

					for (const auto & enemyUnit : m_bot.GetKnownEnemyUnits())
					{
						const auto enemyDistSq = Util::DistSq(enemyUnit, buildingPos);
						const auto enemyAttackRange = Util::GetAttackRangeForTarget(enemyUnit.getUnitPtr(), building.getUnitPtr(), m_bot);
						if (enemyAttackRange > 3 && enemyDistSq < enemyAttackRange * enemyAttackRange)
						{
							shouldRepair = false;
							break;
						}
					}
				}
				break;
		}
		if (shouldRepair)
		{
			int reparator = maxReparator - floor(maxReparator * building.getHitPointsPercentage() / 100);
			std::set<Unit> workers = getWorkers();
			for (auto & worker : workers)
			{
				Unit repairedUnit = m_workerData.getWorkerRepairTarget(worker);
				if (repairedUnit.isValid() && repairedUnit.getTag() == building.getTag())
				{
					alreadyRepairing++;
					if (reparator == alreadyRepairing)//Already enough repairer, stop checking how many are repairing
					{
						break;
					}
				}
			}

			for (int i = 0; i < reparator - alreadyRepairing; i++)
			{
				Unit worker = getClosestAvailableWorkerTo(building.getPosition(), 0.f, true, true);
				if (worker.isValid())
					setRepairWorker(worker, building);
				else
					break;
			}
		}
	}
}

Unit WorkerManager::getClosestAvailableWorkerTo(const CCPosition & pos, CCUnitID workerToIgnore, float minHpPercentage, bool filterMoving, bool filterDifferentHeight) const
{
	auto workersToIgnore = std::vector<CCUnitID>();
	workersToIgnore.push_back(workerToIgnore);
	return getClosestAvailableWorkerTo(pos, workersToIgnore, minHpPercentage, filterMoving, false, filterDifferentHeight);
}

Unit WorkerManager::getClosestAvailableWorkerTo(const CCPosition & pos, const std::vector<CCUnitID> & workersToIgnore, float minHpPercentage, bool filterMoving, bool allowCombatWorkers, bool filterDifferentHeight) const
{
	//TODO priorise workers on far patches? Or maybe rebalance workers in a lowPriorityCheck or when the worker drops off his mineral?
	//TODO doesnt priorize idle workers?

	Unit closestMineralWorker;
	auto closestDist = 0.f;

	const BaseLocation * base = nullptr;
	const auto & baseLocations = m_bot.Bases().getBaseLocations();
	for (const auto baseLocation : baseLocations)
	{
		if (Util::DistSq(pos, Util::GetPosition(baseLocation->getDepotTilePosition())) < 10 * 10 && Util::TerrainHeight(pos) == Util::TerrainHeight(baseLocation->getDepotPosition()))
		{
			base = baseLocation;
			break;
		}
	}

	// for each of our workers
	for (auto & worker : m_workerData.getWorkers())
	{
		if (!worker.isValid() || std::find(workersToIgnore.begin(), workersToIgnore.end(), worker.getTag()) != workersToIgnore.end()) { continue; }
		const sc2::Unit* workerPtr = worker.getUnitPtr();
		if (workerPtr->health < minHpPercentage * workerPtr->health_max)
			continue;

		// if it is a mineral worker, Idle or None
		int workerJob = m_workerData.getWorkerJob(worker);
		auto workerSquad = m_bot.Commander().Combat().getSquadData().getUnitSquad(worker);
		if (!isFree(worker) && (!allowCombatWorkers || workerJob != WorkerJobs::Combat))
			continue;
		if (isReturningCargo(worker))
			continue;
		if (m_workerData.isProxyWorker(worker) && Util::DistSq(pos, m_bot.Buildings().getProxyLocation()) > 20 * 20)
			continue;
		if (filterMoving && worker.isMoving() && (!allowCombatWorkers || workerJob != WorkerJobs::Combat))
			continue;
		if (filterDifferentHeight && Util::TerrainHeight(pos) != Util::TerrainHeight(worker.getPosition()))
			continue;
		
		auto dist = Util::DistSq(worker.getPosition(), pos);
		if (base != nullptr && dist > 20 * 20)
		{
			dist = base->getGroundDistance(worker.getPosition());
			dist *= dist;
		}
		if (!closestMineralWorker.isValid() || dist < closestDist)
		{
			closestMineralWorker = worker;
			closestDist = dist;
		}
	}
	return closestMineralWorker;
}


Unit WorkerManager::getClosestAvailableWorkerTo(const CCPosition & pos, float minHpPercentage, bool filterMoving, bool filterDifferentHeight) const
{
    return getClosestAvailableWorkerTo(pos, CCUnitID{}, minHpPercentage, filterMoving, filterDifferentHeight);
}

Unit WorkerManager::getClosestGasWorkerTo(const CCPosition & pos, CCUnitID workerToIgnore, float minHpPercentage) const
{
	Unit closestMineralWorker;
	double closestDist = std::numeric_limits<double>::max();

	// for each of our workers
	for (auto & worker : m_workerData.getWorkers())
	{
		if (!worker.isValid() || worker.getTag() == workerToIgnore) { continue; }
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
				///TODO: Maybe it should by ground distance? Not sure how it will affect performance
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
    if (depot.isValid() && depot.isCompleted())
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
    return getClosestAvailableWorkerTo(refinery.getPosition());
}

Unit WorkerManager::getGasWorker(Unit refinery, bool checkReturningCargo, bool checkInsideRefinery) const
{
	auto workers = m_workerData.getAssignedWorkersRefinery(refinery);
	if (checkReturningCargo || checkInsideRefinery)
	{
		for (auto & worker : workers)
		{
			if (m_bot.Commander().isInside(worker.getTag()))
			{
				continue;
			}

			if (checkInsideRefinery)
			{
				//Skip workers inside the geyser
				if (isInsideGeyser(worker))
				{
					continue;
				}
			}
			if (checkReturningCargo)
			{
				//Skip workers returning cargo
				if (isReturningCargo(worker))
				{
					continue;
				}
			}
			return worker;
		}
	}

	//If we found no suitable workers (so probably 1 returning cargo and 1 inside the geyser OR only 1 worker), then return the returning one if exist
	if (checkReturningCargo && checkInsideRefinery)
	{
		for (auto & worker : workers)
		{
			//Skip workers inside the geyser
			if (isInsideGeyser(worker))
			{
				continue;
			}

			//Return the worker returning cargo in priority over the worker inside the geyser
			return worker;
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
		if (baseLocation && baseLocation->getPosition().x == basePosition.x && baseLocation->getPosition().y == basePosition.y)
		{
			return unit;
		}
	}
	return {};
}

int WorkerManager::getWorkerCountAtBasePosition(CCPosition basePosition) const
{
	return getWorkerData().getCountWorkerAtDepot(getDepotAtBasePosition(basePosition));
}

// gets a builder for BuildingManager to use
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
Unit WorkerManager::getBuilder(Building & b, bool setJobAsBuilder, bool filterMoving, const std::vector<CCUnitID> unusableWorkers) const
{
	bool isValid;
	std::vector<CCUnitID> invalidWorkers = unusableWorkers;
	Unit builderWorker;
	
	do
	{
		builderWorker = Unit();
		isValid = true;
		if (invalidWorkers.size() == 0)//Priorize Idle workers
		{
			std::vector<CCUnitID> mineralWorkers;
			for (auto & worker : m_workerData.getMineralWorkers())
			{
				mineralWorkers.push_back(worker.getTag());
			}
			builderWorker = getClosestAvailableWorkerTo(Util::GetPosition(b.finalPosition), mineralWorkers, 0, filterMoving);
		}
		if(!builderWorker.isValid())//If no idle workers available, use mineral workers
		{
			builderWorker = getClosestAvailableWorkerTo(Util::GetPosition(b.finalPosition), invalidWorkers, 0, filterMoving);
		}

		if (!builderWorker.isValid())	// If we can't find a free worker
		{
			// Check for a combat worker
			builderWorker = getClosestAvailableWorkerTo(Util::GetPosition(b.finalPosition), invalidWorkers, 0, filterMoving, true);
			if (!builderWorker.isValid())	//If no worker left to check
			{
				break;
			}
		}

		//Check if worker is already building something else
		//TODO This shouldn't be needed (shouldn't happen), right?
		for (auto & building : m_bot.Buildings().getBuildings())
		{
			if (building.builderUnit.isValid() && building.builderUnit.getTag() == builderWorker.getTag())
			{
				invalidWorkers.push_back(builderWorker.getTag());
				isValid = false;
				break;
			}
		}
	} while (!isValid);

    // if the worker exists (one may not have been found in rare cases)
    if (builderWorker.isValid() && setJobAsBuilder && m_workerData.getWorkerJob(builderWorker) != WorkerJobs::Build)
    {
		m_workerData.setWorkerJob(builderWorker, WorkerJobs::Build, b.buildingUnit);
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
#ifdef PUBLIC_RELEASE
	return;
#endif
    if (!m_bot.Config().DrawResourceInfo)
    {
        return;
    }

    for (auto & worker : m_workerData.getWorkers())
    {
        if (!worker.isValid()) { continue; }
		if (worker.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_MULE)
		{
			continue;
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
#ifdef PUBLIC_RELEASE
	return;
#endif
    if (!m_bot.Config().DrawWorkerInfo)
    {
        return;
    }

    for (auto & worker : m_workerData.getWorkers())
    {
		std::ostringstream oss;
		if (worker.getType().getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_MULE)
		{
			if (muleHarvests.find(worker.getTag()) != muleHarvests.end())
			{
				oss << muleHarvests[worker.getTag()].finishedHarvestCount;
			}
			else
			{
				oss << 0;
			}
		}
		else
		{
			auto code = m_workerData.getJobCode(worker);
			oss << code;

			if (strcmp(code, "B") == 0)
			{
				std::string buildingType = "UNKNOWN";
				for (auto b : m_bot.Buildings().getBuildings())
				{
					if (b.builderUnit.getTag() == worker.getTag())
					{
						buildingType = b.type.getName();
						break;
					}
				}

				oss << " (" << buildingType << ")";
			}
			if (m_workerData.isProxyWorker(worker))
			{
				oss << " [Proxy]";
			}
		}

		m_bot.Map().drawText(worker.getPosition(), oss.str());

		if (true)//Advantage mineral worker display
		{
			auto & workers = getWorkers();
			for (auto & mineralWorker : m_workerData.m_mineralWorkersMap)
			{
				std::ostringstream oss;
				oss << mineralWorker.second.size();
				m_bot.Map().drawText(mineralWorker.first.getPosition(), oss.str());

				for (auto & worker : workers)
				{
					//if (m_workerData.getWorkerJob(worker) != WorkerJobs::Minerals)
					//{
					//	continue;
					//}
					if (std::find(mineralWorker.second.begin(), mineralWorker.second.end(), worker.getTag()) != mineralWorker.second.end())//worker.getTag() != mineralWorker.second)
					{
						m_bot.Map().drawLine(mineralWorker.first.getPosition(), worker.getPosition(), CCColor(0, 255, 0));
					}
				}
			}
		}
    }
}
 
bool WorkerManager::isFree(Unit worker) const
{
	if (worker.getType().isMule())
		return false;
	int job = m_workerData.getWorkerJob(worker);
    return job == WorkerJobs::Minerals || job == WorkerJobs::Idle || job == WorkerJobs::None || (m_workerData.isProxyWorker(worker) && job != WorkerJobs::Build);
}

bool WorkerManager::isInsideGeyser(Unit worker) const
{
	int job = m_workerData.getWorkerJob(worker);
	if (job == WorkerJobs::Gas)
	{
		auto loop = m_bot.GetGameLoop();
		if (worker.getUnitPtr()->last_seen_game_loop != m_bot.GetGameLoop())
		{
			return true;
		}
		return false;
	}
	return false;
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
	return worker.isReturningCargo();
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

const std::set<Unit> & WorkerManager::getWorkers() const
{
	return m_workerData.getWorkers();
}

WorkerData & WorkerManager::getWorkerData() const
{
	return m_workerData;
}

void WorkerManager::setMineralMuleDeathFrame(sc2::Tag mineralTag)
{
	mineralMuleDeathFrame[mineralTag] = m_bot.GetCurrentFrame() + 1344;
}

bool WorkerManager::isMineralMuleAvailable(sc2::Tag mineralTag)
{
	return mineralMuleDeathFrame.find(mineralTag) == mineralMuleDeathFrame.end();
}