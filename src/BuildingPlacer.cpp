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
    m_reserveMap = std::vector< std::vector<bool> >(m_bot.Map().totalWidth(), std::vector<bool>(m_bot.Map().totalHeight(), false));
	//[STUCK]TODO create other map here

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
		}

		auto geysers = baseLocation->getGeysers();
		for (auto geyser : geysers)
		{
			reserveTiles(basePosition, geyser.getTilePosition());
		}
	}
}

bool BuildingPlacer::canBuildDepotHere(int bx, int by, std::vector<Unit> minerals, std::vector<Unit> geysers) const
{
	UnitType depot = Util::GetResourceDepotType();
	if (canBuildHere(bx, by, depot, 0, true, false, false))//Do not need to check if interesting other buildings, since it is called at the start of the game only.
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
	if (canBuildHere(bx, by, MetaTypeEnum::Bunker.getUnitType(), 0, true, false, false))//Do not need to check if interesting other buildings, since it is called at the start of the game only.
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
bool BuildingPlacer::canBuildHere(int bx, int by, const UnitType & type, int buildDistAround, bool ignoreReserved, bool checkInfluenceMap, bool includeExtraTiles) const
{
	// height and width of the building
	int width = type.tileWidth();
	int height = type.tileHeight();

	// define the rectangle of the building spot
	int x = bx - buildDistAround;
	int y = by - buildDistAround;

	//If its not safe. We only check one tile since its very likely to be the save result for all tiles. This avoid a little bit of lag.
	if (checkInfluenceMap && Util::PathFinding::HasCombatInfluenceOnTile(CCTilePosition(bx, by), false, m_bot))
	{
		//TODO don't think this can happen, there is a check earlier
		return false;
	}

	auto tiles = getTilesForBuildLocation(x, y, type, width + buildDistAround * 2, height + buildDistAround * 2, includeExtraTiles);//Include padding (buildDist) so we test all the tiles
	auto buildingTerrainHeight = -1;
	if (!type.isRefinery())
	{
		for (auto tile : tiles)
		{
			//Validate reserved tiles and buildable
			if (!buildable(type, tile.x, tile.y, ignoreReserved))
			{
				return false;
			}
		}
	}
	else
	{
		if (!isGeyserAssigned(CCTilePosition(x, y)))//Validate the geyser isnt already used.
		{
			return false;
		}
	}

    return !isEnemyUnitBlocking(CCTilePosition(bx, by), type);
}

bool BuildingPlacer::isEnemyUnitBlocking(CCTilePosition center, UnitType type) const
{
	for (auto tagUnit : m_bot.GetEnemyUnits())
	{
		if (tagUnit.second.getType().isBuilding())
		{
			continue;
		}

		if (intersects(tagUnit.second, Util::GetPosition(center), type.radius()))
		{
			return true;
		}
	}
	return false;
}

//Check if a unit and a building location intersect
bool BuildingPlacer::intersects(Unit unit, CCPosition buildingAbsoluteCenter, int buildingRadius) const
{
	auto x = unit.getPosition().x;
	auto y = unit.getPosition().y;
	auto r = unit.getUnitPtr()->radius;

	float distanceX = abs(x - buildingAbsoluteCenter.x);
	float distanceY = abs(y - buildingAbsoluteCenter.y);

	float distance_sq = pow(distanceX, 2) + pow(distanceY, 2);

	return distance_sq <= pow(r + buildingRadius, 2);
}

std::vector<CCTilePosition> BuildingPlacer::getTilesForBuildLocation(Unit building) const
{
	BOT_ASSERT(building.getType().isBuilding(), "Should not call getTilesForBuildLocation on a none building unit.");
	auto position = building.getTilePosition();
	auto type = building.getType();
	return getTilesForBuildLocation(position.x, position.y, type, type.tileWidth(), type.tileHeight(), true);
}

std::vector<CCTilePosition> BuildingPlacer::getTilesForBuildLocation(int bx, int by, const UnitType & type, int width, int height, bool includeExtraTiles) const
{
	//width and height are not taken from the Type to allow a padding around the building of we want to.
	int offset = getBuildingCenterOffset(bx, by, width, height);

	int x = bx - offset;
	int y = by - offset;

	//tiles for the actual building
	std::vector<CCTilePosition> tiles;
	for (int i = 0; i < width; i++)
	{
		for (int j = 0; j < height; j++)
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
					for (int i = 0; i < 2; i++)
					{
						for (int j = 0; j < 2; j++)
						{
							tiles.push_back(CCTilePosition(x + width + i, y + j));
						}
					}
				}
			}
		}

		//tiles below for the building exit
		switch ((sc2::UNIT_TYPEID)type.getAPIUnitType())
		{
			case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
			case sc2::UNIT_TYPEID::TERRAN_FACTORY:
			case sc2::UNIT_TYPEID::PROTOSS_GATEWAY:
			case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
			{
				for (int i = 0; i < width; i++)
				{
					tiles.push_back(CCTilePosition(x + i, y - 1));
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

CCTilePosition BuildingPlacer::getBuildLocationNear(const Building & b, int buildDist, bool ignoreReserved, bool checkInfluenceMap, bool includeExtraTiles) const
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

    // iterate through the list until we've found a suitable location
    for (size_t i(0); i < closestToBuilding.size(); ++i)
    {
        auto & pos = closestToBuilding[i];

        if (canBuildHere(pos.x, pos.y, b.type, buildDist, ignoreReserved, checkInfluenceMap, includeExtraTiles))
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

	//Prevents buildings from being built next to each other, expect for defensive buildings (turret, bunker, photo cannon, spine and spore) and lowered supply depot.
	//This WILL prevent Terran/Protoss from being able to walls with anything else than offensive buildings, some changes will need to be made to make it possible.
	//[STUCK]TODO Check unittype to know if we need to validate the tile further or not
	//[STUCK]Should be ignored for all offensive buildings.

	//Check for supply depot in the way, they are not in the blockedTiles map
	for (auto & b : m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED))//TODO could be simplified
	{
		CCTilePosition position = b.getTilePosition();
		auto tiles = getTilesForBuildLocation(position.x, position.y, b.getType(), 2, 2, false);
		for (auto tile : tiles)
		{
			if (tile.x == x && tile.y == y)
			{
				return false;
			}
		}
	}
	
	//check reserved tiles
	if (!ignoreReservedTiles && m_reserveMap[x][y])
	{
		return false;
	}
	
	//check if buildable
	if (!m_bot.Map().isBuildable(x, y) && !type.isGeyser())
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

void BuildingPlacer::reserveTiles(int bx, int by, int width, int height)
{
	//[STUCK]TODO This logic has to change a bit or a new function needs to be added

	auto tiles = getTilesForBuildLocation(bx, by, UnitType(), width, height, true);
	for (auto tile : tiles)
	{
		m_reserveMap[tile.x][tile.y] = true;
	}
}

void BuildingPlacer::reserveTiles(CCTilePosition start, CCTilePosition end)//Used only for zones, not for buildings. Like mineral all the way to the depot for example.
{
	//[STUCK]TODO This logic has to change a bit or a new function needs to be added

	int rwidth = (int)m_reserveMap.size();
	int rheight = (int)m_reserveMap[0].size();
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
			m_reserveMap[x][y] = true;
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

    int rwidth = (int)m_reserveMap.size();
    int rheight = (int)m_reserveMap[0].size();

	CCColor yellow = CCColor(255, 255, 0);
    for (int x = 0; x < rwidth; ++x)
    { 
        for (int y = 0; y < rheight; ++y)
        {
			if (m_reserveMap[x][y])
			{
				m_bot.Map().drawTile(x, y, yellow);
			}
        }
    }

	//[STUCK]TODO draw other reserved tile type in a different color
}

void BuildingPlacer::freeTiles(int bx, int by, int width, int height)
{
	//[STUCK]TODO This logic has to change a bit or a new function needs to be added

	auto tiles = getTilesForBuildLocation(bx, by, UnitType(), width, height, true);
	for (auto tile : tiles)
	{
		m_reserveMap[tile.x][tile.y] = false;
	}
}

void BuildingPlacer::freeTilesForTurrets(CCTilePosition position)
{
	freeTiles(position.x, position.y, 2, 2);
}

void BuildingPlacer::freeTilesForBunker(CCTilePosition position)
{
	freeTiles(position.x, position.y, 3, 3);
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
	//[STUCK]TODO This logic has to change a bit or a new function needs to be added

    int rwidth = (int)m_reserveMap.size();
    int rheight = (int)m_reserveMap[0].size();
    if (x < 0 || y < 0 || x >= rwidth || y >= rheight)
    {
        return false;
    }

    return m_reserveMap[x][y];
}