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
bool BuildingPlacer::canBuildHere(int bx, int by, const Building & b) const
{
	//TODO: Unused, it is outdated, check canBuildHereWithSpace instead
	BOT_ASSERT(true, "Unused, it is outdated, check canBuildHereWithSpace instead");

    // check the reserve map
    for (int x = bx; x < bx + b.type.tileWidth(); x++)
    {
        for (int y = by; y < by + b.type.tileHeight(); y++)
        {
            if (!buildable(b.type, x, y) || m_reserveMap[x][y])
            {
                return false;
            }
        }
    }

    // if it overlaps a base location return false
    if (tileOverlapsBaseLocation(bx, by, b.type))
    {
        return false;
    }

    return true;
}

//returns true if we can build this type of unit here with the specified amount of space.
bool BuildingPlacer::canBuildHereWithSpace(int bx, int by, const Building & b, int buildDist, bool ignoreReserved) const
{
    UnitType type = b.type;

    //if we can't build here, we of course can't build here with space (it is checked again in the loop below)
    if (!buildable(b.type, bx, by) || (!ignoreReserved && m_reserveMap[bx][by]))
    {
        return false;
    }

    // height and width of the building
    int width  = b.type.tileWidth();
    int height = b.type.tileHeight();

    // define the rectangle of the building spot
    int startx = bx - buildDist;
    int starty = by - buildDist;
    int endx   = bx + width + buildDist;
    int endy   = by + height + buildDist;

    // TODO: recalculate start and end positions for addons

    // if this rectangle doesn't fit on the map we can't build here
	const CCPosition mapMin = m_bot.Map().mapMin();
	const CCPosition mapMax = m_bot.Map().mapMax();
    if (startx < mapMin.x || starty < mapMin.y || endx >= mapMax.x || endy >= mapMax.y || endx < bx + width)
    {
        return false;
    }
	
    // if we can't build here, or space is reserved, we can't build here
    for (int x = startx; x < endx; x++)
    {
        for (int y = starty; y < endy; y++)
        {
            if (!b.type.isRefinery())
            {
                if (!buildable(b.type, x, y) || (!ignoreReserved && m_reserveMap[x][y]))
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
			if (!m_bot.Map().canBuildTypeAtPosition(startx + width, starty, UnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR, m_bot)))
			{
				return false;
			}
			break;
		}
		default:
			break;
	}
    return true;
}

CCTilePosition BuildingPlacer::getBuildLocationNear(const Building & b, int buildDist, bool ignoreReserved) const
{
	//If the space is not walkable, look arround for a walkable space. The result may not be the most optimal location.
	int offset = 1;
	int direction = 0;
	auto buildLocation = b.desiredPosition;
	if (false)
	{
		buildLocation = CCTilePosition(55, 170);
	}
	while(!m_bot.Map().isWalkable(buildLocation) || m_bot.Map().getClosestTilesTo(buildLocation).size() < 10)
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
				printf("Should never happen [BuildingPlacer::getBuildLocationNear]");
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

		if (offset == 25)//Did not find any walkable space within 25 tiles in all directions
		{
			buildLocation = b.desiredPosition;//Avoids crashing, but this won't work well.
		}
		else
		{
			direction++;
		}
	}

    // get the precomputed vector of tile positions which are sorted closes to this location
    auto & closestToBuilding = m_bot.Map().getClosestTilesTo(buildLocation);

    // iterate through the list until we've found a suitable location
    for (size_t i(0); i < closestToBuilding.size(); ++i)
    {
        auto & pos = closestToBuilding[i];

        if (canBuildHereWithSpace(pos.x, pos.y, b, buildDist, ignoreReserved))
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

bool BuildingPlacer::buildable(const UnitType type, int x, int y) const
{
    // TODO: doesnt take units on the map into account
	bool isBuildable = m_bot.Map().isBuildable(x, y);
	bool canBuildAtPosition = type.isAddon() || m_bot.Map().canBuildTypeAtPosition(x, y, type);
	bool isOkWithCreep = Util::IsZerg(m_bot.GetSelfRace()) || !m_bot.Observation()->HasCreep(CCPosition(x, y));
	return isBuildable && canBuildAtPosition && isOkWithCreep;	//Replaced !m_bot.Map().canBuildTypeAtPosition(x, y, b.type)) with isBuildable.
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

		for (auto & unit : units)
		{
			// check to see if it's next to one of our depots
			if (unit.getType().isResourceDepot() && Util::DistSq(unit, geyserPos) < 10 * 10)
			{
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