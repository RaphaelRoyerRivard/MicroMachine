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

	auto bases = m_bot.Bases().getBaseLocations();
	for (auto baseLocation : bases)
	{
		auto basePosition = CCTilePosition(baseLocation->getDepotPosition().x, baseLocation->getDepotPosition().y);
		auto minerals = baseLocation->getMinerals();
		for (auto mineral : minerals)
		{
			reserveTiles(basePosition, mineral.getTilePosition());
		}

		auto geysers = baseLocation->getGeysers();
		for (auto geyser : geysers)
		{
			reserveTiles(basePosition, CCTilePosition(geyser.getTilePosition().x + 1, geyser.getTilePosition().y + 1));//+1 so we have the center tile instead of the bottom left.
		}
	}
}

// makes final checks to see if a building can be built at a certain location
bool BuildingPlacer::canBuildHere(int bx, int by, const Building & b, bool ignoreReservedTiles) const
{
	//TODO: Unused, it is outdated, check canBuildHereWithSpace instead
	BOT_ASSERT(true, "Unused, it is outdated, check canBuildHereWithSpace instead");

	// if it overlaps a base location return false
	if (!ignoreReservedTiles && tileOverlapsBaseLocation(bx, by, b.type))
	{
		return false;
	}

    // check the reserve map
    for (int x = bx; x < bx + b.type.tileWidth(); x++)
    {
        for (int y = by; y < by + b.type.tileHeight(); y++)
        {
            if ((!ignoreReservedTiles && m_reserveMap[x][y]) || !buildable(b.type, x, y, ignoreReservedTiles))
            {
                return false;
            }
        }
    }
	//TODO removed unneeded Query
	/*if (!ignoreReservedTiles && !m_bot.Map().canBuildTypeAtPosition(bx, by, b.type))
	{
		return false;
	}*/
	return true;
}

//returns true if we can build this type of unit here with the specified amount of space.
bool BuildingPlacer::canBuildHereWithSpace(int bx, int by, const Building & b, int buildDist, bool ignoreReserved, bool checkInfluenceMap) const
{
	UnitType type = b.type;

    // height and width of the building
    int width  = b.type.tileWidth();
    int height = b.type.tileHeight();

    // define the rectangle of the building spot
    int startx = bx - buildDist;
    int starty = by - buildDist;
    int endx   = bx + width + buildDist;
    int endy   = by + height + buildDist;

    // if this rectangle doesn't fit on the map we can't build here
	const CCPosition mapMin = m_bot.Map().mapMin();
	const CCPosition mapMax = m_bot.Map().mapMax();
    if (startx < mapMin.x || starty < mapMin.y || endx >= mapMax.x || endy >= mapMax.y || endx < bx + width)
    {
        return false;
    }

	//If its not safe. We only check one tile since its very likely to be the save result for all tiles. This avoid a little bit of lag.
	if (checkInfluenceMap && Util::PathFinding::HasInfluenceOnTile(CCTilePosition(bx, by), false, m_bot))
	{
		//TODO don't think this can happen, there is a check earlier
		return false;
	}
	
    // if we can't build here, or space is reserved, we can't build here
    for (int x = startx; x < endx; x++)
    {
        for (int y = starty; y < endy; y++)
        {
            if (!b.type.isRefinery())
            {
                if ((!ignoreReserved && m_reserveMap[x][y]) || !buildable(b.type, x, y))
                {
                    return false;
                }
            }
        }
    }

	//Test if there is space for an addon
	switch ((sc2::UNIT_TYPEID)b.type.getAPIUnitType())
	{
		case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
		case sc2::UNIT_TYPEID::TERRAN_FACTORY:
		case sc2::UNIT_TYPEID::TERRAN_STARPORT:
		{
			for (int x = 0; x < 2; x++)
			{
				for (int y = 0; y < 2; y++)
				{
					if ((!ignoreReserved && m_reserveMap[startx + width + x][starty + y]) || !buildable(b.type, startx + width + x, starty + y, false))
					{
						return false;
					}
				}
			}
			break;
		}
		default:
			break;
	}

	//Validate tiles below are free so units don't likely get stuck
	switch ((sc2::UNIT_TYPEID)b.type.getAPIUnitType())
	{
		case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
		case sc2::UNIT_TYPEID::TERRAN_FACTORY:
		case sc2::UNIT_TYPEID::PROTOSS_GATEWAY:
		case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
		{
			for (int x = 0; x < 3; x++)
			{
				if ((!ignoreReserved && m_reserveMap[startx + width + x][starty - 1]) || !buildable(b.type, startx + width + x, starty - 1, false))
				{
					return false;
				}
			}
			break;
		}
		default:
			break;
	}
	m_bot.StopProfiling("0.1 AddonLagCheck");
    return true;
}

CCTilePosition BuildingPlacer::getBuildLocationNear(const Building & b, int buildDist, bool ignoreReserved, bool checkInfluenceMap) const
{
	//If the space is not walkable, look arround for a walkable space. The result may not be the most optimal location.
	const int MAX_OFFSET = 5;
	int offset = 1;
	int direction = 0;
	auto buildLocation = b.desiredPosition;

	while(!m_bot.Map().isWalkable(buildLocation) ||
		m_bot.Map().getClosestTilesTo(buildLocation).size() < 10 ||
		(checkInfluenceMap && Util::PathFinding::HasInfluenceOnTile(buildLocation, false, m_bot)))
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

        if (canBuildHereWithSpace(pos.x, pos.y, b, buildDist, ignoreReserved, checkInfluenceMap))
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
        int bx1 = (int)base->getDepotPosition().x;
        int by1 = (int)base->getDepotPosition().y;
        int bx2 = bx1 + Util::GetTownHall(m_bot.GetSelfRace(), m_bot).tileWidth();
        int by2 = by1 + Util::GetTownHall(m_bot.GetSelfRace(), m_bot).tileHeight();

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
	//Do not check for reservedTiles here, bool is not properly named.
	// TODO: doesnt take units on the map into account
	//ignoreReservedTiles is used for more than just ignoring reserved tiles.

	//Check if tiles are blocked
	if (!ignoreReservedTiles && !type.isGeyser())
	{
		if (m_bot.Commander().Combat().isTileBlocked(x - 1, y - 1))
		{
			return false;
		}
	}

	//Check for supply depot in the way, they are not in the blockedTiles map
	for (auto & b : m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED))//TODO could be simplified
	{
		CCTilePosition position = b.getTilePosition();
				
		//(x, y) is inside a supply depot
		if (position.x == x && position.y == y)
		{
			return false;
		}
		if (position.x + 1 == x && position.y == y)
		{
			return false;
		}
		if (position.x == x && position.y + 1 == y)
		{
			return false;
		}
		if (position.x + 1 == x && position.y + 1 == y)
		{
			return false;
		}
	}
	
	//check if buildable
	if (!ignoreReservedTiles && !m_bot.Map().isBuildable(x, y))
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
    int rwidth = (int)m_reserveMap.size();
    int rheight = (int)m_reserveMap[0].size();
    for (int x = bx; x < bx + width && x < rwidth; x++)
    {
        for (int y = by; y < by + height && y < rheight; y++)
        {
            m_reserveMap[x][y] = true;
        }
    }
}

void BuildingPlacer::reserveTiles(CCTilePosition start, CCTilePosition end)
{
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
				m_bot.Map().drawTile(x - 1, y - 1, yellow);
			}
        }
    }
}

void BuildingPlacer::freeTiles(int bx, int by, int width, int height)
{
    int rwidth = (int)m_reserveMap.size();
    int rheight = (int)m_reserveMap[0].size();

    for (int x = bx; x < bx + width && x < rwidth; x++)
    {
        for (int y = by; y < by + height && y < rheight; y++)
        {
            m_reserveMap[x][y] = false;
        }
    }
}

void BuildingPlacer::freeTilesForTurrets(CCTilePosition position)
{
	freeTiles(position.x, position.y, 2, 2);
}

CCTilePosition BuildingPlacer::getRefineryPosition()
{
	m_bot.StartProfiling("getRefineryPosition");
    CCPosition closestGeyser(0, 0);
    double minGeyserDistanceFromHome = std::numeric_limits<double>::max();
    CCPosition homePosition = m_bot.GetStartLocation();
	auto units = m_bot.UnitInfo().getUnits(Players::Self);

    for (auto & geyser : m_bot.UnitInfo().getUnits(Players::Neutral))
    {
        if (!geyser.getType().isGeyser())
        {
            continue;
        }

        CCPosition geyserPos(geyser.getPosition());

		//Check if refinery is already assigned to a building task (m_building)
		auto assigned = false;
		for (auto & refinery : m_bot.Buildings().getBuildings())
		{
			if (!refinery.type.isGeyser())
			{
				continue;
			}

			if (Util::GetTilePosition(geyserPos) == refinery.finalPosition)
			{
				assigned = true;
				break;
			}
		}
		if (assigned)
		{
			continue;
		}

		for (auto & unit : units)
		{
			// check to see if it's next to one of our depots
			if (unit.getType().isResourceDepot() && Util::DistSq(unit, geyserPos) < 10 * 10)
			{
				//TODO Good?
				//Skip base if underattack or if we can't figure out to which base it belongs
				auto baseLocation = m_bot.Bases().getBaseContainingPosition(geyser.getPosition(), Players::Self);
				if (baseLocation != nullptr &&baseLocation->isUnderAttack())
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
    }
	m_bot.StopProfiling("getRefineryPosition");

#ifdef SC2API
    return CCTilePosition((int)closestGeyser.x, (int)closestGeyser.y);
#else
    return CCTilePosition(closestGeyser);
#endif
}

bool BuildingPlacer::isReserved(int x, int y) const
{
    int rwidth = (int)m_reserveMap.size();
    int rheight = (int)m_reserveMap[0].size();
    if (x < 0 || y < 0 || x >= rwidth || y >= rheight)
    {
        return false;
    }

    return m_reserveMap[x][y];
}