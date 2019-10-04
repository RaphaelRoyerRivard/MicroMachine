#pragma once

#include "WorkerData.h"
#include <list>

class Building;
class CCBot;

//Using in first frame worker split to sort the map by value
typedef std::function<bool(std::pair<Unit, int>, std::pair<Unit, int>)> Comparator;

class WorkerManager
{
    CCBot & m_bot;
	bool m_isFirstFrame = true;
	int gasWorkersTarget = 3;
	std::list<Unit> buildingAutomaticallyRepaired;
	std::list<Unit> depletedGeyser;

	//<MuleId, <isReturningCargo, <harvest count, target mineral ID>>>, we are not removing killed mules from this map, but it doesn't really matter
	std::map<CCUnitID, std::pair<bool, std::pair<int, sc2::Tag> > > muleHarvests;

    mutable WorkerData  m_workerData;
    Unit m_previousClosestWorker;

    void setMineralWorker(const Unit & unit);
    
	void handleMineralWorkers();
	void handleMules();
    void handleGasWorkers();
	void handleIdleWorkers();
    void handleRepairWorkers();
	void repairCombatBuildings();
	void lowPriorityChecks();

public:

	const float MIN_HP_PERCENTAGE_TO_FIGHT = 0.25f;

    WorkerManager(CCBot & bot);

    void onStart();
    void onFrame();

    void finishedWithWorker(const Unit & unit);
    void drawResourceDebugInfo();
    void drawWorkerInformation();
    void setScoutWorker(Unit worker);
    void setCombatWorker(Unit worker);
    void setRepairWorker(Unit worker,const Unit & unitToRepair);
    void stopRepairing(Unit worker);

    int  getNumMineralWorkers();
    int  getNumGasWorkers();
    int  getNumWorkers();
	std::set<Unit> getWorkers() const;
	WorkerData & getWorkerData() const;
	
	sc2::Tag getMuleTargetTag(const Unit mule);
	void setMuleTargetTag(const Unit mule, const sc2::Tag mineral);

    bool isWorkerScout(Unit worker) const;
    bool isFree(Unit worker) const;
	bool isInsideGeyser(Unit worker) const;
    bool isBuilder(Unit worker) const;
	bool isReturningCargo(Unit worker) const;
	bool canHandleMoreRefinery() const;

    Unit getBuilder(Building & b, bool setJobAsBuilder = true, bool filterMoving = true) const;
	Unit getMineralWorker(Unit refinery) const;
	Unit getGasWorker(Unit refinery, bool checkReturningCargo, bool checkInsideRefinery) const;
	int  getGasWorkersTarget() const;
	Unit getDepotAtBasePosition(CCPosition basePosition) const;
	int  getWorkerCountAtBasePosition(CCPosition basePosition) const;
    Unit getClosestDepot(Unit worker) const;
	Unit getClosestMineralWorkerTo(const CCPosition & pos, float minHpPercentage = 0.f, bool filterMoving = true) const;
	Unit getClosestMineralWorkerTo(const CCPosition & pos, CCUnitID workerToIgnore, float minHpPercentage = 0.f, bool filterMoving = true) const;
	Unit getClosestMineralWorkerTo(const CCPosition & pos, const std::vector<CCUnitID> & workersToIgnore, float minHpPercentage, bool filterMoving = true) const;
	Unit getClosestGasWorkerTo(const CCPosition & pos, float minHpPercentage = 0.f) const;
	Unit getClosestGasWorkerTo(const CCPosition & pos, CCUnitID workerToIgnore, float minHpPercentage = 0.f) const;
	Unit getClosest(const Unit unit, const std::list<Unit> units) const;
	//std::list<Unit> orderByDistance(const std::list<Unit> units, CCPosition pos, bool closestFirst);
};