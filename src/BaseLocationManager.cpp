#include "BaseLocationManager.h"
#include "Util.h"

#include "CCBot.h"

BaseLocationManager::BaseLocationManager(CCBot & bot)
    : m_bot(bot)
	, m_areBaseLocationPtrsSorted(false)
{
    
}

void BaseLocationManager::onStart()
{
    m_tileBaseLocations = std::vector<std::vector<BaseLocation *>>(m_bot.Map().totalWidth(), std::vector<BaseLocation *>(m_bot.Map().totalHeight(), nullptr));
    m_playerStartingBaseLocations[Players::Self]  = nullptr;
    m_playerStartingBaseLocations[Players::Enemy] = nullptr;
    
    // a BaseLocation will be anything where there are minerals to mine
    // so we will first look over all minerals and cluster them based on some distance
    const CCPositionType clusterDistanceSq = Util::TileToPosition(12*12);
    
    // stores each cluster of resources based on some ground distance
    std::vector<std::vector<Unit>> resourceClusters;
    for (auto & mineral : m_bot.GetUnits())
    {
        // skip minerals that don't have more than 100 starting minerals
        // these are probably stupid map-blocking minerals to confuse us
        if (!mineral.getType().isMineral())
        {
            continue;
        }

#ifndef SC2API
        // for BWAPI we have to eliminate minerals that have low resource counts
        if (mineral.getUnitPtr()->getResources() < 100) { continue; }
#endif

        bool foundCluster = false;
        for (auto & cluster : resourceClusters)
        {
            float distSq = Util::DistSq(mineral, Util::CalcCenter(cluster));
            
            // quick initial air distance check to eliminate most resources
            if (distSq < clusterDistanceSq)
            {
                // now do a more expensive ground distance check
                //float groundDist = dist; //m_bot.Map().getGroundDistance(mineral.pos, Util::CalcCenter(cluster));
                //if (groundDist >= 0 && groundDist < clusterDistance)
                {
                    cluster.push_back(mineral);
                    foundCluster = true;
                    break;
                }
            }
        }

        if (!foundCluster)
        {
            resourceClusters.push_back(std::vector<Unit>());
            resourceClusters.back().push_back(mineral);
        }
    }

    // add geysers only to existing resource clusters
    for (auto & geyser : m_bot.GetUnits())
    {
        if (!geyser.getType().isGeyser())
        {
            continue;
        }

        for (auto & cluster : resourceClusters)
        {
            //int groundDist = m_bot.Map().getGroundDistance(geyser.pos, Util::CalcCenter(cluster));
            float groundDistSq = Util::DistSq(geyser, Util::CalcCenter(cluster));

            if (/*groundDist >= 0 &&*/ groundDistSq < clusterDistanceSq)
            {
                cluster.push_back(geyser);
                break;
            }
        }
    }

    // add the base locations if there are more than 4 resouces in the cluster
    int baseID = 0;
    for (auto & cluster : resourceClusters)
    {
        if (cluster.size() > 4)
        {
            m_baseLocationData.push_back(BaseLocation(m_bot, baseID++, cluster));
        }
    }

    // construct the vectors of base location pointers, this is safe since they will never change
    for (auto & baseLocation : m_baseLocationData)
    {
        m_baseLocationPtrs.push_back(&baseLocation);

        // if it's a start location, add it to the start locations
        if (baseLocation.isStartLocation())
        {
            m_startingBaseLocations.push_back(&baseLocation);
        }

        // if it's our starting location, set the pointer
        if (baseLocation.isPlayerStartLocation(Players::Self))
        {
            m_playerStartingBaseLocations[Players::Self] = &baseLocation;
        }

        if (baseLocation.isPlayerStartLocation(Players::Enemy))
        {
            m_playerStartingBaseLocations[Players::Enemy] = &baseLocation;
        }
    }
	if (m_playerStartingBaseLocations[Players::Self] == nullptr)//Player start base location not found
	{
		Util::DisplayError("Invalid setup detected.", "0x0000000", m_bot);
	}

    // construct the map of tile positions to base locations
	const CCPosition mapMin = m_bot.Map().mapMin();
	const CCPosition mapMax = m_bot.Map().mapMax();
    for (int x = mapMin.x; x < mapMax.x; ++x)
    {
        for (int y = mapMin.y; y < mapMax.y; ++y)
        {
            for (auto & baseLocation : m_baseLocationData)
            {
                CCPosition pos(Util::TileToPosition(x + 0.5f), Util::TileToPosition(y + 0.5f));

                if (baseLocation.containsPosition(pos))
                {
                    m_tileBaseLocations[x][y] = &baseLocation;
                    
                    break;
                }
            }
        }
    }

    // construct the sets of occupied base locations
    m_occupiedBaseLocations[Players::Self] = std::set<BaseLocation *>();
    m_occupiedBaseLocations[Players::Enemy] = std::set<BaseLocation *>();
}

void BaseLocationManager::onFrame()
{
	m_bot.StartProfiling("0.6.0   drawBaseLocations");
    drawBaseLocations();
	m_bot.StopProfiling("0.6.0   drawBaseLocations");

	if (m_bot.Bases().getPlayerStartingBaseLocation(Players::Self) == nullptr)
	{
		m_bot.StartProfiling("0.6.1   FixNullPlayerStartingBaseLocation");
		FixNullPlayerStartingBaseLocation();
		m_bot.StopProfiling("0.6.1   FixNullPlayerStartingBaseLocation");
	}

	m_bot.StartProfiling("0.6.2   resetBaseLocations");
    // reset the player occupation information for each location
    for (auto & baseLocation : m_baseLocationData)
    {
        baseLocation.setPlayerOccupying(Players::Self, false);
		baseLocation.setResourceDepot({});
        baseLocation.setPlayerOccupying(Players::Enemy, false);
    }
	m_bot.StopProfiling("0.6.2   resetBaseLocations");

	m_bot.StartProfiling("0.6.3   updateBaseLocations");
    // for each unit on the map, update which base location it may be occupying
    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        // we only care about resource depots
        if (!unit.getType().isResourceDepot())
            continue;

        BaseLocation * baseLocation = getBaseLocation(unit.getPosition());

        if (baseLocation != nullptr)
        {
            baseLocation->setPlayerOccupying(unit.getPlayer(), true);
			baseLocation->setResourceDepot(unit);
        }
    }
	m_bot.StopProfiling("0.6.3   updateBaseLocations");

	m_bot.StartProfiling("0.6.4   updateEnemyBaseLocations");
    // update enemy base occupations
    for (const auto & kv : m_bot.UnitInfo().getUnitInfoMap(Players::Enemy))
    {
        const UnitInfo & ui = kv.second;

        if (!m_bot.Data(ui.type).isBuilding)
        {
            continue;
        }

        BaseLocation * baseLocation = getBaseLocation(ui.lastPosition);

        if (baseLocation != nullptr)
        {
            baseLocation->setPlayerOccupying(Players::Enemy, true);
        }
    }
	m_bot.StopProfiling("0.6.4   updateEnemyBaseLocations");

    // update the starting locations of the enemy player
    // this will happen one of two ways:
    
    // 1. we've seen the enemy base directly, so the baselocation will know
    if (m_playerStartingBaseLocations[Players::Enemy] == nullptr)
    {
		m_bot.StartProfiling("0.6.5   updateEnemyStartingBaseLocation");
        for (auto & baseLocation : m_baseLocationData)
        {
            if (baseLocation.isPlayerStartLocation(Players::Enemy))
            {
                m_playerStartingBaseLocations[Players::Enemy] = &baseLocation;
            }
        }
		m_bot.StopProfiling("0.6.5   updateEnemyStartingBaseLocation");
    }

    // 2. we've explored every other start location and haven't seen the enemy yet
    if (m_playerStartingBaseLocations[Players::Enemy] == nullptr)
    {
		m_bot.StartProfiling("0.6.6   updateEnemyStartingBaseLocation2");
        int numStartLocations = (int)getStartingBaseLocations().size();
        int numExploredLocations = 0;
        BaseLocation * unexplored = nullptr;

        for (auto & baseLocation : m_baseLocationData)
        {
            if (!baseLocation.isStartLocation())
            {
                continue;
            }

            if (baseLocation.isExplored())
            {
                numExploredLocations++;
            }
            else
            {
                unexplored = &baseLocation;
            }
        }

        // if we have explored all but one location, then the unexplored one is the enemy start location
        if (numExploredLocations == numStartLocations - 1 && unexplored != nullptr)
        {
            m_playerStartingBaseLocations[Players::Enemy] = unexplored;
            unexplored->setPlayerOccupying(Players::Enemy, true);
        }
		m_bot.StopProfiling("0.6.6   updateEnemyStartingBaseLocation2");
    }

	m_bot.StartProfiling("0.6.7   setOccupiedBaseLocations");
    // update the occupied base locations for each player
    m_occupiedBaseLocations[Players::Self] = std::set<BaseLocation *>();
    m_occupiedBaseLocations[Players::Enemy] = std::set<BaseLocation *>();
    for (auto & baseLocation : m_baseLocationData)
    {
        if (baseLocation.isOccupiedByPlayer(Players::Self))
        {
            m_occupiedBaseLocations[Players::Self].insert(&baseLocation);
        }

        if (baseLocation.isOccupiedByPlayer(Players::Enemy))
        {
            m_occupiedBaseLocations[Players::Enemy].insert(&baseLocation);
        }
    }
	m_bot.StopProfiling("0.6.7   setOccupiedBaseLocations");

	if(!m_areBaseLocationPtrsSorted && m_playerStartingBaseLocations[Players::Enemy] != nullptr)
	{
		sortBaseLocationPtrs();
	}

    // draw the debug information for each base location
    
}

BaseLocation * BaseLocationManager::getBaseLocation(const CCPosition & pos) const
{
    if (!m_bot.Map().isValidPosition(pos)) { return nullptr; }

#ifdef SC2API
    return m_tileBaseLocations[(int)pos.x][(int)pos.y];
#else
    return m_tileBaseLocations[pos.x / 32][pos.y / 32];
#endif
}

void BaseLocationManager::drawBaseLocations()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
    if (!m_bot.Config().DrawBaseLocationInfo)
    {
        return;
    }

    for (auto & baseLocation : m_baseLocationData)
    {
        baseLocation.draw();
    }

    // draw a purple sphere at the next expansion location
    CCTilePosition nextExpansionPosition = getNextExpansionPosition(Players::Self);

    m_bot.Map().drawCircle(Util::GetPosition(nextExpansionPosition), 1, CCColor(255, 0, 255));
    m_bot.Map().drawText(Util::GetPosition(nextExpansionPosition), "Next Expansion Location", CCColor(255, 0, 255));
}

const std::vector<const BaseLocation *> & BaseLocationManager::getBaseLocations() const
{
    return m_baseLocationPtrs;
}

const std::vector<const BaseLocation *> & BaseLocationManager::getStartingBaseLocations() const
{
    return m_startingBaseLocations;
}

const BaseLocation * BaseLocationManager::getPlayerStartingBaseLocation(int player) const
{
    return m_playerStartingBaseLocations.at(player);
}

void BaseLocationManager::FixNullPlayerStartingBaseLocation()
{
	const BaseLocation * startBase = m_playerStartingBaseLocations.at(Players::Self);
	if (startBase == nullptr)
	{
		Util::DisplayError("Invalid setup detected.", "0x0000001", m_bot);
		for (auto & baseLocation : m_baseLocationData)
		{
			if (baseLocation.isPlayerStartLocation(Players::Self))
			{
				m_bot.Actions()->SendChat("[FIXED] Error was fixed. 0x0000001 : 0x0000000");
				m_playerStartingBaseLocations[Players::Self] = &baseLocation;
				return;
			}
		}
		for (auto & baseLocation : m_baseLocationData)
		{
			if (baseLocation.isOccupiedByPlayer(Players::Self))
			{
				m_bot.Actions()->SendChat("[FIXED] Error was fixed. 0x0000001 : 0x0000001");
				m_playerStartingBaseLocations[Players::Self] = &baseLocation;
				return;
			}
		}
	}
}

const std::set<BaseLocation *> & BaseLocationManager::getOccupiedBaseLocations(int player) const
{
    return m_occupiedBaseLocations.at(player);
}

int BaseLocationManager::getBaseCount(int player, bool isCompleted) const
{
	std::vector<sc2::UNIT_TYPEID> baseTypes;
	switch (m_bot.GetSelfRace())
	{
		case CCRace::Terran:
			baseTypes = { sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER, sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND , sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS };
			break;
		case CCRace::Protoss:
			baseTypes = { sc2::UNIT_TYPEID::PROTOSS_NEXUS };
			break;
		case CCRace::Zerg:
			baseTypes = { sc2::UNIT_TYPEID::ZERG_HATCHERY, sc2::UNIT_TYPEID::ZERG_LAIR , sc2::UNIT_TYPEID::ZERG_HIVE };
			break;
	}
	return m_bot.Buildings().getBuildingCountOfType(baseTypes, isCompleted);
}

const BaseLocation* BaseLocationManager::getNextExpansion(int player, bool checkBuildable) const
{
	const BaseLocation * homeBase = getPlayerStartingBaseLocation(player);
	BOT_ASSERT(homeBase, "No home base detected");

	const BaseLocation * closestBase = nullptr;
	int minDistance = std::numeric_limits<int>::max();

	CCPosition homeTile = homeBase->getPosition();

	for (auto & base : getBaseLocations())
	{
		// skip mineral only and starting locations (TODO: fix this)
		if (base->isMineralOnly() || base->isStartLocation() || base->isOccupiedByPlayer(Players::Self) || base->isOccupiedByPlayer(Players::Enemy))
		{
			continue;
		}

		// get the tile position of the base
		auto tile = base->getDepotPosition();

		bool buildingInTheWay = false; // TODO: check if there are any units on the tile
		for (auto unit : m_bot.GetUnits())
		{
			if (unit.isValid() && (unit.getPlayer() == Players::Self || unit.getPlayer() == Players::Enemy) && unit.getType().isBuilding())
			{
				int height = unit.getType().tileHeight() * 0.5;
				int width = unit.getType().tileWidth() * 0.5;
				//tile position is always at the center, but if its an odd width or height, it picks the bottom left '+' of the center
				//Check if the position of the building is within the border of the ressource depot
				if (tile.x + 3 >= unit.getTilePosition().x - width && tile.x - 2 <= unit.getTilePosition().x + width && tile.y + 3 >= unit.getTilePosition().y - height && tile.y - 2 <= unit.getTilePosition().y + height)
				{
					buildingInTheWay = true;
					break;
				}
			}
		}
		if (buildingInTheWay)
		{
			continue;
		}
		
		//Check if buildable (creep check), using CC for building size, should work for all races.
		if (checkBuildable && !m_bot.Buildings().getBuildingPlacer().canBuildHere(tile.x, tile.y, Building(MetaTypeEnum::CommandCenter.getUnitType(), tile)))
		{
			continue;
		}

		// the base's distance from our main nexus
		int distanceFromHome = homeBase->getGroundDistance(tile);

		// if it is not connected, continue
		if (distanceFromHome < 0)
		{
			continue;
		}

		if (!closestBase || distanceFromHome < minDistance)
		{
			closestBase = base;
			minDistance = distanceFromHome;
		}
	}

	return closestBase;
}

CCTilePosition BaseLocationManager::getNextExpansionPosition(int player, bool checkBuildable) const
{
	auto closestBase = getNextExpansion(player);
	return closestBase ? closestBase->getDepotPosition() : CCTilePosition(0, 0);
}

CCTilePosition BaseLocationManager::getBasePosition(int player, int index) const
{
	BOT_ASSERT(index >= 0, "Negative index given to BaseLocationManager::getBasePosition");
	BOT_ASSERT(index < m_baseLocationPtrs.size(), "Index to high in BaseLocationManager::getBasePosition");

	if (player == Players::Self)
		index = m_baseLocationPtrs.size() - index - 1;

	CCTilePosition position = m_baseLocationPtrs[index]->getDepotPosition();
	BOT_ASSERT(position.x != 0.f || position.y != 0.f, "Base location is 0,0");
	return position;
}

CCTilePosition BaseLocationManager::getClosestBasePosition(const sc2::Unit* unit, int player, bool shiftTowardsResourceDepot) const
{
	CCTilePosition closestBase = Util::GetTilePosition(m_bot.Map().center());
	float minDistance = 0.f;
	for (auto & base : m_baseLocationData)
	{
		if (!base.isOccupiedByPlayer(player))
			continue;

		const float dist = Util::DistSq(base.getPosition(), unit->pos);
		if (minDistance == 0.f || dist < minDistance)
		{
			minDistance = dist;
			if (shiftTowardsResourceDepot)
			{
				CCPosition vectorTowardsBase = Util::GetPosition(base.getDepotPosition()) - base.getPosition();
				Util::Normalize(vectorTowardsBase);
				closestBase = Util::GetTilePosition(base.getPosition() + vectorTowardsBase);
			}
			else
			{
				closestBase = Util::GetTilePosition(base.getPosition());
			}
		}
	}
	return closestBase;
}

const BaseLocation* BaseLocationManager::getBaseContainingPosition(const CCPosition position, int player) const
{
	for (auto & base : m_baseLocationData)
	{
		if (!base.isOccupiedByPlayer(player))
			continue;

		if (base.containsPosition(position))
		{
			return &base;
		}
	}
	return nullptr;
}

void BaseLocationManager::sortBaseLocationPtrs()
{
	//Sorting base locations from closest to opponent's starting base to farthest
	std::vector<const BaseLocation *> sortedBaseLocationPtrs;
	std::map<const BaseLocation *, float> baseLocationDistances;
	const BaseLocation * enemyStartingBaseLocation = m_playerStartingBaseLocations[Players::Enemy];
	for(const auto baseLocation : m_baseLocationPtrs)
	{
		baseLocationDistances.insert_or_assign(baseLocation, Util::DistSq(enemyStartingBaseLocation->getPosition(), baseLocation->getPosition()));
	}
	while(!baseLocationDistances.empty())
	{
		float smallestDistance = 0.f;
		const BaseLocation * baseLocation = nullptr;
		for(const auto baseLocationDistancePair : baseLocationDistances)
		{
			if(!baseLocation || baseLocationDistancePair.second < smallestDistance)
			{
				smallestDistance = baseLocationDistancePair.second;
				baseLocation = baseLocationDistancePair.first;
			}
		}
		sortedBaseLocationPtrs.push_back(baseLocation);
		baseLocationDistances.erase(baseLocation);
	}
	m_baseLocationPtrs = sortedBaseLocationPtrs;
	m_areBaseLocationPtrsSorted = true;
}