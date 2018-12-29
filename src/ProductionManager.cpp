#include "ProductionManager.h"
#include "Util.h"
#include "CCBot.h"

ProductionManager::ProductionManager(CCBot & bot)
    : m_bot             (bot)
    , m_queue           (bot)
	, m_initialBuildOrderFinished(false)
{

}

void ProductionManager::setBuildOrder(const BuildOrder & buildOrder)
{
    m_queue.clearAll();

    for (size_t i(0); i<buildOrder.size(); ++i)
    {
        m_queue.queueAsLowestPriority(buildOrder[i], true);
    }
}


void ProductionManager::onStart()
{
    setBuildOrder(m_bot.Strategy().getOpeningBookBuildOrder());
	if (m_queue.isEmpty())
		Util::DisplayError("Initial build order is empty.", "0x00000003", m_bot, true);
	supplyProvider = Util::GetSupplyProvider(m_bot.GetSelfRace(), m_bot);
	supplyProviderType = MetaType(supplyProvider, m_bot);

	workerType = Util::GetWorkerType(m_bot.GetSelfRace(), m_bot);
	workerMetatype = MetaType(workerType, m_bot);

	//Upgrades
	const std::list<std::list<MetaType>> terranUpgrades = {
		{ MetaTypeEnum::TerranInfantryWeaponsLevel1, MetaTypeEnum::TerranInfantryWeaponsLevel2, MetaTypeEnum::TerranInfantryWeaponsLevel3 },
		{ MetaTypeEnum::TerranInfantryArmorsLevel1, MetaTypeEnum::TerranInfantryArmorsLevel2, MetaTypeEnum::TerranInfantryArmorsLevel3 },
		{ MetaTypeEnum::TerranVehicleWeaponsLevel1, MetaTypeEnum::TerranVehicleWeaponsLevel2, MetaTypeEnum::TerranVehicleWeaponsLevel3 },
		{ MetaTypeEnum::TerranShipWeaponsLevel1, MetaTypeEnum::TerranShipWeaponsLevel2, MetaTypeEnum::TerranShipWeaponsLevel3 },
		{ MetaTypeEnum::TerranVehicleAndShipArmorsLevel1, MetaTypeEnum::TerranVehicleAndShipArmorsLevel2, MetaTypeEnum::TerranVehicleAndShipArmorsLevel3 },
	};

	const std::list<std::list<MetaType>> protossUpgrades = {
		{ MetaTypeEnum::ProtossGroundWeaponsLevel1, MetaTypeEnum::ProtossGroundWeaponsLevel2, MetaTypeEnum::ProtossGroundWeaponsLevel3 },
		{ MetaTypeEnum::ProtossGroundArmorsLevel1, MetaTypeEnum::ProtossGroundArmorsLevel2, MetaTypeEnum::ProtossGroundArmorsLevel3 },
		{ MetaTypeEnum::ProtossAirWeaponsLevel1, MetaTypeEnum::ProtossAirWeaponsLevel2, MetaTypeEnum::ProtossAirWeaponsLevel3 },
		{ MetaTypeEnum::ProtossAirArmorsLevel1, MetaTypeEnum::ProtossAirArmorsLevel2, MetaTypeEnum::ProtossAirArmorsLevel3 },
		{ MetaTypeEnum::ProtossShieldsLevel1, MetaTypeEnum::ProtossShieldsLevel2, MetaTypeEnum::ProtossShieldsLevel3 },
	};

	const std::list<std::list<MetaType>> zergUpgrades = {
		{ MetaTypeEnum::ZergMeleeWeaponsLevel1, MetaTypeEnum::ZergMeleeWeaponsLevel2, MetaTypeEnum::ZergMeleeWeaponsLevel3 },
		{ MetaTypeEnum::ZergMissileWeaponsLevel1, MetaTypeEnum::ZergMissileWeaponsLevel2, MetaTypeEnum::ZergMissileWeaponsLevel3 },
		{ MetaTypeEnum::ZergGroundArmorsLevel1, MetaTypeEnum::ZergGroundArmorsLevel2, MetaTypeEnum::ZergGroundArmorsLevel3 },
		{ MetaTypeEnum::ZergFlyerWeaponsLevel1, MetaTypeEnum::ZergFlyerWeaponsLevel2, MetaTypeEnum::ZergFlyerWeaponsLevel3 },
		{ MetaTypeEnum::ZergFlyerArmorsLevel1, MetaTypeEnum::ZergFlyerArmorsLevel2, MetaTypeEnum::ZergFlyerArmorsLevel3 },
	};

	switch (m_bot.GetSelfRace())
	{
		case CCRace::Terran:
		{
			possibleUpgrades = terranUpgrades;
			break;
		}
		case CCRace::Protoss:
		{
			possibleUpgrades = protossUpgrades;
			break;
		}
		case CCRace::Zerg:
		{
			possibleUpgrades = zergUpgrades;
			break;
		}
	}
}

void ProductionManager::onFrame()
{
	m_bot.StartProfiling("1.0 lowPriorityChecks");
	lowPriorityChecks();
	validateUpgradesProgress();
	m_bot.StopProfiling("1.0 lowPriorityChecks");
	m_bot.StartProfiling("2.0 manageBuildOrderQueue");
    manageBuildOrderQueue();
	m_bot.StopProfiling("2.0 manageBuildOrderQueue");
	m_bot.StartProfiling("3.0 QueueDeadBuildings");
	QueueDeadBuildings();
	m_bot.StopProfiling("3.0 QueueDeadBuildings");

    // TODO: if nothing is currently building, get a new goal from the strategy manager
    // TODO: triggers for game things like cloaked units etc

    drawProductionInformation();
}

// on unit destroy
void ProductionManager::onUnitDestroy(const Unit & unit)
{
    // TODO: might have to re-do build order if a vital unit died
}

void ProductionManager::manageBuildOrderQueue()
{
    // if there is nothing in the queue, oh well
	if (!m_initialBuildOrderFinished && m_queue.isEmpty())
	{
		m_initialBuildOrderFinished = true;
	}

	if(!m_initialBuildOrderFinished && m_bot.Strategy().isWorkerRushed())
	{
		m_initialBuildOrderFinished = true;
		m_queue.clearAll();
	}

	m_bot.StartProfiling("2.1   putImportantBuildOrderItemsInQueue");
	if(m_initialBuildOrderFinished && m_bot.Config().AutoCompleteBuildOrder)
    {
		putImportantBuildOrderItemsInQueue();
    }
	m_bot.StopProfiling("2.1   putImportantBuildOrderItemsInQueue");

	if (m_queue.isEmpty())
		return;

	m_bot.StartProfiling("2.2   checkQueue");
    // the current item to be used
    BuildOrderItem currentItem = m_queue.getHighestPriorityItem();

    // while there is still something left in the queue
    while (!m_queue.isEmpty())
    {
		//check if we have the prerequirements.
		if (!hasRequired(currentItem.type, true) || !hasProducer(currentItem.type, true))
		{
			m_bot.StartProfiling("2.2.1     fixBuildOrderDeadlock");
			fixBuildOrderDeadlock(currentItem);
			currentItem = m_queue.getHighestPriorityItem();
			m_bot.StopProfiling("2.2.1     fixBuildOrderDeadlock");
			continue;
		}

		if (currentlyHasRequirement(currentItem.type))
		{
			// TODO: if it's a building and we can't make it yet, predict the worker movement to the location, remove pre-movement

			//Build supply depot at ramp against protoss
			if (m_bot.Observation()->GetFoodCap() <= 15 && currentItem.type == MetaTypeEnum::SupplyDepot && m_bot.GetPlayerRace(Players::Enemy) == CCRace::Protoss &&
				m_bot.GetGameLoop() > 5 && getFreeMinerals() > 30)
			{
				const CCPosition centerMap(m_bot.Map().width() / 2, m_bot.Map().height() / 2);
				if (!rampSupplyDepotWorker.isValid())
				{
					rampSupplyDepotWorker = m_bot.Workers().getClosestMineralWorkerTo(centerMap);
				}
				if (rampSupplyDepotWorker.isValid())
				{
					if (getFreeMinerals() + getExtraMinerals() >= 100)
					{
						rampSupplyDepotWorker.move(rampSupplyDepotWorker.getTilePosition());
						create(rampSupplyDepotWorker, currentItem, rampSupplyDepotWorker.getTilePosition());
						m_queue.removeCurrentHighestPriorityItem();
						break;
					}
					else
					{
						rampSupplyDepotWorker.move(centerMap);
					}
				}
			}

			//TODO: TEMP build barrack away from the ramp to protect it from worker rush
			if (!firstBarrackBuilt && currentItem.type == MetaTypeEnum::Barracks && m_bot.GetPlayerRace(Players::Enemy) == CCRace::Protoss &&
				meetsReservedResourcesWithExtra(MetaTypeEnum::Barracks))
			{
				firstBarrackBuilt = true;

				auto baseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
				auto supplyDepots = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT);
				
				if (supplyDepots.begin() != supplyDepots.end())//If we have a depot
				{
					auto supplyDepot = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT).begin()->second;

					auto basePosition = baseLocation->getDepotPosition();
					auto point = supplyDepot.getTilePosition();
					CCTilePosition target = CCTilePosition(basePosition.x + (basePosition.x - point.x), basePosition.y + (basePosition.y - point.y));
					if (target.x < 0)
					{
						target.x = 5;//5 instead of 0, since there is always a border we can't walk to on the edge of the map
					}
					else if (target.x > m_bot.Map().width())
					{
						target.x = m_bot.Map().width() - 5;//5 instead of 0, since there is always a border we can't walk to on the edge of the map
					}
					if (target.y < 0)
					{
						target.y = 5;//5 instead of 0, since there is always a border we can't walk to on the edge of the map
					}
					else if (target.y > m_bot.Map().height())
					{
						target.y = m_bot.Map().height() - 5;//5 instead of 0, since there is always a border we can't walk to on the edge of the map
					}

					Unit producer = getProducer(currentItem.type);
					create(producer, currentItem, target);
					m_queue.removeCurrentHighestPriorityItem();

					break;
				}				
			}

			// if we can make the current item
			if (meetsReservedResources(currentItem.type))
			{
				Unit producer = getProducer(currentItem.type);
				if (canMakeNow(producer, currentItem.type))
				{
					// create it and remove it from the _queue
					create(producer, currentItem);
					m_queue.removeCurrentHighestPriorityItem();

					// don't actually loop around in here
					break;
				}
			}

			// is a building (doesn't include addons, because no travel time) and we can make it soon (canMakeSoon)
			if (m_bot.Data(currentItem.type).isBuilding && !m_bot.Data(currentItem.type).isAddon && !currentItem.type.getUnitType().isMorphedBuilding() && meetsReservedResourcesWithExtra(currentItem.type))
			{
				Building b(currentItem.type.getUnitType(), m_bot.GetBuildingArea());
				//Get building location
				const CCTilePosition targetLocation = m_bot.Buildings().getNextBuildingLocation(b, false);
				if (targetLocation != CCTilePosition(0, 0))
				{
					Unit worker = m_bot.Workers().getClosestMineralWorkerTo(Util::GetPosition(targetLocation));
					if (worker.isValid())
					{
						worker.move(targetLocation);

						// create it and remove it from the _queue
						create(worker, currentItem);
						m_queue.removeCurrentHighestPriorityItem();

						// don't actually loop around in here
						break;
					}
				}
				else
				{
					Util::DisplayError("Invalid build location for " + currentItem.type.getName(), "0x0000002", m_bot);
				}
			}
		}
		              
    	// if we can't skip the current item, we stop here
		if (!m_queue.canSkipItem())
			break;

        // otherwise, skip it
        m_queue.skipItem();

        // and get the next one
        currentItem = m_queue.getNextHighestPriorityItem();
    }
	m_bot.StopProfiling("2.2   checkQueue");
}

void ProductionManager::putImportantBuildOrderItemsInQueue()
{
	if (m_bot.Config().AllowDebug && m_bot.GetCurrentFrame() % 10)
		return;

	const float productionScore = getProductionScore();
	const auto productionBuildingCount = getProductionBuildingsCount();
	const auto productionBuildingAddonCount = getProductionBuildingsAddonsCount();
	const auto baseCount = m_bot.Bases().getBaseCount(Players::Self, true);

	const int currentStrategy = m_bot.Strategy().getCurrentStrategyPostBuildOrder();

	// build supply if we need some
	const auto supplyWithAdditionalSupplyDepot = m_bot.GetMaxSupply() + m_bot.Buildings().countBeingBuilt(supplyProvider) * 8;
	if(m_bot.GetCurrentSupply() + 1.75 * getUnitTrainingBuildings(m_bot.GetSelfRace()).size() + baseCount > supplyWithAdditionalSupplyDepot && !m_queue.contains(supplyProviderType) && supplyWithAdditionalSupplyDepot < 200 && !m_bot.Strategy().isWorkerRushed())
	{
		m_queue.queueAsHighestPriority(supplyProviderType, false);
	}

	if (m_bot.GetSelfRace() == sc2::Race::Terran)
	{
		// Logic for building Orbital Commands and Refineries
		const size_t ccCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::CommandCenter.getUnitType(), false, true);
		const size_t completedCCCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::CommandCenter.getUnitType(), true, true);
		if(completedCCCount > 0)
		{
			if (!m_queue.contains(MetaTypeEnum::OrbitalCommand) && !m_queue.contains(MetaTypeEnum::PlanetaryFortress))
			{
				const size_t orbitalCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::OrbitalCommand.getUnitType(), false, true);
				if (orbitalCount < 3)
				{
					m_queue.queueAsHighestPriority(MetaTypeEnum::OrbitalCommand, false);
				}
				else
				{
					m_queue.queueAsHighestPriority(MetaTypeEnum::PlanetaryFortress, false);
				}
				m_queue.queueAsLowestPriority(MetaTypeEnum::Refinery, false);
				m_queue.queueAsLowestPriority(MetaTypeEnum::Refinery, false);
			}
		}
		else if (ccCount == 0 && !m_queue.contains(MetaTypeEnum::CommandCenter))
		{
			m_queue.queueAsLowestPriority(MetaTypeEnum::CommandCenter, false);
		}

		// Strategy base logic
		switch (currentStrategy)
		{
			case StrategyPostBuildOrder::TERRAN_REAPER :
			{
				/*if (!m_queue.contains(MetaTypeEnum::Starport))
				{
					m_queue.queueAsLowestPriority(MetaTypeEnum::Starport, false);
				}
				if (!m_queue.contains(MetaTypeEnum::StarportTechLab))
				{
					m_queue.queueAsLowestPriority(MetaTypeEnum::StarportTechLab, false);
				}*/
				//if (productionScore < (float)baseCount)
				{
					bool hasPicked = false;
					MetaType toBuild;
					const int starportCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Starport.getUnitType(), false, true);
					if (productionBuildingAddonCount < productionBuildingCount)
					{//Addon
						const int starportAddonCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportTechLab.getUnitType(), false, true) +
							m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportReactor.getUnitType(), false, true);
						if (starportCount > starportAddonCount)
						{
							toBuild = MetaTypeEnum::StarportTechLab;
							hasPicked = true;
						}

						if (hasPicked && !m_queue.contains(toBuild))
						{
							m_queue.queueItem(BuildOrderItem(toBuild, 1, false));
						}
					}
					if(!hasPicked)
					{//Building
						const int barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true);
						if (barracksCount < 1)
						{
							toBuild = MetaTypeEnum::Barracks;
							hasPicked = true;
						}
						else if (starportCount < baseCount * 2)
						{
							toBuild = MetaTypeEnum::Starport;
							hasPicked = true;
						}

						if (hasPicked && !m_queue.contains(toBuild) && !m_queue.contains(MetaTypeEnum::CommandCenter))
						{
							m_queue.queueAsLowestPriority(toBuild, false);
						}
					}
				}

				if (!m_queue.contains(MetaTypeEnum::BansheeCloak) && std::find(startedUpgrades.begin(), startedUpgrades.end(), MetaTypeEnum::BansheeCloak) == startedUpgrades.end())
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::BansheeCloak, 0, false));
					startedUpgrades.push_back(MetaTypeEnum::BansheeCloak);
					Util::Log(__FUNCTION__, "Queue BansheeCloak");
				}

				if (!m_queue.contains(MetaTypeEnum::HyperflightRotors) && std::find(startedUpgrades.begin(), startedUpgrades.end(), MetaTypeEnum::HyperflightRotors) == startedUpgrades.end())
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::HyperflightRotors, 0, false));
					startedUpgrades.push_back(MetaTypeEnum::HyperflightRotors);
					Util::Log(__FUNCTION__, "Queue HyperFlightRotors");
				}

				if (!m_queue.contains(MetaTypeEnum::Reaper))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Reaper, 0, false));
				}

				const int bansheeCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Banshee.getUnitType(), false, true);
				const int vikingCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Viking.getUnitType(), false, true);

				if (bansheeCount + vikingCount >= 5)
				{
					auto metaTypeShipArmor = queueUpgrade(MetaTypeEnum::TerranVehicleAndShipArmorsLevel1);
					Util::Log(__FUNCTION__, "queue " + metaTypeShipArmor.getName());
				}

				/*int reaperCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Reaper.getUnitType(), false, true);
				if (reaperCount > 3)
				{
					auto metaTypeInfantryWeapon = queueUpgrade(MetaTypeEnum::TerranInfantryWeaponsLevel1);
				}*/

				if (!m_queue.contains(MetaTypeEnum::Banshee))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Banshee, 0, false));
				}

				if (m_bot.Strategy().isEarlyRushed() && !m_queue.contains(MetaTypeEnum::Hellion))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Hellion, 0, false));
				}

				if (m_bot.Strategy().shouldProduceAntiAir())
				{
					if (!m_queue.contains(MetaTypeEnum::Viking) && vikingCount < 2 * bansheeCount)
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Viking, 0, false));
					}

					if (!m_queue.contains(MetaTypeEnum::HiSecAutoTracking) && std::find(startedUpgrades.begin(), startedUpgrades.end(), MetaTypeEnum::HiSecAutoTracking) == startedUpgrades.end())
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::HiSecAutoTracking, 0, false));
						startedUpgrades.push_back(MetaTypeEnum::HiSecAutoTracking);
						Util::Log(__FUNCTION__, "queue HiSecAutoTracking");
					}
				}
				break;
			}
			case StrategyPostBuildOrder::TERRAN_ANTI_SPEEDLING :
			{
				// the strategy does not use the barracks, so we remove the barracks score
				//const int barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true);
				//if (productionScore - 0.25f * barracksCount < (float)baseCount)
				{
					bool hasPicked = false;
					MetaType toBuild;
					const int factoryAddonCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::FactoryTechLab.getUnitType(), false, true) +
						m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::FactoryReactor.getUnitType(), false, true);
					const int starportCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Starport.getUnitType(), false, true);
					const int starportAddonCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportTechLab.getUnitType(), false, true) +
						m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportReactor.getUnitType(), false, true);
					if (factoryAddonCount < 1 || starportCount > starportAddonCount)
					{//Addon
						if (factoryAddonCount < 1)
						{
							// Commented because it will be added automatically as when we need to do the upgrade (and we might want to wait to build hellions instead)
							//toBuild = MetaTypeEnum::FactoryTechLab;
							//hasPicked = true;
						}
						else if (starportCount > starportAddonCount)
						{
							toBuild = MetaTypeEnum::StarportTechLab;
							hasPicked = true;
						}

						if (hasPicked && !m_queue.contains(toBuild))
						{
							m_queue.queueItem(BuildOrderItem(toBuild, 1, false));
						}
					}
					if (!hasPicked)
					{//Building
						const int factoryCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Factory.getUnitType(), false, true);
						if (factoryCount < baseCount)
						{
							toBuild = MetaTypeEnum::Factory;
							hasPicked = true;
						}
						else if (starportCount < baseCount)
						{
							toBuild = MetaTypeEnum::Starport;
							hasPicked = true;
						}

						if (hasPicked && !m_queue.contains(toBuild) && !m_queue.contains(MetaTypeEnum::CommandCenter))
						{
							m_queue.queueAsLowestPriority(toBuild, false);
						}
					}
				}

				const int hellionCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Hellion.getUnitType(), true, true);
				if (hellionCount >= 2 && !m_queue.contains(MetaTypeEnum::InfernalPreIgniter) && std::find(startedUpgrades.begin(), startedUpgrades.end(), MetaTypeEnum::InfernalPreIgniter) == startedUpgrades.end())
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::InfernalPreIgniter, 0, false));
					startedUpgrades.push_back(MetaTypeEnum::InfernalPreIgniter);
					Util::Log(__FUNCTION__, "queue InfernalPreIgniter");
				}

				const int bansheeCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Banshee.getUnitType(), true, true);
				const int vikingCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Viking.getUnitType(), true, true);
				if(hellionCount + bansheeCount + vikingCount >= 5)
					auto vehiculeUpgrade = queueUpgrade(MetaTypeEnum::TerranVehicleAndShipArmorsLevel1);
				
				if (!m_queue.contains(MetaTypeEnum::Hellion))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Hellion, 0, false));
				}

				/*if (m_bot.Strategy().shouldProduceAntiAir() && !m_queue.contains(MetaTypeEnum::Marine))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marine, 0, false));
				}*/

				if (!m_queue.contains(MetaTypeEnum::Banshee))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Banshee, 0, false));
				}

				if (m_bot.Strategy().shouldProduceAntiAir())
				{
					if (!m_queue.contains(MetaTypeEnum::Viking) && vikingCount < 2 * bansheeCount)
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Viking, 0, false));
					}

					if (!m_queue.contains(MetaTypeEnum::HiSecAutoTracking) && std::find(startedUpgrades.begin(), startedUpgrades.end(), MetaTypeEnum::HiSecAutoTracking) == startedUpgrades.end())
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::HiSecAutoTracking, 0, false));
						startedUpgrades.push_back(MetaTypeEnum::HiSecAutoTracking);
						Util::Log(__FUNCTION__, "queue HiSecAutoTracking");
					}
				}

				break;
			}
			/*case StrategyPostBuildOrder::TERRAN_MARINE_MARAUDER:
			{
				if (productionScore < (float)baseCount)
				{
					bool hasPicked = false;
					MetaType toBuild;
					if (productionBuildingAddonCount < productionBuildingCount)
					{//Addon
						int barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true);
						int barracksAddonCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksTechLab.getUnitType(), false, true) + 
							m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksReactor.getUnitType(), false, true);
						if (barracksCount > barracksAddonCount)
						{
							toBuild = MetaTypeEnum::BarracksTechLab;
							hasPicked = true;
						}

						if (hasPicked && !m_queue.contains(toBuild))
						{
							m_queue.queueItem(BuildOrderItem(toBuild, 1, false));
						}
					}
					if (!hasPicked)
					{//Building
						int barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true);
						if (barracksCount < baseCount * 2)
						{
							toBuild = MetaTypeEnum::Barracks;
							hasPicked = true;
						}

						if (hasPicked && !m_queue.contains(toBuild) && !m_queue.contains(MetaTypeEnum::CommandCenter))
						{
							m_queue.queueAsLowestPriority(toBuild, false);
						}
					}
				}

				if (std::find(startedUpgrades.begin(), startedUpgrades.end(), MetaTypeEnum::Stimpack) == startedUpgrades.end() && !m_queue.contains(MetaTypeEnum::Stimpack))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Stimpack, 0, false));
					startedUpgrades.push_back(MetaTypeEnum::Stimpack);
				}

				int marinesCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Marine.getUnitType(), false, true);
				int maraudersCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Marauder.getUnitType(), false, true);
				if (!m_queue.contains(MetaTypeEnum::Marine) && marinesCount < maraudersCount * 2 + 1)
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marine, 0, false));
				}

				if (!m_queue.contains(MetaTypeEnum::Marauder))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marauder, 0, false));
				}
				break;
			}*/
			case StrategyPostBuildOrder::WORKER_RUSH_DEFENSE:
			{
				if (!m_queue.contains(MetaTypeEnum::Reaper))
				{
					m_queue.queueAsHighestPriority(MetaTypeEnum::Reaper, false);
				}

				if (!m_queue.contains(MetaTypeEnum::Refinery) && m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Refinery.getUnitType(), false, false) == 0)
				{
					m_queue.queueAsHighestPriority(MetaTypeEnum::Refinery, false);
				}
				return;
			}
			/*case StrategyPostBuildOrder::TERRAN_ANTI_AIR:
			{
				bool hasPicked = false;
				MetaType toBuild;
				const int barrackTechlabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksTechLab.getUnitType(), false, true);
				const int barrackReactorCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksReactor.getUnitType(), false, true);
				const int starportCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Starport.getUnitType(), false, true);
				const int starportAddonCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportTechLab.getUnitType(), false, true) + 
					m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportReactor.getUnitType(), false, true);
				if (barrackTechlabCount + barrackReactorCount < 1 || starportCount > starportAddonCount)
				{//Addon
					if (barrackTechlabCount + barrackReactorCount < 1)
					{
						toBuild = MetaTypeEnum::BarracksReactor;
						hasPicked = true;
					}
					else if (starportCount > starportAddonCount)
					{
						toBuild = MetaTypeEnum::StarportTechLab;
						hasPicked = true;
					}

					if (hasPicked && !m_queue.contains(toBuild))
					{
						m_queue.queueItem(BuildOrderItem(toBuild, 1, false));
					}
				}
				if (!hasPicked)
				{//Building
					int barrackCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true);
					if (barrackCount < baseCount)
					{
						toBuild = MetaTypeEnum::Barracks;
						hasPicked = true;
					}
					else if (starportCount < baseCount)
					{
						toBuild = MetaTypeEnum::Starport;
						hasPicked = true;
					}

					if (hasPicked && !m_queue.contains(toBuild) && !m_queue.contains(MetaTypeEnum::CommandCenter))
					{
						m_queue.queueAsLowestPriority(toBuild, false);
					}
				}

				if (!m_queue.contains(MetaTypeEnum::Marine))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marine, 0, false));
				}

				int bansheeCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Banshee.getUnitType(), false, true);
				int vikingCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Viking.getUnitType(), false, true);
				if (vikingCount < 2 * bansheeCount)
				{
					if (!m_queue.contains(MetaTypeEnum::Viking))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Viking, 0, false));
					}
				}
				else
				{
					if (!m_queue.contains(MetaTypeEnum::Banshee))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Banshee, 0, false));
					}
				}

				auto infantryUpgrade = queueUpgrade(MetaTypeEnum::TerranInfantryWeaponsLevel1);
				auto vehiculeUpgrade = queueUpgrade(MetaTypeEnum::TerranVehicleAndShipArmorsLevel1);
				break;
			}*/
			case StrategyPostBuildOrder::NO_STRATEGY:
				break;
			default:
			{
				assert("This strategy doesn't exist.");
			}
		}
	}
	
	if (!m_queue.contains(workerMetatype) && !m_queue.contains(MetaTypeEnum::OrbitalCommand))//check queue
	{
		const int maxWorkersPerBase = 27;//21 mineral (to prepare for the next expansion), 6 gas
		const int maxWorkers = maxWorkersPerBase * 3;//maximum of 3 bases.
		const int workerCount = m_bot.Workers().getNumWorkers();
		if (baseCount * maxWorkersPerBase > workerCount && workerCount < maxWorkers)
		{
			if (currentStrategy != StrategyPostBuildOrder::WORKER_RUSH_DEFENSE)//check strategy
			{
				m_queue.queueItem(BuildOrderItem(workerMetatype, 2, false));
			}
		}
	}
}

void ProductionManager::QueueDeadBuildings()
{
	std::vector<Unit> deadBuildings;
	auto baseBuildings = m_bot.Buildings().getBaseBuildings();
	auto previousBuildings = m_bot.Buildings().getPreviousBaseBuildings();
	for (int i = 0; i < previousBuildings.size(); i++)
	{
		auto unit = previousBuildings.at(i);
		auto it = std::find(baseBuildings.begin(), baseBuildings.end(), unit);
		if (it == baseBuildings.end())
		{
			deadBuildings.push_back(unit);

			//Manual find of the worker to free him.
			auto & buildings = m_bot.Buildings().getPreviousBuildings();
			auto buildingsIt = buildings.begin();
			for (; buildingsIt != buildings.end(); ++buildingsIt)
			{
				if (buildingsIt->buildingUnit.isValid() && buildingsIt->buildingUnit.getTag() == unit.getTag())
				{
					m_bot.Workers().finishedWithWorker(buildingsIt->builderUnit);
					std::vector<Building> toRemove;
					toRemove.push_back(*buildingsIt);
					m_bot.Buildings().removeBuildings(toRemove);
					break;
				}
			}
		}
	}
	for (int i = 0; i < deadBuildings.size(); i++)
	{
		MetaType type = MetaType(deadBuildings.at(i).getType(), m_bot);
		if (!m_queue.contains(type))
		{
			m_queue.queueItem(BuildOrderItem(type, 0, false));
		}
	}

	m_bot.Buildings().updatePreviousBuildings();
	m_bot.Buildings().updatePreviousBaseBuildings();
}

void ProductionManager::fixBuildOrderDeadlock(BuildOrderItem & item)
{
	const TypeData& typeData = m_bot.Data(item.type);

	// check to see if we have the prerequisites for the item
    if (!hasRequired(item.type, true))
    {
        std::cout << item.type.getName() << " needs a requirement: " << typeData.requiredUnits[0].getName() << "\n";
		BuildOrderItem requiredItem = m_queue.queueItem(BuildOrderItem(MetaType(typeData.requiredUnits[0], m_bot), 0, item.blocking));
        fixBuildOrderDeadlock(requiredItem);
        return;
    }

    // build the producer of the unit if we don't have one
    if (!hasProducer(item.type, true))
    {
        if (item.type.getUnitType().isWorker() && m_bot.Observation()->GetFoodWorkers() == 0)
        {
            // We no longer have worker and no longer have buildings to do more, so we are rip...
            return;
        }
		std::cout << item.type.getName() << " needs a producer: " << typeData.whatBuilds[0].getName() << "\n";
		BuildOrderItem producerItem = m_queue.queueItem(BuildOrderItem(MetaType(typeData.whatBuilds[0], m_bot), 0, item.blocking));
        fixBuildOrderDeadlock(producerItem);
    }

    // build a refinery if we don't have one and the thing costs gas
    auto refinery = Util::GetRefinery(m_bot.GetSelfRace(), m_bot);
	if (typeData.gasCost > 0 && m_bot.UnitInfo().getUnitTypeCount(Players::Self, refinery, false, true) == 0)
    {
		m_queue.queueAsHighestPriority(MetaType(refinery, m_bot), true);
    }

    // build supply if we need some
    if (typeData.supplyCost > m_bot.GetMaxSupply() - m_bot.GetCurrentSupply() && !m_bot.Buildings().isBeingBuilt(supplyProvider))
    {
        m_queue.queueAsHighestPriority(supplyProviderType, true);
    }
}

void ProductionManager::lowPriorityChecks()
{
	if (m_bot.GetGameLoop() % 10)
	{
		return;
	}

	// build a refinery if we are missing one
	//TODO doesn't handle extra hatcheries, doesn't handle rich geyser
	auto refinery = Util::GetRefinery(m_bot.GetSelfRace(), m_bot);
	if (!m_queue.contains(MetaType(refinery, m_bot)))
	{
		if (m_initialBuildOrderFinished && !m_bot.Strategy().isWorkerRushed())
		{
			auto baseCount = m_bot.Bases().getBaseCount(Players::Self, true);
			auto geyserCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, refinery, false, true);
			if (geyserCount < baseCount * 2)
			{
				m_queue.queueAsHighestPriority(MetaType(refinery, m_bot), false);
			}
		}
	}

	//build turrets in mineral field
	//TODO only supports terran, turret position isn't always optimal(check BaseLocation to optimize it)
	if (m_bot.Strategy().shouldProduceAntiAir())
	{
		auto engeneeringBayCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::EngineeringBay.getUnitType(), false, true);
		if (engeneeringBayCount <= 0 && !m_queue.contains(MetaTypeEnum::EngineeringBay))
		{
			m_queue.queueAsHighestPriority(MetaTypeEnum::EngineeringBay, false);
		}

		if (!m_bot.Buildings().isConstructingType(MetaTypeEnum::MissileTurret.getUnitType()))
		{
			int engeneeringCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::EngineeringBay.getUnitType(), true, true);
			if (engeneeringCount > 0)
			{
				for (auto base : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
				{
					auto hasTurret = false;
					auto position = base->getTurretPosition();
					auto buildings = m_bot.Buildings().getFinishedBuildings();
					for (auto b : buildings)
					{
						if (b.getTilePosition() == position)
						{
							hasTurret = true;
							break;
						}
					}
					if (!hasTurret)
					{
						m_bot.Buildings().getBuildingPlacer().freeTilesForTurrets(position);
						auto worker = m_bot.Workers().getClosestMineralWorkerTo(CCPosition(position.x, position.y));
						create(worker, BuildOrderItem(MetaTypeEnum::MissileTurret, 1000, false), position);
					}
				}
			}
		}
	}
}

bool ProductionManager::currentlyHasRequirement(MetaType currentItem)
{
	auto requiredUnits = m_bot.Data(currentItem).requiredUnits;
	if (requiredUnits.empty())
	{
		return true;
	}

	for (auto & required : m_bot.Data(currentItem).requiredUnits)
	{
		sc2::UNIT_TYPEID type = currentItem.getUnitType().getAPIUnitType();
		if (currentItem.getUnitType().isResourceDepot())
		{
			if (m_bot.Bases().getBaseCount(Players::Self) > 0)
				continue;
			return false;
		}
		if (m_bot.UnitInfo().getUnitTypeCount(Players::Self, required, true, true) <= 0)
		{
			//Only for terran because all their bases are used for the same prerequirements. Not the case for zergs.
			//TODO zerg might need something similar
			sc2::UNIT_TYPEID type = required.getAPIUnitType();
			switch (type)
			{
				case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER:
				case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
				case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
				case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
				case sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
				{
					return m_bot.Bases().getBaseCount(Players::Self) > 0;
					break;
				}
				case sc2::UNIT_TYPEID::TERRAN_TECHLAB:
				{//TODO Techlabs may all be considereed the same, should verify and fix if it is the case.
					switch ((sc2::UNIT_TYPEID)currentItem.getUnitType().getAPIUnitType())
					{
						case sc2::UNIT_TYPEID::TERRAN_MARAUDER:
						case sc2::UNIT_TYPEID::TERRAN_GHOST:
						{
							return m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksTechLab.getUnitType(), true, true) >= 0;
						}
						case sc2::UNIT_TYPEID::TERRAN_SIEGETANK:
						case sc2::UNIT_TYPEID::TERRAN_THOR:
						{
							return m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::FactoryTechLab.getUnitType(), true, true) >= 0;
						}
						case sc2::UNIT_TYPEID::TERRAN_RAVEN:
						case sc2::UNIT_TYPEID::TERRAN_BANSHEE:
						case sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER:
						{
							return m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportTechLab.getUnitType(), true, true) >= 0;
						}
					}
				}
				default:
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool ProductionManager::hasRequired(const MetaType& metaType, bool checkInQueue)
{
	const TypeData& typeData = m_bot.Data(metaType);

	if (typeData.requiredUnits.empty())
		return true;

	for (auto & required : typeData.requiredUnits)
	{
		if (m_bot.UnitInfo().getUnitTypeCount(Players::Self, required, false, true) > 0 || m_bot.Buildings().isBeingBuilt(required))
			return true;

		if (checkInQueue && m_queue.contains(MetaType(required, m_bot)))
			return true;
	}

	return false;
}

bool ProductionManager::hasProducer(const MetaType& metaType, bool checkInQueue)
{
	const TypeData& typeData = m_bot.Data(metaType);

	if (typeData.whatBuilds.empty())
		return true;

	for (auto & producer : typeData.whatBuilds)
	{
		if (metaType.getUnitType().isWorker() && m_bot.Bases().getBaseCount(Players::Self) > 0)
			return true;
		if (metaType.isAddon())
		{
			int count = m_bot.UnitInfo().getUnitTypeCount(Players::Self, producer, false, false);
			auto buildings = m_bot.Buildings().getBaseBuildings();
			for (auto building : buildings)
			{
				if (building.getType() == producer)
				{
					if (building.getAddonTag() == 0)
					{
						return true;
					}
					--count;
				}
			}
			if (count > 0)	// This hack is because there is a moment where the building is purchased and removed from the queue but not yet in the buildings manager
				return true;
			
			//Addons do not check further or else we could try to build an addon on a building with an addon already.
			return checkInQueue && m_queue.contains(MetaType(producer, m_bot));
		}
		if (m_bot.UnitInfo().getUnitTypeCount(Players::Self, producer, false, true) > 0)
			return true;
		if (m_bot.Buildings().isBeingBuilt(producer))
			return true;
		if (checkInQueue && m_queue.contains(MetaType(producer, m_bot)))
			return true;
	}

	return false;
}

Unit ProductionManager::getProducer(const MetaType & type, CCPosition closestTo) const
{
	// get all the types of units that cna build this type
	auto & producerTypes = m_bot.Data(type).whatBuilds;
	bool priorizeReactor = false;
	bool isTypeAddon = m_bot.Data(type).isAddon;

	if (m_bot.GetSelfRace() == CCRace::Terran)
	{
		//check if we can prioritize a building with a reactor instead of a techlab
		auto typeName = type.getName();
		if (typeName == "Marine" ||
			typeName == "Reaper" ||
			typeName == "WidowMine" |
			typeName == "Hellion" ||
			typeName == "VikingFighter" ||
			typeName == "Medivac" ||
			typeName == "Liberator")
		{
			for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
			{
				// reasons a unit can not train the desired type
				if (!unit.isValid()) { continue; }
				if (!unit.isCompleted()) { continue; }
				if (unit.isFlying()) { continue; }
				if (std::find(producerTypes.begin(), producerTypes.end(), unit.getType()) == producerTypes.end()) { continue; }
				if (unit.isAddonTraining()) { continue; }

				//Building can produce unit, now check if addon is reactor and available
				auto addonTag = unit.getAddonTag();
				if (addonTag == 0)
				{
					continue;
				}

				auto addon = m_bot.GetUnit(addonTag);
				auto addonType = (sc2::UNIT_TYPEID)addon.getAPIUnitType();
				switch (addonType)
				{
					case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
					case sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR:
					case sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR:
					{
						priorizeReactor = true;
						break;
					}
				}
				break;
			}
		}
	}

    // make a set of all candidate producers
    std::vector<Unit> candidateProducers;
    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        // reasons a unit can not train the desired type
		if (!unit.isValid()) { continue; }
		if (!unit.isCompleted()) { continue; }
		if (unit.isFlying()) { continue; }
        if (std::find(producerTypes.begin(), producerTypes.end(), unit.getType()) == producerTypes.end()) { continue; }

		bool isBuilding = m_bot.Data(unit).isBuilding;
		if (isBuilding && unit.isTraining() && unit.getAddonTag() == 0) { continue; }//TODO might break other races
		if (isBuilding && m_bot.GetSelfRace() == CCRace::Terran)
		{//If is terran, check for Reactor addon
			sc2::Tag addonTag = unit.getAddonTag();
			sc2::UNIT_TYPEID unitType = unit.getAPIUnitType();
			
			if (addonTag != 0)
			{
				bool addonIsReactor = false;
				auto addon = m_bot.GetUnit(addonTag);
				switch ((sc2::UNIT_TYPEID)addon.getAPIUnitType())
				{
					case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
					case sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR:
					case sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR:
					{
						addonIsReactor = true;
						break;
					}
				}

				if (unit.isTraining() && !addonIsReactor)
				{//skip, Techlab can't build two units
					continue;
				}
				
				if (addonIsReactor && unit.isAddonTraining())
				{//skip, reactor at max capacity
					continue;
				}

				//Skip techlab if we have an available reactor
				if (priorizeReactor && !addonIsReactor)
				{
					continue;
				}
			}
		}

		switch ((sc2::UNIT_TYPEID)unit.getAPIUnitType())
		{
			case sc2::UNIT_TYPEID::TERRAN_SCV:
			case sc2::UNIT_TYPEID::PROTOSS_PROBE:
			case sc2::UNIT_TYPEID::ZERG_DRONE:
			{
				if (!m_bot.Workers().isFree(unit))
				{
					continue;
				}
				break;
			}
			case sc2::UNIT_TYPEID::TERRAN_MULE:
			{
				continue;
			}
		}

        // TODO: if unit is not powered continue
        // TODO: if the type requires an addon and the producer doesn't have one

		// if the type we want to produce has required units, we make sure the unit is one of those unit types
		if (m_bot.Data(type).requiredUnits.size() > 0)
		{
			bool hasRequiredUnit = false;
			for (UnitType requiredUnit : m_bot.Data(type).requiredUnits)
			{
				if (!requiredUnit.isAddon())
				{
					// maybe we don't hve what is needed, but it seems to already work for non addon units
					hasRequiredUnit = true;
					break;
				}
				else	// addon
				{
					if (unit.getUnitPtr()->add_on_tag != 0)
					{
						Unit addon = m_bot.GetUnit(unit.getUnitPtr()->add_on_tag);
						if (requiredUnit.getAPIUnitType() == addon.getAPIUnitType())
						{
							hasRequiredUnit = true;
							break;
						}
					}
				}
			}
			if (!hasRequiredUnit) { continue; }
		}

		//if the type is an addon, some special cases
		if (isTypeAddon)
		{
			//Skip if the building already has an addon
			if (unit.getUnitPtr()->add_on_tag != 0)
			{
				continue;
			}
		}

        // if we haven't cut it, add it to the set of candidates
        candidateProducers.push_back(unit);
    }

    return getClosestUnitToPosition(candidateProducers, closestTo);
}

int ProductionManager::getProductionBuildingsCount() const
{
	switch (m_bot.GetSelfRace())
	{
		case CCRace::Terran:
		{

			return m_bot.Buildings().getBuildingCountOfType({
				sc2::UNIT_TYPEID::TERRAN_BARRACKS, 
				sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING, 
				sc2::UNIT_TYPEID::TERRAN_FACTORY, 
				sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING, 
				sc2::UNIT_TYPEID::TERRAN_STARPORT, 
				sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING });
		}
		case CCRace::Protoss:
		{
			//TODO
			return 0;
		}
		case CCRace::Zerg:
		{
			//TODO
			return 0;
		}
	}
	return 0;
}

int ProductionManager::getProductionBuildingsAddonsCount() const
{
	switch (m_bot.GetSelfRace())
	{
		case CCRace::Terran:
		{

			return m_bot.Buildings().getBuildingCountOfType({
				sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR,
				sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB,
				sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR,
				sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB,
				sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR,
				sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB });
		}
		case CCRace::Protoss:
		{
			return 0;
		}
		case CCRace::Zerg:
		{
			return 0;
		}
	}
	return 0;
}

float ProductionManager::getProductionScore() const
{
	float score = 0;
	switch (m_bot.GetSelfRace())
	{
		case CCRace::Terran:
		{
			score += m_bot.Buildings().getBuildingCountOfType({
				sc2::UNIT_TYPEID::TERRAN_BARRACKS }) * 0.25f;
			score += m_bot.Buildings().getBuildingCountOfType({
				sc2::UNIT_TYPEID::TERRAN_FACTORY }) * 0.25f;
			score += m_bot.Buildings().getBuildingCountOfType({
				sc2::UNIT_TYPEID::TERRAN_STARPORT }) * 0.5f;

			score += m_bot.Buildings().getBuildingCountOfType({
				sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR,
				sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB }) * 0.25f;
			score += m_bot.Buildings().getBuildingCountOfType({
				sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR,
				sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB }) * 0.25f;
			score += m_bot.Buildings().getBuildingCountOfType({
				sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR,
				sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB }) * 0.5f;
			break;
		}
		case CCRace::Protoss:
		{
			return 0;
		}
		case CCRace::Zerg:
		{
			return 0;
		}
	}
	return score;
}

float ProductionManager::getProductionScoreInQueue()
{
	float score = 0;
	
	switch (m_bot.GetSelfRace())
	{
		case CCRace::Terran:
		{
			score += m_queue.contains(MetaTypeEnum::Barracks) ? 0.25f : 0;
			score += m_queue.contains(MetaTypeEnum::Factory) ? 0.25f : 0;
			score += m_queue.contains(MetaTypeEnum::Starport) ? 0.25f : 0;

			score += m_queue.contains(MetaTypeEnum::BarracksReactor) ? 0.25f : 0;
			score += m_queue.contains(MetaTypeEnum::BarracksTechLab) ? 0.25f : 0;
			score += m_queue.contains(MetaTypeEnum::FactoryReactor) ? 0.25f : 0;
			score += m_queue.contains(MetaTypeEnum::FactoryTechLab) ? 0.25f : 0;
			score += m_queue.contains(MetaTypeEnum::StarportReactor) ? 0.25f : 0;
			score += m_queue.contains(MetaTypeEnum::StarportTechLab) ? 0.25f : 0;
			break;
		}
		case CCRace::Protoss:
		{
			return 0;
		}
		case CCRace::Zerg:
		{
			return 0;
		}
	}
	return score;
}

std::vector<Unit> ProductionManager::getUnitTrainingBuildings(CCRace race)
{
	std::set<sc2::UnitTypeID> unitTrainingBuildingTypes;
	switch (race)
	{
		case CCRace::Terran:
			unitTrainingBuildingTypes.insert(sc2::UNIT_TYPEID::TERRAN_BARRACKS);
			unitTrainingBuildingTypes.insert(sc2::UNIT_TYPEID::TERRAN_STARPORT);
			unitTrainingBuildingTypes.insert(sc2::UNIT_TYPEID::TERRAN_FACTORY);
			break;
		case CCRace::Protoss:
			//TODO complete
			break;
		case CCRace::Zerg:
			//TODO complete
			break;
	}

	std::vector<Unit> trainers;
	for (auto unit : m_bot.UnitInfo().getUnits(Players::Self))
	{
		// reasons a unit can not train the desired type
		if (unitTrainingBuildingTypes.find(unit.getType().getAPIUnitType()) == unitTrainingBuildingTypes.end()) { continue; }
		if (!unit.isCompleted()) { continue; }
		if (unit.isFlying()) { continue; }

		// TODO: if unit is not powered continue

		// if we haven't cut it, add it to the set of trainers
		trainers.push_back(unit);
	}

	return trainers;
}

MetaType ProductionManager::queueUpgrade(const MetaType & type)
{
	auto previousUpgradeName = std::string();
	auto upgradeName = type.getName();
	bool started = false;
	bool categoryFound = false;

	for (auto & upCategory : possibleUpgrades)
	{
		for (auto & potentialUpgrade : upCategory)
		{
			if (started)
			{
				previousUpgradeName = upgradeName;
				upgradeName = potentialUpgrade.getName();
				started = false;
			}
			if (potentialUpgrade.getName() == upgradeName)
			{
				categoryFound = true;

				for (const auto & startedUpgrade : startedUpgrades)//If startedUpgrades.contains
				{
					if (startedUpgrade.getName() == upgradeName)
					{
						started = true;
						previousUpgradeName = potentialUpgrade.getName();
						break;
					}
				}
				if (!started)//if not started, return it.
				{
					//Can't merge both if since [empty] isn't a MetaType.
					if (previousUpgradeName.empty() || !m_queue.contains(MetaType(previousUpgradeName, m_bot)))
					{
						if (potentialUpgrade.getName() != "MetaType")//If we found the right category, and we haven't done all the upgrades.
						{
							m_queue.queueAsLowestPriority(potentialUpgrade, false);
							startedUpgrades.push_back(potentialUpgrade);
							return potentialUpgrade;
						}
					}
					//Did not finish previous upgrade.
				}
				//if started, return the next one.
			}
		}
		if (categoryFound)
		{
			break;
		}
	}
	return {};
}

void ProductionManager::validateUpgradesProgress()
{
	std::vector<MetaType> toRemove;
	for (std::pair<const MetaType, Unit> & upgrade : incompletUpgrades)
	{
		bool found = false;
		for (auto & order : upgrade.second.getUnitPtr()->orders)
		{
			float progress = order.progress;
			if (order.ability_id == m_bot.Data(upgrade.first.getUpgrade()).buildAbility)
			{
				found = true;
				if (progress > 0.95f)//About to finish, lets consider it done.
				{
					toRemove.push_back(upgrade.first);
					Util::Log(__FUNCTION__, "upgrade finished " + upgrade.first.getName());
				}
			}
		}
		if (!found)
		{
			toRemove.push_back(upgrade.first);
			startedUpgrades.remove(upgrade.first);
			Util::Log(__FUNCTION__, "upgrade failed to start " + upgrade.first.getName());
		}
	}
	for (auto & remove : toRemove)
	{
		incompletUpgrades.erase(remove);
	}
}

Unit ProductionManager::getClosestUnitToPosition(const std::vector<Unit> & units, CCPosition closestTo) const
{
    if (units.empty())
    {
        return {};
    }

    // if we don't care where the unit is return the first one we have
    if (closestTo.x == 0 && closestTo.y == 0)
    {
        return units[0];
    }

    Unit closestUnit;
    double minDist = std::numeric_limits<double>::max();

    for (auto & unit : units)
    {
        double distance = Util::DistSq(unit, closestTo);
        if (!closestUnit.isValid() || distance < minDist)
        {
            closestUnit = unit;
            minDist = distance;
        }
    }

    return closestUnit;
}

// this function will check to see if all preconditions are met and then create a unit
void ProductionManager::create(const Unit & producer, BuildOrderItem & item, CCTilePosition position)
{
    if (!producer.isValid())
    {
        return;
    }

    // if we're dealing with a building
    if (item.type.isBuilding())
    {
        if (item.type.getUnitType().isMorphedBuilding())
        {
            producer.morph(item.type.getUnitType());
        }
        else
        {
			if (position == CCTilePosition())
			{
				position = Util::GetTilePosition(m_bot.GetStartLocation());
			}

			m_bot.Buildings().addBuildingTask(item.type.getUnitType(), position);
        }
    }
    // if we're dealing with a non-building unit
    else if (item.type.isUnit())
    {
        producer.train(item.type.getUnitType());
    }
    else if (item.type.isUpgrade())
    {
		Micro::SmartAbility(producer.getUnitPtr(), m_bot.Data(item.type.getUpgrade()).buildAbility, m_bot);

		auto it = incompletUpgrades.find(item.type);
		if (it != incompletUpgrades.end())
		{
			Util::DisplayError("Trying to start an already started upgrade.", "0x00000006", m_bot);
		}
		incompletUpgrades.insert(std::make_pair(item.type, producer));
		Util::Log(__FUNCTION__, "upgrade starting " + item.type.getName());
    }
}

bool ProductionManager::canMakeNow(const Unit & producer, const MetaType & type)
{
    if (!producer.isValid())
    {
        return false;
    }
	if (m_bot.Data(type).isAddon)
	{//Do not check for abilities, buildings don't seem to have it listed. They always have the ability anyway, so its safe.
		return true;
	}

#ifdef SC2API
	sc2::AvailableAbilities available_abilities = m_bot.Query()->GetAbilitiesForUnit(producer.getUnitPtr());

	// quick check if the unit can't do anything it certainly can't build the thing we want
	if (available_abilities.abilities.empty())
	{
		return false;
	}
	else
	{
		// check to see if one of the unit's available abilities matches the build ability type
		sc2::AbilityID MetaTypeAbility = m_bot.Data(type).buildAbility;
		for (const sc2::AvailableAbility & available_ability : available_abilities.abilities)
		{
			if (available_ability.ability_id == MetaTypeAbility)
			{
				return true;
			}
		}
		if (type.isUpgrade())//TODO Not safe, is a fix for upgrades having the wrong ID
		{
			std::ostringstream oss;
			oss << "Unit can't create upgrade, but should be able. ID: " << MetaTypeAbility << " UnitType: " << producer.getType().getName();
			Util::DisplayError(oss.str(), "0x00000005", m_bot);
			return true;
		}
	}

	return false;
#else
	bool canMake = meetsReservedResources(type);
	if (canMake)
	{
		if (type.isUnit())
		{
			canMake = BWAPI::Broodwar->canMake(type.getUnitType().getAPIUnitType(), producer.getUnitPtr());
		}
		else if (type.isTech())
		{
			canMake = BWAPI::Broodwar->canResearch(type.getTechType(), producer.getUnitPtr());
		}
		else if (type.isUpgrade())
		{
			canMake = BWAPI::Broodwar->canUpgrade(type.getUpgrade(), producer.getUnitPtr());
		}
		else
		{
			BOT_ASSERT(false, "Unknown type");
		}
	}

	return canMake;
#endif
}

bool ProductionManager::detectBuildOrderDeadlock()
{
    // TODO: detect build order deadlocks here
    return false;
}

int ProductionManager::getFreeMinerals()
{
    return m_bot.GetMinerals() - m_bot.Buildings().getReservedMinerals();
}

int ProductionManager::getFreeGas()
{
    return m_bot.GetGas() - m_bot.Buildings().getReservedGas();
}

int ProductionManager::getExtraMinerals()
{
	int extraMinerals = 0;
	std::set<Unit> workers = m_bot.Workers().getWorkers();
	for (auto w : workers) {
		if (m_bot.Workers().getWorkerData().getWorkerJob(w) == WorkerJobs::Minerals && m_bot.Workers().isReturningCargo(w))
		{ 
			if (w.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_MULE)
			{
				extraMinerals += 25;
			}
			else
			{
				extraMinerals += 5;
			}
		}
	}
	return extraMinerals;
}

int ProductionManager::getExtraGas()
{
	int extraGas = 0;
	std::set<Unit> workers = m_bot.Workers().getWorkers();
	for (auto w : workers) {
		if (m_bot.Workers().getWorkerData().getWorkerJob(w) == WorkerJobs::Gas && m_bot.Workers().isReturningCargo(w))
		{
			extraGas += 4;
		}
	}
	return extraGas;
}

// return whether or not we meet resources, including building reserves
bool ProductionManager::meetsReservedResources(const MetaType & type)
{
    return (m_bot.Data(type).mineralCost <= getFreeMinerals()) && (m_bot.Data(type).gasCost <= getFreeGas());
}

// return whether or not we meet resources, including building reserves
bool ProductionManager::meetsReservedResourcesWithExtra(const MetaType & type)
{
	assert("Addons cannot use extra ressources", m_bot.Data(type).isAddon);
	return (m_bot.Data(type).mineralCost <= getFreeMinerals() + getExtraMinerals()) && (m_bot.Data(type).gasCost <= getFreeGas() + getExtraGas());
}

void ProductionManager::drawProductionInformation()
{
    if (!m_bot.Config().DrawProductionInfo)
    {
        return;
    }

    std::stringstream ss;
    ss << "Production Information\n\n";

    ss << m_queue.getQueueInformation() << "\n\n";
	m_bot.Map().drawTextScreen(0.01f, 0.01f, ss.str(), CCColor(255, 255, 0));

	ss.str(std::string());
	ss << "Free Mineral:     " << getFreeMinerals() << "\n";
	ss << "Free Gas:         " << getFreeGas() << "\n\n";
	m_bot.Map().drawTextScreen(0.75f, 0.05f, ss.str(), CCColor(255, 255, 0));

	ss.str(std::string());
	ss << "Being built:      \n";
	for (auto & underConstruction : m_bot.Buildings().getBuildings())
	{
		ss << underConstruction.type.getName() << "\n";
	}
	for (auto & incompletUpgrade : incompletUpgrades)
	{
		ss << incompletUpgrade.first.getName() << "\n";
	}
	m_bot.Map().drawTextScreen(0.01f, 0.4f, ss.str(), CCColor(255, 255, 0));	
}
