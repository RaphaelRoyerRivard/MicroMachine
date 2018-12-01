#include "CCBot.h"
#include "Util.h"

CCBot::CCBot()
    : m_map(*this)
    , m_bases(*this)
    , m_unitInfo(*this)
    , m_workers(*this)
	, m_buildings(*this)
    , m_gameCommander(*this)
    , m_strategy(*this)
    , m_techTree(*this)
{
}

void CCBot::OnGameFullStart() {}//end?
void CCBot::OnGameEnd() {}//Start
void CCBot::OnUnitDestroyed(const sc2::Unit*) {}
void CCBot::OnUnitCreated(const sc2::Unit*) {}
void CCBot::OnUnitIdle(const sc2::Unit*) {}
void CCBot::OnUpgradeCompleted(sc2::UpgradeID upgrade)
{
	if(upgrade == sc2::UPGRADE_ID::BANSHEECLOAK)
	{
		m_strategy.setBansheeCloakCompleted(true);
	}
}
void CCBot::OnBuildingConstructionComplete(const sc2::Unit*) {}
void CCBot::OnNydusDetected() {}
void CCBot::OnUnitEnterVision(const sc2::Unit*) {}
void CCBot::OnNuclearLaunchDetected() {}

void CCBot::OnGameStart() //full start
{
    m_config.readConfigFile();

    // add all the possible start locations on the map
#ifdef SC2API
    for (auto & loc : Observation()->GetGameInfo().enemy_start_locations)
    {
        m_baseLocations.push_back(loc);
    }
    m_baseLocations.push_back(Observation()->GetStartLocation());
#else
    for (auto & loc : BWAPI::Broodwar->getStartLocations())
    {
        m_baseLocations.push_back(BWAPI::Position(loc));
    }

    // set the BWAPI game flags
    BWAPI::Broodwar->setLocalSpeed(m_config.SetLocalSpeed);
    BWAPI::Broodwar->setFrameSkip(m_config.SetFrameSkip);
    
    if (m_config.CompleteMapInformation)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);
    }

    if (m_config.UserInput)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
    }
#endif

	//Initialize list of MetaType
	MetaTypeEnum::Initialize(*this);
	selfRace = GetPlayerRace(Players::Self);
    
    setUnits();
    m_techTree.onStart();
    m_strategy.onStart();
    m_map.onStart();
    m_unitInfo.onStart();
    m_bases.onStart();
    m_workers.onStart();
	m_buildings.onStart();

    m_gameCommander.onStart();
}

void CCBot::OnStep()
{
	StartProfiling("0 OnStep");	//Do not remove
	m_gameLoop = Observation()->GetGameLoop();

	StartProfiling("0.1 checkKeyState");
	checkKeyState();
	StopProfiling("0.1 checkKeyState");

	StartProfiling("0.2 setUnits");
    setUnits();
	StopProfiling("0.2 setUnits");

	StartProfiling("0.3 clearDeadUnits");
	clearDeadUnits();
	StopProfiling("0.3 clearDeadUnits");

	StartProfiling("0.4 m_map.onFrame");
	m_map.onFrame();
	StopProfiling("0.4 m_map.onFrame");

	StartProfiling("0.5 m_unitInfo.onFrame");
    m_unitInfo.onFrame();
	StopProfiling("0.5 m_unitInfo.onFrame");

	StartProfiling("0.6 m_bases.onFrame");
    m_bases.onFrame();
	StopProfiling("0.6 m_bases.onFrame");

	StartProfiling("0.7 m_workers.onFrame");
    m_workers.onFrame();
	StopProfiling("0.7 m_workers.onFrame");

	StartProfiling("0.8 m_buildings.onFrame");
	m_buildings.onFrame();
	StopProfiling("0.8 m_buildings.onFrame");

	StartProfiling("0.9 m_strategy.onFrame");
    m_strategy.onFrame();
	StopProfiling("0.9 m_strategy.onFrame");

	StartProfiling("0.10 m_gameCommander.onFrame");
	m_gameCommander.onFrame();
	StopProfiling("0.10 m_gameCommander.onFrame");

	StopProfiling("0 OnStep");	//Do not remove

#ifdef SC2API
	if (Config().AllowDebug)
	{
		drawProfilingInfo();
		Debug()->SendDebug();
	}
#endif
}

#pragma optimize( "checkKeyState", off )
void CCBot::checkKeyState()
{
	if (!m_config.AllowDebug || !m_config.AllowKeyControl)
	{
		return;
	}
	if (GetAsyncKeyState(VK_DELETE))
	{
		printf("Pausing...");
	}
	if (GetAsyncKeyState('1'))
	{
		m_config.DrawProductionInfo = !m_config.DrawProductionInfo;
	}
	if (GetAsyncKeyState('2'))
	{
		m_config.DrawHarassInfo = !m_config.DrawHarassInfo;
	}
	if (GetAsyncKeyState('3'))
	{
		m_config.DrawUnitPowerInfo = !m_config.DrawUnitPowerInfo;
	}
	if (GetAsyncKeyState('4'))
	{
		m_config.DrawWorkerInfo = !m_config.DrawWorkerInfo;
	}
	if (GetAsyncKeyState('8'))
	{
		m_config.DrawProfilingInfo = !m_config.DrawProfilingInfo;
	}
	if (GetAsyncKeyState('9'))
	{
		m_config.DrawUnitID = !m_config.DrawUnitID;
	}
	if (GetAsyncKeyState('0'))
	{
		m_config.DrawReservedBuildingTiles = !m_config.DrawReservedBuildingTiles;
	}
}
#pragma optimize( "checkKeyState", on )


void CCBot::setUnits()
{
    m_allUnits.clear();
	m_unitCount.clear();
	m_unitCompletedCount.clear();
#ifdef SC2API
	bool zergEnemy = GetPlayerRace(Players::Enemy) == CCRace::Zerg;
    for (auto unitptr : Observation()->GetUnits())
    {
		Unit unit(unitptr, *this);
		if (!unit.isValid())
			continue;
		if (!unit.isAlive())
			continue;
		if (unitptr->alliance == sc2::Unit::Self || unitptr->alliance == sc2::Unit::Ally)
		{
			m_allyUnits.insert_or_assign(unitptr->tag, unit);
			m_unitCount[unit.getAPIUnitType()]++;
			if (unit.isCompleted())
			{
				m_unitCompletedCount[unit.getAPIUnitType()]++;
			}
			if (unitptr->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
			{
				m_enemyUnits.insert_or_assign(unitptr->tag, unit);
			}
		}
		else if (unitptr->alliance == sc2::Unit::Enemy)
		{
			m_enemyUnits.insert_or_assign(unitptr->tag, unit);
			// If the enemy zergling was seen last frame
			if (zergEnemy && !m_strategy.enemyHasMetabolicBoost() && unitptr->unit_type == sc2::UNIT_TYPEID::ZERG_ZERGLING
				&& unitptr->last_seen_game_loop == GetGameLoop())
			{
				auto& lastSeenPair = m_lastSeenPosUnits[unitptr->tag];
				if (lastSeenPair.first != CCPosition(0, 0) && lastSeenPair.second == GetGameLoop() - 1)
				{
					const float dist = Util::Dist(unitptr->pos, lastSeenPair.first);
					const float speed = Util::getSpeedOfUnit(unitptr, *this);
					const float realSpeed = dist * 16.f;	// Magic number calculated from real values
					if (realSpeed > speed + 1.f)
					{
						// This is a Speedling!!!
						m_strategy.setEnemyHasMetabolicBoost(true);
						Actions()->SendChat("Speedlings won't save you my friend");
					}
				}
			}
			if (!m_strategy.shouldProduceAntiAir())
			{
				// If unit is flying and not part of the following list, we should produce Anti Air units
				if (unitptr->is_flying)
				{
					switch (sc2::UNIT_TYPEID(unitptr->unit_type))
					{
					case sc2::UNIT_TYPEID::ZERG_OVERLORD:
					case sc2::UNIT_TYPEID::ZERG_OVERLORDCOCOON:
					case sc2::UNIT_TYPEID::ZERG_OVERSEER:
					case sc2::UNIT_TYPEID::PROTOSS_OBSERVER:
						break;
					default:
						m_strategy.setShouldProduceAntiAir(true);
						Actions()->SendChat("Producing air units eh? Have you met my Vikings?");
					}
				}

				// If the opponent has built a building that can produce flying units, we should produce Anti Air units
				if (unit.getType().isBuilding())
				{
					switch (sc2::UNIT_TYPEID(unitptr->unit_type))
					{
					case sc2::UNIT_TYPEID::TERRAN_STARPORT:
					case sc2::UNIT_TYPEID::PROTOSS_STARGATE:
					case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
					case sc2::UNIT_TYPEID::PROTOSS_FLEETBEACON:
					case sc2::UNIT_TYPEID::ZERG_GREATERSPIRE:
					case sc2::UNIT_TYPEID::ZERG_SPIRE:
					case sc2::UNIT_TYPEID::ZERG_HIVE:
						m_strategy.setShouldProduceAntiAir(true);
						Actions()->SendChat("You are finally ready to produce air units :o took you long enough");
					default:
						break;
					}
				}
			}
			m_lastSeenPosUnits.insert_or_assign(unitptr->tag, std::pair<CCPosition, uint32_t>(unitptr->pos, GetGameLoop()));
		}
        m_allUnits.push_back(unit);
    }

	m_knownEnemyUnits.clear();
	for(auto& enemyUnitPair : m_enemyUnits)
	{
		bool ignoreEnemyUnit = false;
		const Unit& enemyUnit = enemyUnitPair.second;
		const sc2::Unit* enemyUnitPtr = enemyUnit.getUnitPtr();
		// If the unit is not were we last saw it, ignore it
		if (GetGameLoop() != enemyUnitPtr->last_seen_game_loop && Map().isVisible(enemyUnit.getPosition()))
			ignoreEnemyUnit = true;
		// If mobile unit is not seen for too long (around 4s), ignore it
		else if (!enemyUnit.getType().isBuilding() && enemyUnitPtr->last_seen_game_loop + 100 < GetGameLoop())
			ignoreEnemyUnit = true;
		if(!ignoreEnemyUnit)
			m_knownEnemyUnits.push_back(enemyUnit);
	}
#else
    for (auto & unit : BWAPI::Broodwar->getAllUnits())
    {
        m_allUnits.push_back(Unit(unit, *this));
    }
#endif
}

void CCBot::clearDeadUnits()
{
	std::vector<sc2::Tag> unitsToRemove;
	// Find dead ally units
	for (auto& pair : m_allyUnits)
	{
		auto& unit = pair.second;
		if (!unit.isAlive())
			unitsToRemove.push_back(unit.getUnitPtr()->tag);
	}
	// Remove dead ally units
	for (auto tag : unitsToRemove)
	{
		m_allyUnits.erase(tag);
		std::cout << "Dead ally unit removed from map" << std::endl;
	}
	unitsToRemove.clear();
	// Find dead ally units
	for (auto& pair : m_enemyUnits)
	{
		auto& unit = pair.second;
		if (!unit.isAlive())
			unitsToRemove.push_back(unit.getUnitPtr()->tag);
	}
	// Remove dead ally units
	for (auto tag : unitsToRemove)
	{
		m_enemyUnits.erase(tag);
		std::cout << "Dead enemy unit removed from map" << std::endl;
	}
}

uint32_t CCBot::GetGameLoop() const
{
	return m_gameLoop;
}

CCRace CCBot::GetPlayerRace(int player) const
{
#ifdef SC2API
	if (player == Players::Self)
	{
		for (auto & playerInfo : Observation()->GetGameInfo().player_info)
		{
			if (playerInfo.player_id != player)
			{
				return playerInfo.race_actual;
			}
		}
		BOT_ASSERT(false, "Didn't find player to get their race");
	}

    auto ourID = Observation()->GetPlayerID();
    for (auto & playerInfo : Observation()->GetGameInfo().player_info)
    {
        if (playerInfo.player_id != ourID)
        {
            return playerInfo.race_requested;
        }
    }

    BOT_ASSERT(false, "Didn't find player to get their race");
    return sc2::Race::Random;
#else
    if (player == Players::Self)
    {
        return BWAPI::Broodwar->self()->getRace();
    }
    else
    {
        return BWAPI::Broodwar->enemy()->getRace();
    }
#endif
}

CCRace CCBot::GetSelfRace() const
{
	return selfRace;
}

BotConfig & CCBot::Config()
{
     return m_config;
}

const MapTools & CCBot::Map() const
{
    return m_map;
}

StrategyManager & CCBot::Strategy()
{
    return m_strategy;
}

const BaseLocationManager & CCBot::Bases() const
{
    return m_bases;
}

GameCommander & CCBot::Commander()
{
	return m_gameCommander;
}

const UnitInfoManager & CCBot::UnitInfo() const
{
    return m_unitInfo;
}

int CCBot::GetCurrentFrame() const
{
#ifdef SC2API
    return (int)GetGameLoop();
#else
    return BWAPI::Broodwar->getFrameCount();
#endif
}

const TypeData & CCBot::Data(const UnitType & type) const
{
    return m_techTree.getData(type);
}

const TypeData & CCBot::Data(const Unit & unit) const
{
    return m_techTree.getData(unit.getType());
}

const TypeData & CCBot::Data(const CCUpgrade & type) const
{
    return m_techTree.getData(type);
}

const TypeData & CCBot::Data(const MetaType & type) const
{
    return m_techTree.getData(type);
}

WorkerManager & CCBot::Workers()
{
    return m_workers;
}

BuildingManager & CCBot::Buildings()
{
	return m_buildings;
}

int CCBot::GetCurrentSupply() const
{
#ifdef SC2API
    return Observation()->GetFoodUsed();
#else
    return BWAPI::Broodwar->self()->supplyUsed();
#endif
}

int CCBot::GetMaxSupply() const
{
#ifdef SC2API
    return Observation()->GetFoodCap();
#else
    return BWAPI::Broodwar->self()->supplyTotal();
#endif
}

int CCBot::GetMinerals() const
{
#ifdef SC2API
    return Observation()->GetMinerals();
#else
    return BWAPI::Broodwar->self()->minerals();
#endif
}

int CCBot::GetGas() const
{
#ifdef SC2API
    return Observation()->GetVespene();
#else
    return BWAPI::Broodwar->self()->gas();
#endif
}

Unit CCBot::GetUnit(const CCUnitID & tag) const
{
#ifdef SC2API
    return Unit(Observation()->GetUnit(tag), *(CCBot *)this);
#else
    return Unit(BWAPI::Broodwar->getUnit(tag), *(CCBot *)this);
#endif
}


int CCBot::GetUnitCount(sc2::UNIT_TYPEID type, bool completed) const
{ 
	if (completed && m_unitCompletedCount.find(type) != m_unitCompletedCount.end())
	{
		return m_unitCompletedCount.at(type);
	}
	else if (!completed)
	{
		int boughtButNotBeingBuilt = m_buildings.countBoughtButNotBeingBuilt(type);
		if(m_unitCount.find(type) != m_unitCount.end())
			return m_unitCount.at(type) + boughtButNotBeingBuilt;
		return boughtButNotBeingBuilt;
	}
	return 0;
}

std::map<sc2::Tag, Unit> & CCBot::GetAllyUnits()
{
	return m_allyUnits;
}

std::map<sc2::Tag, Unit> CCBot::GetAllyUnits(sc2::UNIT_TYPEID type)
{
	std::map<sc2::Tag, Unit> units;
	for (auto unit : m_allyUnits)
	{
		if (unit.second.getAPIUnitType() == type)
		{
			units.insert(unit);
		}
	}
	return units;
}

std::map<sc2::Tag, Unit> & CCBot::GetEnemyUnits()
{
	return m_enemyUnits;
}

const std::vector<Unit> & CCBot::GetUnits() const
{
	return m_allUnits;
}

/*
 * This methods returns the enemy units minus the ones that we might think they have moved (useful for combat micro).
 */
const std::vector<Unit> & CCBot::GetKnownEnemyUnits() const
{
	return m_knownEnemyUnits;
}

CCPosition CCBot::GetStartLocation() const
{
#ifdef SC2API
    return Observation()->GetStartLocation();
#else
    return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
#endif
}

const std::vector<CCPosition> & CCBot::GetStartLocations() const
{
    return m_baseLocations;
}

#ifdef SC2API
void CCBot::OnError(const std::vector<sc2::ClientError> & client_errors, const std::vector<std::string> & protocol_errors)
{
    
}
#endif

void CCBot::StartProfiling(const std::string & profilerName)
{
	if (m_config.DrawProfilingInfo)
	{
		auto & profiler = m_profilingTimes[profilerName];	// Get the profiling queue tuple
		profiler.start = std::chrono::steady_clock::now();	// Set the start time (third element of the tuple) to now
	}
}

void CCBot::StopProfiling(const std::string & profilerName)
{
	if (m_config.DrawProfilingInfo)
	{
		auto & profiler = m_profilingTimes[profilerName];	// Get the profiling queue tuple

		const auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - profiler.start).count();

		profiler.total += elapsedTime;						// Add the time to the total of the last 100 steps
		auto & queue = profiler.queue;
		if (queue.empty())
		{
			while (queue.size() < 49)
				queue.push_front(0);						// Fill up the queue with zeros
			queue.push_front(elapsedTime);					// Add the time to the queue
		}
		else
			queue[0] += elapsedTime;						// Add the time to the queue
	}
}

void CCBot::drawProfilingInfo()
{
	if (m_config.DrawProfilingInfo)
	{
		const std::string stepString = "0 OnStep";
		long long stepTime = 0;
		const auto it = m_profilingTimes.find(stepString);
		if(it != m_profilingTimes.end())
		{
			stepTime = (*it).second.total / (*it).second.queue.size();
		}

		std::string profilingInfo = "Profiling info (ms)";
		for (auto & mapPair : m_profilingTimes)
		{
			const std::string& key = mapPair.first;
			auto& profiler = m_profilingTimes.at(mapPair.first);
			auto& queue = profiler.queue;
			const size_t queueCount = queue.size();
			const long long time = profiler.total / queueCount;
			profilingInfo += "\n" + mapPair.first + ": " + std::to_string(0.001f * time);
			if (key != stepString && time * 10 > stepTime)
			{
				profilingInfo += " !";
				if (time * 4 > stepTime)
				{
					profilingInfo += "!!";
				}
			}

			if(queue.size() >= 50)
			{
				queue.push_front(0);
				profiler.total -= queue[50];
				queue.pop_back();
			}
		}
		m_map.drawTextScreen(0.45f, 0.01f, profilingInfo);
	}
}