#pragma once

#include "Common.h"

#include "MapTools.h"
#include "BaseLocationManager.h"
#include "UnitInfoManager.h"
#include "WorkerManager.h"
#include "BuildingManager.h"
#include "BotConfig.h"
#include "GameCommander.h"
#include "StrategyManager.h"
#include "TechTree.h"
#include "MetaType.h"
#include "Unit.h"

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
    BotConfig               m_config;
    TechTree                m_techTree;
    GameCommander           m_gameCommander;
	CCPosition				m_startLocation;
	CCTilePosition			m_buildingArea;
	std::map<sc2::UNIT_TYPEID, int> m_unitCount;
	std::map<sc2::UNIT_TYPEID, int> m_unitCompletedCount;
	std::map<sc2::Tag, Unit> m_allyUnits;
	std::map<sc2::Tag, Unit> m_enemyUnits;
	std::map<sc2::Tag, std::pair<CCPosition, uint32_t>> m_lastSeenPosUnits;
	std::vector<Unit>       m_allUnits;
	std::vector<Unit>       m_knownEnemyUnits;
    std::vector<CCPosition> m_enemyBaseLocations;
	CCRace selfRace;
	std::map<std::string, Profiler> m_profilingTimes;
	std::mutex m_command_mutex;

	void checkKeyState();
	void setUnits();
	void clearDeadUnits();
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
    const BaseLocationManager & Bases() const;
		  GameCommander & Commander();
    const MapTools & Map() const;
    const UnitInfoManager & UnitInfo() const;
    StrategyManager & Strategy();
    const TypeData & Data(const UnitType & type) const;
    const TypeData & Data(const CCUpgrade & type) const;
    const TypeData & Data(const MetaType & type) const;
    const TypeData & Data(const Unit & unit) const;
	uint32_t GetGameLoop() const;
    const CCRace GetPlayerRace(int player) const;
	const CCRace GetSelfRace() const;
    const CCPosition GetStartLocation() const;
	const CCTilePosition GetBuildingArea() const;

    int GetCurrentFrame() const;
    int GetMinerals() const;
    int GetCurrentSupply() const;
    int GetMaxSupply() const;
    int GetGas() const;
    Unit GetUnit(const CCUnitID & tag) const;
    const std::vector<Unit> & GetUnits() const;
	int GetUnitCount(sc2::UNIT_TYPEID type, bool completed = false) const;
	std::map<sc2::Tag, Unit> & CCBot::GetAllyUnits();
	std::map<sc2::Tag, Unit> CCBot::GetAllyUnits(sc2::UNIT_TYPEID type);
	std::map<sc2::Tag, Unit> & CCBot::GetEnemyUnits();
	const std::vector<Unit> & GetKnownEnemyUnits() const;
    const std::vector<CCPosition> & GetEnemyStartLocations() const;
	void StartProfiling(const std::string & profilerName);
	void StopProfiling(const std::string & profilerName);
	std::mutex & GetCommandMutex();
};