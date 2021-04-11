#pragma once
#include "Common.h"
#include "Unit.h"
#include "BaseLocation.h"
#include <list>

class CCBot;

namespace WorkerJobs
{
    enum { Minerals, Gas, Build, Combat, Idle, Repair, Move, Scout, GeyserProtect, None, Num };
}

class WorkerData
{
    CCBot & m_bot;

    std::set<Unit>          m_workers;
    std::set<Unit>          m_proxyWorkers;
    std::set<Unit>          m_depots;
	std::set<Unit>			m_idleWorkers;
	std::set<Unit>			m_mineralWorkers;
    std::map<int, int>      m_workerJobCount;
    std::map<Unit, int>     m_workerJobMap;
    std::map<Unit, int>     m_refineryWorkerCount;
    std::map<Unit, int>     m_depotWorkerCount;
    std::map<Unit, Unit>    m_workerRefineryMap;
	std::map<Unit, std::vector<Unit>> m_refineryWorkerMap;
    std::map<Unit, std::set<Unit>> m_workerRepairing;
	std::map<const BaseLocation*, std::list<Unit>> m_repairStationWorkers;
    std::map<Unit, Unit>    m_workerRepairTarget;
	Unit					m_idleMineralTarget;

    void clearPreviousJob(const Unit & unit);
    std::set<Unit> getWorkerRepairingThatTarget(const Unit & unit);
	const Unit GetBestMineralWithLessWorkersInLists(const std::vector<Unit> & closeMinerals, const std::vector<Unit> & farMinerals, const CCPosition location) const;

public:
	//Public variables for simplicity
	std::map<Unit, Unit>	m_workerMineralMap;
	std::map<Unit, std::list<sc2::Tag>> m_mineralWorkersMap;
	std::map<Unit, Unit>    m_workerDepotMap;	//<worker, depot>

    WorkerData(CCBot & bot);

    void    workerDestroyed(const Unit & unit);
    void    updateAllWorkerData();
	void	updateIdleMineralTarget();
    void    updateWorker(const Unit & unit);
    void    setWorkerJob(const Unit & unit, int job, Unit jobUnit = Unit());
    void    drawDepotDebugInfo();
    size_t  getNumWorkers() const;
    int     getWorkerJobCount(int job) const;
    int     getNumAssignedWorkers(const Unit & unit);
	std::vector<Unit> getAssignedWorkersRefinery(const Unit & unit);
    int     getWorkerJob(const Unit & unit) const;
	bool	isReturningCargo(const Unit & unit) const;
	bool	isActivelyReturningCargo(const Unit & unit) const;
	int		getCountWorkerAtDepot(const Unit & depot) const;
    Unit    getMineralToMine(const Unit & unit, const CCPosition location) const;
    Unit    getWorkerDepot(const Unit & unit) const;
    const char * getJobCode(const Unit & unit);
    const std::set<Unit> & getWorkers() const;
    const std::set<Unit> & getProxyWorkers() const;
	const std::set<Unit> getIdleWorkers() const;
	const std::set<Unit> getMineralWorkers() const;
	void sendIdleWorkerToIdleSpot(const Unit & worker, bool force);
	void sendIdleWorkerToMiningSpot(const Unit & worker, bool force);
	bool isProxyWorker(const Unit & unit) const;
	void setProxyWorker(const Unit & unit);
	bool isAnyMineralAvailable(CCPosition workerCurrentPosition) const;
	void removeProxyWorker(const Unit & unit);
	void clearProxyWorkers();
	std::map<const BaseLocation*, std::list<Unit>>& getRepairStationWorkers();
	void validateRepairStationWorkers();
	Unit GetBestMineralInList(const std::vector<Unit> & unitsToTest, CCPosition depotPosition, bool checkVisibility) const;
	Unit GetBestMineralInList(const std::vector<Unit> & unitsToTest, const Unit & worker, bool checkVisibility) const;
    Unit getWorkerRepairTarget(const Unit & unit) const;
	int getWorkerRepairingTargetCount(const Unit & unit);
    const std::set<Unit> getWorkerRepairingThatTargetC(const Unit & unit) const;
    void WorkerStoppedRepairing(const Unit & unit);
	Unit updateWorkerDepot(const Unit & worker, const Unit & mineral);
};