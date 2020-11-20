#pragma once

#include "BaseLocation.h"

class CCBot;

class BaseLocationManager
{
    CCBot & m_bot;

	bool m_areBaseLocationPtrsSorted;
    std::vector<BaseLocation>                       m_baseLocationData;
    std::vector<BaseLocation *>						m_baseLocationPtrs;
    std::vector<const BaseLocation *>               m_startingBaseLocations;
    std::map<int, const BaseLocation *>             m_playerStartingBaseLocations;
    std::map<int, std::set<BaseLocation *>>			m_occupiedBaseLocations;
    std::vector<std::vector<BaseLocation *>>        m_tileBaseLocations;
	std::vector<std::vector<bool>>					m_resourceProximity;

	const int NearBaseLocationTileDistance = 38;
	const float TerrainHeightCostMultiplier = 5.f;

	void sortBaseLocationPtrs();

public:

    BaseLocationManager(CCBot & bot);
    
    void onStart();
	bool affectToCluster(std::vector<std::vector<Unit>> & resourceClusters, Unit & resource, float maxDistanceWithCluster) const;
    void onFrame();
    void drawBaseLocations();
	void drawTileBaseLocationAssociations() const;
	void drawResourceProxity();

	BaseLocation * getBaseLocation(const CCPosition & pos) const;
    const std::vector<BaseLocation *> & getBaseLocations() const;
    const std::vector<const BaseLocation *> & getStartingBaseLocations() const;
    const std::set<BaseLocation *> & getOccupiedBaseLocations(int player) const;
	int getFreeBaseLocationCount() const;
	BaseLocation * getClosestOccupiedBaseLocationForUnit(const Unit unit) const;
	BaseLocation * getFarthestOccupiedBaseLocation() const;
    const BaseLocation * getPlayerStartingBaseLocation(int player) const;
	void FixNullPlayerStartingBaseLocation();
	int getBaseCount(int player, bool isCompleted = false) const;

	BaseLocation* getNextExpansion(int player, bool checkBlocked, bool checkBuildable, bool ignoreReservedTiles) const;
	CCTilePosition getNextExpansionPosition(int player, bool checkBlocked, bool checkBuildable, bool ignoreReservedTiles) const;
	CCTilePosition getBasePosition(int player, int index) const;
	BaseLocation* getClosestBase(const CCPosition position, bool checkContains) const;
	CCTilePosition getClosestBasePosition(const sc2::Unit* unit, int player = Players::Self, bool shiftTowardsResourceDepot = false, bool checkContainsMinerals = false, bool checkUnderAttack = false) const;
	const BaseLocation* getBaseForDepotPosition(const CCTilePosition position) const;
	const BaseLocation* getBaseForDepot(const Unit depot) const;
	void SetLocationAsBlocked(const CCPosition position, UnitType type);
	void ClearBlockedLocations();
	std::vector<Unit> getEnemyUnitsNear(CCTilePosition center) const;
	const BaseLocation* getBaseContainingPosition(const CCPosition position, int player = -1) const;
	bool isInProximityOfResources(int x, int y) const;
	int getAccessibleMineralFieldCount() const;
};
