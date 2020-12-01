#pragma once
//#define PUBLIC_RELEASE
//#define NO_UNITS
//#define NO_EXPANSION
//#define NO_PRODUCTION
//#define NO_BUILDING
//#define NO_MICRO

//Debug only draw only
//#define COMPUTE_WALKABLE_TILES

#ifdef PUBLIC_RELEASE
#undef NO_UNITS
#undef NO_PRODUCTION
#undef NO_BUILDING
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
#include "Unit.h"
#include "RepairStationManager.h"

class CCBot : public sc2::Agent 
{
	struct Profiler
	{
		Profiler() :total(0), count(0) {};
		std::deque<std::pair<long long, int>> queue;
		long long total;
		int count;
		std::chrono::steady_clock::time_point start;
	};

	//TEMP [deleteAllTheseTagsAtOnce] all 4 variables below are just for debug, delete them when the action bug test is deleted
	int actionFrame = 0;
	int actionTotal = 0;
	CCTilePosition target;
	CCTilePosition prevTarget;

	uint32_t				m_gameLoop = 0;
	uint32_t				m_previousGameLoop;
	int						m_previousMacroGameLoop;
	bool					m_combatSimulatorInitialized = false;
	uint32_t				m_skippedFrames;
	uint32_t				m_lastProfilingLagOutput = 0;
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
	std::map<sc2::UNIT_TYPEID, int> m_deadAllyUnitsCount;
	std::map<sc2::Tag, Unit> m_allyUnits;
	std::map<sc2::Tag, Unit> m_enemyUnits;
	std::map<sc2::Tag, Unit> m_neutralUnits;
	std::set<sc2::Tag> m_parasitedUnits;
	std::map<sc2::Tag, std::pair<CCPosition, uint32_t>> m_lastSeenPosUnits;
	std::map<sc2::Tag, CCPosition> m_previousFrameEnemyPos;
	std::map<sc2::Tag, uint32_t> m_KD8ChargesSpawnFrame;
	std::vector<Unit>       m_allUnits;
	std::vector<Unit>       m_knownEnemyUnits;
	std::vector<Unit>		m_enemyBuildingsUnderConstruction;
    std::vector<CCPosition> m_enemyBaseLocations;
	std::map<sc2::UNIT_TYPEID, std::vector<Unit>> m_enemyUnitsPerType;
	std::map<sc2::UNIT_TYPEID, std::vector<Unit>> m_allyUnitsPerType;
	std::map<const sc2::Unit *, std::set<const sc2::Unit *>> m_enemyUnitsBeingRepaired;
	std::set<const sc2::Unit *> m_enemyRepairingSCVs;
	std::set<const sc2::Unit *> m_enemySCVBuilders;
	std::set<const sc2::Unit *> m_enemyWorkersGoingInRefinery;
	std::list<sc2::Units> m_stackedEnemyWorkers;
	CCRace selfRace;
	CCRace enemyRace = sc2::Random;
	std::map<std::string, Profiler> m_profilingTimes;
	std::mutex m_command_mutex;
	bool m_concede;
	bool m_saidHallucinationLine;
	std::string m_botVersion;
	std::string m_opponentId;
	bool m_player1IsHuman;
	bool m_realtime;
	std::stringstream m_versionMessage;

	std::chrono::steady_clock::time_point m_lastFrameEndTime;

	//KeyState
	bool keyDelete = false;
	bool keyEnd = false;
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
	bool keyF1 = false;
	bool keyF2 = false;

	void checkKeyState();
	void setUnits();
	void identifyEnemyRepairingSCVs();
	void identifyEnemySCVBuilders();
	void identifyEnemyWorkersGoingIntoRefinery();
	void identifyStackedEnemyWorkers();
	void clearDeadUnits();
	void clearDuplicateUnits();
	void updatePreviousFrameEnemyUnitPos();
	void checkForConcede();
	void drawProfilingInfo();

    void OnError(const std::vector<sc2::ClientError> & client_errors, 
                 const std::vector<std::string> & protocol_errors = {}) override;

public:

	CCBot(std::string botVersion, bool realtime);
	~CCBot();

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

          BotConfig & Config();
          WorkerManager & Workers();
		  BuildingManager & Buildings();
		  BaseLocationManager & Bases();
		  CombatAnalyzer & Analyzer();
		  GameCommander & Commander();
		  TechTree & Tech() { return m_techTree; }
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
    CCRace GetPlayerRace(int player) const;
	CCRace GetSelfRace() const;
    CCPosition GetStartLocation() const;
	const CCTilePosition GetBuildingArea(MetaType buildingType);

	void IssueGameStartCheats();
	void IssueCheats();
	void DebugMenu();
    uint32_t GetCurrentFrame() const;
	uint32_t getPreviousFrame() const { return m_previousGameLoop; };
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
	Unit GetUnit(const sc2::PassengerUnit & passenger);
	const sc2::Unit * GetUnitPtr(const CCUnitID & tag) const;
    const std::vector<Unit> & GetUnits() const;
	int GetUnitCount(sc2::UNIT_TYPEID type, bool completed = false, bool underConstruction = false);
	const std::map<sc2::UNIT_TYPEID, int> & GetCompletedUnitCounts() const { return m_unitCompletedCount; }
	int GetDeadAllyUnitsCount(sc2::UNIT_TYPEID type) const;
	std::map<sc2::Tag, Unit> & GetAllyUnits();
	const std::vector<Unit> & GetAllyUnits(sc2::UNIT_TYPEID type);
	const std::vector<Unit> GetAllyDepotUnits();//Cannot be by reference, vector created in function
	const std::vector<Unit> GetAllyGeyserUnits();//Cannot be by reference, vector created in function
	std::map<sc2::Tag, Unit> & GetEnemyUnits();
	const std::map<const sc2::Unit *, std::set<const sc2::Unit *>> & GetEnemyUnitsBeingRepaired() const { return m_enemyUnitsBeingRepaired; }
	const std::set<const sc2::Unit *> & GetEnemyRepairingSCVs() const { return m_enemyRepairingSCVs; }
	const std::set<const sc2::Unit *> & GetEnemySCVBuilders() const { return m_enemySCVBuilders; }
	const std::set<const sc2::Unit *> & GetEnemyWorkersGoingInRefinery() const { return m_enemyWorkersGoingInRefinery; }
	const std::list<sc2::Units> & GetStackedEnemyWorkers() const { return m_stackedEnemyWorkers; }
	const std::vector<Unit> & GetKnownEnemyUnits() const;
	const std::vector<Unit> & GetEnemyUnits(sc2::UnitTypeID type);
	const std::vector<Unit> & GetEnemyBuildingsUnderConstruction() const { return m_enemyBuildingsUnderConstruction; }
	std::map<sc2::Tag, Unit> & GetNeutralUnits();
	bool IsParasited(const sc2::Unit * unit) const;
	std::map<sc2::Tag, CCPosition> & GetPreviousFrameEnemyPos() { return m_previousFrameEnemyPos; }
    const std::vector<CCPosition> & GetStartLocations() const;
    const std::vector<CCPosition> & GetEnemyStartLocations() const;
	void StartProfiling(const std::string & profilerName);
	void StopProfiling(const std::string & profilerName);
	void drawTimeControl();
	std::mutex & GetCommandMutex();
	bool shouldConcede() const { return m_concede; }
	std::string GetOpponentId() const { return m_opponentId; }
	void SetOpponentId(std::string opponentId) { m_opponentId = opponentId; }
	bool IsPlayer1Human() const { return m_player1IsHuman; }
	void SetPlayer1IsHuman(bool player1Human) { m_player1IsHuman = player1Human; }
	void CheckGameResult();
};