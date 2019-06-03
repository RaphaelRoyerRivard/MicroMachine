#pragma once
//#define PUBLIC_RELEASE
//#define NO_UNITS
//#define NO_PRODUCTION

#ifdef PUBLIC_RELEASE
#undef NO_UNITS
#undef NO_PRODUCTION
#endif

#include "Common.h"

#include "MapTools.h"
#include "BaseLocationManager.h"
#include "UnitInfoManager.h"
#include "WorkerManager.h"
#include "BuildingManager.h"
#include "BotConfig.h"
#include "CombatAnalyzer.h"
#include "GameCommander.h"
#include "StrategyManager.h"
#include "TechTree.h"
#include "MetaType.h"
#include "Unit.h"
#include "RepairStationManager.h"

#ifdef SC2API
class CCBot : public sc2::Agent 
#else
class CCBot
#endif
{
	struct Profiler
	{
		Profiler() :total(0) {};
		std::deque<long long> queue;
		long long total;
		std::chrono::steady_clock::time_point start;
	};

	uint32_t				m_gameLoop;
    MapTools                m_map;
    BaseLocationManager     m_bases;
    UnitInfoManager         m_unitInfo;
    WorkerManager           m_workers;
	BuildingManager			m_buildings;
	StrategyManager         m_strategy;
	RepairStationManager    m_repairStations;
    BotConfig               m_config;
    TechTree                m_techTree;
	CombatAnalyzer			m_combatAnalyzer;
    GameCommander           m_gameCommander;
	CCPosition				m_startLocation;
	CCTilePosition			m_buildingArea;
	int						m_reservedMinerals = 0;				// minerals reserved for planned buildings
	int						m_reservedGas = 0;					// gas reserved for planned buildings
	std::map<sc2::UNIT_TYPEID, int> m_unitCount;
	std::map<sc2::UNIT_TYPEID, int> m_unitCompletedCount;
	std::map<sc2::Tag, Unit> m_allyUnits;
	std::map<sc2::Tag, Unit> m_enemyUnits;
	std::map<sc2::Tag, Unit> m_neutralUnits;
	std::set<sc2::Tag> m_parasitedUnits;
	std::map<sc2::Tag, std::pair<CCPosition, uint32_t>> m_lastSeenPosUnits;
	std::map<sc2::Tag, CCPosition> m_previousFrameEnemyPos;
	std::map<sc2::Tag, uint32_t> m_KD8ChargesSpawnFrame;
	std::vector<Unit>       m_allUnits;
	std::vector<Unit>       m_knownEnemyUnits;
    std::vector<CCPosition> m_enemyBaseLocations;
	std::map<sc2::UNIT_TYPEID, std::vector<Unit>> m_knownEnemyUnitsPerType;
	std::map<sc2::UNIT_TYPEID, std::vector<Unit>> m_allyUnitsPerType;
	CCRace selfRace;
	std::map<std::string, Profiler> m_profilingTimes;
	std::mutex m_command_mutex;
	bool m_concede;
	bool m_saidHallucinationLine;

	//KeyState
	bool key1 = false;
	bool key2 = false;
	bool key3 = false;
	bool key4 = false;
	bool key5 = false;
	bool key6 = false;
	bool key7 = false;
	bool key8 = false;
	bool key9 = false;
	bool key0 = false;

	void checkKeyState();
	void setUnits();
	void clearDeadUnits();
	void updatePreviousFrameEnemyUnitPos();
	void checkForConcede();
	void drawProfilingInfo();

#ifdef SC2API
    void OnError(const std::vector<sc2::ClientError> & client_errors, 
                 const std::vector<std::string> & protocol_errors = {}) override;
#endif

public:

	CCBot(std::string botVersion = "");

#ifdef SC2API
	void OnGameFullStart() override;
    void OnGameStart() override;
	void OnGameEnd() override;
    void OnStep() override;
	void OnUnitDestroyed(const sc2::Unit*) override;
	void OnUnitCreated(const sc2::Unit*) override;
	void OnUnitIdle(const sc2::Unit*) override;
	void OnUpgradeCompleted(sc2::UpgradeID) override;
	void OnBuildingConstructionComplete(const sc2::Unit*) override;
	void OnNydusDetected() override;
	void OnUnitEnterVision(const sc2::Unit*) override;
	void OnNuclearLaunchDetected() override;
#else
    void OnGameStart();
    void OnStep();
#endif

          BotConfig & Config();
          WorkerManager & Workers();
		  BuildingManager & Buildings();
		  BaseLocationManager & Bases();
		  CombatAnalyzer & CombatAnalyzer();
		  GameCommander & Commander();
    const MapTools & Map() const;
    const UnitInfoManager & UnitInfo() const;
	StrategyManager & Strategy();
	RepairStationManager & RepairStations() { return m_repairStations; }
    const TypeData & Data(const UnitType & type);
    const TypeData & Data(const CCUpgrade & type) const;
    const TypeData & Data(const MetaType & type);
    const TypeData & Data(const Unit & unit);
	const TypeData & Data(const sc2::UNIT_TYPEID & type);
	uint32_t GetGameLoop() const;
    const CCRace GetPlayerRace(int player) const;
	const CCRace GetSelfRace() const;
    const CCPosition GetStartLocation() const;
	const CCTilePosition GetBuildingArea() const;

	void IssueCheats();
    uint32_t GetCurrentFrame() const;
    int GetMinerals() const;
    int GetCurrentSupply() const;
    int GetMaxSupply() const;
    int GetGas() const;
	int GetReservedMinerals();
	int GetReservedGas();
	void ReserveMinerals(int reservedMinerals);
	void ReserveGas(int reservedGas);
	void FreeMinerals(int freedMinerals);
	void FreeGas(int freedGas);
	int GetFreeMinerals();
	int GetFreeGas();
    Unit GetUnit(const CCUnitID & tag) const;
    const std::vector<Unit> & GetUnits() const;
	int GetUnitCount(sc2::UNIT_TYPEID type, bool completed = false);
	std::map<sc2::Tag, Unit> & GetAllyUnits();
	const std::vector<Unit> & GetAllyUnits(sc2::UNIT_TYPEID type);
	const std::vector<Unit> GetAllyDepotUnits();//Cannot be by reference, vector created in function
	const std::vector<Unit> GetAllyGeyserUnits();//Cannot be by reference, vector created in function
	std::map<sc2::Tag, Unit> & GetEnemyUnits();
	const std::vector<Unit> & GetKnownEnemyUnits() const;
	const std::vector<Unit> & GetKnownEnemyUnits(sc2::UnitTypeID type);
	std::map<sc2::Tag, Unit> & GetNeutralUnits();
	bool IsParasited(const sc2::Unit * unit) const;
	std::map<sc2::Tag, CCPosition> & GetPreviousFrameEnemyPos() { return m_previousFrameEnemyPos; }
    const std::vector<CCPosition> & GetStartLocations() const;
    const std::vector<CCPosition> & GetEnemyStartLocations() const;
	void StartProfiling(const std::string & profilerName);
	void StopProfiling(const std::string & profilerName);
	std::mutex & GetCommandMutex();
	bool shouldConcede() const { return m_concede; }
};