#pragma once

#include "Common.h"
#include "DistanceMap.h"
#include "Unit.h"
#include <map>
#include <vector>

class CCBot;

class BaseLocation
{
    CCBot &                     m_bot;
    DistanceMap                 m_distanceMap;

	CCTilePosition				m_turretPosition;
	CCPosition					m_depotPosition;
    CCTilePosition              m_depotTilePosition;
    CCPosition                  m_centerOfResources;
    std::vector<Unit>           m_geysers;
    std::vector<Unit>           m_minerals;
	Unit						m_resourceDepot;
	std::vector<Unit>			m_gasBunkers;
	bool						m_isUnderAttack;
	bool						m_isBlocked;
	bool						m_isSplitGeyser;
	std::vector<CCTilePosition> m_gasBunkerLocations;

	CCTilePosition				m_centerOfMinerals;

    std::vector<CCPosition>     m_mineralPositions;
    std::vector<CCPosition>     m_geyserPositions;

    std::map<CCPlayer, bool>    m_isPlayerOccupying;
    std::map<CCPlayer, bool>    m_isPlayerStartLocation;
        
    int                         m_baseID;
    CCPositionType              m_left;
    CCPositionType              m_right;
    CCPositionType              m_top;
    CCPositionType              m_bottom;
    bool                        m_isStartLocation;
	bool						m_snapshotsRemoved = false;

	const int ApproximativeBaseLocationTileDistance = 30;
public:
    BaseLocation(CCBot & bot, int baseID, const std::vector<Unit> & resources);
    
    int getGroundDistance(const CCPosition & pos) const;
    int getGroundDistance(const CCTilePosition & pos) const;
    bool isStartLocation() const;
    bool isPlayerStartLocation(CCPlayer player) const;
    bool isMineralOnly() const;
	bool containsPositionApproximative(const CCPosition & pos, int maxDistance = 0) const;
	bool containsUnitApproximative(const Unit & unit, int maxDistance = 0) const;
    bool containsPosition(const CCPosition & pos) const;
	const CCTilePosition & getTurretPosition() const;
	const std::vector<CCTilePosition> & getGasBunkerLocations() const;
	const CCPosition & getDepotPosition() const;
    const CCTilePosition & getDepotTilePosition() const;
	int getOptimalMineralWorkerCount() const;
    const CCPosition & getPosition() const;
    const std::vector<Unit> & getGeysers() const;
    const std::vector<Unit> & getMinerals() const;
	const Unit& getResourceDepot() const { return m_resourceDepot; }
	void setResourceDepot(Unit resourceDepot) { m_resourceDepot = resourceDepot; }
	const std::vector<Unit> & getGasBunkers() const { return m_gasBunkers; }
	void clearGasBunkers() { m_gasBunkers = std::vector<Unit>(); }
	void addGasBunkers(Unit gasBunker) { m_gasBunkers.push_back(gasBunker); }
	bool isUnderAttack() const { return m_isUnderAttack; }
	void setIsUnderAttack(bool isUnderAttack) { m_isUnderAttack = isUnderAttack; }
	bool isBlocked() const { return m_isBlocked; }
	void setIsBlocked(bool isBlocked) { m_isBlocked = isBlocked; }
	const CCTilePosition getCenterOfMinerals() const;
    bool isOccupiedByPlayer(CCPlayer player) const;
    bool isExplored() const;

    void setPlayerOccupying(CCPlayer player, bool occupying);

    const std::vector<CCTilePosition> & getClosestTiles() const;
	const bool & isGeyserSplit() const;

    void draw();
};
