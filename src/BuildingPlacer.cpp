#include "Common.h"
#include "BuildingPlacer.h"
#include "CCBot.h"
#include "Building.h"
#include "Util.h"

BuildingPlacer::BuildingPlacer(CCBot & bot)
    : m_bot(bot)
{

}

void BuildingPlacer::onStart()
{
	m_resourceBlockedTiles = std::vector< std::vector<bool> >(m_bot.Map().totalWidth(), std::vector<bool>(m_bot.Map().totalHeight(), false));
    m_reserveBuildingMap = std::vector< std::vector<bool> >(m_bot.Map().totalWidth(), std::vector<bool>(m_bot.Map().totalHeight(), false));
#ifdef COMPUTE_WALKABLE_TILES
	m_reserveWalkableMap = std::vector< std::vector<bool> >(m_bot.Map().totalWidth(), std::vector<bool>(m_bot.Map().totalHeight(), false));
#endif

	auto bases = m_bot.Bases().getBaseLocations();
	for (auto baseLocation : bases)
	{
		const auto depotPosition = Util::GetPosition(baseLocation->getDepotTilePosition());
		const auto centerOfMinerals = Util::GetPosition(baseLocation->getCenterOfMinerals());
		const auto towardsOutside = Util::Normalized(depotPosition - centerOfMinerals);
		const auto basePosition = Util::GetTilePosition(depotPosition + towardsOutside * 2);
		auto minerals = baseLocation->getMinerals();
		for (auto mineral : minerals)
		{
			reserveTiles(basePosition, mineral.getTilePosition());
			
			//m_resourceBlockedTiles used to expand in main, minerals are not 100% accurate, but close enough
			for (int x = -4; x <= 3; x++)
			{
				for (int y = -3; y <= 3; y++)
				{
					if (abs(x) == abs(y))
						continue;
					m_resourceBlockedTiles[mineral.getTilePosition().x + x][mineral.getTilePosition().y + y] = true;
				}
			}
		}

		auto geysers = baseLocation->getGeysers();
		for (auto geyser : geysers)
		{
			reserveTiles(basePosition, geyser.getTilePosition());

			//m_resourceBlockedTiles used to expand in main
			for (int x = -4; x <= 4; x++)
			{
				for (int y = -4; y <= 4; y++)
				{
					if (abs(x) == abs(y))
						continue;
					m_resourceBlockedTiles[geyser.getTilePosition().x + x][geyser.getTilePosition().y + y] = true;
				}
			}
		}
	}
}

bool BuildingPlacer::canBuildDepotHere(int bx, int by, std::vector<Unit> minerals, std::vector<Unit> geysers) const
{
	if (canBuildHere(bx, by, Util::GetResourceDepotType(), true, false, false))//Do not need to check if interesting other buildings, since it is called at the start of the game only.
	{
		// check the reserve map
		for (int x = bx - 2; x <= bx + 2; x++)
		{
			for (int y = by - 2; y <= by + 2; y++)
			{
				if (m_bot.Bases().isInProximityOfResources(x, y))
				{
					return false;
				}
			}
		}
		return true;
	}
	return false;
}

bool BuildingPlacer::canBuildBunkerHere(int bx, int by, int depotX, int depotY, std::vector<CCPosition> geysersPos) const
{
	if (canBuildHere(bx, by, MetaTypeEnum::Bunker.getUnitType(), true, false, false))//Do not need to check if interesting other buildings, since it is called at the start of the game only.
	{
		// check intersecting depot
		for (int x = bx - 1; x <= bx + 1; x++)
		{
			for (int y = by - 1; y <= by + 1; y++)
			{
				for (int dx = depotX - 2; dx <= depotX + 2; dx++)
				{
					for (int dy = depotY - 2; dy <= depotY + 2; dy++)
					{
						if (x == dx && y == dy)
						{
							return false;
						}
					}
				}
			}
		}

		// check next to depot
		bool nextToDepot = false;
		for (int x = bx - 1; x <= bx + 1; x++)
		{
			for (int y = by - 1; y <= by + 1; y++)
			{
				for (int dx = -3; dx <= 3; dx++)
				{
					for (int dy = -3; dy <= 3; dy++)
					{
						if (abs(dx) == abs(dy))//Prevent bunker and depot from being diagonally adjacent, since its not efficient
						{
							continue;
						}
						if (x == depotX + dx && y == depotY + dy)
						{
							nextToDepot = true;
							break;
						}
					}
				}
			}
		}
		if (!nextToDepot)
		{
			return false;
		}

		//check too close to geyser
		/*for (auto geyserPos : geysersPos)
		{
			for (int x = bx - 1; x <= bx + 1; x++)
			{
				for (int y = by - 1; y <= by + 1; y++)
				{
					for (int dx = geyserPos.x - 1; dx <= geyserPos.x + 1; dx++)
					{
						for (int dy = geyserPos.y - 1; dy <= geyserPos.y + 1; dy++)
						{
							if (x == dx && y == dy)
							{
								return false;
							}
						}
					}
				}
			}
		}*/
		return true;
	}
	return false;
}

//returns true if we can build this type of unit here with the specified amount of space.
bool BuildingPlacer::canBuildHere(int bx, int by, const UnitType & type, bool ignoreReserved, bool checkInfluenceMap, bool includeExtraTiles, bool ignoreExtraBorder) const
{
	// height and width of the building
	int width = type.tileWidth();
	int height = type.tileHeight();

	//If its not safe. We only check one tile since its very likely to be the same result for all tiles. This avoid a little bit of lag.
	if (checkInfluenceMap && Util::PathFinding::HasCombatInfluenceOnTile(CCTilePosition(bx, by), false, m_bot))
	{
		return false;
	}

	if (!type.isRefinery())
	{
		auto buildingTiles = getTilesForBuildLocation(bx, by, type, width, height, includeExtraTiles, 0);
		auto walkableTiles = getTilesForBuildLocation(bx, by, type, width, height, includeExtraTiles, ignoreExtraBorder ? 0 : 1);
		for (auto & tile : walkableTiles)
		{
			if (!m_bot.Map().isValidTile(tile))//prevent tiles below 0 or above the map size
			{
				return false;
			}

			if (std::find(buildingTiles.begin(), buildingTiles.end(), tile) != buildingTiles.end())//If its a buildingTile
			{
				//Validate reserved tiles and buildable
				if (!buildable(type, tile.x, tile.y, ignoreReserved))
				{
					return false;
				}
			}
			else if(!ignoreReserved //If its (only) a walkableTile
				&& type.getAPIUnitType() != sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)//Skip the walkable tiles for supply depots, since they will be walkable tiles in the end
			{
				if (!buildable(type, tile.x, tile.y, false))//Check for the size is to prevent a frame 1 crash
				{//The walkable tile is reserved to build something
					return false;
				}
			}
		}
	}
	else
	{
		if (!isGeyserAssigned(CCTilePosition(bx, by)))//Validate the geyser isnt already used.
		{
			return false;
		}
	}

	return true;
}

bool BuildingPlacer::isBuildingBlockedByCreep(CCTilePosition pos, UnitType type) const
{
	//Similar code can be found in BuildingPlacer.canBuildHere
	auto tiles = getTilesForBuildLocation(pos.x, pos.y, type, type.tileWidth(), type.tileHeight(), false, 0);
	for (auto & tile : tiles)
	{
		//Validate if the tile has creep blocking the expand
		if (m_bot.Observation()->HasCreep(CCPosition(tile.x, tile.y)))
		{
			return true;
			break;
		}
	}
	return false;
}

std::vector<CCTilePosition> BuildingPlacer::getTilesForBuildLocation(Unit building) const
{
	BOT_ASSERT(building.getType().isBuilding(), "Should not call getTilesForBuildLocation on a none building unit.");
	auto position = building.getTilePosition();
	auto type = building.getType();
	return getTilesForBuildLocation(position.x, position.y, type, type.tileWidth(), type.tileHeight(), true, 0);
}

std::vector<CCTilePosition> BuildingPlacer::getTilesForBuildLocation(int bx, int by, const UnitType & type, int width, int height, bool includeExtraTiles, int extraBorder) const
{
	//width and height are not taken from the Type to allow a padding around the building of we want to.
	int offset = getBuildingCenterOffset(bx, by, width, height);

	int x = bx - offset;
	int y = by - offset;

	//tiles for the actual building
	std::vector<CCTilePosition> tiles;
	for (int i = -extraBorder; i < width + extraBorder; i++)
	{
		for (int j = -extraBorder; j < height + extraBorder; j++)
		{
			tiles.push_back(CCTilePosition(x + i, y + j));
		}
	}

	if (includeExtraTiles)
	{
		//tiles for the addon
		switch ((sc2::UNIT_TYPEID)type.getAPIUnitType())
		{
			case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
			case sc2::UNIT_TYPEID::TERRAN_FACTORY:
			case sc2::UNIT_TYPEID::TERRAN_STARPORT:
			{
				//Shouldnt validate the addon if the building is in the wall
				if (!m_bot.Buildings().isWallPosition(bx, by))//Must not consider the offset
				{
					for (int i = -extraBorder; i < 2 + extraBorder; i++)
					{
						for (int j = -extraBorder; j < 2 + extraBorder; j++)
						{
							tiles.push_back(CCTilePosition(x + width + i, y + j));
						}
					}
				}
			}
		}
	}

	return tiles;
}

CCTilePosition BuildingPlacer::getBottomLeftForBuildLocation(int bx, int by, const UnitType & type) const
{
	int offset = getBuildingCenterOffset(bx, by, type.tileWidth(), type.tileHeight());

	return CCTilePosition(bx - offset, by - offset);
}

int BuildingPlacer::getBuildingCenterOffset(int x, int y, int width, int height) const
{
	//Offset (x,y) based on the building center
	switch (width)
	{
		case 2:
		case 3:
			return 1;
		case 5:
			return 2;
		default:
			std::ostringstream oss;
			oss << "No building of size (" << width << ", " << height << ") exists.";
			Util::DisplayError(oss.str(), "0x00000010", m_bot, false);
			return 0;
	}
}

CCTilePosition BuildingPlacer::getBuildLocationNear(const Building & b, bool ignoreReserved, bool checkInfluenceMap, bool includeExtraTiles, bool ignoreExtraBorder, bool forceSameHeight) const
{
	//If the space is not walkable, look around for a walkable space. The result may not be the most optimal location.
	const int MAX_OFFSET = 5;
	int offset = 1;
	int direction = 0;
	auto buildLocation = b.desiredPosition;

	while(!m_bot.Map().isWalkable(buildLocation) ||
		m_bot.Map().getClosestTilesTo(buildLocation).size() < 10 ||
		(checkInfluenceMap && Util::PathFinding::HasCombatInfluenceOnTile(buildLocation, false, m_bot)))
	{
		switch (direction)
		{
			case 0://right
				buildLocation.x = b.desiredPosition.x + offset;
				buildLocation.y = b.desiredPosition.y;
				break;
			case 1://left
				buildLocation.x = b.desiredPosition.x - offset;
				buildLocation.y = b.desiredPosition.y;
				break;
			case 2://up
				buildLocation.x = b.desiredPosition.x;
				buildLocation.y = b.desiredPosition.y + offset;
				break;
			case 3://down
				buildLocation.x = b.desiredPosition.x;
				buildLocation.y = b.desiredPosition.y - offset;
				break;
			case 4://diag up-right
				buildLocation.x = b.desiredPosition.x + offset;
				buildLocation.y = b.desiredPosition.y + offset;
				break;
			case 5://diag up-left
				buildLocation.x = b.desiredPosition.x - offset;
				buildLocation.y = b.desiredPosition.y + offset;
				break;
			case 6://diag down-right
				buildLocation.x = b.desiredPosition.x + offset;
				buildLocation.y = b.desiredPosition.y - offset;
				break;
			case 7://diag down-left
				buildLocation.x = b.desiredPosition.x - offset;
				buildLocation.y = b.desiredPosition.y - offset;
				direction = -1;//will be 0 after the ++
				offset++;
				break;
			default:
				Util::DisplayError("Should never happen [BuildingPlacer::getBuildLocationNear]", "0x00000008", m_bot, false);
				break;
		}
		if (buildLocation.x < 0)
		{
			buildLocation.x = 0;
		}
		if (buildLocation.y < 0)
		{
			buildLocation.y = 0;
		}
		if (buildLocation.x >= m_bot.Map().mapMax().x)
		{
			buildLocation.x = m_bot.Map().mapMax().x - 1;
		}
		if (buildLocation.y >= m_bot.Map().mapMax().y)
		{
			buildLocation.y = m_bot.Map().mapMax().y - 1;
		}

		if (offset == MAX_OFFSET)//Did not find any walkable space within 25 tiles in all directions
		{
			buildLocation = b.desiredPosition;//Avoids crashing, but this won't work well.
		}
		direction++;
	}

    // get the precomputed vector of tile positions which are sorted closes to this location
    auto & closestToBuilding = m_bot.Map().getClosestTilesTo(buildLocation);
	auto desiredHeight = Util::TerrainHeight(buildLocation);

    // iterate through the list until we've found a suitable location
    for (size_t i(0); i < closestToBuilding.size(); ++i)
    {
        auto & pos = closestToBuilding[i];
		auto posHeight = Util::TerrainHeight(pos);
		if (forceSameHeight && posHeight != desiredHeight)
			continue;

        if (canBuildHere(pos.x, pos.y, b.type, ignoreReserved, checkInfluenceMap, includeExtraTiles, ignoreExtraBorder))
        {
			return pos;
        }
    }

    //printf("Building Placer Failure: %s - Took %lf ms\n", b.type.getName().c_str(), ms);
	printf("Building Placer Failure, couldn't find anywhere valide to place it");
    return CCTilePosition(0, 0);
}

CCTilePosition BuildingPlacer::getBunkerBuildLocationNear(const Building & b, int depotX, int depotY, std::vector<CCPosition> geysersPos) const
{ 
	//If the space is not walkable, look arround for a walkable space. The result may not be the most optimal location.
	auto buildLocation = b.desiredPosition;

	// get the precomputed vector of tile positions which are sorted closes to this location
	auto & closestToBuilding = m_bot.Map().getClosestTilesTo(buildLocation);

	// iterate through the list until we've found a suitable location
	for (size_t i(0); i < closestToBuilding.size(); ++i)
	{
		auto & pos = closestToBuilding[i];

		if (canBuildBunkerHere(pos.x, pos.y, depotX, depotY, geysersPos))
		{
			return pos;
		}
	}

	//printf("Building Placer Failure: %s - Took %lf ms\n", b.type.getName().c_str(), ms);
	printf("Building Placer Failure, couldn't find anywhere valide to place it");
	return CCTilePosition(0, 0);
}

bool BuildingPlacer::tileOverlapsBaseLocation(int x, int y, UnitType type) const
{
    // if it's a resource depot we don't care if it overlaps
    if (type.isResourceDepot())
    {
        return false;
    }

    // dimensions of the proposed location
    int tx1 = x;
    int ty1 = y;
    int tx2 = tx1 + type.tileWidth();
    int ty2 = ty1 + type.tileHeight();

    // for each base location
    for (const BaseLocation * base : m_bot.Bases().getBaseLocations())
    {
        // dimensions of the base location
        int bx1 = (int)base->getDepotTilePosition().x;
        int by1 = (int)base->getDepotTilePosition().y;
		int bx2 = bx1 + 4;// Util::GetTownHall(m_bot.GetSelfRace(), m_bot).tileWidth();
		int by2 = by1 + 3;// Util::GetTownHall(m_bot.GetSelfRace(), m_bot).tileHeight();

        // conditions for non-overlap are easy
        bool noOverlap = (tx2 < bx1) || (tx1 > bx2) || (ty2 < by1) || (ty1 > by2);

        // if the reverse is true, return true
        if (!noOverlap)
        {
            return true;
        }
    }

    // otherwise there is no overlap
    return false;
}

bool BuildingPlacer::buildable(const UnitType type, int x, int y, bool ignoreReservedTiles) const
{
	//Validate the position is within the map
	const CCTilePosition min = m_bot.Map().mapMin();
	const CCTilePosition max = m_bot.Map().mapMax();
	if (x < min.x || y < min.y || x >= max.x || y >= max.y)
	{
		return false;
	}

	//Check if tiles are blocked, checks if there is another buildings in the way
	if (!type.isGeyser())
	{
		if (!type.isAddon() && m_bot.Commander().Combat().isTileBlocked(x, y))//Cannot check blocked tiles for addons, otherwise it cancels itself on frame 1.
		{
			return false;
		}
	}

	//TODO Might want to prevents buildings from being built next to each other, expect for defensive buildings (turret, bunker, photo cannon, spine and spore) and lowered supply depot.

	//Check for supply depot in the way, they are not in the blockedTiles map
	std::vector<sc2::UNIT_TYPEID> supplyDepotTypes = { sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED, sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT };
	for (auto supplyDepotType : supplyDepotTypes)
	{
		for (auto & b : m_bot.GetAllyUnits(supplyDepotType))
		{
			CCTilePosition position = b.getTilePosition();
			auto tiles = getTilesForBuildLocation(position.x, position.y, b.getType(), 2, 2, false, 0);
			for (auto & tile : tiles)
			{
				if (tile.x == x && tile.y == y)
				{
					return false;
				}
			}
		}
	}
	
	//check reserved tiles
	if (!ignoreReservedTiles && m_reserveBuildingMap[x][y])
	{
		return false;
	}
	
	//check if buildable
	if (!m_bot.Map().isBuildable(x, y) && !type.isGeyser())
	{
		return false;
	}

	if (type.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER && !m_resourceBlockedTiles.empty() && m_resourceBlockedTiles[x][y])//Used specifically to prevent CC expands built in the main from being in an invalid location
	{
		return false;
	}

	/*if (!ignoreReservedTiles && !m_bot.Observation()->IsPathable(sc2::Point2D(x, y)))
	{
		return false;
	}

	if (!ignoreReservedTiles && !m_bot.Observation()->IsPlacable(sc2::Point2D(x, y)))
	{
		return false;
	}*/

	if (!Util::IsZerg(m_bot.GetSelfRace()) && m_bot.Observation()->HasCreep(CCPosition(x, y)))
	{
		return false;
	}
	return true;
}

void BuildingPlacer::reserveTiles(UnitType type, CCTilePosition pos)//(int bx, int by, int width, int height)
{
	auto buildingTiles = getTilesForBuildLocation(pos.x, pos.y, type, type.tileWidth(), type.tileHeight(), true, 0);
	auto walkableTiles = getTilesForBuildLocation(pos.x, pos.y, type, type.tileWidth(), type.tileHeight(), true, 1);
	for (auto & tile : buildingTiles)
	{
		m_reserveBuildingMap[tile.x][tile.y] = true;
	}
#ifdef COMPUTE_WALKABLE_TILES
	for (auto & tile : walkableTiles)
	{
		m_reserveWalkableMap[tile.x][tile.y] = true;
	}
#endif
}

void BuildingPlacer::reserveTiles(CCTilePosition start, CCTilePosition end)//Used only for zones, not for buildings. Like mineral all the way to the depot for example.
{
	int rwidth = (int)m_reserveBuildingMap.size();
	int rheight = (int)m_reserveBuildingMap[0].size();
	int minX;
	int maxX;
	if (start.x > end.x)
	{
		minX = end.x;
		maxX = start.x;
	}
	else
	{
		minX = start.x;
		maxX = end.x;
	}

	int minY;
	int maxY;
	if (start.y > end.y)
	{
		minY = end.y;
		maxY = start.y;
	}
	else
	{
		minY = start.y;
		maxY = end.y;
	}

	for (int x = minX; x <= maxX && x < rwidth; x++)
	{
		for (int y = minY; y <= maxY && y < rheight; y++)
		{
			m_reserveBuildingMap[x][y] = true;
		}
	}
}

void BuildingPlacer::drawReservedTiles()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
    if (!m_bot.Config().DrawReservedBuildingTiles)
    {
        return;
    }

    int rwidth = (int)m_reserveBuildingMap.size();
    int rheight = (int)m_reserveBuildingMap[0].size();

	CCColor white = CCColor(255, 255, 255);
	CCColor yellow = CCColor(255, 255, 0);
    for (int x = 0; x < rwidth; ++x)
    { 
        for (int y = 0; y < rheight; ++y)
        {
			if (m_reserveBuildingMap[x][y])
			{
				m_bot.Map().drawTile(x, y, yellow);
			}
#ifdef COMPUTE_WALKABLE_TILES
			else if (m_reserveWalkableMap[x][y])
			{
				m_bot.Map().drawTile(x, y, white);
			}
#endif
        }
    }
}

void BuildingPlacer::freeTiles(int bx, int by, int width, int height, bool setBlocked)
{
	auto buildingTiles = getTilesForBuildLocation(bx, by, UnitType(), width, height, true, 0);
	auto walkableTiles = getTilesForBuildLocation(bx, by, UnitType(), width, height, true, 1);
	for (auto & tile : buildingTiles)
	{
		m_reserveBuildingMap[tile.x][tile.y] = false;
		if (setBlocked)
		{
			m_bot.Commander().Combat().setBlockedTile(tile.x, tile.y);
		}
	}
#ifdef COMPUTE_WALKABLE_TILES
	for (auto & tile : walkableTiles)
	{
		m_reserveWalkableMap[tile.x][tile.y] = false;
	}
#endif
}

void BuildingPlacer::freeTilesForTurrets(CCTilePosition position)
{
	freeTiles(position.x, position.y, 2, 2, false);
}

void BuildingPlacer::freeTilesForBunker(CCTilePosition position)
{
	freeTiles(position.x, position.y, 3, 3, false);
}

CCTilePosition BuildingPlacer::getRefineryPosition()
{
	m_bot.StartProfiling("getRefineryPosition");
    CCPosition closestGeyser(0, 0);
    double minGeyserDistanceFromHome = std::numeric_limits<double>::max();
    CCPosition homePosition = m_bot.GetStartLocation();
	//auto& depots = m_bot.GetAllyDepotUnits();
	auto& bases = m_bot.Bases().getOccupiedBaseLocations(Players::Self);

	for (auto & base : bases)
	{
		//We validate the player because if the opponent expand to a location we own (proxy), we don't want to build refineries there.
		// base under attack condition was commented because it broke the bot when getting cannon rushed and also there is already pathfinding done to make sure the path to the geyser is safe.
		if (!base->getResourceDepot().isValid() || base->getResourceDepot().getPlayer() != Players::Self)// || base->isUnderAttack())
		{
			continue;
		}

		for (auto & geyser : base->getGeysers())
		{
			CCPosition geyserPos(geyser.getPosition());
			CCTilePosition geyserTilePos = Util::GetTilePosition(geyserPos);

			if (isGeyserAssigned(geyserTilePos))
			{
				continue;
			}

			const double homeDistance = Util::DistSq(geyser, homePosition);
			if (homeDistance < minGeyserDistanceFromHome)
			{
				if (m_bot.Query()->Placement(sc2::ABILITY_ID::BUILD_REFINERY, geyserPos))
				{
					minGeyserDistanceFromHome = homeDistance;
					closestGeyser = geyserPos;
				}
			}
			break;
		}
	}
	m_bot.StopProfiling("getRefineryPosition");

#ifdef SC2API
    return Util::GetTilePosition(closestGeyser);
#else
    return CCTilePosition(closestGeyser);
#endif
}

bool BuildingPlacer::isGeyserAssigned(CCTilePosition geyserTilePos) const
{
	//Check if refinery is already assigned to a building task (m_building)
	for (auto & refinery : m_bot.GetAllyUnits(Util::GetRefineryType().getAPIUnitType()))
	{
		if (geyserTilePos == refinery.getTilePosition())
		{
			return true;
		}
	}
	for (auto & b : m_bot.Buildings().getBuildings())
	{
		if (b.buildCommandGiven && b.finalPosition == geyserTilePos)
		{
			return true;
		}
	}
	return false;
}

bool BuildingPlacer::isReserved(int x, int y) const
{
    int rwidth = (int)m_reserveBuildingMap.size();
    int rheight = (int)m_reserveBuildingMap[0].size();
    if (x < 0 || y < 0 || x >= rwidth || y >= rheight)
    {
        return false;
    }

    return m_reserveBuildingMap[x][y];
}