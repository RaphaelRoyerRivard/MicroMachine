#pragma once

#include "WorkerData.h"

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
	std::map<CCUnitID, std::pair<bool, int>> muleHarvests;//<MuleId, <isReturningCargo, harvest count>>, we are not removing killed mules from this map, but it doesn't really matter

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
	std::set<Unit> WorkerManager::getWorkers() const;
	WorkerData & WorkerManager::getWorkerData() const;
    bool isWorkerScout(Unit worker) const;
    bool isFree(Unit worker) const;
    bool isBuilder(Unit worker) const;
	bool isReturningCargo(Unit worker) const;
	bool canHandleMoreRefinery() const;

    Unit getBuilder(Building & b, bool setJobAsBuilder = true) const;
	Unit getMineralWorker(Unit refinery) const;
	Unit getGasWorker(Unit refinery, bool checkReturningCargo = false) const;
	int  getGasWorkersTarget() const;
	Unit getDepotAtBasePosition(CCPosition basePosition) const;
	int  getWorkerCountAtBasePosition(CCPosition basePosition) const;
    Unit getClosestDepot(Unit worker) const;
	Unit getClosestMineralWorkerTo(const CCPosition & pos, float minHpPercentage = 0.f) const;
	Unit getClosestMineralWorkerTo(const CCPosition & pos, CCUnitID workerToIgnore, float minHpPercentage = 0.f) const;
	Unit getClosestMineralWorkerTo(const CCPosition & pos, std::vector<CCUnitID> workerToIgnore, float minHpPercentage) const;
	Unit getClosestGasWorkerTo(const CCPosition & pos, float minHpPercentage = 0.f) const;
	Unit getClosestGasWorkerTo(const CCPosition & pos, CCUnitID workerToIgnore, float minHpPercentage = 0.f) const;
	Unit getClosest(const Unit unit, const std::list<Unit> units) const;
	//std::list<Unit> WorkerManager::orderByDistance(const std::list<Unit> units, CCPosition pos, bool closestFirst);
};

