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
	m_gameLoop = Observation()->GetGameLoop();
		
	checkKeyState();
		
    setUnits();
	clearDeadUnits();
	m_map.onFrame();	
    m_unitInfo.onFrame();	
    m_bases.onFrame();
    m_workers.onFrame();	
	m_buildings.onFrame();	
    m_strategy.onFrame();
	
	m_gameCommander.onFrame();

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
#ifdef SC2API
    Control()->GetObservation();
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
		}
		else if (unitptr->alliance == sc2::Unit::Enemy)
		{
			m_enemyUnits.insert_or_assign(unitptr->tag, unit);
			// If the enemy zergling was seen last frame
			if (zergEnemy && !m_strategy.enemyHasMetabolicBoost() && unitptr->unit_type == sc2::UNIT_TYPEID::ZERG_ZERGLING
				&& m_lastSeenUnits.find(unitptr->tag) != m_lastSeenUnits.end() && m_lastSeenUnits[unitptr->tag] == GetGameLoop() - 1)
			{
				float dist = Util::Dist(unitptr->pos, m_lastSeenPosUnits[unitptr->tag]);
				float speed = Util::getSpeedOfUnit(unitptr, *this);
				float realSpeed = dist * 16.f;	// Magic number calculated from real values
				if(realSpeed > speed + 1.f)
				{
					// This is a Speedling!!!
					m_strategy.setEnemyHasMetabolicBoost(true);
					Actions()->SendChat("Speedlings won't save you my friend");
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
				switch(sc2::UNIT_TYPEID(unitptr->unit_type))
				{
				case sc2::UNIT_TYPEID::TERRAN_STARPORT:
				case sc2::UNIT_TYPEID::PROTOSS_STARGATE:
				case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
				case sc2::UNIT_TYPEID::PROTOSS_FLEETBEACON:
				case sc2::UNIT_TYPEID::ZERG_GREATERSPIRE:
				case sc2::UNIT_TYPEID::ZERG_SPIRE:
				case sc2::UNIT_TYPEID::ZERG_HIVE:
				case sc2::UNIT_TYPEID::PROTOSS_COLOSSUS:
					m_strategy.setShouldProduceAntiAir(true);
					Actions()->SendChat("You are finally ready to produce air units :o took you long enough");
				default:
					break;
				}
			}
			m_lastSeenPosUnits.insert_or_assign(unitptr->tag, unitptr->pos);
		}
		if(unitptr->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
			m_enemyUnits.insert_or_assign(unitptr->tag, unit);
        m_allUnits.push_back(Unit(unitptr, *this));
		m_lastSeenUnits.insert_or_assign(unitptr->tag, GetGameLoop());
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

const GameCommander & CCBot::Commander() const
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
	if ((!completed && m_unitCount.find(type) != m_unitCount.end()) || (completed && m_unitCompletedCount.find(type) != m_unitCompletedCount.end()))
		return completed ? m_unitCompletedCount.at(type) : m_unitCount.at(type);
	else
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

uint32_t CCBot::GetLastStepSeenUnit(sc2::Tag tag)
{
	return m_lastSeenUnits[tag];
}

const std::vector<Unit> & CCBot::GetUnits() const
{
	return m_allUnits;
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

void CCBot::StartProfiling(const std::string & profiler)
{
	if (m_config.DrawProfilingInfo)
	{
		auto & tuple = m_profilingTimes[profiler];				// Get the profiling queue tuple
		std::get<2>(tuple) = std::chrono::steady_clock::now();	// Set the start time (third element of the tuple) to now
	}
}

void CCBot::StopProfiling(const std::string & profiler)
{
	if (m_config.DrawProfilingInfo)
	{
		auto & tuple = m_profilingTimes[profiler];	// Get the profiling queue tuple
		const auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - std::get<2>(tuple)).count();

		auto & totalTime = std::get<1>(tuple);		
		totalTime += elapsedTime;					// Add the time to the total of the last 100 steps
		auto & queue = std::get<0>(tuple);	
		queue.push_back(elapsedTime);				// Add the time to the queue
		if (queue.size() > 100)
		{
			totalTime -= queue[0];					// Remove the old time from the total of the last 100 steps
			queue.pop_front();						// Remove the old time from the queue
		}
	}
}

void CCBot::drawProfilingInfo() const
{
	if (m_config.DrawProfilingInfo)
	{
		std::string profilingInfo = "Profiling info (ms)";
		for (auto & mapPair : m_profilingTimes)
		{
			auto & totalTime = std::get<1>(mapPair.second);
			const size_t queueCount = std::get<0>(mapPair.second).size();
			profilingInfo += "\n" + mapPair.first + ": " + std::to_string(0.001f * totalTime / queueCount);
		}
		m_map.drawTextScreen(0.45f, 0.01f, profilingInfo);
	}
}