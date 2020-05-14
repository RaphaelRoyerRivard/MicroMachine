#pragma once

#include "Common.h"
#include "BuildOrder.h"
#include "BuildOrderQueue.h"
#include "Unit.h"
#include "Building.h"
#include <list>

class CCBot;

class ProductionManager
{
    CCBot &       m_bot;
	uint32_t m_lastLowPriorityCheckFrame = 0;
    BuildOrderQueue m_queue;
	bool m_initialBuildOrderFinished;
	bool m_ccShouldBeInQueue = false;
	Unit rampSupplyDepotWorker;
	std::list<MetaType> incompleteUpgradesMetatypes;
	std::map<MetaType, Unit> incompleteUpgrades;
	std::map<MetaType, float> incompleteUpgradesProgress;
	std::list<std::list<MetaType>> possibleUpgrades;//Does not include tech
	std::list<std::list<MetaType>> reversePossibleUpgrades;//Does not include tech
	std::map<std::string, MetaType> alternateUpgrades;//Tech do not have alternate upgrades
	bool firstBarrackBuilt = false;
	bool firstBarracksTechlab = true;
	UnitType supplyProvider;
	MetaType supplyProviderType;
	UnitType workerType;
	MetaType workerMetatype;

	void	validateUpgradesProgress();
    Unit    getClosestUnitToPosition(const std::vector<Unit> & units, CCPosition closestTo) const;
    bool    canMakeNow(const Unit & producer, const MetaType & type);
    bool    detectBuildOrderDeadlock();
    void    setBuildOrder(const BuildOrder & buildOrder);
    bool    create(const Unit & producer, BuildOrderItem & item, CCTilePosition desidredPosition, bool reserveResources = true, bool filterMovingWorker = true, bool canBePlacedElsewhere = true);
	bool    create(const Unit & producer, Building & b, bool filterMovingWorker = true);
    void    manageBuildOrderQueue();
	bool	ShouldSkipQueueItem(const BuildOrderItem & currentItem) const;
	void	putImportantBuildOrderItemsInQueue();
	void	QueueDeadBuildings();

	void	fixBuildOrderDeadlock(BuildOrderItem & item);
	void	lowPriorityChecks();
	bool	currentlyHasRequirement(MetaType currentItem) const;
	bool	hasRequiredUnit(const UnitType& unitType, bool checkInQueue) const;
	bool	hasProducer(const MetaType& metaType, bool checkInQueue);

	bool	ValidateBuildingTiming(Building & b) const;

public:
	int supplyBlockedFrames = 0;

    ProductionManager(CCBot & bot);

    void    onStart();
    void    onFrame(bool executeMacro);
    void    onUnitDestroy(const Unit & unit);
    void    drawProductionInformation();

	bool	hasRequired(const MetaType& metaType, bool checkInQueue) const;
    Unit getProducer(const MetaType & type, CCPosition closestTo = CCPosition(0, 0)) const;
	std::vector<sc2::UNIT_TYPEID> getProductionBuildingTypes() const;
	int getProductionBuildingsCount() const;
	int getProductionBuildingsAddonsCount() const;
	float getProductionScore() const;
	float getProductionScoreInQueue();
	int getExtraMinerals();
	int getExtraGas();
	bool isTechQueuedOrStarted(const MetaType & type);
	bool isTechStarted(const MetaType & type);
	bool isTechFinished(const MetaType & type) const;
	void queueTech(const MetaType & type);
	bool queueUpgrade(const MetaType & type, bool balanceUpgrades, bool ifFinishedTryHigherLevel);
	bool meetsReservedResources(const MetaType & type, int additionalReservedMineral = 0, int additionalReservedGas = 0);
	bool meetsReservedResourcesWithExtra(const MetaType & type, int additionalMineral, int additionalGas, int additionalReservedMineral, int additionalReservedGas);
	bool canMakeAtArrival(const Building & building, const Unit & worker, int additionalReservedMineral, int additionalReservedGas);
	std::vector<Unit> getUnitTrainingBuildings(CCRace race);
	int getSupplyNeedsFromProductionBuildings() const;
	void clearQueue();
	BuildOrderItem queueAsHighestPriority(const MetaType & type, bool blocking);
};
