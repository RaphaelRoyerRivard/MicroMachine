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

	bool blocking = !m_bot.Strategy().isProxyStartingStrategy();
    for (size_t i(0); i<buildOrder.size(); ++i)
    {
		if (!blocking && buildOrder[i].getUnitType() == MetaTypeEnum::Barracks.getUnitType())
			blocking = true;
        m_queue.queueAsLowestPriority(buildOrder[i], blocking);
    }
}


void ProductionManager::onStart()
{
	/*const auto expansion = m_bot.Bases().getNextExpansion(Players::Self, false, false);
	const auto centerOfMinerals = Util::GetPosition(expansion->getCenterOfMinerals());
	const auto expansionPosition = Util::GetPosition(expansion->getDepotPosition());
	const auto towardsMinerals = Util::Normalized(centerOfMinerals - expansionPosition);
	m_bot.Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, centerOfMinerals, 2, 1);
	m_bot.Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PYLON, centerOfMinerals + towardsMinerals*2, 2, 1);
	const auto startLocation = m_bot.GetStartLocation();
	const auto mapCenter = m_bot.Map().center();
	const auto towardsMapCenter = Util::Normalized(mapCenter - startLocation);
	m_bot.Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, startLocation + towardsMapCenter * 10, 2, 1);
	m_bot.Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PYLON, startLocation + towardsMapCenter * 12, 2, 1);*/
	
    setBuildOrder(m_bot.Strategy().getOpeningBookBuildOrder());
	if (m_queue.isEmpty())
		Util::DisplayError("Initial build order is empty.", "0x00000003", m_bot, true);
	supplyProvider = Util::GetSupplyProvider();
	supplyProviderType = MetaType(supplyProvider, m_bot);

	workerMetatype = MetaType(Util::GetWorkerType(), m_bot);
	
	switch (m_bot.GetSelfRace())
	{
		case CCRace::Terran:
		{
			possibleUpgrades = {
				{ MetaTypeEnum::TerranInfantryWeaponsLevel1, MetaTypeEnum::TerranInfantryWeaponsLevel2, MetaTypeEnum::TerranInfantryWeaponsLevel3 },
				{ MetaTypeEnum::TerranInfantryArmorsLevel1, MetaTypeEnum::TerranInfantryArmorsLevel2, MetaTypeEnum::TerranInfantryArmorsLevel3 },
				{ MetaTypeEnum::TerranVehicleWeaponsLevel1, MetaTypeEnum::TerranVehicleWeaponsLevel2, MetaTypeEnum::TerranVehicleWeaponsLevel3 },
				{ MetaTypeEnum::TerranShipWeaponsLevel1, MetaTypeEnum::TerranShipWeaponsLevel2, MetaTypeEnum::TerranShipWeaponsLevel3 },
				{ MetaTypeEnum::TerranVehicleAndShipArmorsLevel1, MetaTypeEnum::TerranVehicleAndShipArmorsLevel2, MetaTypeEnum::TerranVehicleAndShipArmorsLevel3 },
			};

			reversePossibleUpgrades = {
				{ MetaTypeEnum::TerranInfantryWeaponsLevel3, MetaTypeEnum::TerranInfantryWeaponsLevel2, MetaTypeEnum::TerranInfantryWeaponsLevel1 },
				{ MetaTypeEnum::TerranInfantryArmorsLevel3, MetaTypeEnum::TerranInfantryArmorsLevel2, MetaTypeEnum::TerranInfantryArmorsLevel1 },
				{ MetaTypeEnum::TerranVehicleWeaponsLevel3, MetaTypeEnum::TerranVehicleWeaponsLevel2, MetaTypeEnum::TerranVehicleWeaponsLevel1 },
				{ MetaTypeEnum::TerranShipWeaponsLevel3, MetaTypeEnum::TerranShipWeaponsLevel2, MetaTypeEnum::TerranShipWeaponsLevel1 },
				{ MetaTypeEnum::TerranVehicleAndShipArmorsLevel3, MetaTypeEnum::TerranVehicleAndShipArmorsLevel2, MetaTypeEnum::TerranVehicleAndShipArmorsLevel1 },
			};

			alternateUpgrades[MetaTypeEnum::TerranInfantryWeaponsLevel1.getName()] = MetaTypeEnum::TerranInfantryArmorsLevel1;
			alternateUpgrades[MetaTypeEnum::TerranInfantryArmorsLevel1.getName()] = MetaTypeEnum::TerranInfantryWeaponsLevel1;
			alternateUpgrades[MetaTypeEnum::TerranInfantryWeaponsLevel2.getName()] = MetaTypeEnum::TerranInfantryArmorsLevel2;
			alternateUpgrades[MetaTypeEnum::TerranInfantryArmorsLevel2.getName()] = MetaTypeEnum::TerranInfantryWeaponsLevel2;
			alternateUpgrades[MetaTypeEnum::TerranInfantryWeaponsLevel3.getName()] = MetaTypeEnum::TerranInfantryArmorsLevel3;
			alternateUpgrades[MetaTypeEnum::TerranInfantryArmorsLevel3.getName()] = MetaTypeEnum::TerranInfantryWeaponsLevel3;

			alternateUpgrades[MetaTypeEnum::TerranShipWeaponsLevel1.getName()] = MetaTypeEnum::TerranVehicleAndShipArmorsLevel1;
			alternateUpgrades[MetaTypeEnum::TerranVehicleWeaponsLevel1.getName()] = MetaTypeEnum::TerranVehicleAndShipArmorsLevel1;
			alternateUpgrades[MetaTypeEnum::TerranShipWeaponsLevel2.getName()] = MetaTypeEnum::TerranVehicleAndShipArmorsLevel2;
			alternateUpgrades[MetaTypeEnum::TerranVehicleWeaponsLevel2.getName()] = MetaTypeEnum::TerranVehicleAndShipArmorsLevel2;
			alternateUpgrades[MetaTypeEnum::TerranShipWeaponsLevel3.getName()] = MetaTypeEnum::TerranVehicleAndShipArmorsLevel3;
			alternateUpgrades[MetaTypeEnum::TerranVehicleWeaponsLevel3.getName()] = MetaTypeEnum::TerranVehicleAndShipArmorsLevel3;

			alternateUpgrades[MetaTypeEnum::TerranVehicleAndShipArmorsLevel1.getName()] = MetaTypeEnum::TerranVehicleAndShipArmorsLevel2;//Doesn't have an alternate
			alternateUpgrades[MetaTypeEnum::TerranVehicleAndShipArmorsLevel2.getName()] = MetaTypeEnum::TerranVehicleAndShipArmorsLevel3;
			alternateUpgrades[MetaTypeEnum::TerranVehicleAndShipArmorsLevel3.getName()] = MetaTypeEnum::TerranVehicleAndShipArmorsLevel3;
			break;
		}
		case CCRace::Protoss:
		{
			possibleUpgrades = {
				{ MetaTypeEnum::ProtossGroundWeaponsLevel1, MetaTypeEnum::ProtossGroundWeaponsLevel2, MetaTypeEnum::ProtossGroundWeaponsLevel3 },
				{ MetaTypeEnum::ProtossGroundArmorsLevel1, MetaTypeEnum::ProtossGroundArmorsLevel2, MetaTypeEnum::ProtossGroundArmorsLevel3 },
				{ MetaTypeEnum::ProtossAirWeaponsLevel1, MetaTypeEnum::ProtossAirWeaponsLevel2, MetaTypeEnum::ProtossAirWeaponsLevel3 },
				{ MetaTypeEnum::ProtossAirArmorsLevel1, MetaTypeEnum::ProtossAirArmorsLevel2, MetaTypeEnum::ProtossAirArmorsLevel3 },
				{ MetaTypeEnum::ProtossShieldsLevel1, MetaTypeEnum::ProtossShieldsLevel2, MetaTypeEnum::ProtossShieldsLevel3 },
			};

			reversePossibleUpgrades = {
				{ MetaTypeEnum::ProtossGroundWeaponsLevel3, MetaTypeEnum::ProtossGroundWeaponsLevel2, MetaTypeEnum::ProtossGroundWeaponsLevel1 },
				{ MetaTypeEnum::ProtossGroundArmorsLevel3, MetaTypeEnum::ProtossGroundArmorsLevel2, MetaTypeEnum::ProtossGroundArmorsLevel1 },
				{ MetaTypeEnum::ProtossAirWeaponsLevel3, MetaTypeEnum::ProtossAirWeaponsLevel2, MetaTypeEnum::ProtossAirWeaponsLevel1 },
				{ MetaTypeEnum::ProtossAirArmorsLevel3, MetaTypeEnum::ProtossAirArmorsLevel2, MetaTypeEnum::ProtossAirArmorsLevel1 },
				{ MetaTypeEnum::ProtossShieldsLevel3, MetaTypeEnum::ProtossShieldsLevel2, MetaTypeEnum::ProtossShieldsLevel1 },
			};

			alternateUpgrades[MetaTypeEnum::ProtossGroundWeaponsLevel1.getName()] = MetaTypeEnum::ProtossGroundArmorsLevel1;
			alternateUpgrades[MetaTypeEnum::ProtossGroundArmorsLevel1.getName()] = MetaTypeEnum::ProtossGroundWeaponsLevel1;
			alternateUpgrades[MetaTypeEnum::ProtossGroundWeaponsLevel2.getName()] = MetaTypeEnum::ProtossGroundArmorsLevel2;
			alternateUpgrades[MetaTypeEnum::ProtossGroundArmorsLevel2.getName()] = MetaTypeEnum::ProtossGroundWeaponsLevel2;
			alternateUpgrades[MetaTypeEnum::ProtossGroundWeaponsLevel3.getName()] = MetaTypeEnum::ProtossGroundArmorsLevel3;
			alternateUpgrades[MetaTypeEnum::ProtossGroundArmorsLevel3.getName()] = MetaTypeEnum::ProtossGroundWeaponsLevel3;

			alternateUpgrades[MetaTypeEnum::ProtossAirWeaponsLevel1.getName()] = MetaTypeEnum::ProtossAirArmorsLevel1;
			alternateUpgrades[MetaTypeEnum::ProtossAirArmorsLevel1.getName()] = MetaTypeEnum::ProtossAirWeaponsLevel1;
			alternateUpgrades[MetaTypeEnum::ProtossAirWeaponsLevel2.getName()] = MetaTypeEnum::ProtossAirArmorsLevel2;
			alternateUpgrades[MetaTypeEnum::ProtossAirArmorsLevel2.getName()] = MetaTypeEnum::ProtossAirWeaponsLevel2;
			alternateUpgrades[MetaTypeEnum::ProtossAirWeaponsLevel3.getName()] = MetaTypeEnum::ProtossAirArmorsLevel3;
			alternateUpgrades[MetaTypeEnum::ProtossAirArmorsLevel3.getName()] = MetaTypeEnum::ProtossAirWeaponsLevel3;

			alternateUpgrades[MetaTypeEnum::ProtossShieldsLevel1.getName()] = MetaTypeEnum::ProtossShieldsLevel2;//Doesn't have an alternate
			alternateUpgrades[MetaTypeEnum::ProtossShieldsLevel2.getName()] = MetaTypeEnum::ProtossShieldsLevel3;
			alternateUpgrades[MetaTypeEnum::ProtossShieldsLevel3.getName()] = MetaTypeEnum::ProtossShieldsLevel3;
			break;
		}
		case CCRace::Zerg:
		{
			possibleUpgrades = {
				{ MetaTypeEnum::ZergMeleeWeaponsLevel1, MetaTypeEnum::ZergMeleeWeaponsLevel2, MetaTypeEnum::ZergMeleeWeaponsLevel3 },
				{ MetaTypeEnum::ZergMissileWeaponsLevel1, MetaTypeEnum::ZergMissileWeaponsLevel2, MetaTypeEnum::ZergMissileWeaponsLevel3 },
				{ MetaTypeEnum::ZergGroundArmorsLevel1, MetaTypeEnum::ZergGroundArmorsLevel2, MetaTypeEnum::ZergGroundArmorsLevel3 },
				{ MetaTypeEnum::ZergFlyerWeaponsLevel1, MetaTypeEnum::ZergFlyerWeaponsLevel2, MetaTypeEnum::ZergFlyerWeaponsLevel3 },
				{ MetaTypeEnum::ZergFlyerArmorsLevel1, MetaTypeEnum::ZergFlyerArmorsLevel2, MetaTypeEnum::ZergFlyerArmorsLevel3 },
			};

			reversePossibleUpgrades = {
				{ MetaTypeEnum::ZergMeleeWeaponsLevel3, MetaTypeEnum::ZergMeleeWeaponsLevel2, MetaTypeEnum::ZergMeleeWeaponsLevel1 },
				{ MetaTypeEnum::ZergMissileWeaponsLevel3, MetaTypeEnum::ZergMissileWeaponsLevel2, MetaTypeEnum::ZergMissileWeaponsLevel1 },
				{ MetaTypeEnum::ZergGroundArmorsLevel3, MetaTypeEnum::ZergGroundArmorsLevel2, MetaTypeEnum::ZergGroundArmorsLevel1 },
				{ MetaTypeEnum::ZergFlyerWeaponsLevel3, MetaTypeEnum::ZergFlyerWeaponsLevel2, MetaTypeEnum::ZergFlyerWeaponsLevel1 },
				{ MetaTypeEnum::ZergFlyerArmorsLevel3, MetaTypeEnum::ZergFlyerArmorsLevel2, MetaTypeEnum::ZergFlyerArmorsLevel1 },
			};

			alternateUpgrades[MetaTypeEnum::ZergMeleeWeaponsLevel1.getName()] = MetaTypeEnum::ZergGroundArmorsLevel1;
			alternateUpgrades[MetaTypeEnum::ZergGroundArmorsLevel1.getName()] = MetaTypeEnum::ZergMeleeWeaponsLevel1;
			alternateUpgrades[MetaTypeEnum::ZergMeleeWeaponsLevel2.getName()] = MetaTypeEnum::ZergGroundArmorsLevel2;
			alternateUpgrades[MetaTypeEnum::ZergGroundArmorsLevel2.getName()] = MetaTypeEnum::ZergMeleeWeaponsLevel2;
			alternateUpgrades[MetaTypeEnum::ZergMeleeWeaponsLevel3.getName()] = MetaTypeEnum::ZergGroundArmorsLevel3;
			alternateUpgrades[MetaTypeEnum::ZergGroundArmorsLevel3.getName()] = MetaTypeEnum::ZergMeleeWeaponsLevel3;

			alternateUpgrades[MetaTypeEnum::ZergMissileWeaponsLevel1.getName()] = MetaTypeEnum::ZergGroundArmorsLevel1;
			alternateUpgrades[MetaTypeEnum::ZergGroundArmorsLevel1.getName()] = MetaTypeEnum::ZergMissileWeaponsLevel1;
			alternateUpgrades[MetaTypeEnum::ZergMissileWeaponsLevel2.getName()] = MetaTypeEnum::ZergGroundArmorsLevel2;
			alternateUpgrades[MetaTypeEnum::ZergGroundArmorsLevel2.getName()] = MetaTypeEnum::ZergMissileWeaponsLevel2;
			alternateUpgrades[MetaTypeEnum::ZergMissileWeaponsLevel3.getName()] = MetaTypeEnum::ZergGroundArmorsLevel3;
			alternateUpgrades[MetaTypeEnum::ZergGroundArmorsLevel3.getName()] = MetaTypeEnum::ZergMissileWeaponsLevel3;

			alternateUpgrades[MetaTypeEnum::ZergFlyerWeaponsLevel1.getName()] = MetaTypeEnum::ZergFlyerArmorsLevel1;
			alternateUpgrades[MetaTypeEnum::ZergFlyerArmorsLevel1.getName()] = MetaTypeEnum::ZergFlyerWeaponsLevel1;
			alternateUpgrades[MetaTypeEnum::ZergFlyerWeaponsLevel2.getName()] = MetaTypeEnum::ZergFlyerArmorsLevel2;
			alternateUpgrades[MetaTypeEnum::ZergFlyerArmorsLevel2.getName()] = MetaTypeEnum::ZergFlyerWeaponsLevel2;
			alternateUpgrades[MetaTypeEnum::ZergFlyerWeaponsLevel3.getName()] = MetaTypeEnum::ZergFlyerArmorsLevel3;
			alternateUpgrades[MetaTypeEnum::ZergFlyerArmorsLevel3.getName()] = MetaTypeEnum::ZergFlyerWeaponsLevel3;

			alternateUpgrades[MetaTypeEnum::ZergGroundArmorsLevel1.getName()] = MetaTypeEnum::ZergGroundArmorsLevel2;//Doesn't have an alternate
			alternateUpgrades[MetaTypeEnum::ZergGroundArmorsLevel2.getName()] = MetaTypeEnum::ZergGroundArmorsLevel3;
			alternateUpgrades[MetaTypeEnum::ZergGroundArmorsLevel3.getName()] = MetaTypeEnum::ZergGroundArmorsLevel3;
			break;
		}
	}
}

void ProductionManager::onFrame(bool executeMacro)
{
	if (executeMacro)
	{
		m_bot.StartProfiling("1.0 lowPriorityChecks");
		lowPriorityChecks();
		validateUpgradesProgress();
		m_bot.StopProfiling("1.0 lowPriorityChecks");
		m_bot.StartProfiling("2.0 manageBuildOrderQueue");
		manageBuildOrderQueue();
		m_bot.StopProfiling("2.0 manageBuildOrderQueue");
		/*m_bot.StartProfiling("3.0 QueueDeadBuildings");
		QueueDeadBuildings();
		m_bot.StopProfiling("3.0 QueueDeadBuildings");*/

		// TODO: if nothing is currently building, get a new goal from the strategy manager
		// TODO: triggers for game things like cloaked units etc
	}
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
	int highestPriority = currentItem.priority;
	int additionalReservedMineral = 0;
	int additionalReservedGas = 0;
	bool isSupplyCap = false;
	
    // while there is still something left in the queue
    while (!m_queue.isEmpty())
    {
#ifdef NO_BUILDING
		if (currentItem.type.isBuilding())
		{
			m_queue.removeCurrentHighestPriorityItem();
			continue;
		}
#endif

		if (!ShouldSkipQueueItem(currentItem))	// Checking the initial BO first makes it so that in realtime we start the refinery before the barracks...
		{
			//check if we have the prerequirements.
			if (!hasRequired(currentItem.type, true) || !hasProducer(currentItem.type, true))
			{
				m_bot.StartProfiling("2.2.1     fixBuildOrderDeadlock");
				fixBuildOrderDeadlock(currentItem);
				//currentItem = m_queue.getHighestPriorityItem();
				m_bot.StopProfiling("2.2.1     fixBuildOrderDeadlock");
				//continue;
			}

			const auto barrackCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), true, true);
			const auto factoryCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Factory.getUnitType(), true, true);
			// Proxy buildings
			if (m_bot.GetCurrentFrame() < 4032 /* 3 min */ && ((currentItem.type == MetaTypeEnum::Barracks && m_bot.Strategy().isProxyStartingStrategy() && barrackCount < 2) || 
				(currentItem.type == MetaTypeEnum::Factory && m_bot.Strategy().isProxyFactoryStartingStrategy() && factoryCount == 0)))
			{
				const auto proxyLocation = Util::GetPosition(m_bot.Buildings().getProxyLocation());
				Unit producer = getProducer(currentItem.type, proxyLocation);
				Building b(currentItem.type.getUnitType(), proxyLocation);
				b.finalPosition = proxyLocation;
				if (canMakeAtArrival(b, producer, additionalReservedMineral, additionalReservedGas))
				{
					if (create(producer, currentItem, proxyLocation, false, false, true))
					{
						m_queue.removeCurrentHighestPriorityItem();
						break;
					}
				}
			}
			else if (currentlyHasRequirement(currentItem.type))
			{
				//Check if we already have an idle production building of that type
				bool idleProductionBuilding = false;
#ifndef NO_UNITS
				if (currentItem.type.isBuilding())
				{
					auto productionBuildingTypes = getProductionBuildingTypes();
					const sc2::UnitTypeID itemType = currentItem.type.getUnitType().getAPIUnitType();

					//If its a production building
					if (std::find(productionBuildingTypes.begin(), productionBuildingTypes.end(), itemType) != productionBuildingTypes.end())
					{
						auto & productionBuildings = m_bot.GetAllyUnits(itemType);
						for (auto & productionBuilding : productionBuildings)
						{
							//Check if this building is idle
							if (productionBuilding.isProductionBuildingIdle() && !productionBuilding.isBeingConstructed())
							{
								idleProductionBuilding = true;
								break;
							}
						}
					}
				}
#endif

				if (!idleProductionBuilding)
				{
					auto data = m_bot.Data(currentItem.type);
					// if we can make the current item
					m_bot.StartProfiling("2.2.2     tryingToBuild");
					if (meetsReservedResources(currentItem.type, additionalReservedMineral, additionalReservedGas))
					{
						m_bot.StartProfiling("2.2.3     Build without premovement");
						Unit producer = getProducer(currentItem.type);
						// build supply if we need some (SupplyBlock)
						if (producer.isValid())
						{
							if (m_bot.GetMaxSupply() < 200 && m_bot.Data(currentItem.type.getUnitType()).supplyCost > m_bot.GetMaxSupply() - m_bot.GetCurrentSupply())
							{
								supplyBlockedFrames++;
#if _DEBUG
								Util::DisplayError("Supply blocked. ", "0x00000007", m_bot);
#else
								Util::Log(__FUNCTION__, "Supply blocked | 0x00000007", m_bot);
#endif
							}

							if (canMakeNow(producer, currentItem.type))
							{
								// create it and remove it from the _queue
								if (create(producer, currentItem, m_bot.GetBuildingArea(currentItem.type)))
								{
									m_queue.removeCurrentHighestPriorityItem();

									// don't actually loop around in here
									m_bot.StopProfiling("2.2.2     tryingToBuild");
									m_bot.StopProfiling("2.2.3     Build without premovement");
									break;
								}
								else if (!m_initialBuildOrderFinished)
								{
									Util::DebugLog(__FUNCTION__, "Failed to place " + currentItem.type.getName() + " during initial build order. Skipping.", m_bot);
									m_queue.removeCurrentHighestPriorityItem();
								}
							}
						}
						m_bot.StopProfiling("2.2.3     Build without premovement");
					}
					else if (data.isBuilding
						&& !data.isAddon
						&& !currentItem.type.getUnitType().isMorphedBuilding()
						&& !data.isResourceDepot)//TODO temporary until we have a better solution, allow this if the enemy base doesn't look aggressive
					{
						// is a building (doesn't include addons, because no travel time) and we can make it soon (canMakeSoon)

						m_bot.StartProfiling("2.2.4     Build with premovement");
						Building b(currentItem.type.getUnitType(), m_bot.GetBuildingArea(currentItem.type));
						//Get building location

						m_bot.StartProfiling("2.2.5     getNextBuildingLocation");
						const CCTilePosition targetLocation = m_bot.Buildings().getNextBuildingLocation(b, true, true);
						m_bot.StopProfiling("2.2.5     getNextBuildingLocation");
						if (targetLocation != CCTilePosition(0, 0))
						{
							Unit worker = m_bot.Workers().getClosestMineralWorkerTo(Util::GetPosition(targetLocation));
							if (worker.isValid())
							{
								b.finalPosition = targetLocation;
								if (canMakeAtArrival(b, worker, additionalReservedMineral, additionalReservedGas))
								{
									// create it and remove it from the _queue
									if (create(worker, b) && worker.isValid())
									{
										worker.move(targetLocation);
										m_queue.removeCurrentHighestPriorityItem();
									}

									// don't actually loop around in here
									m_bot.StopProfiling("2.2.2     tryingToBuild");
									m_bot.StopProfiling("2.2.4     Build with premovement");
									break;
								}
							}
						}
						else
						{
							if (currentItem.type.getUnitType().getAPIUnitType() != Util::GetRefineryType().getAPIUnitType())//Supresses the refinery related errors
							{
								Util::DisplayError("Invalid build location for " + currentItem.type.getName(), "0x0000002", m_bot);
							}
						}
						m_bot.StopProfiling("2.2.4     Build with premovement");
					}
					m_bot.StopProfiling("2.2.2     tryingToBuild");
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

bool ProductionManager::ShouldSkipQueueItem(const BuildOrderItem & currentItem) const
{
	bool shouldSkip = false;
	const auto completedFactoryCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Factory.getUnitType(), true, true);
	const auto ccs = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::CommandCenter.getUnitType(), false, true);
	const auto orbitals = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::OrbitalCommand.getUnitType(), false, true);
	const auto deadCCs = m_bot.GetDeadAllyUnitsCount(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER);
	const auto deadOrbitals = m_bot.GetDeadAllyUnitsCount(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND);
	const bool hasStartedFirstExpand = ccs + orbitals + deadCCs + deadOrbitals > 1;
	const auto earlyExpand = m_bot.Strategy().getStartingStrategy() == EARLY_EXPAND && m_bot.GetCurrentFrame() < 3360 && m_bot.GetFreeMinerals() < 700 && !hasStartedFirstExpand;	// 2:30 min
	const auto proxyMaraudersStrategy = m_bot.Strategy().getStartingStrategy() == PROXY_MARAUDERS;
	if (currentItem.type.getUnitType().isRefinery())
	{
		const bool hasBarracks = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true, true) > 0;
		shouldSkip = !hasBarracks;
	}
	else if (currentItem.type.getUnitType().isResourceDepot())
	{
		shouldSkip = m_bot.Strategy().isEarlyRushed() && m_bot.GetMinerals() < 800;
	}
	if (!shouldSkip)
	{
		if (m_bot.Strategy().getStartingStrategy() == PROXY_CYCLONES)
		{
			if (currentItem.type == MetaTypeEnum::FactoryTechLab || currentItem.type == MetaTypeEnum::Factory)
			{
				const bool hasBarracks = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), true, true) > 0;
				const auto unattachedTechlabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::TechLab.getUnitType(), false, false, false);
				const auto barracksTechlabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksTechLab.getUnitType(), false, false, false);
				const auto factoryTechlabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::FactoryTechLab.getUnitType(), false, false, false);
				const auto proxySwapDone = m_bot.Buildings().IsProxySwapDone();
				if (!hasBarracks || (completedFactoryCount > 0 && !proxySwapDone && completedFactoryCount <= unattachedTechlabCount + barracksTechlabCount + factoryTechlabCount))
					shouldSkip = true;
			}
			else if (currentItem.type == MetaTypeEnum::Reaper)
			{
				const bool hasBarracksTechlab = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksTechLab.getUnitType()) > 0;
				if (hasBarracksTechlab)
				{
					shouldSkip = true;
				}
				else
				{
					const bool hasTechlab = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::TechLab.getUnitType(), true, true) > 0;
					const bool reaperCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Reaper.getUnitType());
					if (!hasTechlab && reaperCount == 1)
						shouldSkip = true;
				}
			}
		}
		else if (proxyMaraudersStrategy)
		{
			if (currentItem.type == MetaTypeEnum::Factory)
			{
				const auto baseCount = m_bot.Bases().getBaseCount(Players::Self, false);
				// Let the factory be built if we have too much resources
				const bool tooMuchResources = m_bot.GetMinerals() > 600 && m_bot.GetGas() > 200;
				if (baseCount < 2 && !tooMuchResources)
					shouldSkip = true;
			}
			else if (currentItem.type == MetaTypeEnum::CommandCenter)
			{
				/*const auto maraudersCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Marauder.getUnitType(), false, true);
				shouldSkip = maraudersCount < 2;
				const auto factoryCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Factory.getUnitType(), false, true);
				shouldSkip = factoryCount < 1;*/
			}
			else if (currentItem.type == MetaTypeEnum::Refinery)
			{
				const auto hasRefinery = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Refinery.getUnitType(), false, true) > 0;
				/*const auto hasCompletedBarracks = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), true, true) > 0;
				shouldSkip = hasRefinery && !hasCompletedBarracks;*/
				const auto baseCount = m_bot.Bases().getBaseCount(Players::Self, false);
				if (hasRefinery && baseCount < 2)
					shouldSkip = true;
			}
		}
		else if (earlyExpand)
		{
			if (currentItem.type == MetaTypeEnum::Refinery)
			{
				const auto hasRefinery = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Refinery.getUnitType(), false, true) > 0;
				shouldSkip = hasRefinery;
			}
			else if (currentItem.type == MetaTypeEnum::Factory || currentItem.type == MetaTypeEnum::Starport)
			{
				shouldSkip = true;
			}
		}
		else if (m_bot.Strategy().getStartingStrategy() == WORKER_RUSH)
		{
			shouldSkip = true;
		}
	}
	return shouldSkip;
}

void ProductionManager::putImportantBuildOrderItemsInQueue()
{
#ifdef NO_PRODUCTION
	return;
#endif

	const auto enemyRace = m_bot.GetPlayerRace(Players::Enemy);
	const float productionScore = getProductionScore();
	const auto productionBuildingCount = getProductionBuildingsCount();
	const auto productionBuildingAddonCount = getProductionBuildingsAddonsCount();
	const auto totalBaseCount = m_bot.Bases().getBaseCount(Players::Self, false);
	const auto finishedBaseCount = m_bot.Bases().getBaseCount(Players::Self, true);
	const int bansheeCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Banshee.getUnitType(), false, true);
	const auto starportCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Starport.getUnitType(), false, true);
	const auto barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true);
	const auto completedSupplyProviders = m_bot.UnitInfo().getUnitTypeCount(Players::Self, supplyProvider, true, true);

	const auto currentStrategy = m_bot.Strategy().getCurrentStrategyPostBuildOrder();
	const auto startingStrategy = m_bot.Strategy().getStartingStrategy();

	// build supply if we need some
	const auto supplyWithAdditionalSupplyDepot = m_bot.GetMaxSupply() + m_bot.Buildings().countBeingBuilt(supplyProvider) * 8;
	if(m_bot.GetCurrentSupply() + getSupplyNeedsFromProductionBuildings() + finishedBaseCount > supplyWithAdditionalSupplyDepot
		&& !m_queue.contains(supplyProviderType)
		&& supplyWithAdditionalSupplyDepot < 200
		&& !m_bot.Strategy().isWorkerRushed())
	{
		m_queue.queueAsHighestPriority(supplyProviderType, false);
	}

	if (m_bot.GetSelfRace() == sc2::Race::Terran)
	{
		// Logic for building Orbital Commands and Refineries
		UnitType depot = Util::GetRessourceDepotType();
		const size_t boughtDepotCount = m_bot.Buildings().countBoughtButNotBeingBuilt(depot.getAPIUnitType());
		const size_t incompletedDepotCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, depot, false, true, true);
		const size_t completedDepotCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, depot, true, true);//Only counts unupgraded CC, on purpose.
		if(m_bot.GetSelfRace() == CCRace::Terran && completedDepotCount > 0)
		{
			if (!m_queue.contains(MetaTypeEnum::OrbitalCommand) && !m_queue.contains(MetaTypeEnum::PlanetaryFortress))
			{
				const size_t orbitalCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::OrbitalCommand.getUnitType(), false, true);
				if (true/*orbitalCount < 3*/)//TODO This is temporary logic. Always build Orbital
				{
					m_queue.queueAsHighestPriority(MetaTypeEnum::OrbitalCommand, false);
				}
				else
				{
					m_queue.queueAsHighestPriority(MetaTypeEnum::PlanetaryFortress, false);
				}
			}
		}
		else if (boughtDepotCount == 0 && incompletedDepotCount < 2 && !m_queue.contains(MetaTypeEnum::CommandCenter))
		{
#ifndef NO_EXPANSION
			const bool enoughMinerals = m_bot.GetFreeMinerals() >= 600;
			const int workerCount = m_bot.Workers().getWorkerData().getWorkerJobCount(WorkerJobs::Minerals);
			const int mineralPatches = m_bot.Bases().getAccessibleMineralFieldCount();
			const int WORKER_OFFSET = 10;//Start building earlier because we will be producing more of them while be build.
			const bool enoughWorkers = workerCount > mineralPatches * 2 - WORKER_OFFSET;
			if (enoughMinerals || enoughWorkers)
			{
				m_queue.queueAsLowestPriority(MetaTypeEnum::CommandCenter, false);
			}
#endif
		}

		if (!m_queue.contains(workerMetatype))//check queue
		{
			//[Worker limit][Max worker]
			const int maxWorkersPerBase = 27;//21 mineral (to prepare for the next expansion), 6 gas
			const int maxWorkers = maxWorkersPerBase * 3;//maximum of 3 bases.
			const int workerCount = m_bot.Workers().getNumWorkers();
			if (totalBaseCount * maxWorkersPerBase > workerCount && workerCount < maxWorkers)
			{
				if (currentStrategy != WORKER_RUSH_DEFENSE)//check strategy
				{
					m_queue.queueItem(BuildOrderItem(workerMetatype, 1, false));
				}
			}
		}

		// Strategy base logic
		switch (currentStrategy)
		{
			case TERRAN_CLASSIC :
			{
				const bool proxyCyclonesStrategy = startingStrategy == PROXY_CYCLONES;
				const bool proxyMaraudersStrategy = startingStrategy == PROXY_MARAUDERS && m_bot.GetCurrentFrame() <= 6720;	// 5 minutes
				const bool hasFusionCore = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::FusionCore.getUnitType(), true, true) > 0;
				const auto reaperCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Reaper.getUnitType(), false, true);

				const int maraudersCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Marauder.getUnitType(), false, true);
				const int enemyStalkerCount = m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::PROTOSS_STALKER).size();
				const int enemyRoachAndRavagerCount = m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::ZERG_ROACH).size() + m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::ZERG_RAVAGER).size() + m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::ZERG_RAVAGERCOCOON).size();
				const int enemyUnitsWeakAgainstMarauders = enemyStalkerCount + enemyRoachAndRavagerCount;
				const bool enemyEarlyRoachWarren = m_bot.GetCurrentFrame() < 4032 && !m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::ZERG_ROACHWARREN).empty();	// 3 minutes
				const bool pumpOutMarauders = proxyMaraudersStrategy || enemyUnitsWeakAgainstMarauders >= 5;
				const bool produceMarauders = pumpOutMarauders || enemyEarlyRoachWarren || maraudersCount < enemyUnitsWeakAgainstMarauders;
				
				if (productionBuildingAddonCount < productionBuildingCount)
				{//Addon
					bool hasPicked = false;
					MetaType toBuild;
					//const auto createdBarracksTechLabsCount = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB).size();
					const auto barracksTechLabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksTechLab.getUnitType(), false, true);
					const auto barracksReactorCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksReactor.getUnitType(), false, true);
					const auto barracksAddonCount = barracksTechLabCount + barracksReactorCount;
					const auto starportTechLabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportTechLab.getUnitType(), false, true);
					const auto starportReactorCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportReactor.getUnitType(), false, true);
					const auto starportAddonCount = starportTechLabCount + starportReactorCount;
					if ((reaperCount > 0 || pumpOutMarauders) && barracksCount > barracksAddonCount && (pumpOutMarauders || (proxyCyclonesStrategy && firstBarracksTechlab)))
					{
						firstBarracksTechlab = false;
						toBuild = MetaTypeEnum::BarracksTechLab;
						hasPicked = true;
					}
					else if (starportCount > starportAddonCount)
					{
						if (m_bot.Strategy().enemyHasProtossHighTechAir() || (m_bot.Strategy().shouldProduceAntiAirOffense() && starportTechLabCount > starportReactorCount) || (proxyMaraudersStrategy && starportReactorCount == 0))
							toBuild = MetaTypeEnum::StarportReactor;
						else//if (!proxyMaraudersStrategy || hasFusionCore), not required since it is either Reactor else it is TechLab
							toBuild = MetaTypeEnum::StarportTechLab;
						hasPicked = true;
					}

					if (hasPicked && !m_queue.contains(toBuild))
					{
						m_queue.queueItem(BuildOrderItem(toBuild, 1, false));
					}
				}

				//Building
				bool hasPicked = false;
				MetaType toBuild;
				const int completedBarracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), true, true);
				const int factoryCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Factory.getUnitType(), false, true);
				if (barracksCount < 1 || (pumpOutMarauders && completedSupplyProviders == 1 && barracksCount < 2) || (hasFusionCore && m_bot.GetFreeMinerals() >= 550 /*For a BC and a Barracks*/ && barracksCount * 2 < finishedBaseCount))
				{
					toBuild = MetaTypeEnum::Barracks;
					hasPicked = true;
				}
				else if (m_bot.Strategy().enemyHasMassZerglings() && factoryCount * 2 < finishedBaseCount)
				{
					toBuild = MetaTypeEnum::Factory;
					hasPicked = true;
				}
				else if (starportCount < finishedBaseCount && (!produceMarauders || totalBaseCount > 1))
				{
					toBuild = MetaTypeEnum::Starport;
					hasPicked = true;
				}

				const bool forceProductionBuilding = proxyMaraudersStrategy && toBuild == MetaTypeEnum::Barracks;
				if (hasPicked && !m_queue.contains(toBuild) && (forceProductionBuilding || !m_queue.contains(MetaTypeEnum::CommandCenter) || m_bot.GetFreeMinerals() > 400 || m_bot.Bases().getFreeBaseLocationCount() == 0))
				{
					bool idleProductionBuilding = false;
					const auto & productionBuildings = m_bot.GetAllyUnits(toBuild.getUnitType().getAPIUnitType());
					const int totalProductionBuildings = m_bot.UnitInfo().getUnitTypeCount(Players::Self, toBuild.getUnitType(), false, true);
					if (productionBuildings.size() == totalProductionBuildings)
					{
						for (const auto & productionBuilding : productionBuildings)
						{
							if (productionBuilding.isProductionBuildingIdle())
							{
								idleProductionBuilding = true;
								break;
							}
						}
						if (forceProductionBuilding || !idleProductionBuilding)
							m_queue.queueAsLowestPriority(toBuild, false);
					}
				}

#ifndef NO_UNITS
				if (!produceMarauders && (reaperCount == 0 || (factoryCount == 0 && !m_bot.Strategy().enemyHasMassZerglings() && m_bot.Analyzer().GetRatio(sc2::UNIT_TYPEID::TERRAN_REAPER) > 1.5f)) && !m_queue.contains(MetaTypeEnum::Reaper))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Reaper, 0, false));
				}
#endif

				const int battlecruiserCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Battlecruiser.getUnitType(), false, true);
				const int vikingCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Viking.getUnitType(), false, true);
				const int enemyVikingCount = m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER).size();
				const bool hasEnoughVikings = enemyVikingCount == 0 || vikingCount >= enemyVikingCount * 1.5f;
				const int medivacCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Medivac.getUnitType(), false, true);
				const int enemyTempestCount = m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::PROTOSS_TEMPEST).size();
				bool makeBattlecruisers = false;

				if(finishedBaseCount >= 3 && hasEnoughVikings)
				{
#ifndef NO_UNITS
					if (enemyTempestCount == 0)
					{
						makeBattlecruisers = true;
						
						if (!m_queue.contains(MetaTypeEnum::Battlecruiser))
						{
							m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Battlecruiser, 1, false));
						}

						if (hasFusionCore)
						{
							if (m_bot.GetFreeMinerals() >= 450 /*for a BC*/ && !m_queue.contains(MetaTypeEnum::Marine))
							{
								m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marine, 0, false));
							}

							if (!isTechQueuedOrStarted(MetaTypeEnum::YamatoCannon) && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::BATTLECRUISERENABLESPECIALIZATIONS))
							{
								queueTech(MetaTypeEnum::YamatoCannon);
							}
						}
					}
					else
					{
						m_queue.removeAllOfType(MetaTypeEnum::Battlecruiser);
						m_queue.removeAllOfType(MetaTypeEnum::YamatoCannon);
						m_queue.removeAllOfType(MetaTypeEnum::FusionCore);
					}
#endif
				}

				const bool stopBanshees = (makeBattlecruisers && hasFusionCore) || m_bot.Strategy().enemyHasProtossHighTechAir() || proxyMaraudersStrategy;
				if (stopBanshees)
				{
					m_queue.removeAllOfType(MetaTypeEnum::Banshee);
					m_queue.removeAllOfType(MetaTypeEnum::BansheeCloak);
					m_queue.removeAllOfType(MetaTypeEnum::HyperflightRotors);
				}
				else
				{
					const auto earlyRushed = m_bot.Strategy().isEarlyRushed();
					//const auto bansheeInProduction = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Banshee.getUnitType(), false, true, true) > 0;
					const auto researchBansheeCloak = !earlyRushed;	// || bansheeInProduction;
					const auto researchBansheeSpeed = !earlyRushed && bansheeCount > 0;

					// Banshee Cloak upgrade
					if (researchBansheeCloak)
					{
						if (!isTechQueuedOrStarted(MetaTypeEnum::BansheeCloak) && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::BANSHEECLOAK))
						{
							queueTech(MetaTypeEnum::BansheeCloak);
						}
					}
					else
					{
						m_queue.removeAllOfType(MetaTypeEnum::BansheeCloak);
					}

					// Banshee Speed upgrade
					if (researchBansheeSpeed)
					{
						if (m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::BANSHEECLOAK) && !isTechQueuedOrStarted(MetaTypeEnum::HyperflightRotors) && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::BANSHEESPEED))
						{
							queueTech(MetaTypeEnum::HyperflightRotors);
						}
					}
					else
					{
						m_queue.removeAllOfType(MetaTypeEnum::HyperflightRotors);
					}

#ifndef NO_UNITS
					if (/*isTechStarted(MetaTypeEnum::BansheeCloak) &&*/ !m_queue.contains(MetaTypeEnum::Banshee))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Banshee, 0, false));
					}
#endif
				}
#ifndef NO_UNITS
				if (m_bot.Strategy().enemyCurrentlyHasInvisible() && !m_queue.contains(MetaTypeEnum::Raven) && m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Raven.getUnitType(), false, true) < 1)
				{
					m_queue.queueAsHighestPriority(MetaTypeEnum::Raven, false);
				}

				bool enoughMedivacs = true;
				if (produceMarauders)
				{
					if (!m_queue.contains(MetaTypeEnum::Marauder))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marauder, 0, false));
					}

					if (maraudersCount > 0 && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::PUNISHERGRENADES) && !isTechQueuedOrStarted(MetaTypeEnum::ConcussiveShells))
					{
						queueTech(MetaTypeEnum::ConcussiveShells);
					}

					if (m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::PUNISHERGRENADES) && maraudersCount >= 5 && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::STIMPACK) && !isTechQueuedOrStarted(MetaTypeEnum::Stimpack))
					{
						queueTech(MetaTypeEnum::Stimpack);
					}

					// 1 Medivac for every 4 Marauders
					if (medivacCount < floor(maraudersCount / 4.f))
					{
						enoughMedivacs = false;
						if (!m_queue.contains(MetaTypeEnum::Medivac))
						{
							m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Medivac, 0, false));
						}
					}
				}
#endif

				const int cycloneCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Cyclone.getUnitType(), false, true);
				const int hellionCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Hellion.getUnitType(), false, true);
				const int thorCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Thor.getUnitType(), false, true);
				const int deadHellionCount = m_bot.GetDeadAllyUnitsCount(sc2::UNIT_TYPEID::TERRAN_HELLION);
				const bool shouldProduceFirstCyclone = startingStrategy == PROXY_CYCLONES && cycloneCount == 0;
				const auto finishedArmory = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Armory.getUnitType(), true, true) > 0;
				const int enemyZealotCount = m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::PROTOSS_ZEALOT).size();
				const int enemyZerglingCount = m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::ZERG_ZERGLING).size() + m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::ZERG_ZERGLINGBURROWED).size();
				// We want to build Thors against Protoss, but only after we have an Armory and we don't want more Thors than Cyclones
				if (startingStrategy != PROXY_CYCLONES && enemyRace == sc2::Protoss && finishedArmory && thorCount + 1 < cycloneCount)
				{
					m_queue.removeAllOfType(MetaTypeEnum::Cyclone);
					m_queue.removeAllOfType(MetaTypeEnum::MagFieldAccelerator);
					if (!m_queue.contains(MetaTypeEnum::Thor))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Thor, 0, false));
					}
				}
				// We want at least 1 Hellion for every 2 enemy Zealot or 4 enemy Zergling. Against Zerg, we want to make at least 1 asap to defend against 
				else if (!shouldProduceFirstCyclone && ((hellionCount + 1) * 2 < enemyZealotCount || (hellionCount + 1) * 4 < enemyZerglingCount || (enemyRace == sc2::Race::Zerg && hellionCount + deadHellionCount == 0)))
				{
					m_queue.removeAllOfType(MetaTypeEnum::Cyclone);
					m_queue.removeAllOfType(MetaTypeEnum::MagFieldAccelerator);
#ifndef NO_UNITS
					if (!m_queue.contains(MetaTypeEnum::Hellion))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Hellion, 0, false));
					}
#endif

					if (hellionCount >= 5 && !m_bot.Strategy().isEarlyRushed() && !isTechQueuedOrStarted(MetaTypeEnum::InfernalPreIgniter) && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::HIGHCAPACITYBARRELS))
					{
						queueTech(MetaTypeEnum::InfernalPreIgniter);
					}
				}
				else
				{
					m_queue.removeAllOfType(MetaTypeEnum::Thor);
					m_queue.removeAllOfType(MetaTypeEnum::Hellion);
					m_queue.removeAllOfType(MetaTypeEnum::InfernalPreIgniter);
#ifndef NO_UNITS
					if (!m_queue.contains(MetaTypeEnum::Cyclone))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Cyclone, 0, false));
					}
#endif

					if (m_bot.Strategy().isEarlyRushed())
					{
						m_queue.removeAllOfType(MetaTypeEnum::MagFieldAccelerator);
					}
					else
					{
						const int cycloneCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Cyclone.getUnitType(), false, true);
						if (cycloneCount > 0 && m_bot.Strategy().enemyHasSeveralArmoredUnits() && !isTechQueuedOrStarted(MetaTypeEnum::MagFieldAccelerator) && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::MAGFIELDLAUNCHERS))
						{
							queueTech(MetaTypeEnum::MagFieldAccelerator);
						}
					}
				}

				if (m_bot.Strategy().shouldProduceAntiAirDefense() && !isTechQueuedOrStarted(MetaTypeEnum::HiSecAutoTracking) && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::HISECAUTOTRACKING))
				{
					queueTech(MetaTypeEnum::HiSecAutoTracking);
				}
				
				if (m_bot.Strategy().shouldProduceAntiAirOffense())
				{
#ifndef NO_UNITS
					bool makeVikings = false;
					if (m_bot.Strategy().enemyHasProtossHighTechAir())
					{
						makeVikings = true;
					}
					else if (!stopBanshees)
					{
						makeVikings = vikingCount < bansheeCount || (m_bot.GetFreeMinerals() >= 300 && m_bot.GetFreeGas() >= 225);
					}
					else if (makeBattlecruisers && hasFusionCore)
					{
						makeVikings = vikingCount < battlecruiserCount * 3 || (m_bot.GetFreeMinerals() >= 550 && m_bot.GetFreeGas() >= 375);
					}
					else
					{
						makeVikings = !hasFusionCore && stopBanshees;
					}
					if (makeVikings)
					{
						if (vikingCount < 50 && !m_queue.contains(MetaTypeEnum::Viking))
						{
							m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Viking, 0, false));
						}
					}
#endif
				}
				else
				{
#ifndef NO_UNITS
					const auto enemyOverseerCount = m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::ZERG_OVERSEER).size();
					if (m_bot.Strategy().enemyOnlyHasFlyingBuildings() || (proxyMaraudersStrategy && enoughMedivacs) || enemyOverseerCount >= 1)
					{
						const int minVikingCount = proxyMaraudersStrategy ? 2 : 1;
						if (vikingCount < minVikingCount && !m_queue.contains(MetaTypeEnum::Viking))
						{
							m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Viking, 0, false));
						}
					}
#endif
				}
				break;
			}
			case MARINE_MARAUDER:
			{
				/*const bool proxyCyclonesStrategy = startingStrategy == PROXY_CYCLONES;
				const bool proxyMaraudersStrategy = startingStrategy == PROXY_MARAUDERS;
				const bool hasFusionCore = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::FusionCore.getUnitType(), true, true) > 0;
				const auto reaperCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Reaper.getUnitType(), false, true);

				if (productionBuildingAddonCount < productionBuildingCount)
				{//Addon
					bool hasPicked = false;
					MetaType toBuild;
					const auto createdBarracksTechLabsCount = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB).size();
					const auto barracksTechLabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksTechLab.getUnitType(), false, true);
					const auto barracksReactorCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksReactor.getUnitType(), false, true);
					const auto barracksAddonCount = createdBarracksTechLabsCount + barracksReactorCount;
					const auto starportTechLabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportTechLab.getUnitType(), false, true);
					const auto starportReactorCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportReactor.getUnitType(), false, true);
					const auto starportAddonCount = starportTechLabCount + starportReactorCount;
					if (reaperCount > 0 && barracksCount > barracksAddonCount && (proxyMaraudersStrategy || (proxyCyclonesStrategy && firstBarracksTechlab)))
					{
						firstBarracksTechlab = false;
						toBuild = MetaTypeEnum::BarracksTechLab;
						hasPicked = true;
					}
					else if (starportCount > starportAddonCount)
					{
						if (hasFusionCore || starportReactorCount > 0)//Build only a single Reactor
							toBuild = MetaTypeEnum::StarportTechLab;
						else
							toBuild = MetaTypeEnum::StarportReactor;
						hasPicked = true;
					}

					if (hasPicked && !m_queue.contains(toBuild))
					{
						m_queue.queueItem(BuildOrderItem(toBuild, 1, false));
					}
				}

				//Building
				bool hasPicked = false;
				MetaType toBuild;
				const int barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true);
				const int completedBarracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), true, true);
				const int factoryCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Factory.getUnitType(), false, true);
				if (barracksCount < 1 || (proxyMaraudersStrategy && completedBarracksCount == 1 && barracksCount < 2) || (hasFusionCore && m_bot.GetFreeMinerals() >= 550 && barracksCount * 2 < finishedBaseCount))	//For a BC and a Barracks
				{
					toBuild = MetaTypeEnum::Barracks;
					hasPicked = true;
				}
				else if (m_bot.Strategy().enemyHasMassZerglings() && factoryCount * 2 < finishedBaseCount)
				{
					toBuild = MetaTypeEnum::Factory;
					hasPicked = true;
				}
				else if (starportCount < finishedBaseCount && (!proxyMaraudersStrategy || totalBaseCount > 1))
				{
					toBuild = MetaTypeEnum::Starport;
					hasPicked = true;
				}

				const bool forceProductionBuilding = proxyMaraudersStrategy && toBuild == MetaTypeEnum::Barracks;
				if (hasPicked && !m_queue.contains(toBuild) && (forceProductionBuilding || !m_queue.contains(MetaTypeEnum::CommandCenter) || m_bot.GetFreeMinerals() > 400 || m_bot.Bases().getFreeBaseLocationCount() == 0))
				{
					bool idleProductionBuilding = false;
					const auto & productionBuildings = m_bot.GetAllyUnits(toBuild.getUnitType().getAPIUnitType());
					const int totalProductionBuildings = m_bot.UnitInfo().getUnitTypeCount(Players::Self, toBuild.getUnitType(), false, true);
					if (productionBuildings.size() == totalProductionBuildings)
					{
						for (const auto & productionBuilding : productionBuildings)
						{
							if (productionBuilding.isProductionBuildingIdle())
							{
								idleProductionBuilding = true;
								break;
							}
						}
						if (forceProductionBuilding || !idleProductionBuilding)
							m_queue.queueAsLowestPriority(toBuild, false);
					}
				}

#ifndef NO_UNITS
				if ((reaperCount == 0 || (factoryCount == 0 && !proxyMaraudersStrategy && !m_bot.Strategy().enemyHasMassZerglings() && m_bot.Analyzer().GetRatio(sc2::UNIT_TYPEID::TERRAN_REAPER) > 1.5f)) && !m_queue.contains(MetaTypeEnum::Reaper))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Reaper, 0, false));
				}
#endif

				const int battlecruiserCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Battlecruiser.getUnitType(), false, true);
				const int vikingCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Viking.getUnitType(), false, true);
				const int enemyTempestCount = m_bot.GetEnemyUnits(sc2::UNIT_TYPEID::PROTOSS_TEMPEST).size();

				if (finishedBaseCount >= 3)
				{
#ifndef NO_UNITS
					if (enemyTempestCount == 0)
					{
						if (!m_queue.contains(MetaTypeEnum::Battlecruiser))
						{
							m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Battlecruiser, 0, false));
						}

						if (hasFusionCore)
						{
							if (m_bot.GetFreeMinerals() >= 450 && !m_queue.contains(MetaTypeEnum::Marine)) //for a BC
							{
								m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marine, 0, false));
							}

							if (!isTechQueuedOrStarted(MetaTypeEnum::YamatoCannon) && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::BATTLECRUISERENABLESPECIALIZATIONS))
							{
								queueTech(MetaTypeEnum::YamatoCannon);
							}
						}
					}
					else
					{
						m_queue.removeAllOfType(MetaTypeEnum::Battlecruiser);
						m_queue.removeAllOfType(MetaTypeEnum::YamatoCannon);
						m_queue.removeAllOfType(MetaTypeEnum::FusionCore);
					}
#endif
				}

#ifndef NO_UNITS
				if ((m_bot.Strategy().enemyCurrentlyHasInvisible() || m_bot.Strategy().enemyHasBurrowingUpgrade() || (enemyRace == sc2::Terran && bansheeCount >= 3)) && !m_queue.contains(MetaTypeEnum::Raven) && m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Raven.getUnitType(), false, true) < 1)
				{
					m_queue.queueAsHighestPriority(MetaTypeEnum::Raven, false);
				}

				if (proxyMaraudersStrategy && reaperCount > 0)
				{
					if (!m_queue.contains(MetaTypeEnum::Marauder))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marauder, 0, false));
					}

					const int maraudersCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Marauder.getUnitType(), false, true);
					if (maraudersCount > 0 && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::PUNISHERGRENADES) && !isTechQueuedOrStarted(MetaTypeEnum::ConcussiveShells))
					{
						queueTech(MetaTypeEnum::ConcussiveShells);
					}
				}
#endif

				const int marineCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Marine.getUnitType(), true, true);
				const int marauderCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Marauder.getUnitType(), true, true);
				const int medivacCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Medivac.getUnitType(), true, true);
				
#ifndef NO_UNITS
				if (marineCount < marauderCount * 2)
				{
					if (!m_queue.contains(MetaTypeEnum::Marine))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marine, 0, false));
					}
				}
				else
				{
					if (!m_queue.contains(MetaTypeEnum::Marauder))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marauder, 0, false));
					}
				}
#endif

				if (m_bot.Strategy().shouldProduceAntiAirDefense() && !isTechQueuedOrStarted(MetaTypeEnum::HiSecAutoTracking) && !m_bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::HISECAUTOTRACKING))
				{
					queueTech(MetaTypeEnum::HiSecAutoTracking);
				}

				if (medivacCount == 0 || (marineCount + marauderCount) > medivacCount * 8)
				{
#ifndef NO_UNITS
					if (!m_queue.contains(MetaTypeEnum::Medivac))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Medivac, 0, false));
					}
#endif
				}
				else if (m_bot.Strategy().shouldProduceAntiAirOffense())
				{
#ifndef NO_UNITS
					if (vikingCount < bansheeCount || vikingCount < battlecruiserCount * 2.5f || !hasFusionCore || m_bot.Strategy().enemyHasProtossHighTechAir())
					{
						if (vikingCount < 50 && !m_queue.contains(MetaTypeEnum::Viking))
						{
							m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Viking, 0, false));
						}
					}
#endif
				}
				else if (m_bot.Strategy().enemyOnlyHasFlyingBuildings() || proxyMaraudersStrategy)
				{
#ifndef NO_UNITS
					if (vikingCount < 1 && !m_queue.contains(MetaTypeEnum::Viking))
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Viking, 0, false));
					}
#endif
				}*/
				break;
			}
			case WORKER_RUSH_DEFENSE:
			{
				m_queue.clearAll();
				const int barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, false);
				if (barracksCount > 0)
				{
					if (m_bot.GetFreeGas() >= 50 || m_bot.Workers().getNumGasWorkers() > 0)
					{
						m_queue.queueAsHighestPriority(MetaTypeEnum::Reaper, false);
					}
					else
					{
						m_queue.queueAsHighestPriority(MetaTypeEnum::Marine, false);
					}
				}
				else
				{
					m_queue.queueAsHighestPriority(MetaTypeEnum::Barracks, false);
				}
				break;
			}
			case NO_STRATEGY:
				break;
			default:
			{
				assert("This strategy doesn't exist.");
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
			auto buildings = m_bot.Buildings().getPreviousBuildings();
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

	// We don't want addons to add new buildings to the queue
	if (typeData.isAddon)
		return;

	// check to see if we have the prerequisites for the item
    if (!hasRequired(item.type, true))
    {
		for (auto & required : typeData.requiredUnits)
		{
			if (!hasRequiredUnit(required, true) && !m_queue.contains(MetaType(required, m_bot)))
			{
				std::cout << item.type.getName() << " needs a requirement: " << required.getName() << "\n";
				BuildOrderItem requiredItem = m_queue.queueItem(BuildOrderItem(MetaType(required, m_bot), 0, item.blocking));
				fixBuildOrderDeadlock(requiredItem);
				break;
			}
		}
        return;
    }

    // build the producer of the unit if we don't have one
	MetaType builder = MetaType(typeData.whatBuilds[0], m_bot);
    if (!hasProducer(item.type, true) && !m_queue.contains(builder))
    {
		std::cout << item.type.getName() << " needs a producer: " << builder.getName() << "\n";
		BuildOrderItem producerItem = m_queue.queueItem(BuildOrderItem(builder, 0, item.blocking));
        fixBuildOrderDeadlock(producerItem);
    }

    // build a refinery if we don't have one and the thing costs gas
	if (typeData.gasCost > 0)
    {
		auto refinery = Util::GetRefineryType();
		if (!m_queue.contains(MetaType(refinery, m_bot)))
		{
			auto richRefinery = Util::GetRichRefineryType();
			auto refineryCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, refinery, false, true);
			auto richRefineryCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, richRefinery, false, true);
			if (refineryCount + richRefineryCount == 0)
			{
				m_queue.queueAsHighestPriority(MetaType(refinery, m_bot), item.blocking);
			}
		}
    }
}

void ProductionManager::lowPriorityChecks()
{
	if (m_bot.GetGameLoop()  - m_lastLowPriorityCheckFrame < 10)
	{
		return;
	}
	m_lastLowPriorityCheckFrame = m_bot.GetGameLoop();

	// build a refinery if we are missing one
	//TODO doesn't handle extra hatcheries
	auto refinery = Util::GetRefineryType();
	if (m_bot.Workers().canHandleMoreRefinery() && !m_queue.contains(MetaType(refinery, m_bot)))
	{
		if (m_initialBuildOrderFinished && !m_bot.Strategy().isWorkerRushed())
		{
			auto& refineries = m_bot.GetAllyGeyserUnits();
			const auto & bases = m_bot.Bases().getBaseLocations();
			for (auto & base : bases)
			{
				if (base == nullptr || !base->isOccupiedByPlayer(Players::Self) || base->isUnderAttack())
					continue;
				
				const Unit& resourceDepot = base->getResourceDepot();
				if (!resourceDepot.isValid())
					continue;

				auto& geysers = base->getGeysers();
				for (auto & geyser : geysers)
				{
					//filter out depleted geysers
					if (geyser.getUnitPtr()->vespene_contents <= 0)
						continue;

					auto position = geyser.getTilePosition();
					bool refineryFound = false;
					for (auto refinery : refineries)
					{
						if (!refinery.isValid())
							continue;
						if (refinery.getTilePosition() == position)
						{
							refineryFound = true;
							break;
						}
					}

					if (!refineryFound)
					{
						m_queue.queueAsLowestPriority(MetaType(refinery, m_bot), false);
					}
				}
			}
		}
	}
	
	//build bunkers in mineral/gas field [optimize economy]
	if (m_bot.GetPlayerRace(Players::Self) == CCRace::Terran && m_bot.Workers().getGasWorkersTarget() > 0)
	{
		auto refineries = m_bot.GetAllyGeyserUnits();
		for (auto base : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
		{
			if (!base->getResourceDepot().isValid() || base->getResourceDepot().getPlayer() != Players::Self || !base->getResourceDepot().isCompleted() || base->isGeyserSplit())//TEMPORARY skip split geysers since they are not yet handled
			{
				continue;
			}

			//Do not build a bunker if either geyser is low on gas or doesn't have at least a started refinery
			if (!base->isGeyserSplit())
			{
				bool lowGasGeyser = false;
				bool missingRefinery = false;

				auto geysers = base->getGeysers();
				for (auto geyser : geysers)
				{
					if (geyser.getUnitPtr()->vespene_contents <= 500)
					{
						lowGasGeyser = true;
						break;
					}

					bool hasRefinery = false;
					for (auto refinery : refineries)
					{
						if (geyser.getPosition() == refinery.getPosition())
						{
							hasRefinery = true;
							break;
						}
					}
					if (!hasRefinery)
					{
						missingRefinery = true;
						break;
					}
				}
				if (lowGasGeyser || missingRefinery)
				{
					continue;
				}
			}

			if (!m_bot.Buildings().isConstructingType(MetaTypeEnum::Bunker.getUnitType()))
			{
				auto bunkers = base->getGasBunkers();
				for (auto bunkerLocation : base->getGasBunkerLocations())
				{
					bool hasBunker = false;
					for (auto bunker : bunkers)
					{
						if (bunkerLocation == bunker.getTilePosition())
						{
							hasBunker = true;
							break;
						}
					}
					if (!hasBunker)//Has no bunker so lets build one
					{
						m_bot.Buildings().getBuildingPlacer().freeTilesForBunker(bunkerLocation);
						auto worker = m_bot.Workers().getClosestMineralWorkerTo(CCPosition(bunkerLocation.x, bunkerLocation.y));
						auto boItem = BuildOrderItem(MetaTypeEnum::Bunker, 0, false);
						create(worker, boItem, bunkerLocation, true, true, false);

						break;
					}
				}
			}
		}
	}

	//build turrets in mineral field
	//TODO only supports terran, turret position isn't always optimal(check BaseLocation to optimize it)
	const bool shouldProduceAntiAirDefense = m_bot.Strategy().shouldProduceAntiAirDefense();
	const bool shouldProduceAntiInvis = m_bot.Strategy().enemyHasInvisible() && m_bot.GetPlayerRace(Players::Enemy) != CCRace::Zerg;//Do not produce turrets VS burrowed zerg units
	if (shouldProduceAntiAirDefense || shouldProduceAntiInvis)
	{
		const auto engineeringBayCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::EngineeringBay.getUnitType(), false, true);
		if (engineeringBayCount <= 0 && !m_queue.contains(MetaTypeEnum::EngineeringBay))
		{
			const int starportCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Starport.getUnitType(), false, true);
			if (!shouldProduceAntiInvis && starportCount > 0)
			{
				const int vikingCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Viking.getUnitType(), false, true);
				if (vikingCount > 0)
				{
					m_queue.queueAsLowestPriority(MetaTypeEnum::EngineeringBay, false);
				}
			}
			else
			{
				m_queue.queueAsHighestPriority(MetaTypeEnum::EngineeringBay, false);
			}
		}

		if (!m_bot.Buildings().isConstructingType(MetaTypeEnum::MissileTurret.getUnitType()))
		{
			const int completedEngineeringBayCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::EngineeringBay.getUnitType(), true, true);
			if (completedEngineeringBayCount > 0)
			{
				for (auto base : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
				{
					if (!base->getResourceDepot().isValid() || base->getResourceDepot().getPlayer() != Players::Self)
						continue;
					
					auto hasTurret = false;
					auto position = base->getTurretPosition();
					auto buildings = m_bot.Buildings().getFinishedBuildings();
					for (auto & b : buildings)
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
						auto boItem = BuildOrderItem(MetaTypeEnum::MissileTurret, 0, false);
						create(worker, boItem, position, true, true, false);
					}
				}
			}
		}
	}
}

bool ProductionManager::currentlyHasRequirement(MetaType currentItem) const
{
 	auto requiredUnits = m_bot.Data(currentItem).requiredUnits;
	if (requiredUnits.empty())
	{
		return true;
	}

	for (auto & required : m_bot.Data(currentItem).requiredUnits)
	{
		
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

bool ProductionManager::hasRequired(const MetaType& metaType, bool checkInQueue) const
{
	const TypeData& typeData = m_bot.Data(metaType);

	if (typeData.requiredUnits.empty())
		return true;

	for (auto & required : typeData.requiredUnits)
	{
		if (!hasRequiredUnit(required, checkInQueue))
			return false;
	}

	return true;
}

bool ProductionManager::hasRequiredUnit(const UnitType& unitType, bool checkInQueue) const
{
	if (m_bot.UnitInfo().getUnitTypeCount(Players::Self, unitType, false, true) > 0)
		return true;

	if (m_bot.Buildings().isBeingBuilt(unitType))
		return true;

	if (checkInQueue && m_queue.contains(MetaType(unitType, m_bot)))
		return true;

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
		const auto unitType = type.getUnitType().getAPIUnitType();
		if (unitType == sc2::UNIT_TYPEID::TERRAN_MARINE ||
			unitType == sc2::UNIT_TYPEID::TERRAN_REAPER ||
			unitType == sc2::UNIT_TYPEID::TERRAN_WIDOWMINE ||
			unitType == sc2::UNIT_TYPEID::TERRAN_HELLION ||
			unitType == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER ||
			unitType == sc2::UNIT_TYPEID::TERRAN_MEDIVAC ||
			unitType == sc2::UNIT_TYPEID::TERRAN_LIBERATOR)
		{

			for (auto & producerType : producerTypes)
			{
				for (auto & unit : m_bot.GetAllyUnits(producerType.getAPIUnitType()))
				{
					// reasons a unit can not train the desired type
					if (!unit.isValid()) { continue; }
					if (!unit.isCompleted()) { continue; }
					if (unit.isFlying()) { continue; }
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
	}

    // make a set of all candidate producers
    std::vector<Unit> candidateProducers;
	for (auto & producerType : producerTypes)
	{
		for (auto & unit : m_bot.GetAllyUnits(producerType.getAPIUnitType()))
		{
			// reasons a unit can not train the desired type
			if (!unit.isValid()) { continue; }
			if (!unit.isCompleted()) { continue; }
			if (unit.isFlying()) { continue; }

			bool isBuilding = m_bot.Data(unit).isBuilding;
			if (isBuilding && unit.isTraining() && unit.getAddonTag() == 0) { continue; }//TODO might break other races
			if (isBuilding && m_bot.GetSelfRace() == CCRace::Terran)
			{
				sc2::UNIT_TYPEID unitType = unit.getAPIUnitType();

				//If the commandcenter should morph instead, don't queue workers on it
				if (unitType == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER && !type.getUnitType().isMorphedBuilding())
				{
					if (m_initialBuildOrderFinished && (m_queue.contains(MetaTypeEnum::OrbitalCommand) || m_queue.contains(MetaTypeEnum::PlanetaryFortress))
						&& m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), true, true) > 0)
					{
						continue;
					}
				}
				else
				{
					//If is terran, check for Reactor addon
					sc2::Tag addonTag = unit.getAddonTag();

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
				const auto & orders = unit.getUnitPtr()->orders;
				bool isMoving = false;
				for (const auto & order : orders)
				{
					if (order.ability_id == sc2::ABILITY_ID::MOVE)
					{
						isMoving = true;
						break;
					}
				}
				if (isMoving)
					continue;
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
						break;
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
	}

    return getClosestUnitToPosition(candidateProducers, closestTo);
}

std::vector<sc2::UNIT_TYPEID> ProductionManager::getProductionBuildingTypes() const
{
	switch (m_bot.GetSelfRace())
	{
		case CCRace::Terran:
		{
			return {sc2::UNIT_TYPEID::TERRAN_BARRACKS,
				sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING,
				sc2::UNIT_TYPEID::TERRAN_FACTORY,
				sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING,
				sc2::UNIT_TYPEID::TERRAN_STARPORT,
				sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING };
		}
		case CCRace::Protoss:
		{
			//TODO
			return {};
		}
		case CCRace::Zerg:
		{
			//TODO
			return {};
		}
	}
	return {};
}

int ProductionManager::getProductionBuildingsCount() const
{
	auto productionBuildingTypes = getProductionBuildingTypes();
	return m_bot.Buildings().getBuildingCountOfType(productionBuildingTypes);
}

int ProductionManager::getProductionBuildingsAddonsCount() const
{
	switch (m_bot.GetSelfRace())
	{
		case CCRace::Terran:
		{
			std::vector<sc2::UNIT_TYPEID> addonTypes = {
				sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR,
				sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB,
				sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR,
				sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB,
				sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR,
				sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB };
			return m_bot.Buildings().getBuildingCountOfType(addonTypes);
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

			std::vector<sc2::UNIT_TYPEID> barracksAddonTypes = {
				sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR,
				sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB };
			score += m_bot.Buildings().getBuildingCountOfType(barracksAddonTypes) * 0.25f;
			std::vector<sc2::UNIT_TYPEID> factoryAddonTypes = {
				sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR,
				sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB };
			score += m_bot.Buildings().getBuildingCountOfType(factoryAddonTypes) * 0.25f;
			std::vector<sc2::UNIT_TYPEID> starportAddonTypes = {
				sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR,
				sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB };
			score += m_bot.Buildings().getBuildingCountOfType(starportAddonTypes) * 0.5f;
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

int ProductionManager::getSupplyNeedsFromProductionBuildings() const
{
	std::map<sc2::UnitTypeID, int> unitTrainingBuildingTypesWithSupplyNeeds;
	switch (m_bot.GetSelfRace())
	{
	case CCRace::Terran:
		unitTrainingBuildingTypesWithSupplyNeeds[sc2::UNIT_TYPEID::TERRAN_BARRACKS] = 1;
		unitTrainingBuildingTypesWithSupplyNeeds[sc2::UNIT_TYPEID::TERRAN_FACTORY] = 2;
		unitTrainingBuildingTypesWithSupplyNeeds[sc2::UNIT_TYPEID::TERRAN_STARPORT] = 2;
		break;
	case CCRace::Protoss:
		//TODO complete
		break;
	case CCRace::Zerg:
		//TODO complete
		break;
	}

	int supplyNeeds = 0;
	for (const auto & typePair : unitTrainingBuildingTypesWithSupplyNeeds)
	{
		const auto & producers = m_bot.GetAllyUnits(typePair.first);
		for (const auto & producer : producers)
		{
			supplyNeeds += typePair.second * (producer.getAddonTag() == 0 ? 1 : 2);
		}
	}

	return supplyNeeds;
}

void ProductionManager::clearQueue()
{
	m_queue.clearAll();
}

BuildOrderItem ProductionManager::queueAsHighestPriority(const MetaType & type, bool blocking)
{
	return m_queue.queueAsHighestPriority(type, blocking);
}

bool ProductionManager::isTechQueuedOrStarted(const MetaType & type)
{
	return isTechStarted(type) || m_queue.contains(type);
}

bool ProductionManager::isTechStarted(const MetaType & type)
{
	return std::find(incompleteUpgradesMetatypes.begin(), incompleteUpgradesMetatypes.end(), type) != incompleteUpgradesMetatypes.end()
		|| m_bot.Strategy().isUpgradeCompleted(type.getUpgrade());
}

bool ProductionManager::isTechFinished(const MetaType & type) const
{
	return m_bot.Strategy().isUpgradeCompleted(type.getUpgrade());
}

void ProductionManager::queueTech(const MetaType & type)
{
	m_queue.queueItem(BuildOrderItem(type, 0, false));
	Util::DebugLog(__FUNCTION__, "Queue " + type.getName(), m_bot);
}

void ProductionManager::validateUpgradesProgress()
{
	if(incompleteUpgrades.empty())
	{
		return;
	}

	std::vector<MetaType> toRemove;
	for (std::pair<const MetaType, Unit> & upgrade : incompleteUpgrades)
	{
		bool found = false;
		float progress = 0.f;
		auto & unit = upgrade.second;
		if (!unit.isValid() || !unit.getUnitPtr()->is_alive)
		{
			toRemove.push_back(upgrade.first);
		}

		auto unitPtr = unit.getUnitPtr();
		
		switch ((sc2::UNIT_TYPEID)upgrade.second.getType().getAPIUnitType())
		{
			case sc2::UNIT_TYPEID::TERRAN_ARMORY:
			case sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY:
			case sc2::UNIT_TYPEID::PROTOSS_FORGE:
			case sc2::UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
			case sc2::UNIT_TYPEID::ZERG_EVOLUTIONCHAMBER:
			case sc2::UNIT_TYPEID::ZERG_SPIRE:
			case sc2::UNIT_TYPEID::ZERG_GREATERSPIRE:
				if (!unitPtr->orders.empty())
				{
					progress = unitPtr->orders.at(0).progress;
					found = true;// Skip because the buildAbility is, for example, RESEARCH_TERRANSHIPWEAPONS = 3699 instead of RESEARCH_TERRANSHIPWEAPONSLEVEL1 = 861
				}
				break;

			default:
				const auto buildAbilityId = m_bot.Data(upgrade.first.getUpgrade()).buildAbility;
				for (auto & order : unitPtr->orders)
				{
					if (order.ability_id == buildAbilityId)
					{
						found = true;
						progress = order.progress;
						break;
					}
				}
		}

		if (found)
		{
			//check if upgrade is no longer progressing (cancelled)
			if (incompleteUpgradesProgress.at(upgrade.first) >= progress)
			{
				toRemove.push_back(upgrade.first);
				Util::DebugLog(__FUNCTION__, "upgrade canceled " + upgrade.first.getName(), m_bot);
			}
			else if (progress > 0.99f)//About to finish, lets consider it done.
			{
				toRemove.push_back(upgrade.first);
				Util::DebugLog(__FUNCTION__, "upgrade finished " + upgrade.first.getName(), m_bot);
			}
			else
			{
				incompleteUpgradesProgress.at(upgrade.first) = progress;
			}
		}
		else
		{
			toRemove.push_back(upgrade.first);
			Util::DebugLog(__FUNCTION__, "upgrade failed to start " + upgrade.first.getName() + ". Unit has " + (unitPtr->orders.empty() ? "no order" : "order of ID " + unitPtr->orders[0].ability_id.to_string()), m_bot);
		}
	}
	for (auto & remove : toRemove)
	{
		incompleteUpgrades.erase(remove);
		incompleteUpgradesMetatypes.remove(remove);
		incompleteUpgradesProgress.erase(remove);
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
	auto minDist = 0.f;
	
	for (auto & unit : units)
	{
		const auto distance = Util::DistSq(unit, closestTo);
		if (!closestUnit.isValid() || distance < minDist)
		{
			closestUnit = unit;
			minDist = distance;
		}
	}

    return closestUnit;
}

// this function will check to see if all preconditions are met and then create a unit
// Used to create unit/tech/buildings (when we have the ressources)
bool ProductionManager::create(const Unit & producer, BuildOrderItem & item, CCTilePosition desidredPosition, bool reserveResources, bool filterMovingWorker, bool canBePlacedElsewhere)
{
	if (!producer.isValid())
	{
		return false;
	}

	bool result = false;
	// if we're dealing with a building
	if (item.type.isBuilding())
	{
		if (item.type.getUnitType().isMorphedBuilding())
		{
			producer.morph(item.type.getUnitType());
			result = true;
		}
		else
		{
			Building b(item.type.getUnitType(), desidredPosition);
			if (ValidateBuildingTiming(b))
			{
				b.reserveResources = reserveResources;
				b.canBeBuiltElseWhere = canBePlacedElsewhere;
				result = m_bot.Buildings().addBuildingTask(b, filterMovingWorker);
			}
		}
	}
	// if we're dealing with a non-building unit
	else if (item.type.isUnit())
	{
		producer.train(item.type.getUnitType());
		result = true;
	}
	else if (item.type.isUpgrade())
	{
		const auto data = m_bot.Data(item.type.getUpgrade());
		Micro::SmartAbility(producer.getUnitPtr(), data.buildAbility, m_bot);

#if _DEBUG
		if (isTechStarted(item.type))
		{
			Util::DisplayError("Trying to start an already started upgrade.", "0x00000006", m_bot);
		}
#endif
		incompleteUpgrades.insert(std::make_pair(item.type, producer));
		incompleteUpgradesMetatypes.push_back(item.type);
		incompleteUpgradesProgress.insert(std::make_pair(item.type, 0.f));
		Util::DebugLog(__FUNCTION__, "upgrade starting " + item.type.getName(), m_bot);
		result = true;
	}

	return result;
}

// this function will check to see if all preconditions are met and then create a unit
// Used for premove
bool ProductionManager::create(const Unit & producer, Building & b, bool filterMovingWorker)
{
    if (!producer.isValid())
    {
        return false;
    }

	if (!ValidateBuildingTiming(b))
	{
		return false;
	}

    if (b.type.isMorphedBuilding())
    {
        producer.morph(b.type);
		return true;
    }

	return m_bot.Buildings().addBuildingTask(b, filterMovingWorker);
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
		Util::DisplayError("Producer of type " + producer.getType().getName() + " cannot activate its ability to produce " + type.getName(), "0x10000000", m_bot, true);
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
			oss << "Unit can't create upgrade, but should be able. Upgrade: " << type.getName() << " UnitType: " << producer.getType().getName();
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
bool ProductionManager::meetsReservedResources(const MetaType & type, int additionalReservedMineral, int additionalReservedGas)
{
	const bool meetsRequiredMinerals = m_bot.Data(type).mineralCost <= (m_bot.Strategy().isWorkerRushed() ? m_bot.GetMinerals() : m_bot.GetFreeMinerals()) - additionalReservedMineral;
	const bool meetsRequiredGas = m_bot.Data(type).gasCost <= (m_bot.Strategy().isWorkerRushed() ? m_bot.GetGas() : m_bot.GetFreeGas()) - additionalReservedGas;
	return meetsRequiredMinerals && meetsRequiredGas;
}

// return whether or not we meet resources, including building reserves
bool ProductionManager::meetsReservedResourcesWithExtra(const MetaType & type, int additionalMineral, int additionalGas, int additionalReservedMineral, int additionalReservedGas)
{
	BOT_ASSERT(!m_bot.Data(type).isAddon, "Addons cannot use extra ressources");
	const bool meetsRequiredMinerals = m_bot.Data(type).mineralCost <= (m_bot.Strategy().isWorkerRushed() ? m_bot.GetMinerals() : m_bot.GetFreeMinerals()) + additionalMineral - additionalReservedMineral;
	const bool meetsRequiredGas = m_bot.Data(type).gasCost <= (m_bot.Strategy().isWorkerRushed() ? m_bot.GetGas() : m_bot.GetFreeGas()) + additionalGas - additionalReservedGas;
	return meetsRequiredMinerals && meetsRequiredGas;
}

bool ProductionManager::canMakeAtArrival(const Building & b, const Unit & worker, int additionalReservedMineral, int additionalReservedGas)
{
	const float mineralRate = m_bot.Observation()->GetScore().score_details.collection_rate_minerals / 60.f / 16.f;
	const float gasRate = m_bot.Observation()->GetScore().score_details.collection_rate_vespene / 60.f / 16.f;

	if (mineralRate == 0 && gasRate == 0)
	{
		return false;
	}

	if (!worker.isValid())
		return false;

	//float distance = Util::PathFinding::FindOptimalPathDistance(worker.getUnitPtr(), Util::GetPosition(b.finalPosition), false, m_bot);
	float distance = Util::Dist(worker.getPosition(), Util::GetPosition(b.finalPosition));
	const float speed = 2.8125f;//Always the same for workers, Util::getSpeedOfUnit(worker.getUnitPtr(), m_bot);

	auto speedPerFrame = speed / 16.f;
	auto travelFrame = distance / speedPerFrame;
	auto mineralGain = travelFrame * mineralRate;
	auto gasGain = travelFrame * gasRate;

	if (meetsReservedResourcesWithExtra(MetaType(b.type, m_bot), mineralGain, gasGain, additionalReservedMineral, additionalReservedGas))
	{
		return true;
	}
	return false;
}

bool ProductionManager::ValidateBuildingTiming(Building & b) const
{
	if (b.type.isRefinery())
	{
		auto nextRefinery = m_bot.Buildings().getBuildingPlacer().getRefineryPosition();
		auto base = m_bot.Bases().getBaseContainingPosition(Util::GetPosition(nextRefinery), Players::Self);
		if (base == nullptr)
		{
			return false;
		}
		auto depot = base->getResourceDepot();
		if (!depot.isValid())
		{
			return false;
		}
		
		auto worker = m_bot.Workers().getClosestMineralWorkerTo(Util::GetPosition(nextRefinery));
		if (!worker.isValid())
		{
			return false;
		}

		auto depotGeyserDifferenceX = base->getDepotTilePosition().x - nextRefinery.x;
		auto depotGeyserDifferenceY = base->getDepotTilePosition().y - nextRefinery.y;

		auto offsetWorkerPos = worker.getPosition();//Offset the worker position by the difference between the depot and refinery. This should give an accurate distance between the worker and the refinery.
		offsetWorkerPos.x += depotGeyserDifferenceX;
		offsetWorkerPos.y += depotGeyserDifferenceY;

		auto workerRefineryDistance = base->getGroundDistance(offsetWorkerPos);
		float workerSpeed = Util::getSpeedOfUnit(worker.getUnitPtr(), m_bot);
		float travelTime = workerRefineryDistance / (workerSpeed * 1.45f);//Multiply the worker speed by 1.45 because moving in diagonal is 41% faster. An extra 0.04 to make sure depot and refineries don't finish the same frame.
		float progressRequired = 1 - ((21.f + travelTime) / 71.f);//Depot takes 71 seconds, refinery takes 21 seconds.

		float progress = depot.getBuildPercentage();
		if (progress < progressRequired)
		{
			//Skip building geysers at bases with a not quite built enough depot.
			return false;
		}
	}
	
	return true;
}

void ProductionManager::drawProductionInformation()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
    if (!m_bot.Config().DrawProductionInfo)
    {
        return;
    }

    std::stringstream ss;
    ss << "Production Information\n\n";

    ss << m_queue.getQueueInformation() << "\n\n";
	m_bot.Map().drawTextScreen(0.01f, 0.01f, ss.str(), CCColor(255, 255, 0));

	ss.str(std::string());
	ss << "Free Mineral:     " << m_bot.GetFreeMinerals() << "\n";
	ss << "Free Gas:         " << m_bot.GetFreeGas() << "\n";
	ss << "Gas Worker Target:" << m_bot.Workers().getGasWorkersTarget() << "\n";
	ss << "Mineral income:   " << m_bot.Observation()->GetScore().score_details.collection_rate_minerals << "\n";
	ss << "Gas income:       " << m_bot.Observation()->GetScore().score_details.collection_rate_vespene << "\n";
	m_bot.Map().drawTextScreen(0.75f, 0.05f, ss.str(), CCColor(255, 255, 0));

	ss.str(std::string());
	ss << "Being built:      \n";
	for (auto & underConstruction : m_bot.Buildings().getBuildings())
	{
		ss << underConstruction.type.getName() << "\n";
	}
	for (auto & incompleteUpgrade : incompleteUpgrades)
	{
		ss << incompleteUpgrade.first.getName() << "\n";
	}
	m_bot.Map().drawTextScreen(0.01f, 0.4f, ss.str(), CCColor(255, 255, 0));	
}
