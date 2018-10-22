#pragma once

#include "WorkerData.h"

class Building;
class CCBot;

class WorkerManager
{
    CCBot & m_bot;
	bool m_isFirstFrame = true;

    mutable WorkerData  m_workerData;
    Unit m_previousClosestWorker;

    void setMineralWorker(const Unit & unit);
    
	void handleMineralWorkers();
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
	void setGasWorker(Unit workerTag);
    void setScoutWorker(Unit worker);
    void setCombatWorker(Unit worker);
	void setBuildingWorker(Unit worker);
    void setBuildingWorker(Unit worker, Building & b);
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
	bool WorkerManager::isReturningCargo(Unit worker) const;

    Unit getBuilder(Building & b, bool setJobAsBuilder = true) const;
	Unit getGasWorker(Unit refinery) const;
	Unit getDepotAtBasePosition(CCPosition basePosition) const;
	int  getWorkerCountAtBasePosition(CCPosition basePosition) const;
    Unit getClosestDepot(Unit worker) const;
	Unit getClosestMineralWorkerTo(const CCPosition & pos, float minHpPercentage = 0.f) const;
	Unit getClosestMineralWorkerTo(const CCPosition & pos, CCUnitID workerToIgnore, float minHpPercentage = 0.f) const;
	Unit getClosest(const Unit unit, const std::list<Unit> units) const;
	//std::list<Unit> WorkerManager::orderByDistance(const std::list<Unit> units, CCPosition pos, bool closestFirst);
};

