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
    m_reserveMap = std::vector< std::vector<bool> >(m_bot.Map().width(), std::vector<bool>(m_bot.Map().height(), false));

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
    // check the reserve map
    for (int x = bx; x < bx + b.type.tileWidth(); x++)
    {
        for (int y = by; y < by + b.type.tileHeight(); y++)
        {
            if (!m_bot.Map().isValidTile(x, y) || m_reserveMap[x][y])
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
bool BuildingPlacer::canBuildHereWithSpace(int bx, int by, const Building & b, int buildDist) const
{
    UnitType type = b.type;

    //if we can't build here, we of course can't build here with space
    if (!canBuildHere(bx, by, b))
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
    if (startx < 0 || starty < 0 || endx > m_bot.Map().width() || endx < bx + width || endy > m_bot.Map().height())
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
                if (!buildable(b, x, y) || m_reserveMap[x][y])
                {
                    return false;
                }
            }
        }
    }

	if (!m_bot.Map().canBuildTypeAtPosition(startx, starty, b.type))
	{
		return false;
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

CCTilePosition BuildingPlacer::getBuildLocationNear(const Building & b, int buildDist) const
{
    //Timer t;
    //t.start();

	//if we can already build at that location
	/*if (canBuildHereWithSpace(b.desiredPosition.x, b.desiredPosition.y, b, buildDist))
	{
		return b.desiredPosition;
	}*/

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
			case 4://diag up-right
				buildLocation.x = b.desiredPosition.x + offset;
				buildLocation.y = b.desiredPosition.y + offset;
			case 5://diag up-left
				buildLocation.x = b.desiredPosition.x - offset;
				buildLocation.y = b.desiredPosition.y + offset;
			case 6://diag down-right
				buildLocation.x = b.desiredPosition.x + offset;
				buildLocation.y = b.desiredPosition.y - offset;
			case 7://diag down-left
				buildLocation.x = b.desiredPosition.x - offset;
				buildLocation.y = b.desiredPosition.y - offset;
				direction = -1;//will be 0 after the ++
				offset++;
			default:
				printf("Should never happen [BuildingPlacer::getBuildLocationNear]");
		}
		if (buildLocation.x < 0)
		{
			buildLocation.x = 0;
		}
		else if (buildLocation.y < 0)
		{
			buildLocation.y = 0;
		}
		if (buildLocation.x > m_bot.Map().width())
		{
			buildLocation.x = m_bot.Map().width();
		}
		else if (buildLocation.y > m_bot.Map().height())
		{
			buildLocation.y = m_bot.Map().height();
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

    //double ms1 = t.getElapsedTimeInMilliSec();

    // iterate through the list until we've found a suitable location
    for (size_t i(0); i < closestToBuilding.size(); ++i)
    {
        auto & pos = closestToBuilding[i];

        if (canBuildHereWithSpace(pos.x, pos.y, b, buildDist))
        {
            //double ms = t.getElapsedTimeInMilliSec();
            //printf("Building Placer Took %d iterations, lasting %lf ms @ %lf iterations/ms, %lf setup ms\n", (int)i, ms, (i / ms), ms1);

            return pos;
        }
    }

    //double ms = t.getElapsedTimeInMilliSec();
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
        int bx2 = bx1 + Util::GetTownHall(m_bot.GetPlayerRace(Players::Self), m_bot).tileWidth();
        int by2 = by1 + Util::GetTownHall(m_bot.GetPlayerRace(Players::Self), m_bot).tileHeight();

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

bool BuildingPlacer::buildable(const Building & b, int x, int y) const
{
    // TODO: doesnt take units on the map into account
	return m_bot.Map().isBuildable(x, y) && m_bot.Map().canBuildTypeAtPosition(x, y, b.type);//Remplaced !m_bot.Map().canBuildTypeAtPosition(x, y, b.type)) with isBuildable.
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

    for (int x = 0; x < rwidth; ++x)
    { 
        for (int y = 0; y < rheight; ++y)
        {
            if (m_reserveMap[x][y])
            {
                m_bot.Map().drawTile(x - 1, y - 1, CCColor(255, 255, 0));
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

CCTilePosition BuildingPlacer::getRefineryPosition()
{
    CCPosition closestGeyser(0, 0);
    double minGeyserDistanceFromHome = std::numeric_limits<double>::max();
    CCPosition homePosition = m_bot.GetStartLocation();

    for (auto & unit : m_bot.GetUnits())
    {
        if (!unit.getType().isGeyser())
        {
            continue;
        }

        CCPosition geyserPos(unit.getPosition());

		for (auto & ourUnit : m_bot.UnitInfo().getUnits(Players::Self))
		{
			// check to see if it's next to one of our depots
			if (ourUnit.getType().isResourceDepot() && Util::Dist(ourUnit, geyserPos) < 10 && m_bot.Query()->Placement(sc2::ABILITY_ID::BUILD_REFINERY, geyserPos))
			{
				double homeDistance = Util::Dist(unit, homePosition);
				if (homeDistance < minGeyserDistanceFromHome)
				{
					minGeyserDistanceFromHome = homeDistance;
					closestGeyser = geyserPos;
				}
				break;
			}
		}
    }

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

