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
}

void ProductionManager::onFrame()
{
	if (m_bot.Bases().getPlayerStartingBaseLocation(Players::Self) == nullptr)
		return;

    manageBuildOrderQueue();

    // TODO: if nothing is currently building, get a new goal from the strategy manager
    // TODO: detect if there's a build order deadlock once per second
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

	if((m_initialBuildOrderFinished && m_bot.Config().AutoCompleteBuildOrder) || m_bot.Strategy().isWorkerRushed())
    {
		putImportantBuildOrderItemsInQueue();
    }

    // the current item to be used
    BuildOrderItem currentItem = m_queue.getHighestPriorityItem();

    // while there is still something left in the queue
    while (!m_queue.isEmpty())
    {
		//check if we have the prerequirements.
		if (!hasRequired(currentItem.type, true) || !hasProducer(currentItem.type, true))
		{
			fixBuildOrderDeadlock(currentItem);
			currentItem = m_queue.getHighestPriorityItem();
			continue;
		}

		if (currentlyHasRequirement(currentItem.type))
		{
			// this is the unit which can produce the currentItem
			Unit producer = getProducer(currentItem.type);

			// TODO: if it's a building and we can't make it yet, predict the worker movement to the location

			//Build supply depot at ramp against protoss
			if (m_bot.Observation()->GetFoodCap() <= 15 && currentItem.type == MetaTypeEnum::SupplyDepot && m_bot.GetPlayerRace(Players::Enemy) == CCRace::Protoss &&
				m_bot.Observation()->GetGameLoop() > 5 && getFreeMinerals() > 30)
			{
				const CCPosition centerMap(m_bot.Map().width() / 2, m_bot.Map().height() / 2);
				if (!rampSupplyDepotWorker.isValid())
				{
					rampSupplyDepotWorker = m_bot.Workers().getClosestMineralWorkerTo(centerMap);
				}
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

			// if we can make the current item
			if (producer.isValid() && canMakeNow(producer, currentItem.type))
			{
				// create it and remove it from the _queue
				create(producer, currentItem);
				m_queue.removeCurrentHighestPriorityItem();

				// don't actually loop around in here
				break;
			}

			// is a building (doesn't include addons, because no travel time) and we can make it soon
			if (producer.isValid() && m_bot.Data(currentItem.type).isBuilding && !m_bot.Data(currentItem.type).isAddon && !currentItem.type.getUnitType().isMorphedBuilding() && canMakeSoon(producer, currentItem.type))
			{
				Building b(currentItem.type.getUnitType(), Util::GetTilePosition(m_bot.GetStartLocation()));
				CCTilePosition targetLocation = m_bot.Buildings().getBuildingLocation(b);
				Unit worker = m_bot.Workers().getClosestMineralWorkerTo(CCPosition{ static_cast<float>(targetLocation.x), static_cast<float>(targetLocation.y) });
				worker.move(targetLocation);

				// create it and remove it from the _queue
				create(producer, currentItem);
				m_queue.removeCurrentHighestPriorityItem();

				// don't actually loop around in here
				break;
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
}

void ProductionManager::putImportantBuildOrderItemsInQueue()
{
	CCRace playerRace = m_bot.GetPlayerRace(Players::Self);
	const float productionScore = getProductionScore();;// +getProductionScoreInQueue();
	const auto productionBuildingCount = getProductionBuildingsCount();
	const auto productionBuildingAddonCount = getProductionBuildingsAddonsCount();
	const auto baseCount = m_bot.Bases().getBaseCount(Players::Self);

	// build supply if we need some
	auto supplyProvider = Util::GetSupplyProvider(playerRace, m_bot);
	auto metaTypeSupplyProvider = MetaType(supplyProvider, m_bot);
	if(m_bot.GetCurrentSupply() + 1.75 * getUnitTrainingBuildings(playerRace).size() + baseCount > m_bot.GetMaxSupply() + m_bot.Buildings().countBeingBuilt(supplyProvider) * 8 && !m_queue.contains(metaTypeSupplyProvider))
	{
		m_queue.queueAsHighestPriority(metaTypeSupplyProvider, false);
	}

	if (playerRace == sc2::Race::Terran)
	{
		// Logic for building Orbital Commands and Refineries
		if(m_ccShouldBeInQueue && !m_queue.contains(MetaTypeEnum::CommandCenter) && !m_bot.Buildings().isBeingBuilt(MetaTypeEnum::CommandCenter.getUnitType()) && !m_queue.contains(MetaTypeEnum::OrbitalCommand))
		{
			m_queue.queueAsLowestPriority(MetaTypeEnum::OrbitalCommand, false);

			m_queue.queueAsLowestPriority(MetaTypeEnum::Refinery, false);
			m_queue.queueAsLowestPriority(MetaTypeEnum::Refinery, false);
			m_ccShouldBeInQueue = false;
		}

		// Logic for building Command Centers
		if (baseCount <= productionScore)
		{
			if (!m_ccShouldBeInQueue && !m_queue.contains(MetaTypeEnum::CommandCenter) && !m_queue.contains(MetaTypeEnum::OrbitalCommand))
			{
				m_queue.queueAsLowestPriority(MetaTypeEnum::CommandCenter, false);
				m_ccShouldBeInQueue = true;
			}
		}

		// Strategy base logic
		int currentStrategy = m_bot.Strategy().getCurrentStrategyPostBuildOrder();
		switch (currentStrategy)
		{
			case StrategyPostBuildOrder::TERRAN_REAPER :
			{
				if (productionScore < (float)baseCount)
				{
					bool hasPicked = false;
					MetaType toBuild;
					if (productionBuildingAddonCount < productionBuildingCount)
					{//Addon
						//int barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true);
						//int barracksReactorCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::BarracksReactor.getUnitType(), false, true);
						int starportCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Starport.getUnitType(), false, true);
						int starportTechLabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportTechLab.getUnitType(), false, true);
						/*if (barracksCount > barracksReactorCount)
						{
							toBuild = MetaTypeEnum::BarracksReactor;
							hasPicked = true;
						}
						else*/ if (starportCount > starportTechLabCount)
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
						int barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true);
						int factoryCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Factory.getUnitType(), false, true);
						int starportCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Starport.getUnitType(), false, true);
						if (barracksCount >= (starportCount + 1) * 2)
						{
							toBuild = MetaTypeEnum::Starport;
							hasPicked = true;
						}
						else
						{
							toBuild = MetaTypeEnum::Barracks;
							hasPicked = true;
						}

						if (hasPicked && !m_queue.contains(toBuild))
						{
							m_queue.queueAsLowestPriority(toBuild, false);
						}
					}
				}

				if (!m_queue.contains(MetaTypeEnum::Reaper))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Reaper, 0, false));
				}

				if (!m_queue.contains(MetaTypeEnum::Banshee) && m_bot.Buildings().getBuildingCountOfType(sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB) > 0)
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Banshee, 0, false));
				}
				break;
			}
			case StrategyPostBuildOrder::TERRAN_ANTI_SPEEDLING :
			{
				if (!m_queue.contains(MetaTypeEnum::InfernalPreIgniter) && std::find(startedUpgrades.begin(), startedUpgrades.end(), MetaTypeEnum::InfernalPreIgniter) == startedUpgrades.end())
				{
					m_queue.queueAsLowestPriority(MetaTypeEnum::InfernalPreIgniter, false);
					startedUpgrades.push_back(MetaTypeEnum::InfernalPreIgniter);
				}

				const auto metaTypeFactory = MetaType("Factory", m_bot);
				if (productionBuildingCount < baseCount && !m_queue.contains(MetaTypeEnum::Factory))
				{
					m_queue.queueAsLowestPriority(MetaTypeEnum::Factory, false);
				}

				if (!m_queue.contains(MetaTypeEnum::Hellion))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Hellion, 0, false));
				}

				if (!m_queue.contains(MetaTypeEnum::Reaper))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Reaper, 0, false));
				}
				break;
			}
			case StrategyPostBuildOrder::TERRAN_MARINE_MARAUDER :
			{
				if (productionBuildingCount < baseCount && !m_queue.contains(MetaTypeEnum::Barracks))
				{
					m_queue.queueAsLowestPriority(MetaTypeEnum::Barracks, false);
				}

				if (!m_queue.contains(MetaTypeEnum::Marine))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marine, 0, false));
				}

				if (!m_queue.contains(MetaTypeEnum::Marauder))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Marauder, 0, false));
				}
				break;
			}
			case StrategyPostBuildOrder::WORKER_RUSH_DEFENSE:
			{
				if (m_queue.getHighestPriorityItem().type.getUnitType().getAPIUnitType() != MetaTypeEnum::Reaper.getUnitType().getAPIUnitType())
				{
					//Queue has highest, there is nothing else we want.
					m_queue.queueAsHighestPriority(MetaTypeEnum::Reaper, true);
					return;
				}
				break;
			}
			case StrategyPostBuildOrder::TERRAN_ANTI_ADEPT:
			{
				if (productionScore < (float)baseCount)
				{
					bool hasPicked = false;
					MetaType toBuild;
					if (productionBuildingAddonCount < productionBuildingCount)
					{//Addon
						int starportCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Starport.getUnitType(), false, true);
						int starportTechLabCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportTechLab.getUnitType(), false, true);
						int starportReactorCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::StarportReactor.getUnitType(), false, true);
						if (starportCount > starportTechLabCount + starportReactorCount)
						{
							if (starportTechLabCount <= starportReactorCount * 2)
							{
								toBuild = MetaTypeEnum::StarportTechLab;
								hasPicked = true;
							}
							else
							{
								toBuild = MetaTypeEnum::StarportReactor;
								hasPicked = true;
							}
						}

						if (hasPicked && !m_queue.contains(toBuild))
						{
							m_queue.queueItem(BuildOrderItem(toBuild, 1, false));
						}
					}
					if (!hasPicked)
					{//Building
						toBuild = MetaTypeEnum::Starport;
						hasPicked = true;

						if (hasPicked && !m_queue.contains(toBuild))
						{
							m_queue.queueAsLowestPriority(toBuild, false);
						}
					}
				}

				int bansheeCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Banshee.getUnitType(), false, true);
				int vikingCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Viking.getUnitType(), false, true);

				if(bansheeCount + vikingCount >= 5)
				{
					auto metaTypeShipWeapon = getUpgradeMetaType(MetaTypeEnum::TerranShipWeaponsLevel1);
					if (std::find(startedUpgrades.begin(), startedUpgrades.end(), metaTypeShipWeapon) == startedUpgrades.end())
					{
						m_queue.queueAsLowestPriority(metaTypeShipWeapon, false);
						startedUpgrades.push_back(metaTypeShipWeapon);
					}
				}

				if (!m_queue.contains(MetaTypeEnum::Banshee))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Banshee, 0, false));
				}

				if (m_bot.Strategy().shouldProduceAntiAir() && !m_queue.contains(MetaTypeEnum::Viking))
				{
					if (vikingCount < 2 * bansheeCount)
					{
						m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Viking, 0, false));
					}
				}

				if (!m_queue.contains(MetaTypeEnum::Reaper))
				{
					m_queue.queueItem(BuildOrderItem(MetaTypeEnum::Reaper, 0, false));
				}
				break;
			}
			default:
			{
				assert("This strategy doesn't exist.");
			}
		}
	}

	const int maxWorkers = (23 + 2) * 3;	//23 resource workers + 2 builders per base, maximum of 3 bases.
	if (m_bot.Workers().getNumWorkers() < maxWorkers && !m_queue.contains(MetaTypeEnum::OrbitalCommand))//baseCount * 23
	{
		auto workerType = Util::GetWorkerType(playerRace, m_bot);
		const auto metaTypeWorker = MetaType(workerType, m_bot);
		if (!m_queue.contains(metaTypeWorker))
		{
			m_queue.queueItem(BuildOrderItem(metaTypeWorker, 2, false));
		}
	}
}

void ProductionManager::fixBuildOrderDeadlock(BuildOrderItem & item)
{
	const TypeData& typeData = m_bot.Data(item.type);

	// check to see if we have the prerequisites for the item
    if (!hasRequired(item.type, true))
    {
        std::cout << item.type.getName() << " needs " << typeData.requiredUnits[0].getName() << "\n";
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
		BuildOrderItem producerItem = m_queue.queueItem(BuildOrderItem(MetaType(typeData.whatBuilds[0], m_bot), 0, item.blocking));
        fixBuildOrderDeadlock(producerItem);
    }

    // build a refinery if we don't have one and the thing costs gas
    auto refinery = Util::GetRefinery(m_bot.GetPlayerRace(Players::Self), m_bot);
    if (typeData.gasCost > 0 && m_bot.UnitInfo().getUnitTypeCount(Players::Self, refinery, false, true) == 0)
    {
        m_queue.queueAsHighestPriority(MetaType(refinery, m_bot), true);
    }

    // build supply if we need some
    auto supplyProvider = Util::GetSupplyProvider(m_bot.GetPlayerRace(Players::Self), m_bot);
    if (typeData.supplyCost > m_bot.GetMaxSupply() - m_bot.GetCurrentSupply() && !m_bot.Buildings().isBeingBuilt(supplyProvider))
    {
        m_queue.queueAsHighestPriority(MetaType(supplyProvider, m_bot), true);
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

    // make a set of all candidate producers
    std::vector<Unit> candidateProducers;
    for (auto unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        // reasons a unit can not train the desired type
        if (std::find(producerTypes.begin(), producerTypes.end(), unit.getType()) == producerTypes.end()) { continue; }
        if (!unit.isCompleted()) { continue; }
		if (unit.isFlying()) { continue; }

		bool isBuilding = m_bot.Data(unit).isBuilding;
		if (isBuilding && unit.isTraining() && unit.getAddonTag() == 0) { continue; }
		if (isBuilding && m_bot.GetPlayerRace(Players::Self) == CCRace::Terran)
		{//If is terran, check for Reactor addon
			sc2::Tag addonTag = unit.getAddonTag();
			if (addonTag != 0 && unit.isTraining())
			{
				bool addonIsReactor = false;
				for (auto unit : m_bot.UnitInfo().getUnits(Players::Self))
				{
					switch ((sc2::UNIT_TYPEID)unit.getAPIUnitType())
					{
						case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
						case sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR:
						case sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR:
						{
							if (unit.getTag() == addonTag)
							{
								addonIsReactor = true;
								break;
							}
						}
					}
				}
				if (unit.isAddonTraining() || !addonIsReactor)
				{//skip, Techlab can't build two units or reactor already has two.
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
		if (m_bot.Data(type).isAddon)
		{
			//validate space next to building is reserved
			auto addonPosition = CCTilePosition(unit.getTilePosition().x + 2, unit.getTilePosition().y);
			Building b(type.getUnitType(), addonPosition);
			if (m_bot.Buildings().getBuildingPlacer().canBuildHere(unit.getTilePosition().x + 2, unit.getTilePosition().y, b))
			{//Tiles are not reserved, since addon logic is reversed, this means we can't build here, there should be an addon already
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
	switch (m_bot.GetPlayerRace(Players::Self))
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
	switch (m_bot.GetPlayerRace(Players::Self))
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
	switch (m_bot.GetPlayerRace(Players::Self))
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
	
	switch (m_bot.GetPlayerRace(Players::Self))
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

MetaType ProductionManager::getUpgradeMetaType(const MetaType type) const
{
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

	std::list<std::list<MetaType>> upgrades;
	switch (m_bot.GetPlayerRace(Players::Self))
	{
		case CCRace::Terran:
		{
			upgrades = terranUpgrades;
			break;
		}
		case CCRace::Protoss:
		{
			upgrades = protossUpgrades;
			break;
		}
		case CCRace::Zerg:
		{
			upgrades = zergUpgrades;
			break;
		}
	}

	bool found = false;
	for (auto upCategory : upgrades)
	{
		for (auto upgrade : upCategory)
		{
			if (found)
			{
				return upgrade;
			}
			if (upgrade.getName().compare(type.getName()) == 0)//Equals
			{
				bool started = false;
				for (auto startedUpgrade : startedUpgrades)//If startedUpgrades.contains
				{
					if (startedUpgrade.getName().compare(type.getName()) == 0)
					{
						started = true;
						break;
					}
				}
				if (!started)//if not started, return it.
				{
					return type;
				}
				//if started, return the next one.
				found = true;
			}
		}
		if (found)
		{//Found, but it was level 3.
			assert("Upgrade level 4 doesn't exist.");
		}
	}
	assert("Upgrade wasn't found.");

	return {};
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
        double distance = Util::Dist(unit, closestTo);
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
			if (position.x == 0 && position.y == 0)
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
    }
}

bool ProductionManager::canMakeNow(const Unit & producer, const MetaType & type)
{
    if (!producer.isValid() || !meetsReservedResources(type))
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
		if (type.isUpgrade())//TODO Not safe, is a fix for upgrades having the wrong ID
		{
			return true;
		}
		for (const sc2::AvailableAbility & available_ability : available_abilities.abilities)
		{
			if (available_ability.ability_id == MetaTypeAbility)
			{
				return true;
			}
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

bool ProductionManager::canMakeSoon(const Unit & producer, const MetaType & type)
{
	if (!producer.isValid() || !meetsReservedResourcesWithExtra(type))
	{
		return false;
	}

	return true;//Do not check the builder abilities, it won't contain the building we want.
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
	assert("Addons cannot use extra ressources",m_bot.Data(type).isAddon);
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

    /*for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        if (unit.isBeingConstructed())
        {
            ss << sc2::UnitTypeToName(unit.unit_type) << " " << unit.build_progress << "\n";
        }
    }*/

    ss << m_queue.getQueueInformation() << "\n";
	ss << "Free Mineral:     " << getFreeMinerals() << "\n";
	ss << "Free Gas:         " << getFreeGas();

    m_bot.Map().drawTextScreen(0.01f, 0.01f, ss.str(), CCColor(255, 255, 0));
}
