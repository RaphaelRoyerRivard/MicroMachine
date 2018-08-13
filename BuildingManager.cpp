#include "BuildingManager.h"
#include "../src/CCBot.h"
#include "../src/Util.h"
#include "../src/Building.h"

BuildingManager::BuildingManager(CCBot & bot)
	: m_bot				(bot)
	, m_buildingData	(bot)
{

}

void BuildingManager::onStart()
{

}

void BuildingManager::onFrame()
{
	/*m_buildingData.updateAllBuildingData();
	handleGasBuildings();
	handleIdleBuildings();

	drawBuildingInformation();

	m_buildingData.drawDepotDebugInfo();

	handleRepairBuildings();*/
}