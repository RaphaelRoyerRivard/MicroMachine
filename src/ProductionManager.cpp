#include "ProductionManager.h"
#include "Util.h"
#include "CCBot.h"

ProductionManager::ProductionManager(CCBot & bot)
    : m_bot             (bot)
    , m_buildingManager (bot)
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
    m_buildingManager.onStart();
    setBuildOrder(m_bot.Strategy().getOpeningBookBuildOrder());
}

void ProductionManager::onFrame()
{
	if (m_bot.Bases().getPlayerStartingBaseLocation(Players::Self) == nullptr)
		return;

    fixBuildOrderDeadlock();
    manageBuildOrderQueue();

    // TODO: if nothing is currently building, get a new goal from the strategy manager
    // TODO: detect if there's a build order deadlock once per second
    // TODO: triggers for game things like cloaked units etc

    m_buildingManager.onFrame();
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

	if(m_initialBuildOrderFinished && m_bot.Config().AutoCompleteBuildOrder)
    {
		putImportantBuildOrderItemsInQueue();
    }

    // the current item to be used
    BuildOrderItem currentItem = m_queue.getHighestPriorityItem();

    // while there is still something left in the queue
    while (!m_queue.isEmpty())
    {
        // this is the unit which can produce the currentItem
        Unit producer = getProducer(currentItem.type);

        // check to see if we can make it right now
        bool canMake = canMakeNow(producer, currentItem.type);

        // TODO: if it's a building and we can't make it yet, predict the worker movement to the location

        // if we can make the current item
        if (producer.isValid() && canMake)
        {
            // create it and remove it from the _queue
            create(producer, currentItem);
            m_queue.removeCurrentHighestPriorityItem();

            // don't actually loop around in here
            break;
        }
        // otherwise, if we can skip the current item
        else if (m_queue.canSkipItem())
        {
            // skip it
            m_queue.skipItem();

            // and get the next one
            currentItem = m_queue.getNextHighestPriorityItem();
        }
        else
        {
            // so break out
            break;
        }
    }
}

void ProductionManager::putImportantBuildOrderItemsInQueue()
{
	CCRace playerRace = m_bot.GetPlayerRace(Players::Self);

	// build supply if we need some
	auto supplyProvider = Util::GetSupplyProvider(playerRace, m_bot);
	auto metaTypeSupplyProvider = MetaType(supplyProvider, m_bot);
	if (!m_queue.contains(metaTypeSupplyProvider) && 
		m_bot.GetCurrentSupply() > (m_bot.GetMaxSupply() - 2 * getUnitTrainingBuildings(playerRace).size()) && 
		!m_buildingManager.isBeingBuilt(supplyProvider))
	{
		m_queue.queueAsHighestPriority(metaTypeSupplyProvider, false);
	}

	if (m_bot.Workers().getNumWorkers() * m_bot.Bases().getOccupiedBaseLocations(Players::Self).size() < 23)
	{
		auto workerType = Util::GetWorkerType(playerRace, m_bot);
		const auto metaTypeWorker = MetaType(workerType, m_bot);
		if (!m_queue.contains(metaTypeWorker))
		{
			m_queue.queueItem(BuildOrderItem(metaTypeWorker, 1, false));
		}
	}

	if (playerRace == sc2::Race::Terran)
	{
		//Continiously build marines
		/*const auto metaTypeMarine = MetaType("Marine", m_bot);
		if (!m_queue.contains(metaTypeMarine))
		{
			m_queue.queueAsLowestPriority(metaTypeMarine, false);
		}

		const auto metaTypeMarauder = MetaType("Marauder", m_bot);
		if (!m_queue.contains(metaTypeMarauder))
		{
			m_queue.queueAsLowestPriority(metaTypeMarauder, false);
		}*/

		//Build additionnal barracks
		const auto metaTypeBarrack = MetaType("Barracks", m_bot);
		if (!m_queue.contains(metaTypeBarrack) && getFreeMinerals() > metaTypeBarrack.getUnitType().mineralPrice())
		{
			m_queue.queueAsLowestPriority(metaTypeBarrack, false);
		}

		const auto metaTypeReaper = MetaType("Reaper", m_bot);
		if (!m_queue.contains(metaTypeReaper))
		{
			m_queue.queueAsLowestPriority(metaTypeReaper, false);
		}
	}
}

void ProductionManager::fixBuildOrderDeadlock()
{
    if (m_queue.isEmpty()) { return; }
    BuildOrderItem & currentItem = m_queue.getHighestPriorityItem();

    // check to see if we have the prerequisites for the topmost item
    bool hasRequired = m_bot.Data(currentItem.type).requiredUnits.empty();
    for (auto & required : m_bot.Data(currentItem.type).requiredUnits)
    {
        if (m_bot.UnitInfo().getUnitTypeCount(Players::Self, required, false) > 0 || m_buildingManager.isBeingBuilt(required))
        {
            hasRequired = true;
            break;
        }
    }

    if (!hasRequired)
    {
        std::cout << currentItem.type.getName() << " needs " << m_bot.Data(currentItem.type).requiredUnits[0].getName() << "\n";
        m_queue.queueAsHighestPriority(MetaType(m_bot.Data(currentItem.type).requiredUnits[0], m_bot), true);
        fixBuildOrderDeadlock();
        return;
    }

    // build the producer of the unit if we don't have one
    bool hasProducer = m_bot.Data(currentItem.type).whatBuilds.empty();
    for (auto & producer : m_bot.Data(currentItem.type).whatBuilds)
    {
        if (m_bot.UnitInfo().getUnitTypeCount(Players::Self, producer, false) > 0 || m_buildingManager.isBeingBuilt(producer))
        {
            hasProducer = true;
            break;
        }
    }

    if (!hasProducer)
    {
        // si on veut faire un worker et qu'on n'a plus de worker et qu'on n'a plus de main building, GG
        bool noMoreWorker = m_bot.Workers().getNumWorkers() == 0;
        if (currentItem.type.getUnitType().isWorker() && noMoreWorker)
        {
            // We no longer have worker and no longer have buildings to do more, so we are rip...
            return;
        }
        m_queue.queueAsHighestPriority(MetaType(m_bot.Data(currentItem.type).whatBuilds[0], m_bot), true);
        fixBuildOrderDeadlock();
    }

    // build a refinery if we don't have one and the thing costs gas
    auto refinery = Util::GetRefinery(m_bot.GetPlayerRace(Players::Self), m_bot);
    if (m_bot.Data(currentItem.type).gasCost > 0 && m_bot.UnitInfo().getUnitTypeCount(Players::Self, refinery, false) == 0)
    {
        m_queue.queueAsHighestPriority(MetaType(refinery, m_bot), true);
    }

    // build supply if we need some
    auto supplyProvider = Util::GetSupplyProvider(m_bot.GetPlayerRace(Players::Self), m_bot);
    if (m_bot.Data(currentItem.type).supplyCost > (m_bot.GetMaxSupply() - m_bot.GetCurrentSupply()) && !m_buildingManager.isBeingBuilt(supplyProvider))
    {
        m_queue.queueAsHighestPriority(MetaType(supplyProvider, m_bot), true);
    }
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
        if (m_bot.Data(unit).isBuilding && unit.isTraining()) { continue; }

        // TODO: if unit is not powered continue
        // TODO: if the type is an addon, some special cases
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

        // if we haven't cut it, add it to the set of candidates
        candidateProducers.push_back(unit);
    }

    return getClosestUnitToPosition(candidateProducers, closestTo);
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
		unitTrainingBuildingTypes.insert(sc2::UNIT_TYPEID::TERRAN_GHOSTACADEMY);
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

Unit ProductionManager::getClosestUnitToPosition(const std::vector<Unit> & units, CCPosition closestTo) const
{
    if (units.size() == 0)
    {
        return Unit();
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
void ProductionManager::create(const Unit & producer, BuildOrderItem & item)
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
            m_buildingManager.addBuildingTask(item.type.getUnitType(), Util::GetTilePosition(m_bot.GetStartLocation()));
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
    return m_bot.GetMinerals() - m_buildingManager.getReservedMinerals();
}

int ProductionManager::getFreeGas()
{
    return m_bot.GetGas() - m_buildingManager.getReservedGas();
}

// return whether or not we meet resources, including building reserves
bool ProductionManager::meetsReservedResources(const MetaType & type)
{
    // return whether or not we meet the resources
    int minerals = m_bot.Data(type).mineralCost;
    int gas = m_bot.Data(type).gasCost;

    return (m_bot.Data(type).mineralCost <= getFreeMinerals()) && (m_bot.Data(type).gasCost <= getFreeGas());
}

void ProductionManager::drawProductionInformation()
{
    if (!m_bot.Config().DrawProductionInfo)
    {
        return;
    }

    std::stringstream ss;
    ss << "Production Information\n\n";

    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        if (unit.isBeingConstructed())
        {
            //ss << sc2::UnitTypeToName(unit.unit_type) << " " << unit.build_progress << "\n";
        }
    }

    ss << m_queue.getQueueInformation();

    m_bot.Map().drawTextScreen(0.01f, 0.01f, ss.str(), CCColor(255, 255, 0));
}
