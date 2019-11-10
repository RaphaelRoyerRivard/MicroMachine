#include "MapTools.h"
#include "Util.h"
#include "CCBot.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <array>

const size_t LegalActions = 4;
const int actionX[LegalActions] ={1, -1, 0, 0};
const int actionY[LegalActions] ={0, 0, 1, -1};

typedef std::vector<std::vector<bool>> vvb;
typedef std::vector<std::vector<int>>  vvi;
typedef std::vector<std::vector<float>>  vvf;

#ifdef SC2API
    #define HALF_TILE 0.5f
#else
    #define HALF_TILE 0
#endif

// constructor for MapTools
MapTools::MapTools(CCBot & bot)
    : m_bot     (bot)
    , m_width   (0)
    , m_height  (0)
    , m_maxZ    (0.0f)
    , m_frame   (0)
{

}

void MapTools::onStart()
{
#ifdef SC2API
	m_totalWidth = m_bot.Observation()->GetGameInfo().width;
	m_totalHeight = m_bot.Observation()->GetGameInfo().height;
	m_min = m_bot.Observation()->GetGameInfo().playable_min;
	m_max = m_bot.Observation()->GetGameInfo().playable_max;
	m_width = m_max.x - m_min.x;
	m_height = m_max.y - m_min.y;
#else
    m_width  = BWAPI::Broodwar->mapWidth();
    m_height = BWAPI::Broodwar->mapHeight();
#endif

    m_walkable       = vvb(m_totalWidth, std::vector<bool>(m_totalHeight, true));
    m_buildable      = vvb(m_totalWidth, std::vector<bool>(m_totalHeight, false));
    m_depotBuildable = vvb(m_totalWidth, std::vector<bool>(m_totalHeight, false));
    m_sectorNumber   = vvi(m_totalWidth, std::vector<int>(m_totalHeight, 0));

	auto & info = m_bot.Observation()->GetGameInfo();
    // Set the boolean grid data from the Map
    for (int x = m_min.x; x < m_max.x; ++x)
    {
        for (int y = m_min.y; y < m_max.y; ++y)
        {
            m_buildable[x][y]       = canBuild(x, y);
            m_depotBuildable[x][y]  = canBuild(x, y);
            m_walkable[x][y]        = m_buildable[x][y] || canWalk(x, y);
        }
    }

#ifdef SC2API
    for (auto & unit : m_bot.Observation()->GetUnits())
    {
        m_maxZ = std::max(unit->pos.z, m_maxZ);
    }

    // set tiles that static resources are on as unbuildable
    for (auto & resource : m_bot.GetUnits())
    {
        if (!resource.getType().isMineral() && !resource.getType().isGeyser())
        {
            continue;
        }

        int width = resource.getType().tileWidth();
        int height = resource.getType().tileHeight();
        int tileX = std::floor(resource.getPosition().x) - (width / 2);
        int tileY = std::floor(resource.getPosition().y) - (height / 2);

        if (!isVisible(resource.getTilePosition().x, resource.getTilePosition().y))
        {
        }

        for (int x=tileX; x<tileX+width; ++x)
        {
            for (int y=tileY; y<tileY+height; ++y)
            {
                m_buildable[x][y] = false;

                // depots can't be built within 3 tiles of any resource
                for (int rx=-3; rx<=3; rx++)
                {
                    for (int ry=-3; ry<=3; ry++)
                    {
                        // sc2 doesn't fill out the corners of the mineral 3x3 boxes for some reason
                        if (std::abs(rx) + std::abs(ry) == 6) { continue; }
                        if (!isValidTile(CCTilePosition(x+rx, y+ry))) { continue; }

                        m_depotBuildable[x+rx][y+ry] = false;
                    }
                }
            }
        }
    }
#else

    // set tiles that static resources are on as unbuildable
    for (auto & resource : BWAPI::Broodwar->getStaticNeutralUnits())
    {
        if (!resource->getType().isResourceContainer())
        {
            continue;
        }

        int tileX = resource->getTilePosition().x;
        int tileY = resource->getTilePosition().y;

        for (int x=tileX; x<tileX+resource->getType().tileWidth(); ++x)
        {
            for (int y=tileY; y<tileY+resource->getType().tileHeight(); ++y)
            {
                m_buildable[x][y] = false;

                // depots can't be built within 3 tiles of any resource
                for (int rx=-3; rx<=3; rx++)
                {
                    for (int ry=-3; ry<=3; ry++)
                    {
                        if (!BWAPI::TilePosition(x+rx, y+ry).isValid())
                        {
                            continue;
                        }

                        m_depotBuildable[x+rx][y+ry] = false;
                    }
                }
            }
        }
    }

#endif

    computeConnectivity();
}

void MapTools::onFrame()
{
    m_frame++;

    draw();
}

void MapTools::computeConnectivity()
{
    // the fringe data structe we will use to do our BFS searches
    std::vector<std::array<int, 2>> fringe;
    fringe.reserve(m_width*m_height);
    int sectorNumber = 0;

    // for every tile on the map, do a connected flood fill using BFS
    for (int x = m_min.x; x < m_max.x; ++x)
    {
        for (int y = m_min.y; y < m_max.y; ++y)
        {
            // if the sector is not currently 0, or the map isn't walkable here, then we can skip this tile
            if (getSectorNumber(x, y) != 0 || !isWalkable(x, y))
            {
                continue;
            }

            // increase the sector number, so that walkable tiles have sectors 1-N
            sectorNumber++;

            // reset the fringe for the search and add the start tile to it
            fringe.clear();
            fringe.push_back({x,y});
            m_sectorNumber[x][y] = sectorNumber;

            // do the BFS, stopping when we reach the last element of the fringe
            for (size_t fringeIndex=0; fringeIndex<fringe.size(); ++fringeIndex)
            {
                auto & tile = fringe[fringeIndex];

                // check every possible child of this tile
                for (size_t a=0; a<LegalActions; ++a)
                {
                    int nextX = tile[0] + actionX[a];
                    int nextY = tile[1] + actionY[a];

                    // if the new tile is inside the map bounds, is walkable, and has not been assigned a sector, add it to the current sector and the fringe
                    if (isValidTile(nextX, nextY) && isWalkable(nextX, nextY) && (getSectorNumber(nextX, nextY) == 0))
                    {
                        m_sectorNumber[nextX][nextY] = sectorNumber;
                        fringe.push_back({nextX, nextY});
                    }
                }
            }
        }
    }
}

bool MapTools::isExplored(const CCTilePosition & pos) const
{
    return isExplored(pos.x, pos.y);
}

bool MapTools::isExplored(const CCPosition & pos) const
{
    return isExplored(Util::GetTilePosition(pos));
}

bool MapTools::isExplored(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY)) { return false; }

    sc2::Visibility vis = m_bot.Observation()->GetVisibility(CCPosition(tileX + HALF_TILE, tileY + HALF_TILE));
    return vis == sc2::Visibility::Fogged || vis == sc2::Visibility::Visible;
}

bool MapTools::isVisible(CCPosition pos) const
{
	return isVisible((int)pos.x, (int)pos.y);
}

bool MapTools::isVisible(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY)) { return false; }

    return m_bot.Observation()->GetVisibility(CCPosition(tileX + HALF_TILE, tileY + HALF_TILE)) == sc2::Visibility::Visible;
}

bool MapTools::isPowered(int tileX, int tileY) const
{
#ifdef SC2API
    for (auto & powerSource : m_bot.Observation()->GetPowerSources())
    {
        if (Util::DistSq(CCPosition(tileX + HALF_TILE, tileY + HALF_TILE), powerSource.position) < powerSource.radius * powerSource.radius)
        {
            return true;
        }
    }

    return false;
#else
    return BWAPI::Broodwar->hasPower(BWAPI::TilePosition(tileX, tileY));
#endif
}

float MapTools::terrainHeight(const CCPosition & point) const
{
	return Util::TerrainHeight(point);
}

float MapTools::terrainHeight(CCTilePosition tile) const
{
	return Util::TerrainHeight(tile);
}

float MapTools::terrainHeight(float x, float y) const
{
	return Util::TerrainHeight(x, y);
}

//int MapTools::getGroundDistance(const CCPosition & src, const CCPosition & dest) const
//{
//    return (int)Util::Dist(src, dest);
//}

int MapTools::getGroundDistance(const CCPosition & src, const CCPosition & dest) const
{
    if (m_allMaps.size() > 50)
    {
        m_allMaps.clear();
    }

    return getDistanceMap(dest).getDistance(src);
}

const DistanceMap & MapTools::getDistanceMap(const CCPosition & pos) const
{
    return getDistanceMap(Util::GetTilePosition(pos));
}

const DistanceMap & MapTools::getDistanceMap(const CCTilePosition & tile) const
{
    std::pair<int,int> pairTile(tile.x, tile.y);

    if (m_allMaps.find(pairTile) == m_allMaps.end())
    {
        m_allMaps[pairTile] = DistanceMap();
        m_allMaps[pairTile].computeDistanceMap(m_bot, tile);
    }

    return m_allMaps[pairTile];
}

int MapTools::getSectorNumber(int x, int y) const
{
    if (!isValidTile(x, y))
    {
        return 0;
    }

    return m_sectorNumber[x][y];
}

bool MapTools::isValidTile(int tileX, int tileY) const
{
    return tileX >= m_min.x && tileY >= m_min.y && tileX < m_max.x && tileY < m_max.y;
}

bool MapTools::isValidTile(const CCTilePosition & tile) const
{
    return isValidTile(tile.x, tile.y);
}

bool MapTools::isValidPosition(const CCPosition & pos) const
{
    return isValidTile(Util::GetTilePosition(pos));
}

void MapTools::drawLine(CCPositionType x1, CCPositionType y1, CCPositionType x2, CCPositionType y2, const CCColor & color) const
{
	const auto p1Height = Util::TerrainHeight(x1, y1);
	const auto p2Height = Util::TerrainHeight(x2, y2);
    m_bot.Debug()->DebugLineOut(sc2::Point3D(x1, y1, p1Height + 0.2f), sc2::Point3D(x2, y2, p2Height + 0.2f), color);
}

void MapTools::drawLine(const CCPosition & p1, const CCPosition & p2, const CCColor & color) const
{
    drawLine(p1.x, p1.y, p2.x, p2.y, color);
}

void MapTools::drawTile(const CCTilePosition& tilePosition, const CCColor & color, float size, bool checkFrustum) const
{
	drawTile(tilePosition.x, tilePosition.y, color, size, checkFrustum);
}

void MapTools::drawTile(int tileX, int tileY, const CCColor & color, float size, bool checkFrustum) const
{
	if (checkFrustum && !isInCameraFrustum(tileX, tileY))
		return;

	size = std::min(1.f, std::max(0.f, size));
	const float margin = (1.f - size) / 2;
    CCPositionType px = Util::TileToPosition((float)tileX) + Util::TileToPosition(margin);
    CCPositionType py = Util::TileToPosition((float)tileY) + Util::TileToPosition(margin);
    CCPositionType d  = Util::TileToPosition(size);

    drawLine(px,     py,     px + d, py,     color);
    drawLine(px + d, py,     px + d, py + d, color);
    drawLine(px + d, py + d, px,     py + d, color);
    drawLine(px,     py + d, px,     py,     color);
}

void MapTools::drawBox(CCPositionType x1, CCPositionType y1, CCPositionType x2, CCPositionType y2, const CCColor & color) const
{
#ifdef SC2API
    m_bot.Debug()->DebugBoxOut(sc2::Point3D(x1, y1, m_maxZ + 2.0f), sc2::Point3D(x2, y2, m_maxZ-5.0f), color);
#else
    drawLine(x1, y1, x1, y2, color);
    drawLine(x1, y2, x2, y2, color);
    drawLine(x2, y2, x2, y1, color);
    drawLine(x2, y1, x1, y1, color);
#endif
}

void MapTools::drawBox(const CCPosition & tl, const CCPosition & br, const CCColor & color) const
{
#ifdef SC2API
    m_bot.Debug()->DebugBoxOut(sc2::Point3D(tl.x, tl.y, m_maxZ + 2.0f), sc2::Point3D(br.x, br.y, m_maxZ-5.0f), color);
#else
    drawBox(tl.x, tl.y, br.x, br.y, color);
#endif
}

void MapTools::drawCircle(const CCPosition & pos, CCPositionType radius, const CCColor & color) const
{
	drawCircle(pos.x, pos.y, radius, color);
}

void MapTools::drawCircle(CCPositionType x, CCPositionType y, CCPositionType radius, const CCColor & color) const
{
	if(isInCameraFrustum(x, y))
		m_bot.Debug()->DebugSphereOut(sc2::Point3D(x, y, m_maxZ), radius, color);
}


void MapTools::drawText(const CCPosition & pos, const std::string & str, const CCColor & color) const
{
	if(isInCameraFrustum(pos.x, pos.y))
		m_bot.Debug()->DebugTextOut(str, sc2::Point3D(pos.x, pos.y, Util::TerrainHeight(pos)), color);
}

void MapTools::drawTextScreen(float xPerc, float yPerc, const std::string & str, const CCColor & color) const
{
#ifdef SC2API
    m_bot.Debug()->DebugTextOut(str, CCPosition(xPerc, yPerc), color);
#else
    BWAPI::Broodwar->drawTextScreen(BWAPI::Position((int)(640*xPerc), (int)(480*yPerc)), str.c_str());
#endif
}

bool MapTools::isConnected(int x1, int y1, int x2, int y2) const
{
    if (!isValidTile(x1, y1) || !isValidTile(x2, y2))
    {
        return false;
    }

    int s1 = getSectorNumber(x1, y1);
    int s2 = getSectorNumber(x2, y2);

    return s1 != 0 && (s1 == s2);
}

bool MapTools::isConnected(const CCTilePosition & p1, const CCTilePosition & p2) const
{
    return isConnected(p1.x, p1.y, p2.x, p2.y);
}

bool MapTools::isConnected(const CCPosition & p1, const CCPosition & p2) const
{
    return isConnected(Util::GetTilePosition(p1), Util::GetTilePosition(p2));
}

bool MapTools::isBuildable(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY))
    {
        return false;
    }

	return m_buildable[tileX][tileY];
}


bool MapTools::isBuildable(const CCTilePosition & tile) const
{
	if (!isValidTile(tile.x, tile.y))
	{
		return false;
	}

	return m_buildable[tile.x][tile.y];
}

bool MapTools::canBuildTypeAtPosition(int tileX, int tileY, const UnitType & type) const
{
#ifdef SC2API
    return m_bot.Query()->Placement(m_bot.Data(type).buildAbility, CCPosition((float)tileX, (float)tileY));
#else
    return BWAPI::Broodwar->canBuildHere(BWAPI::TilePosition(tileX, tileY), type.getAPIUnitType());
#endif
}

void MapTools::printMap()
{
    std::stringstream ss;
    for (int y = m_min.y; y < m_max.y; ++y)
    {
        for (int x = m_min.x; x < m_max.x; ++x)
        {
            ss << isWalkable(x, y);
        }

        ss << "\n";
    }

    std::ofstream out("map.txt");
    out << ss.str();
    out.close();
}

bool MapTools::isDepotBuildableTile(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY))
    {
        return false;
    }

    return m_depotBuildable[tileX][tileY];
}

bool MapTools::isWalkable(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY))
    {
        return false;
    }

    return m_walkable[tileX][tileY];
}

bool MapTools::isWalkable(const CCTilePosition & tile) const
{
    return isWalkable(tile.x, tile.y);
}

bool MapTools::isWalkable(const CCPosition & pos) const
{
    return isWalkable(Util::GetTilePosition(pos));
}

const std::vector<CCTilePosition> & MapTools::getClosestTilesTo(const CCTilePosition & pos) const
{
    return getDistanceMap(pos).getSortedTiles();
}

bool MapTools::canWalk(int tileX, int tileY) 
{
    auto & info = m_bot.Observation()->GetGameInfo();
    sc2::Point2DI pointI(tileX, tileY);
    if (pointI.x < m_min.x || pointI.x >= m_max.x || pointI.y < m_min.y || pointI.y >= m_max.y)
    {
        return false;
    }

	const sc2::Point2D point(tileX, tileY);
	return Util::Pathable(point);
}

bool MapTools::isInCameraFrustum(int x, int y) const
{
	const CCPosition camera = m_bot.Observation()->GetCameraPos();
	return x >= camera.x - 17 && x <= camera.x + 17 && y >= camera.y - 12 && y <= camera.y + 12;
}

bool MapTools::canBuild(int tileX, int tileY) 
{
    auto & info = m_bot.Observation()->GetGameInfo();
    sc2::Point2DI pointI(tileX, tileY);
    if (pointI.x < m_min.x || pointI.x >= m_max.x || pointI.y < m_min.y || pointI.y >= m_max.y)
    {
        return false;
    }

    /*assert(info.placement_grid.data.size() == info.width * info.height);
    unsigned char encodedPlacement = info.placement_grid.data[pointI.x + ((info.height - 1) - pointI.y) * info.width];
    bool decodedPlacement = encodedPlacement == 255 ? true : false;
    return decodedPlacement;*/
	const sc2::Point2D point(tileX, tileY);
	return Util::Placement(point);
}

void MapTools::draw() const
{
#ifdef PUBLIC_RELEASE
	return;
#endif

	if (!m_bot.Config().DrawWalkableSectors && !m_bot.Config().DrawBuildableSectors && !m_bot.Config().DrawTileInfo)
	{
		return;
	}

#ifdef SC2API
    CCPosition camera = m_bot.Observation()->GetCameraPos();
    int sx = (int)(camera.x - 12.0f);
    int sy = (int)(camera.y - 8);
    int ex = sx + 24;
    int ey = sy + 20;

#else
    BWAPI::TilePosition screen(BWAPI::Broodwar->getScreenPosition());
    int sx = screen.x;
    int sy = screen.y;
    int ex = sx + 20;
    int ey = sy + 15;
#endif
	
    for (int x = sx; x < ex; ++x)
    {
        for (int y = sy; y < ey; y++)
        {
            if (!isValidTile((int)x, (int)y))
            {
                continue;
            }

            if (m_bot.Config().DrawWalkableSectors)
            {
                std::stringstream ss;
                ss << getSectorNumber(x, y);
                drawText(CCPosition(Util::TileToPosition(x + 0.5f), Util::TileToPosition(y + 0.5f)), ss.str());
            }

			if (m_bot.Config().DrawBuildableSectors)
			{
				if (isBuildable(x, y))
				{
					drawTile(x, y, CCColor(0, 255, 0));
				}
			}

            if (m_bot.Config().DrawTileInfo)
            {
                CCColor color = isWalkable(x, y) ? CCColor(0, 255, 0) : CCColor(255, 0, 0);
                if (isWalkable(x, y) && !isBuildable(x, y)) { color = CCColor(255, 255, 0); }
                if (isBuildable(x, y) && !isDepotBuildableTile(x, y)) { color = CCColor(127, 255, 255); }
                drawTile(x, y, color);
				std::string terrainHeight(16, '\0');
				std::snprintf(&terrainHeight[0], terrainHeight.size(), "%.2f", m_bot.Map().terrainHeight(x, y));
				std::stringstream ss;
            	ss << std::to_string(x) << "," << std::to_string(y) << "\n" << terrainHeight;
				//m_bot.Map().drawText(CCPosition(x, y), terrainHeight, color);
				m_bot.Map().drawText(CCPosition(x, y), ss.str().c_str(), color);
            }
        }
    }
}
