#pragma once

#include "BaseLocation.h"

class CCBot;

class BaseLocationManager
{
    CCBot & m_bot;

	bool m_areBaseLocationPtrsSorted;
    std::vector<BaseLocation>                       m_baseLocationData;
    std::vector<const BaseLocation *>               m_baseLocationPtrs;
    std::vector<const BaseLocation *>               m_startingBaseLocations;
    std::map<int, const BaseLocation *>             m_playerStartingBaseLocations;
    std::map<int, std::set<BaseLocation *>>			m_occupiedBaseLocations;
    std::vector<std::vector<BaseLocation *>>        m_tileBaseLocations;

	void sortBaseLocationPtrs();

public:

    BaseLocationManager(CCBot & bot);
    
    void onStart();
    void onFrame();
    void drawBaseLocations();

	BaseLocation * getBaseLocation(const CCPosition & pos) const;
    const std::vector<const BaseLocation *> & getBaseLocations() const;
    const std::vector<const BaseLocation *> & getStartingBaseLocations() const;
    const std::set<BaseLocation *> & getOccupiedBaseLocations(int player) const;
	BaseLocation * getClosestOccupiedBaseLocationForUnit(const Unit unit) const;
    const BaseLocation * getPlayerStartingBaseLocation(int player) const;
	void FixNullPlayerStartingBaseLocation();
	int BaseLocationManager::getBaseCount(int player, bool isCompleted = false) const;

	const BaseLocation* getNextExpansion(int player, bool checkBuildable = false) const;
	CCTilePosition getNextExpansionPosition(int player, bool checkBuildable = false) const;
	CCTilePosition getBasePosition(int player, int index) const;
	CCTilePosition getClosestBasePosition(const sc2::Unit* unit, int player = Players::Self, bool shiftTowardsResourceDepot = false) const;
	const BaseLocation* getBaseContainingPosition(const CCPosition position, int player) const;

};
