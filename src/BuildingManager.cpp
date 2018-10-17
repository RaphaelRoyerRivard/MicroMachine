#include <algorithm>
#include "Common.h"
#include "BuildingManager.h"
#include "CCBot.h"
#include "Util.h"

BuildingManager::BuildingManager(CCBot & bot)
    : m_bot(bot)
    , m_buildingPlacer(bot)
    , m_debugMode(false)
    , m_reservedMinerals(0)
    , m_reservedGas(0)
{

}

void BuildingManager::onStart()
{
    m_buildingPlacer.onStart();
}

// gets called every frame from GameCommander
void BuildingManager::onFrame()
{
	updateBaseBuildings();
    validateWorkersAndBuildings();          // check to see if assigned workers have died en route or while constructing
    assignWorkersToUnassignedBuildings();   // assign workers to the unassigned buildings and label them 'planned'    
    constructAssignedBuildings();           // for each planned building, if the worker isn't constructing, send the command    
    checkForStartedConstruction();          // check to see if any buildings have started construction and update data structures    
    checkForDeadTerranBuilders();           // if we are terran and a building is under construction without a worker, assign a new one
    checkForCompletedBuildings();           // check to see if any buildings have completed and update data structures
	castBuildingsAbilities();

    drawBuildingInformation();
}

bool BuildingManager::isBeingBuilt(UnitType type)
{
    for (auto & b : m_buildings)
    {
        if (b.type == type)
        {
            return true;
        }
    }

    return false;
}

int BuildingManager::countBeingBuilt(UnitType type)
{
	int count = 0;
	for (auto & b : m_buildings)
	{
		if (b.type == type)
		{
			count++;
		}
	}

	return count;
}

// STEP 1: DO BOOK KEEPING ON WORKERS WHICH MAY HAVE DIED
void BuildingManager::validateWorkersAndBuildings()
{
    std::vector<Building> toRemove;

    // find any buildings which have become obsolete
    for (auto & b : m_buildings)
    {
        if (b.status != BuildingStatus::UnderConstruction)
        {
            continue;
        }

        auto buildingUnit = b.buildingUnit;

        // TODO: || !b.buildingUnit->getType().isBuilding()
        if (!buildingUnit.isValid())
        {
            toRemove.push_back(b);
        }
    }

    removeBuildings(toRemove);
}

// STEP 2: ASSIGN WORKERS TO BUILDINGS WITHOUT THEM
void BuildingManager::assignWorkersToUnassignedBuildings()
{
    // for each building that doesn't have a builder, assign one
    for (Building & b : m_buildings)
    {
        if (b.status != BuildingStatus::Unassigned)//b.buildingUnit.isBeingConstructed()//|| b.underConstruction)
        {
            continue;
        }

        BOT_ASSERT(!b.builderUnit.isValid(), "Error: Tried to assign a builder to a building that already had one ");

        if (m_debugMode) { printf("Assigning Worker To: %s", b.type.getName().c_str()); }

		if (b.type.isAddon())
		{
			
			MetaType addonType = MetaType(b.type, m_bot);
			Unit producer = m_bot.Commander().Production().getProducer(addonType);
			if (!producer.isValid())
			{
				continue;
			}
			b.builderUnit = producer;
			b.finalPosition = Util::GetTilePosition(producer.getPosition());
		}
		else
		{
			// grab a worker unit from WorkerManager which is closest to this final position
			CCTilePosition testLocation = getBuildingLocation(b);

			// Don't test the location if the building is already started
			if (!b.underConstruction && (!m_bot.Map().isValidTile(testLocation) || (testLocation.x == 0 && testLocation.y == 0)))
			{
				continue;
			}

			b.finalPosition = testLocation;

			// grab the worker unit from WorkerManager which is closest to this final position
			Unit builderUnit = m_bot.Workers().getBuilder(b);
			b.builderUnit = builderUnit;
			if (!b.builderUnit.isValid())
			{
				continue;
			}

			if (!b.underConstruction)
			{
				// reserve this building's space
				m_buildingPlacer.reserveTiles((int)b.finalPosition.x, (int)b.finalPosition.y, b.type.tileWidth(), b.type.tileHeight());

				if (m_bot.GetSelfRace() == CCRace::Terran)
				{
					sc2::UNIT_TYPEID type = b.type.getAPIUnitType();
					switch (type)
					{
						case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
						case sc2::UNIT_TYPEID::TERRAN_FACTORY:
						{
							m_buildingPlacer.reserveTiles((int)b.finalPosition.x, (int)b.finalPosition.y - 1, 3, 1);
						}
						case sc2::UNIT_TYPEID::TERRAN_STARPORT:
						{
							m_buildingPlacer.reserveTiles((int)b.finalPosition.x + 3, (int)b.finalPosition.y, 2, 2);
						}
					}
				}
			}
		}

        b.status = BuildingStatus::Assigned;
    }
}

// STEP 3: ISSUE CONSTRUCTION ORDERS TO ASSIGN BUILDINGS AS NEEDED
void BuildingManager::constructAssignedBuildings()
{
    for (auto & b : m_buildings)
    {
        if (b.status != BuildingStatus::Assigned)
        {
            continue;
        }

        // TODO: not sure if this is the correct way to tell if the building is constructing
        //sc2::AbilityID buildAbility = m_bot.Data(b.type).buildAbility;
        Unit builderUnit = b.builderUnit;

        bool isConstructing = false;

        // if we're zerg and the builder unit is null, we assume it morphed into the building
        if (Util::IsZerg(m_bot.GetSelfRace()))
        {
            if (!builderUnit.isValid())
            {
                isConstructing = true;
            }
        }
        else
        {
            BOT_ASSERT(builderUnit.isValid(), "null builder unit");

            isConstructing = builderUnit.isConstructing(b.type);
        }

        // if that worker is not currently constructing
        if (!isConstructing)
        {
            // if we haven't explored the build position, go there
            if (!isBuildingPositionExplored(b))
            {
                builderUnit.move(b.finalPosition);
            }
            // if this is not the first time we've sent this guy to build this
            // it must be the case that something was in the way of building
            else if (b.buildCommandGiven && b.underConstruction && b.buildingUnit.isBeingConstructed())
            {
                // TODO: in here is where we would check to see if the builder died on the way
                //       or if things are taking too long, or the build location is no longer valid
                // If the building has been started and no more worker are on it.

                b.builderUnit.rightClick(b.buildingUnit);
				b.status = BuildingStatus::UnderConstruction;
            }
            else
            {
                // if it's a refinery, the build command has to be on the geyser unit tag
                if (b.type.isRefinery())
                {
                    // first we find the geyser at the desired location
                    Unit geyser;
                    for (auto unit : m_bot.GetUnits())
                    {
                        if (unit.getType().isGeyser() && Util::Dist(Util::GetPosition(b.finalPosition), unit.getPosition()) < 3)
						{
						geyser = unit;
						break;
						}
					}

					if (geyser.isValid())
					{
						b.builderUnit.buildTarget(b.type, geyser);
					}
					else
					{
						std::cout << "WARNING: NO VALID GEYSER UNIT FOUND TO BUILD ON, SKIPPING REFINERY\n";
					}
				}
				// if it's not a refinery, we build right on the position
				else
				{
					if (b.type.isAddon())
					{
						//Pick the right type of addon to build
						MetaType addonMetatype;
						switch ((sc2::UNIT_TYPEID)b.type.getAPIUnitType())
						{
							case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
							case sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR:
							case sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR:
							{
								addonMetatype = MetaTypeEnum::Reactor;
								break;
							}
							case sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB:
							case sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB:
							case sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB:
							{
								addonMetatype = MetaTypeEnum::TechLab;
								break;
							}
						}
						b.builderUnit.build(b.type, b.finalPosition);
					}
					else
					{
						b.builderUnit.build(b.type, b.finalPosition);
					}
				}

				// set the flag to true
				b.buildCommandGiven = true;
			}
		}
	}
}

// STEP 4: UPDATE DATA STRUCTURES FOR BUILDINGS STARTING CONSTRUCTION
void BuildingManager::checkForStartedConstruction()
{
	// for each building unit which is being constructed
	for (auto buildingStarted : m_bot.UnitInfo().getUnits(Players::Self))
	{
		// filter out units which aren't buildings under construction
		if (!buildingStarted.getType().isBuilding() || !buildingStarted.isBeingConstructed())
		{
			continue;
		}

		// check all our building status objects to see if we have a match and if we do, update it

		for (auto & b : m_buildings)
		{
			if (b.status != BuildingStatus::Assigned)
			{
				continue;
			}
			auto type = b.type;

			// check if the positions match
			int addonOffset = (type.isAddon() ? 3 : 0);
			int dx = b.finalPosition.x + addonOffset - buildingStarted.getTilePosition().x;
			int dy = b.finalPosition.y - buildingStarted.getTilePosition().y;

			if (dx * dx + dy * dy < Util::TileToPosition(1.0f))
			{
				if (b.buildingUnit.isValid())
				{
					std::cout << "Building mis-match somehow\n";
					continue;
				}

				// the resources should now be spent, so unreserve them
				m_reservedMinerals -= buildingStarted.getType().mineralPrice();
				m_reservedGas -= buildingStarted.getType().gasPrice();

                // flag it as started and set the buildingUnit
                b.underConstruction = true;
                b.buildingUnit = buildingStarted;

                // if we are zerg, the buildingUnit now becomes nullptr since it's destroyed
                if (Util::IsZerg(m_bot.GetSelfRace()))
                {
                    b.builderUnit = Unit();
                }
                else if (Util::IsProtoss(m_bot.GetSelfRace()))
                {
                    m_bot.Workers().finishedWithWorker(b.builderUnit);
                    b.builderUnit = Unit();
                }

                // put it in the under construction vector
                b.status = BuildingStatus::UnderConstruction;

				//Check for invalid data
				if (type.tileWidth() > 5 || type.tileHeight() > 5 || type.tileWidth() < 1 || type.tileHeight() < 1)
				{
					std::cout << "Invalid size for free space " << type.tileWidth() << " x " << type.tileHeight() << "\n";
				}
				else
				{
					// free this space
					m_buildingPlacer.freeTiles((int)b.finalPosition.x + addonOffset, (int)b.finalPosition.y, type.tileWidth(), type.tileHeight());
				}

                // only one building will match
                break;
            }
        }
    }
}

// STEP 5: IF WE ARE TERRAN, THIS MATTERS, SO: LOL
void BuildingManager::checkForDeadTerranBuilders() 
{
    if (!Util::IsTerran(m_bot.GetSelfRace()))
        return;

    // for each of our buildings under construction
    for (auto & b : m_buildings)
    {
		// if the building has a builder that died or that is not a builder anymore because of a bug
		if (b.builderUnit.isValid() && (!b.builderUnit.isAlive() || m_bot.Workers().getWorkerData().getWorkerJob(b.builderUnit) != WorkerJobs::Build))
		{
			// grab the worker unit from WorkerManager which is closest to this final position
			Unit builderUnit = m_bot.Workers().getBuilder(b);
			if (builderUnit.isValid())
			{
				b.builderUnit = builderUnit;
				b.status = BuildingStatus::Assigned;
			}
		}
    }
}

// STEP 6: CHECK FOR COMPLETED BUILDINGS
void BuildingManager::checkForCompletedBuildings()
{
    std::vector<Building> toRemove;

    // for each of our buildings under construction
    for (auto & b : m_buildings)
    {
        if (b.status != BuildingStatus::UnderConstruction)
        {
            continue;
        }

        // if the unit has completed
        if (b.buildingUnit.isCompleted())
        {
            // if we are terran, give the worker back to worker manager
            if (Util::IsTerran(m_bot.GetSelfRace()))
            {
                m_bot.Workers().finishedWithWorker(b.builderUnit);
				if (b.buildingUnit.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)
				{
					Micro::SmartAbility(b.buildingUnit.getUnitPtr(), sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER, m_bot);
				}
            }

            // remove this unit from the under construction vector
            toRemove.push_back(b);
        }
    }

    removeBuildings(toRemove);
}

// add a new building to be constructed
void BuildingManager::addBuildingTask(const UnitType & type, const CCTilePosition & desiredPosition)
{
    m_reservedMinerals += m_bot.Data(type).mineralCost;
    m_reservedGas += m_bot.Data(type).gasCost;

    Building b(type, desiredPosition);
    b.status = BuildingStatus::Unassigned;
	
    m_buildings.push_back(b);
}

bool BuildingManager::isConstructingType(const UnitType & type)
{
	for (auto building : m_buildings)
	{
		if (building.type == type)
		{
			return true;
		}
	}
	return false;
}

// TODO: may need to iterate over all tiles of the building footprint
bool BuildingManager::isBuildingPositionExplored(const Building & b) const
{
    return m_bot.Map().isExplored(b.finalPosition);
}


char BuildingManager::getBuildingWorkerCode(const Building & b) const
{
    return b.builderUnit.isValid() ? 'W' : 'X';
}

int BuildingManager::getReservedMinerals()
{
    return m_reservedMinerals;
}

int BuildingManager::getReservedGas()
{
    return m_reservedGas;
}

void BuildingManager::drawBuildingInformation()
{
    m_buildingPlacer.drawReservedTiles();

    if (!m_bot.Config().DrawBuildingInfo)
    {
        return;
    }

    std::stringstream ss;
    ss << "Building Information " << m_buildings.size() << "\n\n\n";

    int yspace = 0;

    for (const auto & b : m_buildings)
    {
        std::stringstream dss;

        if (b.builderUnit.isValid())
        {
            dss << "\n\nBuilder: " << b.builderUnit.getID() << "\n";
        }

        if (b.buildingUnit.isValid())
        {
            dss << "Building: " << b.buildingUnit.getID() << "\n" << b.buildingUnit.getBuildPercentage();
            m_bot.Map().drawText(b.buildingUnit.getPosition(), dss.str());
        }
        
        if (b.status == BuildingStatus::Unassigned)
        {
            ss << "Unassigned " << b.type.getName() << "    " << getBuildingWorkerCode(b) << "\n";
        }
        else if (b.status == BuildingStatus::Assigned)
        {
            ss << "Assigned " << b.type.getName() << "    " << b.builderUnit.getID() << " " << getBuildingWorkerCode(b) << " (" << b.finalPosition.x << "," << b.finalPosition.y << ")\n";

            int x1 = b.finalPosition.x;
            int y1 = b.finalPosition.y;
            int x2 = b.finalPosition.x + b.type.tileWidth();
            int y2 = b.finalPosition.y + b.type.tileHeight();

            m_bot.Map().drawBox((CCPositionType)x1, (CCPositionType)y1, (CCPositionType)x2, (CCPositionType)y2, CCColor(255, 0, 0));
            //m_bot.Map().drawLine(b.finalPosition, m_bot.GetUnit(b.builderUnitTag)->pos, CCColors::Yellow);
        }
        else if (b.status == BuildingStatus::UnderConstruction)
        {
            ss << "Constructing " << b.type.getName() << "    " << getBuildingWorkerCode(b) << "\n";
			if(b.builderUnit.isValid())
				m_bot.Map().drawLine(b.buildingUnit.getPosition(), b.builderUnit.getPosition(), sc2::Colors::White);
        }
    }

    m_bot.Map().drawTextScreen(0.3f, 0.05f, ss.str());
}

std::vector<Unit> BuildingManager::getFinishedBuildings()
{
	return m_baseBuildings;
}

BuildingPlacer& BuildingManager::getBuildingPlacer()
{
	return m_buildingPlacer;
}

std::vector<UnitType> BuildingManager::buildingsQueued() const
{
    std::vector<UnitType> buildingsQueued;

    for (const auto & b : m_buildings)
    {
        if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
        {
            buildingsQueued.push_back(b.type);
        }
    }

    return buildingsQueued;
}

CCTilePosition BuildingManager::getBuildingLocation(const Building & b)
{
    //size_t numPylons = m_bot.UnitInfo().getUnitTypeCount(Players::Self, Util::GetSupplyProvider(m_bot.GetSelfRace(), m_bot), true);

    // TODO: if requires psi and we have no pylons return 0

    if (b.type.isRefinery())
    {
        return m_buildingPlacer.getRefineryPosition();
    }

    if (b.type.isResourceDepot())
    {
        return m_bot.Bases().getNextExpansion(Players::Self);
    }

    // get a position within our region
    // TODO: put back in special pylon / cannon spacing
    return m_buildingPlacer.getBuildLocationNear(b, m_bot.Config().BuildingSpacing);
}

Unit BuildingManager::getClosestResourceDepot(CCPosition position)
{
	std::vector<Unit> resourceDepots;
	for (auto building : m_baseBuildings)
	{
		if(building.getType().isResourceDepot())
		{
			resourceDepots.push_back(building);
		}
	}

	Unit closestBase;
	float smallestDistance = 0.f;
	for (auto & base : resourceDepots)
	{
		const float dist = Util::Dist(base, position);
		if (smallestDistance == 0.f || dist < smallestDistance)
		{
			closestBase = base;
			smallestDistance = dist;
		}
	}

	return closestBase;
}


int BuildingManager::getBuildingCountOfType(const sc2::UNIT_TYPEID & b, bool isCompleted) const
{
	int count = 0;
	for (auto building : m_bot.UnitInfo().getUnits(Players::Self))
	{
		if (building.getAPIUnitType() == b && (!isCompleted || building.isCompleted()))
		{
			count++;
		}
	}
	return count;
}

int BuildingManager::getBuildingCountOfType(std::vector<sc2::UNIT_TYPEID> b, bool isCompleted) const
{
	int count = 0;
	for (auto building : m_bot.UnitInfo().getUnits(Players::Self))
	{
		if (std::find(b.begin(), b.end(), building.getAPIUnitType()) != b.end() && (!isCompleted || building.isCompleted()))
		{
			count++;
		}
	}
	return count;
}

void BuildingManager::removeBuildings(const std::vector<Building> & toRemove)
{
    for (auto & b : toRemove)
    {
        const auto & it = std::find(m_buildings.begin(), m_buildings.end(), b);

        if (it != m_buildings.end())
        {
            m_buildings.erase(it);
        }
    }
}

void BuildingManager::updateBaseBuildings()
{
	m_baseBuildings.clear();
	for (auto building : m_bot.UnitInfo().getUnits(Players::Self))
	{
		// filter out non building or building under construction
		if (!building.getType().isBuilding() || building.isBeingConstructed())
		{
			continue;
		}
		m_baseBuildings.push_back(building);
	}
}

const sc2::Unit * BuildingManager::getClosestMineral(const sc2::Unit * unit) const
{
	auto potentialMinerals = m_bot.UnitInfo().getUnits(Players::Neutral);
	const sc2::Unit * mineralField = nullptr;
	float minDist;
	for (auto mineral : potentialMinerals)
	{
		if (!mineral.getType().isMineral())
		{//Not mineral
			continue;
		}

		const float dist = Util::Dist(mineral.getUnitPtr()->pos, unit->pos);
		if (mineralField == nullptr) {
			mineralField = mineral.getUnitPtr();
			minDist = dist;
		}
		else if (dist < minDist) {
			mineralField = mineral.getUnitPtr();
			minDist = dist;
		}
	}
	return mineralField;
}


void BuildingManager::castBuildingsAbilities()
{
	for (const auto & b : m_baseBuildings)
	{
		auto energy = b.getEnergy();
		if (energy <= 0)
		{
			continue;
		}

		auto id = b.getType().getAPIUnitType();
		if (b.getType().getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND)
		{
			bool hasInvisible = m_bot.Strategy().enemyHasInvisible();
			//Scan
			if (hasInvisible)
			{
				for (auto sighting : m_bot.Commander().Combat().GetInvisibleSighting())
				{
					if (energy >= 50)//TODO: do not scan if no combat unit close by
					{
						Micro::SmartAbility(b.getUnitPtr(), sc2::ABILITY_ID::EFFECT_SCAN, sighting.second.first, m_bot);
					}
				}
			}

			//Mule
			if (energy >= 50 && (!hasInvisible || energy >= 100))
			{
				auto orbitalPosition = b.getPosition();
				auto point = getClosestMineral(b.getUnitPtr())->pos;
				//Get the middle point. Then the middle point of the middle point, then again... so we get a point at 7/8 of the way to the mineral from the Orbital command.
				point.x = (point.x + (point.x + (point.x + orbitalPosition.x) / 2) / 2) / 2;
				point.y = (point.y + (point.y + (point.y + orbitalPosition.y) / 2) / 2) / 2;
				Micro::SmartAbility(b.getUnitPtr(), sc2::ABILITY_ID::EFFECT_CALLDOWNMULE, point, m_bot);
			}
		}
	}
}