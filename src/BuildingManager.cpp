#include <algorithm>
#include "Common.h"
#include "BuildingManager.h"
#include "CCBot.h"
#include "Util.h"

BuildingManager::BuildingManager(CCBot & bot)
    : m_bot(bot)
    , m_buildingPlacer(bot)
    , m_debugMode(false)
{

}

void BuildingManager::onStart()
{
    m_buildingPlacer.onStart();
}

void BuildingManager::onFirstFrame()
{
	//if (m_bot.GetPlayerRace(Players::Enemy) != sc2::Race::Protoss)
	{
		//Ramp wall location
		std::list<CCTilePosition> checkedTiles;
		FindRampTiles(m_rampTiles, checkedTiles, m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getDepotTilePosition());
		FindMainRamp(m_rampTiles);

		auto tilesToBlock = FindRampTilesToPlaceBuilding(m_rampTiles);
		PlaceWallBuildings(tilesToBlock);
	}
}

// gets called every frame from GameCommander
void BuildingManager::onFrame(bool executeMacro)
{
	if (firstFrame)
	{
		firstFrame = false;
		onFirstFrame();
	}
	if (executeMacro)
	{
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
	}
    drawBuildingInformation();
	drawStartingRamp();
	drawWall();
}

void BuildingManager::lowPriorityChecks()
{
	auto frame = m_bot.GetGameLoop();
	if (frame - m_lastLowPriorityFrame < 24)
	{
		return;
	}
	m_lastLowPriorityFrame = frame;

	//Validate buildings are not on creep or blocked
	std::vector<Building> toRemove;
	for (auto & building : m_buildings)
	{
		if (building.status == BuildingStatus::UnderConstruction)//Ignore buildings already being built
		{
			continue;
		}

		auto position = building.finalPosition;
		auto tag = (building.builderUnit.isValid() ? building.builderUnit.getTag() : 0);
		if (!m_buildingPlacer.canBuildHere(position.x, position.y, building.type, 0, true, false, false))
		{
			//Detect if the building was created this frame and we just don't know about it yet.
			bool isAlreadyBuilt = false;
			for (auto b : m_bot.GetAllyUnits())
			{
				if (building.finalPosition == b.second.getTilePosition() && building.type == b.second.getType())
				{
					isAlreadyBuilt = true;
					break;
				}
			}

			if (!isAlreadyBuilt)
			{
				//We are trying to build in an invalid location, remove it so we build it elsewhere.
				auto remove = CancelBuilding(building);
				if (remove.finalPosition != CCTilePosition(0, 0))
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
			const auto tileHeight = m_bot.Map().terrainHeight(currentTile.x, currentTile.y);
			auto slightlyLowerDiagonalNeighbors = 0;
			auto slightlyLowerAdjacentNeighbors = 0;
			for (int x = -1; x <= 1; ++x)
			{
				for (int y = -1; y <= 1; ++y)
				{
					if (x == 0 && y == 0)
						continue;
					const auto neighborTile = CCTilePosition(currentTile.x + x, currentTile.y + y);
					if (!m_bot.Map().isWalkable(neighborTile))
						continue;
					const auto neighborHeightDiff = tileHeight - m_bot.Map().terrainHeight(neighborTile);
					if(neighborHeightDiff >= 0.24f && neighborHeightDiff <= 0.26f || neighborHeightDiff >= 1.99f && neighborHeightDiff <= 2.01f)
					{
						const auto diagonal = x != 0 && y != 0;
						if (diagonal)
							++slightlyLowerDiagonalNeighbors;
						else
							++slightlyLowerAdjacentNeighbors;
					}
				}
			}
			if (slightlyLowerDiagonalNeighbors == 1 || slightlyLowerAdjacentNeighbors == 2)
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
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				if (x == 0 && y == 0 || x != 0 && y != 0)
					continue;	//only adjacent tiles are parsed
				const auto neighbor = CCTilePosition(tile.x + x, tile.y + y);
				if (m_bot.Map().isBuildable(neighbor) && m_bot.Map().isWalkable(neighbor) && m_bot.Map().terrainHeight(neighbor) >= m_bot.Map().terrainHeight(tile))
				{
					//we need to block this tile
					if (std::find(tilesToBlock.begin(), tilesToBlock.end(), neighbor) == tilesToBlock.end())
					{
						tilesToBlock.push_back(neighbor);
					}
				}
			}
		}
	}
	if(tilesToBlock.size() != 3)
	{
		//Handle maps with multiple ramps

		//Create a list of all connected tiles
		std::map<int, std::vector<CCTilePosition>> connectedTiles;
		for (auto & tile : tilesToBlock)
		{
			for (auto & tile2 : tilesToBlock)
			{
				if (tile == tile2)
					continue;
				int diff = abs(tile.x - tile2.x) + abs(tile.y - tile2.y);
				if (diff <= 2)
				{
					connectedTiles[Util::ToMapKey(tile)].push_back(tile2);
				}
			}
		}

		//Connects connected tiles together.
		bool hasChanges = true;
		while (hasChanges)
		{
			hasChanges = false;

			for (auto & connectedTile : connectedTiles)
			{
				for (auto & connectedTile2 : connectedTiles)
				{
					if (connectedTile.first == connectedTile2.first)
						continue;

					//If the connectedTiles contain this tile, add its results
					if(std::find(connectedTile.second.begin(), connectedTile.second.end(), Util::FromCCTilePositionMapKey(connectedTile2.first)) != connectedTile.second.end())
					{
						for (auto & tile : connectedTile2.second)
						{
							//If these connected tiles do not contain this tile yet, add it
							if (std::find(connectedTile.second.begin(), connectedTile.second.end(), tile) == connectedTile.second.end())
							{
								connectedTile.second.push_back(tile);
								hasChanges = true;
							}
						}
					}
				}
			}
		}

		//Check if we found a 3 wide ramp
		bool found = false;
		for (auto & connectedTile : connectedTiles)
		{
			if (connectedTile.second.size() == 3)
			{//found the ramp
				tilesToBlock = connectedTile.second;
				found = true;
				break;
			}
		}
		if (!found)
		{
			Util::DisplayError("Unusual ramp detected, tiles to block = " + std::to_string(tilesToBlock.size()), "0x00000003", m_bot, false);
			return {};
		}
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

void BuildingManager::PlaceWallBuildings(std::vector<CCTilePosition> tilesToBlock)
{
	std::list<CCTilePosition> buildingTiles;
	for (auto & tile : tilesToBlock)
	{
		if (m_bot.Map().isBuildable(tile) &&
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

	bool barrackInWall = !m_bot.Strategy().isProxyStartingStrategy();
	
	if (barrackInWall)
	{
		//Calculate the center of the buildings
		auto centerX = 0.f;
		auto centerY = 0.f;
		for (auto building : buildingTiles)
		{
			centerX += building.x;
			centerY += building.y;
		}
		centerX = centerX / buildingTiles.size();
		centerY = centerY / buildingTiles.size();

		if (centerX > buildingTiles.front().x)
		{
			buildingTiles.front().x -= 1;
		}

		if (centerY > buildingTiles.front().y)
		{
			buildingTiles.front().y -= 1;
		}
	}

	auto i = 0;
	for (auto building : buildingTiles)
	{
		auto position = CCTilePosition(building.x + 1, building.y + 1);

		if (barrackInWall && i == 0)//0 is always the center building
		{
			//offset the barrack in the opposite direction of the center, so we can build it
			m_nextBuildingPosition[MetaTypeEnum::Barracks.getUnitType()].push_back(position);
			m_buildingPlacer.reserveTiles(position.x, position.y, 3, 3);
		}
		else
		{
			m_nextBuildingPosition[MetaTypeEnum::SupplyDepot.getUnitType()].push_back(position);
			m_buildingPlacer.reserveTiles(position.x, position.y, 2, 2);
		}
		m_wallBuildingPosition.push_back(position);

		i++;
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
	{
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

void BuildingManager::FindOpponentMainRamp()
{
	const auto enemyBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
	if(enemyBaseLocation)
	{
		std::list<CCTilePosition> checkedTiles;
		std::list<CCTilePosition> rampTiles;
		FindRampTiles(rampTiles, checkedTiles, enemyBaseLocation->getDepotTilePosition());
		FindMainRamp(rampTiles);
		CCPosition rampPos;
		for (const auto & rampTile : rampTiles)
		{
			rampPos += Util::GetPosition(rampTile);
		}
		rampPos /= rampTiles.size();
		m_enemyMainRamp = rampPos;
	}
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

bool BuildingManager::isWallPosition(int x, int y) const
{
	for (auto wallPos : m_wallBuildingPosition)
	{
		if (x == wallPos.x && y == wallPos.y)
		{
			return true;
		}
	}
	return false;
}

int BuildingManager::countBeingBuilt(UnitType type, bool underConstruction) const
{
	int count = 0;
	for (auto & b : m_buildings)
	{
		if (b.type == type)
		{
			if (!underConstruction || b.status == BuildingStatus::UnderConstruction)
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
				//If the worker died on the way to start the building construction or if the requirements are not met anymore
				if (!b.builderUnit.isValid() || !b.builderUnit.isAlive() || !m_bot.Commander().Production().hasRequired(MetaType(b.type, m_bot), true)
					|| (m_bot.Strategy().isWorkerRushed() && m_buildingPlacer.isEnemyUnitBlocking(b.finalPosition, b.type)))
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
				if (!b.buildingUnit.isValid() || !b.buildingUnit.isAlive())
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

		assignWorkerToUnassignedBuilding(b);
	}
}

bool BuildingManager::assignWorkerToUnassignedBuilding(Building & b, bool filterMovingWorker)
{
    BOT_ASSERT(!b.builderUnit.isValid(), "Error: Tried to assign a builder to a building that already had one ");

    if (m_debugMode) { printf("Assigning Worker To: %s", b.type.getName().c_str()); }

	if (b.type.isAddon())
	{
			
		MetaType addonType = MetaType(b.type, m_bot);
		Unit producer = m_bot.Commander().Production().getProducer(addonType);
				
		if (!producer.isValid())
		{
			return false;
		}

		//Do not build addons if enemies are close by. Equivalent of IsPathSafeToGoal for addons.
		//400hp. Reactor builds in 36 seconds. TechLab builds in 18 seconds. Reactor gains 11.1 hp per second. Reactor gains 22.2 hp per second.
		int maxInfluence = (addonType == MetaTypeEnum::Reactor ? 5 : 10);//These numbers are about 50% lower than the addon hp gain per second 
																		  //just to be on the safe side since influence considers distance.
		auto addonPos = producer.getTilePosition();
		addonPos.x += 3;
		float currentInfluence = Util::PathFinding::GetCombatInfluenceOnTile(addonPos, false, m_bot);
		if (currentInfluence > maxInfluence)
		{
			return false;
		}

		b.builderUnit = producer;
		b.finalPosition = producer.getTilePosition();

		b.status = BuildingStatus::Assigned;
		return true;
	}
	else
	{
		m_bot.StartProfiling("0.8.3.1 getBuildingLocation");
		// grab a worker unit from WorkerManager which is closest to this final position
		bool isRushed = m_bot.Strategy().isEarlyRushed() || m_bot.Strategy().isWorkerRushed();
		CCTilePosition testLocation;
		if (b.canBeBuiltElseWhere)
		{
			testLocation = getNextBuildingLocation(b, !isRushed, true);//Only check m_nextBuildLocation if we are not being rushed
		}
		else
		{
			testLocation = b.desiredPosition;
		}
		m_bot.StopProfiling("0.8.3.1 getBuildingLocation");

		// Don't test the location if the building is already started
		if (!b.underConstruction && (!m_bot.Map().isValidTile(testLocation) || (testLocation.x == 0 && testLocation.y == 0)))
		{
			return false;
		}
			
		b.finalPosition = testLocation;

		// grab the worker unit from WorkerManager which is closest to this final position
		Unit builderUnit = m_bot.Workers().getBuilder(b, false, filterMovingWorker);
		//Test if worker path is safe
		if (!builderUnit.isValid())
		{
			return false;
		}
		m_bot.StartProfiling("0.8.3.2 IsPathToGoalSafe");
		const auto isPathToGoalSafe = Util::PathFinding::IsPathToGoalSafe(builderUnit.getUnitPtr(), Util::GetPosition(b.finalPosition), b.type.isRefinery(), m_bot);
		m_bot.StopProfiling("0.8.3.2 IsPathToGoalSafe");
		if(!isPathToGoalSafe && b.canBeBuiltElseWhere)
		{
			Util::DebugLog(__FUNCTION__, "Path to " + b.type.getName() + " isn't safe", m_bot);
			//TODO checks twice if the path is safe for no reason if we get the same build location, should change location or change builder

			//Not safe, pick another location
			testLocation = getNextBuildingLocation(b, false, true);
			if (!b.underConstruction && (!m_bot.Map().isValidTile(testLocation) || (testLocation.x == 0 && testLocation.y == 0)))
			{
				return false;
			}

			b.finalPosition = testLocation;

			const auto previousBuilder = builderUnit.getUnitPtr();
			// grab the worker unit from WorkerManager which is closest to this final position
			builderUnit = m_bot.Workers().getBuilder(b, false, filterMovingWorker);
			//Test if worker path is safe
			if(!builderUnit.isValid())
			{
				Util::DebugLog(__FUNCTION__, "Couldn't find a builder for " + b.type.getName(), m_bot);
				return false;
			}
			if (builderUnit.getUnitPtr() == previousBuilder)
			{
				return false;
			}
			if (!Util::PathFinding::IsPathToGoalSafe(builderUnit.getUnitPtr(), Util::GetPosition(b.finalPosition), b.type.isRefinery(), m_bot))
			{
				Util::DebugLog(__FUNCTION__, "Path to " + b.type.getName() + " still isn't safe", m_bot);
				return false;
			}
		}
		else if(!isRushed)//Do not remove postion from m_nextBuildLocation if we are being rushed, since we didn't use it.
		{
			//path  is safe, we can remove it from the list
			auto positions = m_nextBuildingPosition.find(b.type);
			if (positions != m_nextBuildingPosition.end())
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
				}
			}
		}

		b.status = BuildingStatus::Assigned;
		return true;
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
		Unit & builderUnit = b.builderUnit;

		//Prevent order spam 
		/*if (b.buildCommandGiven && builderUnit.isValid())
		{
			auto & orders = b.builderUnit.getUnitPtr()->orders;
			if (orders.size() != 0 && orders[0].ability_id != sc2::ABILITY_ID::PATROL)
			{
				//Is not idle and is not patroling, should be trying to build.
				continue;
			}
		}*/


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
				const auto & orders = b.builderUnit.getUnitPtr()->orders;
            	if (orders.empty() || orders[0].ability_id != sc2::ABILITY_ID::MOVE || orders[0].target_pos.x != b.finalPosition.x || orders[0].target_pos.y != b.finalPosition.y)
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
					for (const auto & unit : m_bot.GetUnits())
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
						// If the building is flying, it's because it is in the transition to land somewhere it will have enough space for its addon
						if (b.builderUnit.isFlying())
						{
							const auto landingPosition = liftedBuildingPositions[b.builderUnit.getTag()];
							Micro::SmartAbility(b.builderUnit.getUnitPtr(), sc2::ABILITY_ID::LAND, landingPosition, m_bot);
						}
						else
						{
							// Check if the addon position is blocked
							bool blocked = false;
							for (int x = 2; x <= 3; ++x)
							{
								for (int y = -1; y <= 0; ++y)
								{
									const auto addonPosition = b.builderUnit.getPosition() + CCPosition(x, y);
									if (!getBuildingPlacer().buildable(b.type, addonPosition.x, addonPosition.y, true))
									{
										m_bot.Map().drawTile(Util::GetTilePosition(addonPosition), sc2::Colors::Red);
										blocked = true;
									}
								}
							}
							if (blocked)
							{
								blocked = false;
								// Check if the building can just move 2 tiles to the left
								for (int x = -4; x <= -2; ++x)
								{
									for (int y = -1; y <= 1; ++y)
									{
										const auto newBuildingPosition = b.builderUnit.getPosition() + CCPosition(x, y);
										if (!getBuildingPlacer().buildable(b.builderUnit.getType(), newBuildingPosition.x, newBuildingPosition.y, false))
										{
											m_bot.Map().drawTile(Util::GetTilePosition(newBuildingPosition), sc2::Colors::Red);
											blocked = true;
										}
									}
								}
								CCPosition newBuildingPosition;
								if (blocked)
								{
									// The building cannot move just 2 tiles to the left, so we need to find it a suitable spot
									const auto tempBuilding = Building(b.builderUnit.getType(), b.builderUnit.getPosition());
									newBuildingPosition = Util::GetPosition(getBuildingPlacer().getBuildLocationNear(tempBuilding, 0, false, true, true));
								}
								else
								{
									// Otherwise, just move the buildings 2 tiles to the left
									newBuildingPosition = b.builderUnit.getPosition() - CCPosition(2, 0);
								}
								
								if (newBuildingPosition == CCPosition())
								{
									std::stringstream ss;
									ss << "Cannot find suitable spot for " << b.type.getName() << " to move so it could have a space for its addon";
									Util::Log(__FUNCTION__, ss.str(), m_bot);
									continue;
								}

								b.finalPosition = newBuildingPosition;
								liftedBuildingPositions[b.builderUnit.getTag()] = newBuildingPosition;

								// Free the reserved tiles under the building and for the addon
								getBuildingPlacer().freeTiles(b.builderUnit.getPosition().x, b.builderUnit.getPosition().y - 1, 3, 1);
								getBuildingPlacer().freeTiles(b.builderUnit.getPosition().x + 3, b.builderUnit.getPosition().y, 2, 2);

								// Reserve the files for the new location
								getBuildingPlacer().reserveTiles(newBuildingPosition.x, newBuildingPosition.y - 1, 3, 4);
								getBuildingPlacer().reserveTiles(b.finalPosition.x + 3, b.finalPosition.y, 2, 2);

								// Lift the building (the landing code is approx. 50 lines above)s
								Micro::SmartAbility(b.builderUnit.getUnitPtr(), sc2::ABILITY_ID::LIFT, m_bot);
							}
							else // The addon position is not blocked
							{
								// We free the reserved tiles only when the building is landed (even though the unit is not flying, its type is still a flying one until it landed)
								const std::vector<sc2::UNIT_TYPEID> flyingTypes = { sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING, sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING , sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING };
								const auto it = liftedBuildingPositions.find(b.builderUnit.getTag());
								if (it != liftedBuildingPositions.end() && !Util::Contains(b.builderUnit.getAPIUnitType(), flyingTypes))
								{
									liftedBuildingPositions.erase(it);
									getBuildingPlacer().freeTiles(b.builderUnit.getPosition().x, b.builderUnit.getPosition().y, 3, 3);
									getBuildingPlacer().freeTiles(b.finalPosition.x + 3, b.finalPosition.y, 2, 2);
								}

								// Spam the build ability in case there is a unit blocking it
								Micro::SmartAbility(b.builderUnit.getUnitPtr(), m_bot.Data(b.type).buildAbility, m_bot);
							}
						}
					}
					else
					{
						b.builderUnit.build(b.type, b.finalPosition);
						if (b.type.isResourceDepot() && b.buildCommandGiven)	//if resource depot position is blocked by a unit, send elsewhere
						{
							if (m_bot.GetMinerals() > b.type.mineralPrice())
							{
								// We want the worker to be close so it doesn't flag the base as blocked by error
								const bool closeEnough = Util::DistSq(b.builderUnit, Util::GetPosition(b.finalPosition)) <= 7.f * 7.f;
								// If we can't build here, we can flag it as blocked, checking closeEnough for the tilesBuildable variable is just an optimisation and not part of the logic
								const bool blocked = closeEnough && !m_buildingPlacer.canBuildHere(b.finalPosition.x, b.finalPosition.y, b.type, 0, true, false, false);
								if (blocked)
								{
									m_bot.Bases().SetLocationAsBlocked(Util::GetPosition(b.finalPosition), true);
									b.finalPosition = m_bot.Bases().getNextExpansionPosition(Players::Self, true, false);
									b.buildCommandGiven = false;

									continue;
								}
							}
						}
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
	// check all our building status objects to see if we have a match and if we do, update it
	for (auto & b : m_buildings)
	{
		if (b.status != BuildingStatus::Assigned)
		{
			continue;
		}
		if (b.buildingUnit.isValid())
		{
			std::cout << "Replaced dead worker or Building mis-match somehow\n";
			b.status = BuildingStatus::UnderConstruction;
			continue;
		}
		auto type = b.type;

		// check if the positions match
		int addonOffset = (type.isAddon() ? 3 : 0);

		std::vector<Unit> buildingsOfType;
		switch ((sc2::UNIT_TYPEID)type.getAPIUnitType())
		{
			//Special case for geysers because of rich geysers having a different ID
			case sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR:
			case sc2::UNIT_TYPEID::ZERG_EXTRACTOR:
			case sc2::UNIT_TYPEID::TERRAN_REFINERY:
				buildingsOfType = m_bot.GetAllyGeyserUnits();
				break;
			default:
				buildingsOfType = m_bot.GetAllyUnits(type.getAPIUnitType());
				break;
		}

		// for each building unit which is being constructed
		for (auto building : buildingsOfType)
		{
			// filter out units which aren't buildings under construction
			if (!building.getType().isBuilding() || !building.isBeingConstructed())
			{
				continue;
			}

			int dx = b.finalPosition.x + addonOffset - building.getTilePosition().x;
			int dy = b.finalPosition.y - building.getTilePosition().y;

			if (dx * dx + dy * dy < Util::TileToPosition(1.0f))
			{
				if (b.reserveResources)
				{
					// the resources should now be spent, so unreserve them
					m_bot.FreeMinerals(building.getType().mineralPrice());
					m_bot.FreeGas(building.getType().gasPrice());
				}

				// flag it as started and set the buildingUnit
				b.underConstruction = true;
				b.buildingUnit = building;
				m_buildingsProgress[building.getTag()] = 0;

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

				//Clear blocked locations when starting an expansion
				if (b.type.isResourceDepot())
				{
					m_bot.Bases().ClearBlockedLocations();
				}

				// only one building will match
				break;
			}
		}
	}
}

// STEP 5: IF WE ARE TERRAN, THIS MATTERS
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

		if (b.status != BuildingStatus::UnderConstruction)
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
				if(b.builderUnit.isValid() && b.builderUnit.isAlive())
				{
					// Builder is alright, probably just saving his ass
					const auto workerJob = m_bot.Workers().getWorkerData().getWorkerJob(b.builderUnit);
					if (workerJob == WorkerJobs::Combat)
						continue;
					if (workerJob == WorkerJobs::Build)
					{
						b.builderUnit.rightClick(b.buildingUnit);
						continue;
					}
					// else, we find a new worker
				}
				// Was using a proxy strategy but not anymore
				if (m_bot.Strategy().wasProxyStartingStrategy() && !m_bot.Strategy().isProxyStartingStrategy())
				{
					// The building is close to the proxy location
					if (Util::DistSq(Util::GetPosition(b.finalPosition), m_proxyLocation) < 20 * 20)
					{
						// We do not want to finish it
						continue;
					}
				}
				// grab the worker unit from WorkerManager which is closest to this final position
				Unit newBuilderUnit = m_bot.Workers().getBuilder(b, false);
				if (!newBuilderUnit.isValid())
				{
					Util::DebugLog(__FUNCTION__, "Worker is invalid for building " + b.type.getName(), m_bot);
					continue;
				}
				if (!Util::PathFinding::IsPathToGoalSafe(newBuilderUnit.getUnitPtr(), Util::GetPosition(b.finalPosition), true, m_bot))
				{
					continue;
				}
				m_bot.Workers().getWorkerData().setWorkerJob(newBuilderUnit, WorkerJobs::Build, b.builderUnit);//Set as builder
				b.builderUnit = newBuilderUnit;
				b.status = BuildingStatus::Assigned;
				m_buildingsNewWorker[tag] = newBuilderUnit;
			}
		}
		else
		{
			m_buildingsProgress[tag] = progress;
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
		auto type = b.type.getAPIUnitType();
        if (b.status != BuildingStatus::UnderConstruction)
        {
            continue;
        }

        // if the unit has completed
        if (b.buildingUnit.isCompleted())
        {
			// remove this unit from the under construction vector
			toRemove.push_back(b);

			//If building is part of the wall
			if (std::find(m_wallBuildingPosition.begin(), m_wallBuildingPosition.end(), b.buildingUnit.getTilePosition()) != m_wallBuildingPosition.end())
			{
				m_wallBuildings.push_back(b.buildingUnit);
			}
        	
            // if we are terran, give the worker back to worker manager
            if (Util::IsTerran(m_bot.GetSelfRace()))
            {
				if(b.type.isRefinery())//Worker that built the refinery, will be a gas worker for it.
				{
					m_bot.Workers().getWorkerData().setWorkerJob(b.builderUnit, WorkerJobs::Gas, b.buildingUnit);
				}
				else
				{
					m_bot.Workers().finishedWithWorker(b.builderUnit);
					if (m_bot.Strategy().getStartingStrategy() == PROXY_CYCLONES)
					{
						if (b.buildingUnit.getType() == MetaTypeEnum::Factory.getUnitType() && Util::DistSq(b.buildingUnit, Util::GetPosition(m_proxyLocation)) < 15.f * 15.f)
						{
							m_bot.Workers().getWorkerData().setWorkerJob(b.builderUnit, WorkerJobs::Repair);
							b.builderUnit.move(m_proxyLocation);
						}
					}

					//Handle rally points
					switch ((sc2::UNIT_TYPEID)type)
					{
						case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER:
						case sc2::UNIT_TYPEID::PROTOSS_NEXUS:
						{
							//Set rally in the middle of the minerals
							auto position = b.buildingUnit.getPosition();
							auto base = m_bot.Bases().getBaseContainingPosition(position, Players::Self);
							b.buildingUnit.rightClick(Util::GetPosition(base->getCenterOfMinerals()));
							break;
						}
						case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
						case sc2::UNIT_TYPEID::TERRAN_FACTORY:
						case sc2::UNIT_TYPEID::TERRAN_STARPORT:
						case sc2::UNIT_TYPEID::PROTOSS_GATEWAY:
						case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
						case sc2::UNIT_TYPEID::PROTOSS_STARGATE:
						{
							//Set rally towards enemy base
							const auto enemyBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
							if (enemyBase == nullptr)
							{
								b.buildingUnit.rightClick(m_bot.Map().center());
							}
							else
							{
								b.buildingUnit.rightClick(enemyBase->getPosition());
							}
							break;
						}
					}
				}
            }
        }
    }

    removeBuildings(toRemove);
}

// add a new building to be constructed
// Used for Premove
bool BuildingManager::addBuildingTask(Building & b, bool filterMovingWorker)
{
	b.status = BuildingStatus::Unassigned;

	if (b.builderUnit.isValid())
	{
		//Check if path is safe
		if (!Util::PathFinding::IsPathToGoalSafe(b.builderUnit.getUnitPtr(), Util::GetPosition(b.finalPosition), b.type.isRefinery(), m_bot))
		{
			Util::DebugLog(__FUNCTION__, "Path isn't safe for building " + b.type.getName(), m_bot);
			return false;
		}
	}
	else if (!assignWorkerToUnassignedBuilding(b, filterMovingWorker))//Includes a check to see if path is safe
	{
		return false;
	}

	if (b.reserveResources)
	{
		const TypeData typeData = m_bot.Data(b.type);
		m_bot.ReserveMinerals(typeData.mineralCost);
		m_bot.ReserveGas(typeData.gasCost);
	}

	m_buildings.push_back(b);

	return true;
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

void BuildingManager::drawBuildingInformation()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
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
#ifdef PUBLIC_RELEASE
	return;
#endif
	if (!m_bot.Config().DrawStartingRamp)
	{
		return;
	}

	for (auto tile : m_rampTiles)
	{
		m_bot.Map().drawTile(tile.x, tile.y, CCColor(255, 255, 0));
	}
}

void BuildingManager::drawWall()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
	if (!m_bot.Config().DrawWall)
	{
		return;
	}

	for (auto building : m_wallBuildingPosition)
	{
		m_bot.Map().drawTile(building.x, building.y, CCColor(255, 0, 0));
	}
}

std::vector<Building> & BuildingManager::getBuildings()
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

CCTilePosition BuildingManager::getWallPosition() const
{
	if (m_wallBuildingPosition.empty())
	{
		return CCTilePosition(0, 0);
	}
	return m_wallBuildingPosition.front();
}

std::list<Unit> BuildingManager::getWallBuildings()
{
	return m_wallBuildings;
}

CCTilePosition BuildingManager::getProxyLocation()
{
	if (m_proxyLocation != CCTilePosition())
		return m_proxyLocation;
	
	FindOpponentMainRamp();
	if (m_enemyMainRamp != CCPosition())
	{
		const auto & scv = *m_bot.Workers().getWorkers().begin();
		auto scvPtr = scv.getUnitPtr();
		bool deletePtr = false;
		if (!m_rampTiles.empty())
		{
			deletePtr = true;
			auto fakeSCV = new sc2::Unit();
			fakeSCV->unit_type = sc2::UnitTypeID(sc2::UNIT_TYPEID::TERRAN_SCV);
			fakeSCV->is_flying = false;
			fakeSCV->radius = 0.375f;
			auto rampPos = CCPosition();
			for (auto rampTile : m_rampTiles)
				rampPos += Util::GetPosition(rampTile);
			rampPos /= m_rampTiles.size();
			fakeSCV->pos = sc2::Point3D(rampPos.x, rampPos.y, Util::TerrainHeight(rampPos));
			scvPtr = fakeSCV;
		}
		const auto mainPath = Util::PathFinding::FindOptimalPathWithoutLimit(scvPtr, m_enemyMainRamp, m_bot);
		if (deletePtr)
			delete scvPtr;
		if (mainPath.empty())
			return Util::GetTilePosition(m_bot.Map().center());
		const auto startingBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
		const auto enemyBasePosition = Util::GetPosition(m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy)->getDepotTilePosition());
		const auto enemyNext = m_bot.Bases().getNextExpansion(Players::Enemy, false, false);
		const auto & baseLocations = m_bot.Bases().getBaseLocations();	// Sorted by closest to enemy base
		std::map<float, const BaseLocation*> sortedBases;
		for(auto i=1; i<baseLocations.size(); ++i)
		{
			const auto baseLocation = baseLocations[i];
			if (baseLocation == enemyNext || baseLocation == startingBaseLocation)
				continue;
			const auto dist = baseLocation->getGroundDistance(m_enemyMainRamp);
			const auto startingBaseDist = startingBaseLocation->getGroundDistance(baseLocation->getDepotTilePosition());
			const auto totalDist = dist * 2 + startingBaseDist;
			const auto baseHeight = m_bot.Map().terrainHeight(baseLocation->getDepotTilePosition());
			const auto basePosition = Util::GetPosition(baseLocation->getDepotTilePosition());
			const auto enemyRace = m_bot.GetPlayerRace(Players::Enemy);
			if (enemyRace == sc2::Zerg || enemyRace == sc2::Random)
			{
				if (Util::DistBetweenLineAndPoint(Util::GetPosition(startingBaseLocation->getDepotTilePosition()), enemyBasePosition, basePosition) < 15.f)
				{
					continue;
				}
			}
			auto tooCloseToMainPath = false;
			for (const auto & pathPosition : mainPath)
			{
				if (m_bot.Map().terrainHeight(pathPosition) + 0.5f < baseHeight)
					continue;
				if (Util::DistSq(pathPosition, basePosition) <= 15.f * 15.f)
				{
					tooCloseToMainPath = true;
					break;
				}
			}
			if (!tooCloseToMainPath)
			{
				sortedBases[totalDist] = baseLocation;
			}
		}
		if (sortedBases.begin() != sortedBases.end())
		{
			auto it = sortedBases.begin();
			if (m_bot.Config().RandomProxyLocation)
			{
				const int randomPossibleLocations = 2;
				std::srand(std::time(nullptr));	// Initialize random seed
				const auto maximumRandomBaseIndex = sortedBases.size() >= randomPossibleLocations ? randomPossibleLocations : sortedBases.size();
				const auto randomValue = std::rand();
				const auto randomBaseIndex = randomValue % maximumRandomBaseIndex;
				for (int i = 0; i < randomBaseIndex; ++i)
					++it;
			}
			const auto closestBase = it->second;
			m_proxyLocation = closestBase->getDepotTilePosition();
			const auto depotPos = Util::GetPosition(closestBase->getDepotTilePosition());
			const auto centerOfMinerals = Util::GetPosition(closestBase->getCenterOfMinerals());
			m_proxyLocation2 = depotPos + Util::Normalized(depotPos - centerOfMinerals) * 8;
			return m_proxyLocation;
		}
	}
	return Util::GetTilePosition(m_bot.Map().center());
}

CCPosition BuildingManager::getProxyLocation2()
{
	return m_proxyLocation2;
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

CCTilePosition BuildingManager::getBuildingLocation(const Building & b, bool checkInfluenceMap)
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
		buildingLocation = m_bot.Bases().getNextExpansionPosition(Players::Self, true, false);
		m_bot.StopProfiling("0.8.3.1.2 getNextExpansionPosition");
    }
	else
	{
		// get a position within our region
		// TODO: put back in special pylon / cannon spacing
		m_bot.StartProfiling("0.8.3.1.3 getBuildLocationNear");
		buildingLocation = m_buildingPlacer.getBuildLocationNear(b, m_bot.Config().BuildingSpacing, false, checkInfluenceMap, true);
		m_bot.StopProfiling("0.8.3.1.3 getBuildLocationNear");
	}
	return buildingLocation;
}

CCTilePosition BuildingManager::getNextBuildingLocation(Building & b, bool checkNextBuildingPosition, bool checkInfluenceMap)
{
	if (checkNextBuildingPosition)
	{
		std::map<UnitType, std::list<CCTilePosition>>::iterator it = m_nextBuildingPosition.find(b.type);
		if (it != m_nextBuildingPosition.end())
		{
			if (!it->second.empty())
			{
				return it->second.front();
			}
		}
	}

	CCTilePosition basePosition = b.desiredPosition;
	//Check where we last built (or tried to) a building and start looking from there.
	CCTilePosition position;
	/*for (auto & lastBuiltPosition : m_previousNextBuildingPositionByBase)
	{
		if (lastBuiltPosition.first == basePosition)
		{
			b.desiredPosition = lastBuiltPosition.second;
			break;
		}
	}
	*/
	position = getBuildingLocation(b, checkInfluenceMap);
	/*if (position.x != 0)//No need to check Y
	{
		//Update the last built (or tried to) location.
		bool found = false;
		for (auto & lastBuiltPosition : m_previousNextBuildingPositionByBase)
		{
			if (lastBuiltPosition.first == basePosition)
			{
				lastBuiltPosition.second = position;
				found = true;
				break;
			}
		}
		if (!found)
		{
			m_previousNextBuildingPositionByBase.push_back(std::pair<CCTilePosition, CCTilePosition>(basePosition, position));
		}
	}*/

	return position;
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

void BuildingManager::removeNonStartedBuildingsOfType(sc2::UNIT_TYPEID type)
{
	std::vector<Building> toRemove;
	for (auto & b : m_buildings)
	{
		if (b.type.getAPIUnitType() == type)
		{
			if (!b.buildingUnit.isValid())
			{
				toRemove.push_back(b);
			}
		}
	}

	removeBuildings(toRemove);
}

Building BuildingManager::CancelBuilding(Building b)
{
	auto it = find(m_buildings.begin(), m_buildings.end(), b);
	if (it != m_buildings.end())
	{
		auto position = b.finalPosition;
		if (position != CCPosition())
		{
			m_buildingPlacer.freeTiles(position.x, position.y, b.type.tileWidth(), b.type.tileHeight());

			//Free opposite of reserved tiles in assignWorkersToUnassignedBuildings
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
		}

		//Free resources
		if (b.reserveResources)
		{
			m_bot.FreeMinerals(b.type.mineralPrice());
			m_bot.FreeGas(b.type.gasPrice());
		}

		//Free worker
		if (b.builderUnit.isValid())
		{
			m_bot.Workers().getWorkerData().setWorkerJob(b.builderUnit, WorkerJobs::Idle);
		}

		return b;
	}
	return Building();
}

Building BuildingManager::getBuildingOfBuilder(const Unit & builder) const
{
	for (const auto & building : m_buildings)
	{
		if (building.builderUnit == builder)
			return building;
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

		if (building.isCompleted())
		{
			m_baseBuildings.push_back(building);
			m_finishedBaseBuildings.push_back(building);
		}
		else if (building.isBeingConstructed())
		{
			m_baseBuildings.push_back(building);
		}
	}
}

const sc2::Unit * BuildingManager::getClosestMineral(const CCPosition position) const
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

		const float dist = Util::DistSq(mineral.getUnitPtr()->pos, position);
		if (mineralField == nullptr || dist < minDist) {
			mineralField = mineral.getUnitPtr();
			minDist = dist;
		}
	}
	return mineralField;
}

const sc2::Unit * BuildingManager::getLargestCloseMineral(const CCTilePosition position, bool checkUnderAttack, std::vector<CCUnitID> skipMinerals) const
{
	auto base = m_bot.Bases().getBaseForDepotPosition(position);
	if (base == nullptr)
		return nullptr;
	return getLargestCloseMineral(base->getResourceDepot(), checkUnderAttack, skipMinerals);
}

const sc2::Unit * BuildingManager::getLargestCloseMineral(const Unit unit, bool checkUnderAttack, std::vector<CCUnitID> skipMinerals) const
{
	auto base = m_bot.Bases().getBaseForDepot(unit);
	if (base == nullptr || (checkUnderAttack && base->isUnderAttack()))
	{
		return nullptr;
	}

	auto potentialMinerals = base->getMinerals();
	const sc2::Unit * mineralField = nullptr;
	int maxMinerals = 0;
	for (auto mineral : potentialMinerals)
	{
		int mineralContent = mineral.getUnitPtr()->mineral_contents;
		if (mineralField == nullptr || mineralContent > maxMinerals) {
			mineralField = mineral.getUnitPtr();
			maxMinerals = mineralContent;
		}
	}
	return mineralField;
}

void BuildingManager::castBuildingsAbilities()
{
	RunProxyLogic();

	for (const auto & barracks : m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKS))
	{
		//If the building is in the wall
		if (Util::Contains(barracks, m_wallBuildings))
		{
			const auto base = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
			if (base && base->isUnderAttack())
			{
				if (!m_wallsBarracksPointsTowardBase)
				{
					//Set rally on our starting base
					barracks.rightClick(m_bot.GetStartLocation());
					m_wallsBarracksPointsTowardBase = true;
				}
			}
			else
			{
				if (m_wallsBarracksPointsTowardBase)
				{
					//Set rally towards enemy base
					const auto enemyBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
					if (enemyBase == nullptr)
					{
						barracks.rightClick(m_bot.Map().center());
					}
					else
					{
						barracks.rightClick(enemyBase->getPosition());
					}
					m_wallsBarracksPointsTowardBase = false;
				}
			}
			break;
		}
	}
	
	for (const auto & b : m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND))
	{
		const auto energy = b.getEnergy();
		if (energy < 50 || b.isFlying())
		{
			continue;
		}

		//Scan
		// TODO decomment this block when we are ready to use the scans
		/*bool hasInvisible = m_bot.Strategy().enemyHasInvisible();
		if (hasInvisible)
		{
			for (auto sighting : m_bot.Commander().Combat().GetInvisibleSighting())
			{
				//TODO: do not scan if no combat unit close by
				Micro::SmartAbility(b.getUnitPtr(), sc2::ABILITY_ID::EFFECT_SCAN, sighting.second.first, m_bot);
			}
		}*/

		const auto SCAN_RADIUS = 13;
		const auto & burrowedUnits = m_bot.Analyzer().getBurrowedUnits();
		if (!burrowedUnits.empty())
		{
			const auto & combatUnits = m_bot.Commander().Combat().GetCombatUnits();
			sc2::Units closeBurrowedUnits;
			for (const auto burrowedUnit : burrowedUnits)
			{
				if (burrowedUnit->last_seen_game_loop == m_bot.GetCurrentFrame())
					continue;	// Already visible

				// Check if we have a combat unit near the burrowed unit
				for (const auto & combatUnit : combatUnits)
				{
					auto range = Util::GetAttackRangeForTarget(combatUnit.getUnitPtr(), burrowedUnit, m_bot);
					if (range <= 0.f)
						continue;	// The combat unit cannot attack the burrowed unit
					range += 5.f;	// We add a buffer of 5 tiles
					const auto dist = Util::DistSq(combatUnit, burrowedUnit->pos);
					if (dist <= range * range)
					{
						closeBurrowedUnits.push_back(burrowedUnit);
						break;
					}
				}
			}
			if (!closeBurrowedUnits.empty())
			{
				// Calculate the middle point of all close burrowed unit
				CCPosition middlePoint;
				for (const auto closeBurrowedUnit : closeBurrowedUnits)
				{
					middlePoint += closeBurrowedUnit->pos;
				}
				middlePoint /= closeBurrowedUnits.size();
				// Check to see if a scan on the middle point would cover all of the burrowed units
				bool middleCoversAllPoints = true;
				for (const auto closeBurrowedUnit : closeBurrowedUnits)
				{
					const auto dist = Util::DistSq(middlePoint, closeBurrowedUnit->pos);
					if (dist > SCAN_RADIUS * SCAN_RADIUS)
					{
						middleCoversAllPoints = false;
						break;
					}
				}
				if (!middleCoversAllPoints)
				{
					// Find the burrowed unit that is the most close to the others
					const sc2::Unit * mostCenteredUnit = nullptr;
					int maxNumberOfCoveredUnits = -1;
					for (const auto closeBurrowedUnit : closeBurrowedUnits)
					{
						int numberOfCoveredUnits = 0;
						for (const auto otherCloseBurrowedUnit : closeBurrowedUnits)
						{
							if (Util::DistSq(closeBurrowedUnit->pos, otherCloseBurrowedUnit->pos) <= SCAN_RADIUS * SCAN_RADIUS)
							{
								++numberOfCoveredUnits;
							}
						}
						if (numberOfCoveredUnits > maxNumberOfCoveredUnits)
						{
							mostCenteredUnit = closeBurrowedUnit;
							maxNumberOfCoveredUnits = numberOfCoveredUnits;
						}
					}
					middlePoint = mostCenteredUnit->pos;
				}

				// Check if we already have a scan near that point (might happen because we receive the observations 1 frame later)
				bool closeScan = false;
				const auto & scans = m_bot.Commander().Combat().getAllyScans();
				for (const auto scanPosition : scans)
				{
					if (Util::DistSq(middlePoint, scanPosition) < 5.f * 5.f)
					{
						closeScan = true;
						break;
					}
				}

				if (!closeScan)
				{
					Micro::SmartAbility(b.getUnitPtr(), sc2::ABILITY_ID::EFFECT_SCAN, middlePoint, m_bot);
					m_bot.Commander().Combat().addAllyScan(middlePoint);
				}
			}
		}

		//Mule
		//if (!hasInvisible || energy >= 100)
		{
			std::vector<CCUnitID> skipMinerals;
			for (auto mule : m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_MULE))
			{
				skipMinerals.push_back(m_bot.Workers().getMuleTargetTag(mule));
			}

			CCTilePosition orbitalPosition;
			const sc2::Unit* closestMineral = nullptr;
			auto & bases = m_bot.Bases().getBaseLocations();//Sorted by closest to enemy base
			// Check twice the base locations, but skip the under attack check the second time
			for (int i = 0; i < 2; ++i)
			{
				for (auto base : bases)
				{
					if (!base->isOccupiedByPlayer(Players::Self))
						continue;

					if (i == 0 && base->isUnderAttack())
						continue;

					auto & depot = base->getResourceDepot();
					if (!depot.isValid() || !depot.isCompleted())
						continue;

					closestMineral = getLargestCloseMineral(depot, false, skipMinerals);
					if (closestMineral == nullptr)
					{
						continue;
					}
					orbitalPosition = base->getCenterOfMinerals();

					break;
				}
				if (orbitalPosition != CCPosition())
					break;
			}
			
			if (closestMineral == nullptr)
			{
				//If none of our bases fit the requirements (have minerals + not underattack), drop on closest mineral
				closestMineral = getClosestMineral(b.getPosition());

				if (closestMineral == nullptr)
				{
					Util::DebugLog(__FUNCTION__, "No mineral found.", m_bot);
					continue;
				}
			}

			auto point = closestMineral->pos;

			if (m_bot.Config().StarCraft2Version < "4.11.0" && orbitalPosition != CCPosition())//Validate version because we can drop the mule straigth on the mineral past this version
			{
				//Get the middle point. Then the middle point of the middle point, then again... so we get a point at 7/8 of the way to the mineral from the Orbital command.
				point.x = (point.x + (point.x + (point.x + orbitalPosition.x) / 2) / 2) / 2;
				point.y = (point.y + (point.y + (point.y + orbitalPosition.y) / 2) / 2) / 2;
			}

			Micro::SmartAbility(b.getUnitPtr(), sc2::ABILITY_ID::EFFECT_CALLDOWNMULE, point, m_bot);
		}
	}

	LiftOrLandDamagedBuildings();
}

void BuildingManager::RunProxyLogic()
{
	if (m_bot.Strategy().getStartingStrategy() == PROXY_CYCLONES)
	{
		/*
		Orders in order
		¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
		Barracks
		Reaper
		Factory
		Techlab
		Lift B
		Land B
		Reaper
		Lift F
		Land F
		Cyclone
		Lift B
		*/
		const auto & barracks = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKS);
		const auto & factories = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_FACTORY);
		const auto & techlabs = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_TECHLAB);
		const auto & barracksTechlabs = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB);
		const auto & flyingBarracks = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING);
		// Lift Barracks
		if (m_proxyBarracksPosition == CCPosition())
		{
			if (barracks.size() == 1 && factories.size() == 1 && barracksTechlabs.size() == 1 && barracksTechlabs[0].getBuildPercentage() == 1.0f)
			{
				Micro::SmartAbility(barracks[0].getUnitPtr(), sc2::ABILITY_ID::LIFT, m_bot);
				return;
			}
			if (!flyingBarracks.empty())
			{
				m_proxyBarracksPosition = flyingBarracks[0].getPosition();
				m_buildingPlacer.reserveTiles(m_proxyBarracksPosition.x, m_proxyBarracksPosition.y, 3, 3);
				return;
			}
		}

		// Land Barracks
		if (!m_barracksSentToEnemyBase && flyingBarracks.size() == 1)
		{
			// Called every frame so the barracks can choose a new location if it gets blocked
			const auto barracksFlyingType = UnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING, m_bot);
			const auto barracksBuilding = Building(barracksFlyingType, m_proxyBarracksPosition);
			const auto landingPosition = Util::GetPosition(m_bot.Buildings().getBuildingPlacer().getBuildLocationNear(barracksBuilding, 0, false, true, true));
			Micro::SmartAbility(flyingBarracks[0].getUnitPtr(), sc2::ABILITY_ID::LAND, landingPosition, m_bot);
		}

		// Lift Factory
		if (m_proxyFactoryPosition == CCPosition())
		{
			if (factories.size() == 1 && factories[0].getBuildPercentage() == 1.0f && techlabs.size() == 1 && techlabs[0].getBuildPercentage() == 1.0f)
			{
				m_proxyFactoryPosition = factories[0].getPosition();
				Micro::SmartAbility(factories[0].getUnitPtr(), sc2::ABILITY_ID::LIFT, m_bot);
				return;
			}
		}

		// Land Factory
		if (!m_proxySwapDone && m_proxyBarracksPosition != CCPosition())
		{
			const auto & flyingFactories = m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING);
			if (flyingFactories.size() == 1)
			{
				m_proxySwapInitiated = true;
				Micro::SmartAbility(flyingFactories[0].getUnitPtr(), sc2::ABILITY_ID::LAND, m_proxyBarracksPosition, m_bot);
			}
			else if (m_proxySwapInitiated)
			{
				m_proxySwapDone = true;
			}
		}

		// Lift Barracks and let the CombatCommander take control of it
		if (!m_barracksSentToEnemyBase)
		{
			const auto totalReapers = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Reaper.getUnitType(), true) + m_bot.GetDeadAllyUnitsCount(sc2::UNIT_TYPEID::TERRAN_REAPER);
			if (!barracks.empty() && totalReapers >= 2)
			{
				Micro::SmartAbility(barracks[0].getUnitPtr(), sc2::ABILITY_ID::LIFT, m_bot);
				m_barracksSentToEnemyBase = true;
			}
		}
	}
}

void BuildingManager::LiftOrLandDamagedBuildings()
{
	const std::vector<sc2::UNIT_TYPEID> liftingTypes = {
		sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER,
		sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND,
		sc2::UNIT_TYPEID::TERRAN_BARRACKS,
		sc2::UNIT_TYPEID::TERRAN_FACTORY,
		sc2::UNIT_TYPEID::TERRAN_STARPORT
	};
	const std::vector<sc2::UNIT_TYPEID> landingTypes = {
		sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING,
		sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING,
		sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING,
		sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING,
		sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING
	};
	for (const auto & unitPair : m_bot.GetAllyUnits())
	{
		const auto & unit = unitPair.second;
		if (!unit.isValid())
			continue;
		// If the unit can lift
		if (Util::Contains(unit.getAPIUnitType(), liftingTypes))
		{
			// If the building is damaged
			if (unit.getHitPointsPercentage() <= 50.f && unit.getUnitPtr()->build_progress >= 1.f)
			{
				// We don't want to lift a wall building
				if (Util::Contains(unit, m_wallBuildings))
					continue;
				// And there is danger on the ground but not in the air
				const auto recentDamageTaken = m_bot.Analyzer().getUnitState(unit.getUnitPtr()).GetRecentDamageTaken();
				const bool takenTooMuchDamage = recentDamageTaken >= 50.f || (unit.getHitPointsPercentage() <= 25.f && recentDamageTaken >= 10.f);
				const float airInfluence = Util::PathFinding::GetTotalInfluenceOnTiles(unit.getPosition(), true, unit.getUnitPtr()->radius, m_bot);
				if (takenTooMuchDamage && airInfluence <= 0.f)
				{
					if (unit.isTraining())
					{
						// We don't want to cancel army units, so we only cancel the production of SCV
						if (unit.getType().isResourceDepot())
						{
							Micro::SmartAbility(unit.getUnitPtr(), sc2::ABILITY_ID::CANCEL_LAST, m_bot);
						}
					}
					else
					{
						Micro::SmartAbility(unit.getUnitPtr(), sc2::ABILITY_ID::LIFT, m_bot);
						liftedBuildingPositions[unit.getTag()] = unit.getPosition();
					}
				}
			}
		}
		// If the unit can land
		else if (Util::Contains(unit.getAPIUnitType(), landingTypes))
		{
			CCPosition landingPosition = liftedBuildingPositions[unit.getTag()];
			if (landingPosition != CCPosition())
			{
				// If there is no more danger on the ground
				if (Util::PathFinding::GetTotalInfluenceOnTiles(landingPosition, false, unit.getUnitPtr()->radius, m_bot) <= 0.f)
				{
					Micro::SmartAbility(unit.getUnitPtr(), sc2::ABILITY_ID::LAND, landingPosition, m_bot);
				}
			}
		}
	}
}