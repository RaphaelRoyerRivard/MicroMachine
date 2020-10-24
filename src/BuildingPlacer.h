#pragma once

#include "Common.h"
#include "BuildingData.h"

class CCBot;
class BaseLocation;

class BuildingPlacer
{
    CCBot & m_bot;

    std::vector< std::vector<bool> > m_reserveBuildingMap;
	std::vector< std::vector<bool> > m_reserveWalkableMap;//Only used with #define COMPUTE_WALKABLE_TILES

    // queries for various BuildingPlacer data
	bool isGeyserAssigned(CCTilePosition geyserTilePos) const;
    bool isReserved(int x, int y) const;
    bool tileOverlapsBaseLocation(int x, int y, UnitType type) const;


public:

    BuildingPlacer(CCBot & bot);

    void onStart();

    // determines whether we can build at a given location
	bool buildable(const UnitType type, int x, int y, bool ignoreReservedTiles = false) const;
	bool canBuildDepotHere(int bx, int by, std::vector<Unit> minerals, std::vector<Unit> geysers) const;
	bool canBuildBunkerHere(int bx, int by, int depotX, int depotY, std::vector<CCPosition> geysersPos) const;
    bool canBuildHere(int bx, int by, const UnitType & type, bool ignoreReserved, bool checkInfluenceMap, bool includeExtraTiles, bool ignoreExtraBorder = false) const;
	bool isBuildingBlockedByCreep(CCTilePosition pos, UnitType type) const;
	std::vector<CCTilePosition> getTilesForBuildLocation(Unit building) const;
	std::vector<CCTilePosition> getTilesForBuildLocation(int bx, int by, const UnitType & type, int width, int height, bool includeExtraTiles, int extraBorder) const;
	CCTilePosition getBottomLeftForBuildLocation(int bx, int by, const UnitType & type) const;
	int getBuildingCenterOffset(int x, int y, int width, int height) const;

    // returns a build location near a building's desired location
    CCTilePosition getBuildLocationNear(const Building & b, bool ignoreReserved, bool checkInfluenceMap, bool includeExtraTiles, bool ignoreExtraBorder = false, bool forceSameHeight = false) const;
	CCTilePosition getBunkerBuildLocationNear(const Building & b, int depotX, int depotY, std::vector<CCPosition> geysersPos) const;

    void drawReservedTiles();

	void reserveTiles(UnitType type, CCTilePosition pos);//(int x, int y, int width, int height);
	void reserveTiles(CCTilePosition start, CCTilePosition end);
    void freeTiles(int x, int y, int width, int height, bool setBlocked);
	void freeTilesForTurrets(CCTilePosition position);
	void freeTilesForBunker(CCTilePosition position);
    CCTilePosition getRefineryPosition();
};
