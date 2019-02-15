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

void BuildingManager::onFirstFrame()
{
	//Ramp wall location
	std::list<CCTilePosition> checkedTiles;
	FindRampTiles(rampTiles, checkedTiles, m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getDepotPosition());
	FindMainRamp(rampTiles);

	//Prevents crash when running in Release, will still crash in Debug. 
#if !_DEBUG
	if(rampTiles.size() > 0)
#endif
	{
		auto tilesToBlock = FindRampTilesToPlaceBuilding(rampTiles);
		PlaceSupplyDepots(tilesToBlock);
	}
}

// gets called every frame from GameCommander
void BuildingManager::onFrame()
{
	if (firstFrame)
	{
		firstFrame = false;
		onFirstFrame();
	}
	m_bot.StartProfiling("0.8.0 lowPriorityChecks");
	lowPriorityChecks();
	m_bot.StopProfiling("0.8.0 lowPriorityChecks");
	m_bot.StartProfiling("0.8.1 updateBaseBuildings");
	updateBaseBuildings();
	m_bot.StopProfiling("0.8.1 updateBaseBuildings");
	m_bot.StartProfiling("0.8.2 validateWorkersAndBuildings");
    validateWorkersAndBuildings();          // check to see if assigned workers have died en route or while constructing
	m_bot.StopProfiling("0.8.2 validateWorkersAndBuildings");
	m_bot.StartProfiling("0.8.3 assignWorkersToUnassignedBuildings");
    assignWorkersToUnassignedBuildings();   // assign workers to the unassigned buildings and label them 'planned'
	m_bot.StopProfiling("0.8.3 assignWorkersToUnassignedBuildings");
	m_bot.StartProfiling("0.8.4 constructAssignedBuildings");
    constructAssignedBuildings();           // for each planned building, if the worker isn't constructing, send the command
	m_bot.StopProfiling("0.8.4 constructAssignedBuildings");
	m_bot.StartProfiling("0.8.5 checkForStartedConstruction");
    checkForStartedConstruction();          // check to see if any buildings have started construction and update data structures
	m_bot.StopProfiling("0.8.5 checkForStartedConstruction");
	m_bot.StartProfiling("0.8.6 checkForDeadTerranBuilders");
    checkForDeadTerranBuilders();           // if we are terran and a building is under construction without a worker, assign a new one
	m_bot.StopProfiling("0.8.6 checkForDeadTerranBuilders");
	m_bot.StartProfiling("0.8.7 checkForCompletedBuildings");
    checkForCompletedBuildings();           // check to see if any buildings have completed and update data structures
	m_bot.StopProfiling("0.8.7 checkForCompletedBuildings");
	m_bot.StartProfiling("0.8.8 castBuildingsAbilities");
	castBuildingsAbilities();
	m_bot.StopProfiling("0.8.8 castBuildingsAbilities");

    drawBuildingInformation();
	drawStartingRamp();
	drawWall();
}

void BuildingManager::lowPriorityChecks()
{
	auto frame = m_bot.GetGameLoop();
	if (frame % 24)
	{
		return;
	}

	//Validate buildings are not on creep, does NOT validate if there is an enemy building in the way
	std::vector<Building> toRemove;
	for (auto & building : m_buildings)
	{
		auto position = building.finalPosition;
		if (!m_buildingPlacer.canBuildHere(position.x, position.y, building, true))
		{
			auto it = find(m_buildings.begin(), m_buildings.end(), building);
			if (it != m_buildings.end())
			{
				auto remove = CancelBuilding(building);
				if (remove.finalPosition != CCTilePosition(0,0))
				{
					toRemove.push_back(remove);
				}
			}
		}
	}
	removeBuildings(toRemove);
}

void BuildingManager::FindRampTiles(std::list<CCTilePosition> &rampTiles, std::list<CCTilePosition> &checkedTiles, CCTilePosition currentTile)
{
	if (std::find(checkedTiles.begin(), checkedTiles.end(), currentTile) != checkedTiles.end())
	{
		return;
	}

	checkedTiles.push_front(currentTile);
	if (m_bot.Map().isWalkable(currentTile))
	{
		if (m_bot.Map().isBuildable(currentTile))
		{
			FindRampTiles(rampTiles, checkedTiles, CCTilePosition(currentTile.x + 1, currentTile.y));
			FindRampTiles(rampTiles, checkedTiles, CCTilePosition(currentTile.x - 1, currentTile.y));
			FindRampTiles(rampTiles, checkedTiles, CCTilePosition(currentTile.x, currentTile.y + 1));
			FindRampTiles(rampTiles, checkedTiles, CCTilePosition(currentTile.x, currentTile.y - 1));
		}
		else
		{
			float tileHeight = m_bot.Map().terrainHeight(currentTile.x, currentTile.y);

			float topHeightDiff = tileHeight - m_bot.Map().terrainHeight(currentTile.x + 1, currentTile.y);
			float downHeightDiff = tileHeight - m_bot.Map().terrainHeight(currentTile.x - 1, currentTile.y);
			float rightHeightDiff = tileHeight - m_bot.Map().terrainHeight(currentTile.x, currentTile.y + 1);
			float leftHeightDiff = tileHeight - m_bot.Map().terrainHeight(currentTile.x, currentTile.y - 1);

			bool topIsLower = (isgreaterequal(topHeightDiff, 0.5f) && isgreaterequal(1.f, topHeightDiff));
			bool downIsLower = (isgreaterequal(downHeightDiff, 0.5f) && isgreaterequal(1.f, downHeightDiff));
			bool rightIsLower = (isgreaterequal(rightHeightDiff, 0.5f) && isgreaterequal(1.f, rightHeightDiff));
			bool leftIsLower = (isgreaterequal(leftHeightDiff, 0.5f) && isgreaterequal(1.f, leftHeightDiff));

			//Ramps tiles are 1 lower
			if (topIsLower || downIsLower || rightIsLower || leftIsLower)
			{
				rampTiles.push_back(currentTile);
			}
		}
	}
}

void BuildingManager::FindMainRamp(std::list<CCTilePosition> &rampTiles)
{
	int minDistance = INT_MAX;
	CCTilePosition mainRampTile;
	for (auto & tile : rampTiles)
	{
		int distance = Util::DistSq(Util::GetPosition(tile), m_bot.Map().center());
		if (distance < minDistance)
		{
			minDistance = distance;
			mainRampTile = tile;
		}
	}
	std::list<CCTilePosition> mainRamp;
	mainRamp.push_back(mainRampTile);
	for (auto & tile : rampTiles)
	{
		if (tile == mainRampTile)
		{
			continue;
		}
		if (mainRampTile.x + 1 == tile.x && mainRampTile.y + 1 == tile.y)
		{
			mainRamp.push_back(tile);
		}
		else if (mainRampTile.x + 1 == tile.x && mainRampTile.y - 1 == tile.y)
		{
			mainRamp.push_back(tile);
		}
		else if (mainRampTile.x - 1 == tile.x && mainRampTile.y + 1 == tile.y)
		{
			mainRamp.push_back(tile);
		}
		else if (mainRampTile.x - 1 == tile.x && mainRampTile.y - 1 == tile.y)
		{
			mainRamp.push_back(tile);
		}
	}
}

std::vector<CCTilePosition> BuildingManager::FindRampTilesToPlaceBuilding(std::list<CCTilePosition> &rampTiles)
{
	std::vector<CCTilePosition> tilesToBlock;
	for (auto & tile : rampTiles)
	{
		CCTilePosition below = CCTilePosition(tile.x - 1, tile.y);
		CCTilePosition above = CCTilePosition(tile.x + 1, tile.y);
		CCTilePosition left = CCTilePosition(tile.x, tile.y - 1);
		CCTilePosition right = CCTilePosition(tile.x, tile.y + 1);
		if (m_bot.Map().isBuildable(below) && m_bot.Map().isWalkable(below) && m_bot.Map().terrainHeight(tile) == m_bot.Map().terrainHeight(below))
		{//we need to block this tile
			if (std::find(tilesToBlock.begin(), tilesToBlock.end(), below) == tilesToBlock.end())
			{
				tilesToBlock.push_back(below);
			}
		}
		
		if (m_bot.Map().isBuildable(above) && m_bot.Map().isWalkable(above) && m_bot.Map().terrainHeight(tile) == m_bot.Map().terrainHeight(above))
		{//we need to block this tile
			if (std::find(tilesToBlock.begin(), tilesToBlock.end(), above) == tilesToBlock.end())
			{
				tilesToBlock.push_back(above);
			}
		}

		if (m_bot.Map().isBuildable(left) && m_bot.Map().isWalkable(left) && m_bot.Map().terrainHeight(tile) == m_bot.Map().terrainHeight(left))
		{//we need to block this tile
			if (std::find(tilesToBlock.begin(), tilesToBlock.end(), left) == tilesToBlock.end())
			{
				tilesToBlock.push_back(left);
			}
		}

		if (m_bot.Map().isBuildable(right) && m_bot.Map().isWalkable(right) && m_bot.Map().terrainHeight(tile) == m_bot.Map().terrainHeight(right))
		{//we need to block this tile
			if (std::find(tilesToBlock.begin(), tilesToBlock.end(), right) == tilesToBlock.end())
			{
				tilesToBlock.push_back(right);
			}
		}
	}
	if(tilesToBlock.size() != 3)
	{
		Util::DisplayError("Unusual ramp detected, tiles to block = " + std::to_string(tilesToBlock.size()), "0x00000003", m_bot, false);
		return {};
	}
	
	int swap = 0;
	while((abs(tilesToBlock.at(0).x - tilesToBlock.at(2).x) != 1 || abs(tilesToBlock.at(0).y - tilesToBlock.at(2).y) != 1)
		&& swap < tilesToBlock.size() - 1)
	{
		//Move front to back
		std::rotate(tilesToBlock.begin(), tilesToBlock.begin() + 1, tilesToBlock.end());
		swap++;
	}
	if (swap == tilesToBlock.size() - 1)
	{
		Util::DisplayError("Ramp tiles are wrong.", "0x00000004", m_bot, false);
		return {};
	}
	return tilesToBlock;
}

void BuildingManager::PlaceSupplyDepots(std::vector<CCTilePosition> tilesToBlock)
{
	std::list<CCTilePosition> buildingTiles;
	for (auto tile : tilesToBlock)
	{
		if (m_bot.Map().isBuildable(CCTilePosition(tile.x, tile.y)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x - 1, tile.y)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x, tile.y - 1)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x - 1, tile.y - 1)))
		{
			if (ValidateSupplyDepotPosition(buildingTiles, CCTilePosition(tile.x - 1, tile.y - 1)))
			{
				buildingTiles.push_back(CCTilePosition(tile.x - 1, tile.y - 1));
				continue;
			}
		}
		if (m_bot.Map().isBuildable(CCTilePosition(tile.x + 1, tile.y)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x, tile.y)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x + 1, tile.y - 1)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x, tile.y - 1)))
		{
			if (ValidateSupplyDepotPosition(buildingTiles, CCTilePosition(tile.x, tile.y - 1)))
			{
				buildingTiles.push_back(CCTilePosition(tile.x, tile.y - 1));
				continue;
			}
		}
		if (m_bot.Map().isBuildable(CCTilePosition(tile.x, tile.y + 1)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x - 1, tile.y + 1)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x, tile.y)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x - 1, tile.y)))
		{
			if (ValidateSupplyDepotPosition(buildingTiles, CCTilePosition(tile.x - 1, tile.y)))
			{
				buildingTiles.push_back(CCTilePosition(tile.x - 1, tile.y));
				continue;
			}
		}
		if (m_bot.Map().isBuildable(CCTilePosition(tile.x + 1, tile.y + 1)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x, tile.y + 1)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x + 1, tile.y)) &&
			m_bot.Map().isBuildable(CCTilePosition(tile.x, tile.y)))
		{
			if (ValidateSupplyDepotPosition(buildingTiles, CCTilePosition(tile.x, tile.y)))
			{
				buildingTiles.push_back(CCTilePosition(tile.x, tile.y));
				continue;
			}
		}
		BOT_ASSERT(false, "Can't find possible position for a wall build. This shouldn't happen.");
		//TODO: Check remove the buildingTiles and try again in a different order. To try again, pop front tilesToBlock and push back the front.
	}
	wallBuilding = buildingTiles;
	for (auto building : buildingTiles)
	{
		nextBuildingPosition[MetaTypeEnum::SupplyDepot.getUnitType()].push_back(CCTilePosition(building.x + 1, building.y + 1));
	}
}

bool BuildingManager::ValidateSupplyDepotPosition(std::list<CCTilePosition> buildingTiles, CCTilePosition possibleTile)
{
	std::list<CCTilePosition> possibleTiles;
	possibleTiles.push_back(CCTilePosition(possibleTile.x, possibleTile.y));
	possibleTiles.push_back(CCTilePosition(possibleTile.x + 1, possibleTile.y));
	possibleTiles.push_back(CCTilePosition(possibleTile.x, possibleTile.y + 1));
	possibleTiles.push_back(CCTilePosition(possibleTile.x + 1, possibleTile.y + 1));

	for (auto tile : buildingTiles)
	{//(std::find(my_list.begin(), my_list.end(), my_var) != my_list.end())
		std::list<CCTilePosition> tiles;
		tiles.push_back(CCTilePosition(tile.x, tile.y));
		tiles.push_back(CCTilePosition(tile.x + 1, tile.y));
		tiles.push_back(CCTilePosition(tile.x, tile.y + 1));
		tiles.push_back(CCTilePosition(tile.x + 1, tile.y + 1));
		for (auto t : tiles)
		{
			if (std::find(possibleTiles.begin(), possibleTiles.end(), t) != possibleTiles.end())
			{
				return false;
			}
		}
	}
	return true;
}

bool BuildingManager::isBeingBuilt(UnitType type) const
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

int BuildingManager::countBeingBuilt(UnitType type) const
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

int BuildingManager::countBoughtButNotBeingBuilt(sc2::UNIT_TYPEID type) const
{
	int count = 0;
	for (auto & b : m_buildings)
	{
		if (b.type.getAPIUnitType() == type && b.status < BuildingStatus::UnderConstruction)
		{
			count++;
		}
	}

	return count;
}

void BuildingManager::updatePreviousBuildings()
{
	m_previousBuildings = m_buildings;
}

void BuildingManager::updatePreviousBaseBuildings()
{
	m_previousBaseBuildings = m_baseBuildings;
}

// STEP 1: DO BOOK KEEPING ON WORKERS WHICH MAY HAVE DIED
void BuildingManager::validateWorkersAndBuildings()
{
    std::vector<Building> toRemove;

    // find any buildings which have become obsolete
    for (auto & b : m_buildings)
    {
		switch (b.status)
		{
			case BuildingStatus::Assigned:
			{
				if (!b.builderUnit.isValid() || !b.builderUnit.isAlive())//If the worker died on the way to start the building construction
				{
					auto remove = CancelBuilding(b);
					toRemove.push_back(remove);
					Util::DebugLog("Remove " + b.buildingUnit.getType().getName() + " from underconstruction buildings.", m_bot);
				}
				break;
			}
			case BuildingStatus::Size:
			{
				break;
			}
			case BuildingStatus::Unassigned:
			{
				break;
			}
			case BuildingStatus::UnderConstruction:
			{
				if (!b.buildingUnit.isValid())
				{
					toRemove.push_back(b);
					Util::DebugLog("Remove " + b.buildingUnit.getType().getName() + " from underconstruction buildings.", m_bot);
				}
				break;
			}
		}
        if (b.status != BuildingStatus::UnderConstruction)
        {
            continue;
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
			m_bot.StartProfiling("0.8.3.1 getBuildingLocation");
			// grab a worker unit from WorkerManager which is closest to this final position
			CCTilePosition testLocation = getNextBuildingLocation(b, false);
			m_bot.StopProfiling("0.8.3.1 getBuildingLocation");

			// Don't test the location if the building is already started
			if (!b.underConstruction && (!m_bot.Map().isValidTile(testLocation) || (testLocation.x == 0 && testLocation.y == 0)))
			{
				continue;
			}
			
			b.finalPosition = testLocation;

			// grab the worker unit from WorkerManager which is closest to this final position
			Unit builderUnit = m_bot.Workers().getBuilder(b, false);
			//Test if worker path is safe
			if (!builderUnit.isValid())
			{
				continue;
			}
			m_bot.StartProfiling("0.8.3.2 IsPathToGoalSafe");
			if(!Util::PathFinding::IsPathToGoalSafe(builderUnit.getUnitPtr(), Util::GetPosition(b.finalPosition), m_bot))
			{
				//Not safe, pick another location
				m_bot.StopProfiling("0.8.3.2 IsPathToGoalSafe");
				testLocation = getNextBuildingLocation(b, true);
				if (!b.underConstruction && (!m_bot.Map().isValidTile(testLocation) || (testLocation.x == 0 && testLocation.y == 0)))
				{
					continue;
				}

				b.finalPosition = testLocation;

				// grab the worker unit from WorkerManager which is closest to this final position
				Unit builderUnit = m_bot.Workers().getBuilder(b, false);
				//Test if worker path is safe
				if (!builderUnit.isValid())
				{
					continue;
				}
			}
			else
			{
				m_bot.StopProfiling("0.8.3.2 IsPathToGoalSafe");
				//path  is safe, we can remove it from the list
				auto & positions = nextBuildingPosition.find(b.type);// .pop_front();
				if (positions != nextBuildingPosition.end())
				{
					for (auto & position : positions->second)
					{
						if (position == testLocation)
						{
							positions->second.remove(testLocation);
							break;
						}
					}
				}
			}

			m_bot.Workers().getWorkerData().setWorkerJob(builderUnit, WorkerJobs::Build, b.builderUnit);//Set as builder
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
						//Reserve tiles below the building to ensure units don't get stuck and reserve tiles for addon
						case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
						case sc2::UNIT_TYPEID::TERRAN_FACTORY:
						case sc2::UNIT_TYPEID::TERRAN_STARPORT:
						{
							m_buildingPlacer.reserveTiles((int)b.finalPosition.x, (int)b.finalPosition.y - 1, 3, 1);//Reserve below
							m_buildingPlacer.reserveTiles((int)b.finalPosition.x + 3, (int)b.finalPosition.y, 2, 2);//Reserve addon
						}
						//Reserve tiles for addon
						/*case sc2::UNIT_TYPEID::TERRAN_STARPORT:
						{
							m_buildingPlacer.reserveTiles((int)b.finalPosition.x + 3, (int)b.finalPosition.y, 2, 2);
						}*/
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

		// if we're zerg and the builder unit is null, we assume it morphed into the building
		bool isConstructing = false;
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
                        if (unit.getType().isGeyser() && Util::DistSq(Util::GetPosition(b.finalPosition), unit.getPosition()) < 3 * 3)
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
				m_buildingsProgress[buildingStarted.getTag()] = 0;

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
		if (b.type.isAddon())
		{
			continue;
		}

		//If the building isn't started
		if (!b.buildingUnit.isValid())
		{
			continue;
		}

		// if the building has a builder that died or that is not a builder anymore because of a bug
		if (!b.builderUnit.isValid())
		{
			Util::DebugLog(__FUNCTION__, "BuilderUnit is invalid", m_bot);
			continue;
		}

		auto progress = b.buildingUnit.getUnitPtr()->build_progress;
		auto tag = b.buildingUnit.getTag();
		if (progress <= m_buildingsProgress[tag])
		{
			if (m_buildingsNewWorker.find(tag) == m_buildingsNewWorker.end() || !m_buildingsNewWorker[tag].isAlive() || m_bot.Workers().getWorkerData().getWorkerJob(m_buildingsNewWorker[tag]) != WorkerJobs::Build)
			{
				// grab the worker unit from WorkerManager which is closest to this final position
				Unit newBuilderUnit = m_bot.Workers().getBuilder(b, false);
				if (!newBuilderUnit.isValid())
				{
					Util::DebugLog(__FUNCTION__, "Worker is invalid", m_bot);
					continue;
				}
				if (!Util::PathFinding::IsPathToGoalSafe(newBuilderUnit.getUnitPtr(), Util::GetPosition(b.finalPosition), m_bot))
				{
					continue;
				}
				m_bot.Workers().getWorkerData().setWorkerJob(newBuilderUnit, WorkerJobs::Build, b.builderUnit);//Set as builder
				b.builderUnit = newBuilderUnit;
				b.status = BuildingStatus::Assigned;
				m_buildingsNewWorker[tag] = newBuilderUnit;
			}
		}
		m_buildingsProgress[tag] = progress;
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
				if(b.type.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_REFINERY)//Worker that built the refinery, will be a gas worker for it.
				{
					m_bot.Workers().getWorkerData().setWorkerJob(b.builderUnit, WorkerJobs::Gas, b.buildingUnit);
				}
				else
				{
					m_bot.Workers().finishedWithWorker(b.builderUnit);
				}

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

void BuildingManager::drawStartingRamp()
{
	if (!m_bot.Config().DrawStartingRamp)
	{
		return;
	}

	for (auto tile : rampTiles)
	{
		m_bot.Map().drawTile(tile.x, tile.y, CCColor(255, 255, 0));
	}
}

void BuildingManager::drawWall()
{
	if (!m_bot.Config().DrawWall)
	{
		return;
	}

	for (auto building : wallBuilding)
	{
		m_bot.Map().drawTile(building.x, building.y, CCColor(255, 0, 0));
		break;
	}
}

std::vector<Building> BuildingManager::getBuildings()
{
	return m_buildings;
}

std::vector<Building> BuildingManager::getPreviousBuildings()
{
	return m_previousBuildings;
}


std::vector<Unit> BuildingManager::getBaseBuildings()
{
	return m_baseBuildings;
}

std::vector<Unit> BuildingManager::getFinishedBuildings()
{
	return m_finishedBaseBuildings;
}

std::vector<Unit> BuildingManager::getPreviousBaseBuildings()
{
	return m_previousBaseBuildings;
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

	CCTilePosition buildingLocation;

    if (b.type.isRefinery())
    {
		m_bot.StartProfiling("0.8.3.1.1 getRefineryPosition");
		buildingLocation = m_buildingPlacer.getRefineryPosition();
		m_bot.StopProfiling("0.8.3.1.1 getRefineryPosition");
    }
	else if (b.type.isResourceDepot())
    {
		m_bot.StartProfiling("0.8.3.1.2 getNextExpansionPosition");
		buildingLocation = m_bot.Bases().getNextExpansionPosition(Players::Self, true);
		m_bot.StopProfiling("0.8.3.1.2 getNextExpansionPosition");
    }
	else
	{
		// get a position within our region
		// TODO: put back in special pylon / cannon spacing
		m_bot.StartProfiling("0.8.3.1.3 getBuildLocationNear");
		buildingLocation = m_buildingPlacer.getBuildLocationNear(b, m_bot.Config().BuildingSpacing);
		m_bot.StopProfiling("0.8.3.1.3 getBuildLocationNear");
	}
	return buildingLocation;
}

CCTilePosition BuildingManager::getNextBuildingLocation(const Building & b, bool ignoreNextBuildingPosition)
{
	if (!ignoreNextBuildingPosition)
	{
		std::map<UnitType, std::list<CCTilePosition>>::iterator it = nextBuildingPosition.find(b.type);
		if (it != nextBuildingPosition.end())
		{
			CCTilePosition location;
			if (!it->second.empty())
			{
				location = it->second.front();
			}
			else
			{
				location = getBuildingLocation(b);
			}
			return location;
		}
	}
	return getBuildingLocation(b);
}

Unit BuildingManager::getClosestResourceDepot(CCPosition position)
{
	std::vector<Unit> resourceDepots;
	for (auto building : m_finishedBaseBuildings)
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
		const float dist = Util::DistSq(base, position);
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

int BuildingManager::getBuildingCountOfType(std::vector<sc2::UNIT_TYPEID> & b, bool isCompleted) const
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
			if (it->buildingUnit.isValid())
			{
				m_buildingsProgress.erase(it->buildingUnit.getTag());
				m_buildingsNewWorker.erase(b.buildingUnit.getTag());
			}
            m_buildings.erase(it);
        }
    }
}

Building BuildingManager::CancelBuilding(Building b)
{
	auto it = find(m_buildings.begin(), m_buildings.end(), b);
	if (it != m_buildings.end())
	{
		auto position = b.finalPosition;
		m_buildingPlacer.freeTiles(position.x, position.y, b.type.tileWidth(), b.type.tileHeight());

		//Free oposite of reserved tiles in assignWorkersToUnassignedBuildings
		switch ((sc2::UNIT_TYPEID)b.type.getAPIUnitType())
		{
			//Reserve tiles below the building to ensure units don't get stuck and reserve tiles for addon
		case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
		case sc2::UNIT_TYPEID::TERRAN_FACTORY:
		case sc2::UNIT_TYPEID::TERRAN_STARPORT:
		{
			m_buildingPlacer.freeTiles(position.x, position.y - 1, 3, 1);//Free below
			m_buildingPlacer.freeTiles(position.x + 3, position.y, 2, 2);//Free addon
		}
		}

		m_reservedMinerals -= b.type.mineralPrice();
		m_reservedGas -= b.type.gasPrice();

		return b;
	}
	return Building();
}

void BuildingManager::updateBaseBuildings()
{
	m_baseBuildings.clear();
	m_finishedBaseBuildings.clear();
	for (auto building : m_bot.UnitInfo().getUnits(Players::Self))
	{
		// filter out non building or building under construction
		if (!building.getType().isBuilding())
		{
			continue;
		}
		if (building.isBeingConstructed())
		{
			m_baseBuildings.push_back(building);
			continue;
		}
		m_baseBuildings.push_back(building);
		m_finishedBaseBuildings.push_back(building);
	}
}

const sc2::Unit * BuildingManager::getClosestMineral(const sc2::Unit * unit) const
{
	auto potentialMinerals = m_bot.UnitInfo().getUnits(Players::Neutral);
	const sc2::Unit * mineralField = nullptr;
	float minDist = 0.f;
	for (auto mineral : potentialMinerals)
	{
		if (!mineral.getType().isMineral())
		{//Not mineral
			continue;
		}

		const float dist = Util::DistSq(mineral.getUnitPtr()->pos, unit->pos);
		if (mineralField == nullptr || dist < minDist) {
			mineralField = mineral.getUnitPtr();
			minDist = dist;
		}
	}
	return mineralField;
}

void BuildingManager::castBuildingsAbilities()
{
	for (const auto & b : m_finishedBaseBuildings)
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