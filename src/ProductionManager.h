#pragma once

#include "Common.h"
#include "BuildOrder.h"
#include "BuildOrderQueue.h"
#include "Unit.h"

class CCBot;

class ProductionManager
{
    CCBot &       m_bot;

    BuildOrderQueue m_queue;
	bool m_initialBuildOrderFinished;
	bool m_ccShouldBeInQueue = false;
	Unit rampSupplyDepotWorker;
	std::list<MetaType> startedUpgrades;
	bool firstBarrackBuilt = false;
	UnitType supplyProvider;
	MetaType supplyProviderType;

	MetaType getUpgradeMetaType(const MetaType type) const;
    Unit    getClosestUnitToPosition(const std::vector<Unit> & units, CCPosition closestTo) const;
    bool    canMakeNow(const Unit & producer, const MetaType & type);
	bool    canMakeSoon(const Unit & producer, const MetaType & type);
    bool    detectBuildOrderDeadlock();
    void    setBuildOrder(const BuildOrder & buildOrder);
    void    create(const Unit & producer, BuildOrderItem & item, CCTilePosition position = CCTilePosition(0,0));
    void    manageBuildOrderQueue();
	void	putImportantBuildOrderItemsInQueue();
    int     getFreeMinerals();
    int     getFreeGas();
	int     getExtraMinerals();
	int     getExtraGas();

	void	fixBuildOrderDeadlock(BuildOrderItem & item);
	void	lowPriorityChecks();
	bool	currentlyHasRequirement(MetaType currentItem);
	bool	hasRequired(const MetaType& metaType, bool checkInQueue);
	bool	hasProducer(const MetaType& metaType, bool checkInQueue);

public:

    ProductionManager(CCBot & bot);

    void    onStart();
    void    onFrame();
    void    onUnitDestroy(const Unit & unit);
    void    drawProductionInformation();

    Unit getProducer(const MetaType & type, CCPosition closestTo = CCPosition(0, 0)) const;
	int getProductionBuildingsCount() const;
	int getProductionBuildingsAddonsCount() const;
	float getProductionScore() const;
	float getProductionScoreInQueue();
	bool meetsReservedResources(const MetaType & type);
	bool meetsReservedResourcesWithExtra(const MetaType & type);
	std::vector<Unit> getUnitTrainingBuildings(CCRace race);
};
