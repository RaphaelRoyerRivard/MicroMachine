#include "BaseLocation.h"
#include "Util.h"
#include "CCBot.h"
#include <sstream>
#include <iostream>

BaseLocation::BaseLocation(CCBot & bot, int baseID, const std::vector<Unit> & resources)
    : m_bot(bot)
    , m_baseID               (baseID)
    , m_isStartLocation      (false)
    , m_left                 (std::numeric_limits<CCPositionType>::max())
    , m_right                (std::numeric_limits<CCPositionType>::lowest())
    , m_top                  (std::numeric_limits<CCPositionType>::lowest())
    , m_bottom               (std::numeric_limits<CCPositionType>::max())
	, m_isBlocked			 (false)
{
    m_isPlayerStartLocation[0] = false;
    m_isPlayerStartLocation[1] = false;
    m_isPlayerOccupying[0] = false;
    m_isPlayerOccupying[1] = false;

    CCPositionType resourceCenterX = 0;
    CCPositionType resourceCenterY = 0;

    // add each of the resources to its corresponding container
    for (auto & resource : resources)
    {
        if (resource.getType().isMineral())
        {
            m_minerals.push_back(resource);
            m_mineralPositions.push_back(resource.getPosition());

            // add the position of the minerals to the center
            resourceCenterX += resource.getPosition().x;
            resourceCenterY += resource.getPosition().y;
        }
        else
        {
            m_geysers.push_back(resource);
            m_geyserPositions.push_back(resource.getPosition());

            // pull the resource center toward the geyser if it exists
            resourceCenterX += resource.getPosition().x;
            resourceCenterY += resource.getPosition().y;
        }

        // set the limits of the base location bounding box
        CCPositionType resWidth = Util::TileToPosition(1);
        CCPositionType resHeight = Util::TileToPosition(0.5);

        m_left   = std::min(m_left,   resource.getPosition().x - resWidth);
        m_right  = std::max(m_right,  resource.getPosition().x + resWidth);
        m_top    = std::max(m_top,    resource.getPosition().y + resHeight);
        m_bottom = std::min(m_bottom, resource.getPosition().y - resHeight);
    }

    m_centerOfResources = CCPosition(m_left + (m_right-m_left)/2, m_top + (m_bottom-m_top)/2);

	for (auto & mineral : m_minerals)
	{
		m_centerOfMinerals.x += mineral.getTilePosition().x;
		m_centerOfMinerals.y += mineral.getTilePosition().y;
	}
	if (m_minerals.size() > 0)
	{
		m_centerOfMinerals.x /= m_minerals.size();
		m_centerOfMinerals.y /= m_minerals.size();
	}
	
    // compute this BaseLocation's DistanceMap, which will compute the ground distance
    // from the center of its recourses to every other tile on the map
    m_distanceMap = m_bot.Map().getDistanceMap(m_centerOfResources);

    // check to see if this is a start location for the map, only need to check enemy locations.
    for (auto & pos : m_bot.GetEnemyStartLocations())
    {
        if (containsPositionApproximative(pos))
        {
            m_isStartLocation = true;
			m_depotPosition = pos;
            m_depotTilePosition = Util::GetTilePosition(pos);
			break;
        }
    }
    
	//if its not an enemy start baselocation
	if (!m_isStartLocation)
	{
		// if this base location position is near our own resource depot, it's our start location
		for (auto & unit : m_bot.GetAllyUnits(Util::GetResourceDepotType().getAPIUnitType()))
		{
			CCPosition pos = unit.getPosition();
			if (unit.getType().isResourceDepot() && containsPositionApproximative(pos))
			{
				m_isPlayerStartLocation[Players::Self] = true;
				m_isStartLocation = true;
				m_isPlayerOccupying[Players::Self] = true;
				m_depotPosition = pos;
				m_depotTilePosition = Util::GetTilePosition(pos);
				break;
			}
		}
	}
    
    // if it's not a start location, we need to calculate the depot position
    if (!isStartLocation())
    {
        // the position of the depot will be the closest spot we can build one from the resource center
        for (auto & tile : getClosestTiles())
        {
            // the build position will be up-left of where this tile is
            // this means we are positioning the center of the resource depot
            CCTilePosition buildTile(tile.x, tile.y);

            if(m_bot.Buildings().getBuildingPlacer().canBuildDepotHere(tile.x, tile.y, m_minerals, m_geysers))
			{
				m_depotPosition = CCPosition(tile.x + 0.5, tile.y + 0.5);
                m_depotTilePosition = buildTile;
                break;
            }
        }
    }

	//Determine if the geysers are together or split
	if (m_geyserPositions.size() == 1)
	{
		m_isSplitGeyser = false;//If the base as one geyser only, we consider it has "Together"
	}
	else if (m_geyserPositions.size() == 2)
	{
		auto dist = Util::DistSq(m_geyserPositions[0], m_geyserPositions[1]);
		if (dist > 36)//if further than 6 tiles away
		{
			m_isSplitGeyser = true;
		}
		else
		{
			m_isSplitGeyser = false;
			CCTilePosition geyserDepotCenter = m_depotTilePosition;
			for (auto & geyserPos : m_geyserPositions)
			{
				geyserDepotCenter.x += geyserPos.x;
				geyserDepotCenter.y += geyserPos.y;
			}
			if (m_geyserPositions.size() > 0)
			{
				geyserDepotCenter.x /= (m_geyserPositions.size() + 1);
				geyserDepotCenter.y /= (m_geyserPositions.size() + 1);
			}

			geyserDepotCenter = m_bot.Buildings().getBuildingPlacer().getBunkerBuildLocationNear(Building(MetaTypeEnum::Bunker.getUnitType(), geyserDepotCenter), m_depotTilePosition.x, m_depotTilePosition.y, m_geyserPositions);

			m_gasBunkerLocations.push_back(geyserDepotCenter);
		}
	}
	else
	{
		BOT_ASSERT(m_geyserPositions.size() <= 2, "Unexpected base layout detected.");
	}

	m_isRich = m_minerals.at(0).getType().isRichMineral();

	Building b(MetaTypeEnum::MissileTurret.getUnitType(), m_centerOfMinerals);
	m_turretPosition = m_bot.Buildings().getBuildingPlacer().getBuildLocationNear(b, true, false, true);
}

const CCTilePosition & BaseLocation::getTurretPosition() const
{
	return m_turretPosition;
}

const std::vector<CCTilePosition> & BaseLocation::getGasBunkerLocations() const
{
	return m_gasBunkerLocations;
}

const CCPosition & BaseLocation::getDepotPosition() const
{
	return m_depotPosition;
}

// TODO: calculate the actual depot position
const CCTilePosition & BaseLocation::getDepotTilePosition() const
{
	return m_depotTilePosition;
}

int BaseLocation::getOptimalMineralWorkerCount() const
{
	const int minimumMineral = 50;//at 50, its basically empty.
	int optimalWorkers = 0;
	for (auto & mineral : getMinerals())
	{
		if (mineral.getUnitPtr()->mineral_contents > minimumMineral)
		{
			optimalWorkers += 2;
		}
	}
	return optimalWorkers;
}

int BaseLocation::getOptimalGasWorkerCount() const
{
	int optimalWorkers = 0;
	for (auto & geyser : getGeysers())
	{
		if (geyser.getUnitPtr()->vespene_contents > 0)
		{
			optimalWorkers += getGasBunkers().size() > 0 ? 2 : 3;
		}
	}
	return optimalWorkers;
}

void BaseLocation::setPlayerOccupying(CCPlayer player, bool occupying)
{
    m_isPlayerOccupying[player] = occupying;

	if (occupying && player == Players::Self && m_isBlocked && m_resourceDepot.isValid() && m_resourceDepot.isAlive())//If it was marked as blocked and we expanded there, clear the flag
	{
		clearBlocked();
	}

    // if this base is a start location that's occupied by the enemy, it's that enemy's start location
    if (occupying && player == Players::Enemy && isStartLocation() && m_isPlayerStartLocation[player] == false)
    {
        m_isPlayerStartLocation[player] = true;
    }

	//Replace ressource snapshots
	if(occupying && !m_snapshotsRemoved && player == Players::Self)
	{
		bool refreshResources = m_minerals.empty();
		if(!refreshResources)
		{
			for(auto & mineral : m_minerals)
			{
				if(!mineral.isVisible())
				{
					refreshResources = true;
					break;
				}
			}
		}
		if (refreshResources)
		{
			std::vector<Unit> minerals;
			std::vector<Unit> geysers;
			for (auto & unit : m_bot.UnitInfo().getUnits(Players::Neutral))
			{
				// skip minerals that don't have more than 100 starting minerals
				// these are probably stupid map-blocking minerals to confuse us
				if (!unit.getType().isMineral() && !unit.getType().isGeyser())
				{
					continue;
				}

				if (!unit.isVisible())
					continue;

				if (Util::DistSq(m_centerOfResources, unit.getPosition()) < 10 * 10)
				{
					if (unit.getType().isMineral())
						minerals.push_back(unit);
					else
						geysers.push_back(unit);
				}
			}
			if (minerals.size() == m_minerals.size() && geysers.size() == m_geysers.size())
			{
				m_minerals = minerals;
				m_geysers = geysers;
				m_snapshotsRemoved = true;
			}
		}
	}
}

bool BaseLocation::isOccupiedByPlayer(CCPlayer player) const
{
    return m_isPlayerOccupying.at(player);
}

bool BaseLocation::isExplored() const
{
    return m_bot.Map().isExplored(m_centerOfResources);
}

bool BaseLocation::isPlayerStartLocation(CCPlayer player) const
{
    return m_isPlayerStartLocation.at(player);
}

bool BaseLocation::containsPositionApproximative(const CCPosition & pos, int maxDistance) const
{
	if (!m_bot.Map().isValidPosition(pos) || (pos.x == 0 && pos.y == 0))
	{
		return false;
	}

	const int groundDistance = getGroundDistance(pos);
	return groundDistance > 0 && groundDistance < (maxDistance > 0 ? maxDistance : ApproximativeBaseLocationTileDistance);
}

bool BaseLocation::containsUnitApproximative(const Unit & unit, int maxDistance) const
{
	if (!unit.isValid())
	{
		return false;
	}

	if (unit.isFlying())
	{
		maxDistance = maxDistance > 0 ? maxDistance : ApproximativeBaseLocationTileDistance;
		return Util::DistSq(unit, Util::GetPosition(m_depotTilePosition)) < maxDistance * maxDistance;
	}

	return containsPositionApproximative(unit.getPosition(), maxDistance);
}

bool BaseLocation::containsPosition(const CCPosition & pos) const
{
    if (!m_bot.Map().isValidPosition(pos) || (pos.x == 0 && pos.y == 0))
    {
        return false;
    }

	return m_bot.Bases().getBaseLocation(pos) == this;
}

const std::vector<Unit> & BaseLocation::getGeysers() const
{
    return m_geysers;
}

const std::vector<Unit> & BaseLocation::getMinerals() const
{
    return m_minerals;
}

const CCTilePosition BaseLocation::getCenterOfMinerals() const
{
	return m_centerOfMinerals;
}

const CCPosition & BaseLocation::getPosition() const
{
    return m_centerOfResources;
}

int BaseLocation::getGroundDistance(const CCPosition & pos) const
{
    return m_distanceMap.getDistance(pos);
}

int BaseLocation::getGroundDistance(const CCTilePosition & pos) const
{
    return m_distanceMap.getDistance(pos);
}

bool BaseLocation::isStartLocation() const
{
    return m_isStartLocation;
}

const std::vector<CCTilePosition> & BaseLocation::getClosestTiles() const
{
    return m_distanceMap.getSortedTiles();
}

const bool & BaseLocation::isGeyserSplit() const
{
	return m_isSplitGeyser;
}

const bool & BaseLocation::isRich() const
{
	return m_isRich;
}

void BaseLocation::draw()
{
    CCPositionType radius = Util::TileToPosition(1.0f);

    m_bot.Map().drawCircle(m_centerOfResources, radius, CCColor(255, 255, 0));
	
    std::stringstream ss;
    ss << "BaseLocation:    " << m_baseID << "\n";
    ss << "Start Loc:       " << (isStartLocation() ? "true" : "false") << "\n";
    ss << "Minerals:        " << m_mineralPositions.size() << "\n";
    ss << "Geysers:         " << m_geyserPositions.size() << "\n";
    ss << "Position:        (" << m_depotPosition.x << ", " << m_depotPosition.y << ")\n";
	ss << "Under attack:    " << (m_isUnderAttack ? "true" : "false") << "\n";
	ss << "Blocked:         " << (m_isBlocked ? "true" : "false") << "\n";
	ss << "Blocking units #:" << m_blockingUnits.size() << "\n";
	ss << "Geyser type:     " << (m_isSplitGeyser ? "Split" : "Together") << "\n";
    ss << "Occupied By:     ";

    if (isOccupiedByPlayer(Players::Self))
    {
        ss << "Self ";
    }

    if (isOccupiedByPlayer(Players::Enemy))
    {
        ss << "Enemy ";
    }

    m_bot.Map().drawText(CCPosition(m_left, m_top+3), ss.str().c_str());
    m_bot.Map().drawText(CCPosition(m_left, m_bottom), ss.str().c_str());

    // draw the base bounding box
    m_bot.Map().drawBox(m_left, m_top, m_right, m_bottom);


	//Draw gas bunker
	for (auto gasBunkerPos : m_gasBunkerLocations)
	{
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x, gasBunkerPos.y), CCColor(255,255,255));
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x-1, gasBunkerPos.y), CCColor(255, 255, 255));
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x, gasBunkerPos.y-1), CCColor(255, 255, 255));
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x-1, gasBunkerPos.y-1), CCColor(255, 255, 255));
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x, gasBunkerPos.y), CCColor(125, 125, 125));
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x+1, gasBunkerPos.y), CCColor(255, 255, 255));
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x, gasBunkerPos.y+1), CCColor(255, 255, 255));
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x+1, gasBunkerPos.y+1), CCColor(255, 255, 255));
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x-1, gasBunkerPos.y+1), CCColor(255, 255, 255));
		m_bot.Map().drawTile(CCPosition(gasBunkerPos.x + 1, gasBunkerPos.y - 1), CCColor(255, 255, 255));
	}

    /*for (CCPositionType x=m_left; x < m_right; x += Util::TileToPosition(1.0f))
    {
        m_bot.Map().drawLine(x, m_top, x, m_bottom, CCColor(160, 160, 160));
    }

    for (CCPositionType y=m_bottom; y<m_top; y += Util::TileToPosition(1.0f))
    {
        m_bot.Map().drawLine(m_left, y, m_right, y, CCColor(160, 160, 160));
    }*/

    for (auto & mineralPos : m_mineralPositions)
    {
        m_bot.Map().drawCircle(mineralPos, radius, CCColor(0, 128, 128));
    }

    for (auto & geyserPos : m_geyserPositions)
    {
        m_bot.Map().drawCircle(geyserPos, radius, CCColor(0, 255, 0));
    }

	/*for (auto & g : m_geysers)
	{
		m_bot.Map().drawTile(g.getTilePosition(), CCColor(0, 255, 0));
	}*/

    if (m_isStartLocation)
    {
        m_bot.Map().drawCircle(Util::GetPosition(m_depotTilePosition), radius, CCColor(255, 0, 0));
    }

    m_bot.Map().drawTile(m_depotTilePosition.x, m_depotTilePosition.y, CCColor(0, 0, 255));

    //m_distanceMap.draw(m_bot);
}

bool BaseLocation::isMineralOnly() const
{
    return getGeysers().empty();
}