#include "CCBot.h"
#include "Util.h"

CCBot::CCBot(std::string botVersion)
	: m_map(*this)
	, m_bases(*this)
	, m_unitInfo(*this)
	, m_workers(*this)
	, m_buildings(*this)
	, m_strategy(*this)
	, m_repairStations(*this)
	, m_combatAnalyzer(*this)
	, m_gameCommander(*this)
	, m_techTree(*this)
	, m_concedeNextFrame(false)
	, m_concede(false)
{
	if(botVersion != "")
		Actions()->SendChat(botVersion);
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
        m_enemyBaseLocations.push_back(loc);
    }
	m_startLocation = Observation()->GetStartLocation();
	m_buildingArea = Util::GetTilePosition(m_startLocation);
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
	Util::SetAllowDebug(Config().AllowDebug);
	Util::CreateLog(*this);
	selfRace = GetPlayerRace(Players::Self);
    
    setUnits();
    m_techTree.onStart();
    m_strategy.onStart();
    m_map.onStart();
    m_unitInfo.onStart();
    m_bases.onStart();
    m_workers.onStart();
	m_buildings.onStart();
	m_repairStations.onStart();
	m_combatAnalyzer.onStart();
    m_gameCommander.onStart();

	StartProfiling("0 Starcraft II");
}

void CCBot::OnStep()
{
	StopProfiling("0 Starcraft II");
	StartProfiling("0.0 OnStep");	//Do not remove
	m_gameLoop = Observation()->GetGameLoop();
	if (m_gameLoop % Util::DELAY_BETWEEN_ERROR == 0)
	{
		Util::ClearDisplayedErrors();
	}
	
	StartProfiling("0.1 checkKeyState");
	checkKeyState();
	StopProfiling("0.1 checkKeyState");

	StartProfiling("0.2 setUnits");
    setUnits();
	StopProfiling("0.2 setUnits");

	StartProfiling("0.3 clearDeadUnits");
	clearDeadUnits();
	StopProfiling("0.3 clearDeadUnits");

	checkForConcede();

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

	StartProfiling("0.11 m_repairStations.onFrame");
	m_repairStations.onFrame();
	StopProfiling("0.11 m_repairStations.onFrame");

	StartProfiling("0.12 m_combatAnalyzer.onFrame");
	m_combatAnalyzer.onFrame();
	StopProfiling("0.12 m_combatAnalyzer.onFrame");

	StartProfiling("0.10 m_gameCommander.onFrame");
	m_gameCommander.onFrame();
	StopProfiling("0.10 m_gameCommander.onFrame");

	updatePreviousFrameEnemyUnitPos();

	StopProfiling("0.0 OnStep");	//Do not remove

#ifdef SC2API
	if (Config().AllowDebug)
	{
		drawProfilingInfo();
		Debug()->SendDebug();
	}
#endif
	StartProfiling("0 Starcraft II");
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
		key1 = true;
	}
	else if (key1)
	{
		key1 = false;
		m_config.DrawProductionInfo = !m_config.DrawProductionInfo;
	}

	if (GetAsyncKeyState('2'))
	{
		key2 = true;
	}
	else if (key2)
	{
		key2 = false;
		m_config.DrawHarassInfo = !m_config.DrawHarassInfo;
	}

	if (GetAsyncKeyState('3'))
	{
		key3 = true;
	}
	else if (key3)
	{
		key3 = false;
		m_config.DrawUnitPowerInfo = !m_config.DrawUnitPowerInfo;
	}

	if (GetAsyncKeyState('4'))
	{
		key4 = true;
	}
	else if (key4)
	{
		key4 = false;
		m_config.DrawWorkerInfo = !m_config.DrawWorkerInfo;
	}

	/*if (GetAsyncKeyState('5'))
	{
		key5 = true;
	}
	else if (key5)
	{
		key5 = false;
	}

	if (GetAsyncKeyState('6'))
	{
		key6 = true;
	}
	else if (key6)
	{
		key6 = false;
	}

	if (GetAsyncKeyState('7'))
	{
		key7 = true;
	}
	else if (key7)
	{
		key7 = false;
	}*/

	if (GetAsyncKeyState('8'))
	{
		key8 = true;
	}
	else if (key8)
	{
		key8 = false;
		m_config.DrawProfilingInfo = !m_config.DrawProfilingInfo;
	}

	if (GetAsyncKeyState('9'))
	{
		key9 = true;
	}
	else if (key9)
	{
		key9 = false;
		m_config.DrawUnitID = !m_config.DrawUnitID;
	}

	if (GetAsyncKeyState('0'))
	{
		key0 = true;
	}
	else if (key0)
	{
		key0 = false;
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
	const bool zergEnemy = GetPlayerRace(Players::Enemy) == CCRace::Zerg;
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
			bool isMorphingResourceDepot = false;
			if (unit.getType().isResourceDepot())
			{
				for(auto& order : unit.getUnitPtr()->orders)
				{
					sc2::UnitTypeID morphType = 0;
					switch(uint32_t(order.ability_id))
					{
					case uint32_t(sc2::ABILITY_ID::MORPH_ORBITALCOMMAND):
						morphType = sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND;
						break;
					case uint32_t(sc2::ABILITY_ID::MORPH_PLANETARYFORTRESS):
						morphType = sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS;
						break;
					case uint32_t(sc2::ABILITY_ID::MORPH_LAIR):
						morphType = sc2::UNIT_TYPEID::ZERG_LAIR;
						break;
					case uint32_t(sc2::ABILITY_ID::MORPH_HIVE):
						morphType = sc2::UNIT_TYPEID::ZERG_HIVE;
						break;
					default:
						break;
					}
					if(morphType != 0)
					{
						isMorphingResourceDepot = true;
						++m_unitCount[morphType];
					}
				}
			}
			if (!isMorphingResourceDepot)
			{
				++m_unitCount[unit.getAPIUnitType()];
				if (unit.isCompleted())
				{
					m_unitCompletedCount[unit.getAPIUnitType()]++;
				}
			}
			if (unitptr->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
			{
				auto it = m_KD8ChargesSpawnFrame.find(unitptr->tag);
				if (it == m_KD8ChargesSpawnFrame.end())
				{
					m_KD8ChargesSpawnFrame.insert_or_assign(unitptr->tag, GetGameLoop());
				}
				else
				{
					uint32_t & spawnFrame = it->second;
					if (GetGameLoop() - spawnFrame > 12)	// Will consider our KD8 Charges to be dangerous only after 0.5s
						m_enemyUnits.insert_or_assign(unitptr->tag, unit);
				}
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
					const bool creep = Observation()->HasCreep(unitptr->pos) == Observation()->HasCreep(lastSeenPair.first);
					if (creep && realSpeed > speed + 1.f)
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
						if(unit.getType().isBuilding())
							Actions()->SendChat("Lifting your buildings won't save them forever.");
						else
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
			if(!m_strategy.enemyHasInvisible())
			{
				// If the opponent has built a building that can produce invis units, we should produce Anti Invis units
				if (unit.getType().isBuilding())
				{
					switch (sc2::UNIT_TYPEID(unitptr->unit_type))
					{
					case sc2::UNIT_TYPEID::PROTOSS_DARKSHRINE:
						m_strategy.setEnemyHasInvisible(true);
						Actions()->SendChat("Planning on striking me with cloaked units?");
					default:
						break;
					}
				}
			}
			m_lastSeenPosUnits.insert_or_assign(unitptr->tag, std::pair<CCPosition, uint32_t>(unitptr->pos, GetGameLoop()));
		}
		else //if(unitptr->alliance == sc2::Unit::Neutral)
		{
			m_neutralUnits.insert_or_assign(unitptr->tag, unit);
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
		auto tag = pair.first;
		auto& unit = pair.second;
		if (!unit.isAlive())
		{
			unitsToRemove.push_back(tag);
			if (unit.getUnitPtr()->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
				m_KD8ChargesSpawnFrame.erase(tag);
		}
	}
	// Remove dead ally units
	for (auto tag : unitsToRemove)
	{
		m_allyUnits.erase(tag);
		std::cout << "Dead ally unit removed from map" << std::endl;
	}

	unitsToRemove.clear();
	// Find dead enemy units
	for (auto& pair : m_enemyUnits)
	{
		auto& unit = pair.second;
		// Remove dead unit or old snapshot
		if (!unit.isAlive() || (unit.getUnitPtr()->display_type == sc2::Unit::Snapshot
			&& m_map.isVisible(unit.getPosition())
			&& unit.getUnitPtr()->last_seen_game_loop < GetCurrentFrame()))
			unitsToRemove.push_back(unit.getUnitPtr()->tag);
	}
	// Remove dead enemy units
	for (auto tag : unitsToRemove)
	{
		m_enemyUnits.erase(tag);
		std::cout << "Dead enemy unit removed from map" << std::endl;
	}

	unitsToRemove.clear();
	// Find dead neutral units
	for (auto& pair : m_neutralUnits)
	{
		auto& unit = pair.second;
		if (!unit.isAlive() || (unit.getUnitPtr()->display_type == sc2::Unit::Snapshot
			&& m_map.isVisible(unit.getPosition())
			&& unit.getUnitPtr()->last_seen_game_loop < GetCurrentFrame()))
			unitsToRemove.push_back(unit.getUnitPtr()->tag);
	}
	// Remove dead neutral units
	for (auto tag : unitsToRemove)
	{
		m_neutralUnits.erase(tag);
		//std::cout << "Dead neutral unit removed from map" << std::endl;	//happens too often
	}
}

void CCBot::updatePreviousFrameEnemyUnitPos()
{
	m_previousFrameEnemyPos.clear();
	for (auto & unit : UnitInfo().getUnits(Players::Enemy))
	{
		m_previousFrameEnemyPos.insert_or_assign(unit.getUnitPtr()->tag, unit.getPosition());
	}
}

void CCBot::checkForConcede()
{
	m_concede = m_concedeNextFrame;
	if(m_allyUnits.size() == 1)
	{
		Actions()->SendChat("GG");
		m_concedeNextFrame = true;
	}
}

uint32_t CCBot::GetGameLoop() const
{
	return m_gameLoop;
}

const CCRace CCBot::GetPlayerRace(int player) const
{
#ifdef SC2API
	const bool playerSelf = player == Players::Self;
    const auto ourID = Observation()->GetPlayerID();
    for (auto & playerInfo : Observation()->GetGameInfo().player_info)
    {
		const bool isOurID = playerInfo.player_id == ourID;
        if ((playerSelf && isOurID) ||
			(!playerSelf && !isOurID))
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

const CCRace CCBot::GetSelfRace() const
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

CombatAnalyzer & CCBot::CombatAnalyzer()
{
	return m_combatAnalyzer;
}

GameCommander & CCBot::Commander()
{
	return m_gameCommander;
}

const UnitInfoManager & CCBot::UnitInfo() const
{
    return m_unitInfo;
}

uint32_t CCBot::GetCurrentFrame() const
{
#ifdef SC2API
    return GetGameLoop();
#else
    return BWAPI::Broodwar->getFrameCount();
#endif
}

const TypeData & CCBot::Data(const UnitType & type)
{
    return m_techTree.getData(type);
}

const TypeData & CCBot::Data(const Unit & unit)
{
    return m_techTree.getData(unit.getType());
}

const TypeData & CCBot::Data(const CCUpgrade & type) const
{
    return m_techTree.getData(type);
}

const TypeData & CCBot::Data(const MetaType & type)
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

std::map<sc2::Tag, Unit> & CCBot::GetNeutralUnits()
{
	return m_neutralUnits;
}

const CCPosition CCBot::GetStartLocation() const
{
#ifdef SC2API
    return m_startLocation;
#else
    return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
#endif
}

const CCTilePosition CCBot::GetBuildingArea() const
{
	return m_buildingArea;
}

const std::vector<CCPosition> & CCBot::GetEnemyStartLocations() const
{
    return m_enemyBaseLocations;
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
		const std::string stepString = "0.0 OnStep";
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
		m_map.drawTextScreen(0.79f, 0.1f, profilingInfo);
	}
}

std::mutex & CCBot::GetCommandMutex()
{
	return m_command_mutex;
}