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
	std::map<MetaType, Unit> incompletUpgrades;
	std::list<std::list<MetaType>> possibleUpgrades;//Does not include tech
	bool firstBarrackBuilt = false;
	UnitType supplyProvider;
	MetaType supplyProviderType;
	UnitType workerType;
	MetaType workerMetatype;

	void	queueUpgrade(const MetaType & type);
	bool	isTechStarted(const MetaType & type);
	void	queueTech(const MetaType & type);
	void	validateUpgradesProgress();
    Unit    getClosestUnitToPosition(const std::vector<Unit> & units, CCPosition closestTo) const;
    bool    canMakeNow(const Unit & producer, const MetaType & type);
    bool    detectBuildOrderDeadlock();
    void    setBuildOrder(const BuildOrder & buildOrder);
    void    create(const Unit & producer, BuildOrderItem & item, CCTilePosition position = CCTilePosition(0,0));
    void    manageBuildOrderQueue();
	void	putImportantBuildOrderItemsInQueue();
	void	QueueDeadBuildings();
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
	int supplyBlockedFrames = 0;

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
	bool meetsReservedResources(const MetaType & type, int additionalReservedMineral = 0, int additionalReservedGas = 0);
	bool meetsReservedResourcesWithExtra(const MetaType & type, int additionalReservedMineral = 0, int additionalReservedGas = 0);
	std::vector<Unit> getUnitTrainingBuildings(CCRace race);
};
