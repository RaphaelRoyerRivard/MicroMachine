#pragma once
#include "BaseLocation.h"

class CCBot;

class RepairStationManager
{
public:
	RepairStationManager(CCBot& bot);

	void onStart();
	void onFrame();

	CCPosition getBestRepairStationForUnit(const sc2::Unit* unit);

private:
	CCBot & m_bot;
	std::unordered_map<const BaseLocation*, std::list<const sc2::Unit*>> m_repairStations;
	std::unordered_map<const sc2::Unit*, const BaseLocation*> m_destinations;

	bool isRepairStationValidForBaseLocation(const BaseLocation * baseLocation, bool ignoreUnderAttack = true);
	void reservePlaceForUnit(const sc2::Unit* unit);
	void drawRepairStations();
};

