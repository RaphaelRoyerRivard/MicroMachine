#pragma once

#include "BaseLocation.h"

class CCBot;

class BaseLocationManager
{
    CCBot & m_bot;

    std::vector<BaseLocation>                       m_baseLocationData;
    std::vector<const BaseLocation *>               m_baseLocationPtrs;
    std::vector<const BaseLocation *>               m_startingBaseLocations;
    std::map<int, const BaseLocation *>             m_playerStartingBaseLocations;
    std::map<int, std::set<const BaseLocation *>>   m_occupiedBaseLocations;
    std::vector<std::vector<BaseLocation *>>        m_tileBaseLocations;
	bool m_hasPrintedError;

public:

    BaseLocationManager(CCBot & bot);
    
    void onStart();
    void onFrame();
    void drawBaseLocations();

	BaseLocation * getBaseLocation(const CCPosition & pos) const;
    const std::vector<const BaseLocation *> & getBaseLocations() const;
    const std::vector<const BaseLocation *> & getStartingBaseLocations() const;
    const std::set<const BaseLocation *> & getOccupiedBaseLocations(int player) const;
    const BaseLocation * getPlayerStartingBaseLocation(int player) const;
	void FixNullPlayerStartingBaseLocation();
	int BaseLocationManager::getBaseCount(int player, bool isCompleted = false) const;

	const BaseLocation* getNextExpansion(int player, bool checkBuildable = false) const;
	CCTilePosition getNextExpansionPosition(int player, bool checkBuildable = false) const;
	CCTilePosition getBasePosition(int player, int index) const;

};
