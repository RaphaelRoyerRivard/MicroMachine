#pragma once

#include "../src/BuildingData.h"

class Building;
class CCBot;

class BuildingManager
{
	CCBot & m_bot;

	/*mutable BuildingData  m_buildingData;
	Unit m_previousClosestBuilding;

	void setMineralBuilding(const Unit & unit);*/

	void handleIdleBuildings();
	void handleGasBuildings();
	void handleRepairBuildings();

public:

	BuildingManager(CCBot & bot);

	void onStart();
	void onFrame();
};

