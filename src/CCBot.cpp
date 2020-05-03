#include "CCBot.h"
#include "Util.h"

CCBot::CCBot(std::string botVersion, bool realtime)
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
	, m_concede(false)
	, m_saidHallucinationLine(false)
	, m_botVersion(botVersion)
	, m_previousMacroGameLoop(-1)
	, m_player1IsHuman(false)
	, m_realtime(realtime)
{
}

void CCBot::OnGameFullStart() {}
void CCBot::OnGameEnd()
{
	std::stringstream ss;
	ss << "OnGameEnd ";
	if (GetAllyUnits().size() > GetEnemyUnits().size())
		ss << "Win";
	else
		ss << "Lose";
	std::cout << ss.str() << std::endl;
	Util::Log(__FUNCTION__, ss.str(), *this);
}
void CCBot::OnUnitDestroyed(const sc2::Unit*) {}
void CCBot::OnUnitCreated(const sc2::Unit*) {}
void CCBot::OnUnitIdle(const sc2::Unit*) {}
void CCBot::OnUpgradeCompleted(sc2::UpgradeID upgrade)
{
	Util::DebugLog(__FUNCTION__, "Upgrade " + upgrade.to_string() + " completed", *this);
	m_strategy.setUpgradeCompleted(upgrade);
	switch ((sc2::UPGRADE_ID)upgrade)
	{
		//Terran
		case sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1:
			m_combatAnalyzer.selfTerranBioWeapon = 1;
			break;
		case sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL2:
			m_combatAnalyzer.selfTerranBioWeapon = 2;
			break;
		case sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL3:
			m_combatAnalyzer.selfTerranBioWeapon = 3;
			break;
		case sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1:
			m_combatAnalyzer.selfTerranBioArmor = 1;
			break;
		case sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL2:
			m_combatAnalyzer.selfTerranBioArmor = 2;
			break;
		case sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL3:
			m_combatAnalyzer.selfTerranBioArmor = 3;
			break;
		case sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL1:
			m_combatAnalyzer.selfTerranGroundMechWeapon = 1;
			break;
		case sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL2:
			m_combatAnalyzer.selfTerranGroundMechWeapon = 2;
			break;
		case sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL3:
			m_combatAnalyzer.selfTerranGroundMechWeapon = 3;
			break;
		case sc2::UPGRADE_ID::TERRANSHIPWEAPONSLEVEL1:
			m_combatAnalyzer.selfTerranAirMechWeapon = 1;
			break;
		case sc2::UPGRADE_ID::TERRANSHIPWEAPONSLEVEL2:
			m_combatAnalyzer.selfTerranAirMechWeapon = 2;
			break;
		case sc2::UPGRADE_ID::TERRANSHIPWEAPONSLEVEL3:
			m_combatAnalyzer.selfTerranAirMechWeapon = 3;
			break;
		case sc2::UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL1:
			m_combatAnalyzer.selfTerranMechArmor = 1;
			break;
		case sc2::UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL2:
			m_combatAnalyzer.selfTerranMechArmor = 2;
			break;
		case sc2::UPGRADE_ID::TERRANVEHICLEANDSHIPARMORSLEVEL3:
			m_combatAnalyzer.selfTerranMechArmor = 3;
			break;

		//Protoss
		case sc2::UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1:
			m_combatAnalyzer.selfProtossGroundArmor = 1;
			break;
		case sc2::UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2:
			m_combatAnalyzer.selfProtossGroundArmor = 2;
			break;
		case sc2::UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL3:
			m_combatAnalyzer.selfProtossGroundArmor = 3;
			break;
		case sc2::UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1:
			m_combatAnalyzer.selfProtossGroundWeapon = 1;
			break;
		case sc2::UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2:
			m_combatAnalyzer.selfProtossGroundWeapon = 2;
			break;
		case sc2::UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL3:
			m_combatAnalyzer.selfProtossGroundWeapon = 3;
			break;
		case sc2::UPGRADE_ID::PROTOSSAIRARMORSLEVEL1:
			m_combatAnalyzer.selfProtossAirArmor = 1;
			break;
		case sc2::UPGRADE_ID::PROTOSSAIRARMORSLEVEL2:
			m_combatAnalyzer.selfProtossAirArmor = 2;
			break;
		case sc2::UPGRADE_ID::PROTOSSAIRARMORSLEVEL3:
			m_combatAnalyzer.selfProtossAirArmor = 3;
			break;
		case sc2::UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL1:
			m_combatAnalyzer.selfProtossAirWeapon = 1;
			break;
		case sc2::UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL2:
			m_combatAnalyzer.selfProtossAirWeapon = 2;
			break;
		case sc2::UPGRADE_ID::PROTOSSAIRWEAPONSLEVEL3:
			m_combatAnalyzer.selfProtossAirWeapon = 3;
			break;
		case sc2::UPGRADE_ID::PROTOSSSHIELDSLEVEL1:
			m_combatAnalyzer.selfProtossShield = 1;
			break;
		case sc2::UPGRADE_ID::PROTOSSSHIELDSLEVEL2:
			m_combatAnalyzer.selfProtossShield = 2;
			break;
		case sc2::UPGRADE_ID::PROTOSSSHIELDSLEVEL3:
			m_combatAnalyzer.selfProtossShield = 3;
			break;

		//Zerg
		case sc2::UPGRADE_ID::ZERGMELEEWEAPONSLEVEL1:
			m_combatAnalyzer.selfZergGroundMeleeWeapon = 1;
			break;
		case sc2::UPGRADE_ID::ZERGMELEEWEAPONSLEVEL2:
			m_combatAnalyzer.selfZergGroundMeleeWeapon = 2;
			break;
		case sc2::UPGRADE_ID::ZERGMELEEWEAPONSLEVEL3:
			m_combatAnalyzer.selfZergGroundMeleeWeapon = 3;
			break;
		case sc2::UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL1:
			m_combatAnalyzer.selfZergGroundRangedWeapon = 1;
			break;
		case sc2::UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL2:
			m_combatAnalyzer.selfZergGroundRangedWeapon = 2;
			break;
		case sc2::UPGRADE_ID::ZERGMISSILEWEAPONSLEVEL3:
			m_combatAnalyzer.selfZergGroundRangedWeapon = 3;
			break;
		case sc2::UPGRADE_ID::ZERGGROUNDARMORSLEVEL1:
			m_combatAnalyzer.selfZergGroundArmor = 1;
			break;
		case sc2::UPGRADE_ID::ZERGGROUNDARMORSLEVEL2:
			m_combatAnalyzer.selfZergGroundArmor = 2;
			break;
		case sc2::UPGRADE_ID::ZERGGROUNDARMORSLEVEL3:
			m_combatAnalyzer.selfZergGroundArmor = 3;
			break;
		case sc2::UPGRADE_ID::ZERGFLYERWEAPONSLEVEL1:
			m_combatAnalyzer.selfZergAirWeapon = 1;
			break;
		case sc2::UPGRADE_ID::ZERGFLYERWEAPONSLEVEL2:
			m_combatAnalyzer.selfZergAirWeapon = 2;
			break;
		case sc2::UPGRADE_ID::ZERGFLYERWEAPONSLEVEL3:
			m_combatAnalyzer.selfZergAirWeapon = 3;
			break;
		case sc2::UPGRADE_ID::ZERGFLYERARMORSLEVEL1:
			m_combatAnalyzer.selfZergAirArmor = 1;
			break;
		case sc2::UPGRADE_ID::ZERGFLYERARMORSLEVEL2:
			m_combatAnalyzer.selfZergAirArmor = 2;
			break;
		case sc2::UPGRADE_ID::ZERGFLYERARMORSLEVEL3:
			m_combatAnalyzer.selfZergAirArmor = 3;
			break;
	}
}
void CCBot::OnBuildingConstructionComplete(const sc2::Unit*) {}
void CCBot::OnNydusDetected() {}
void CCBot::OnUnitEnterVision(const sc2::Unit*) {}
void CCBot::OnNuclearLaunchDetected() {}

void CCBot::OnGameStart() //full start
{	
    m_config.readConfigFile();
	if (!m_realtime)
		Util::InitializeCombatSimulator();
	Util::Initialize(*this, GetPlayerRace(Players::Self), Observation()->GetGameInfo());

    // add all the possible start locations on the map
    for (auto & loc : Observation()->GetGameInfo().enemy_start_locations)
    {
        m_enemyBaseLocations.push_back(loc);
    }
	m_startLocation = Observation()->GetStartLocation();
	m_buildingArea = Util::GetTilePosition(m_startLocation);

	//Initialize list of MetaType
	MetaTypeEnum::Initialize(*this);
	Util::SetAllowDebug(Config().AllowDebug);

	// Create logfile
	Util::CreateLog(*this);
	m_versionMessage << "MicroMachine v" << m_botVersion;
	Util::Log(__FUNCTION__, m_versionMessage.str(), *this);
	std::cout << m_versionMessage.str() << std::endl;
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

	if (Config().AllowDebug)
	{
		IssueGameStartCheats();
	}

	StartProfiling("0 Starcraft II");
	m_lastFrameEndTime = std::chrono::steady_clock::now();
}

void CCBot::OnStep()
{
	StopProfiling("0 Starcraft II");
	StartProfiling("0.0 OnStep");	//Do not remove
	m_gameLoop = Observation()->GetGameLoop();
	if (m_realtime && !m_combatSimulatorInitialized && m_gameLoop > 50)
	{
		Util::InitializeCombatSimulator();
		m_combatSimulatorInitialized = true;
	}
	if (!m_versionMessage.str().empty() && m_gameLoop >= 5)
	{
		Actions()->SendChat(m_versionMessage.str(), sc2::ChatChannel::Team);
		m_versionMessage.str("");
		m_versionMessage.clear();
	}
	if (m_gameLoop % Util::DELAY_BETWEEN_ERROR == 0)
	{
		Util::ClearDisplayedErrors();
	}
	const bool executeMacro = m_gameLoop - m_previousMacroGameLoop > 1;
	if (executeMacro)
		m_previousMacroGameLoop = m_gameLoop;
	
	StartProfiling("0.1 checkKeyState");
	if (Config().AllowDebug)
	{
		checkKeyState();
		IssueCheats();
	}
	StopProfiling("0.1 checkKeyState");

	StartProfiling("0.2 setUnits");
    setUnits();
	StopProfiling("0.2 setUnits");

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
	m_workers.onFrame(executeMacro);
	StopProfiling("0.7 m_workers.onFrame");

	StartProfiling("0.8 m_buildings.onFrame");
	m_buildings.onFrame(executeMacro);
	StopProfiling("0.8 m_buildings.onFrame");

	StartProfiling("0.9 m_strategy.onFrame");
    m_strategy.onFrame(executeMacro);
	StopProfiling("0.9 m_strategy.onFrame");

	StartProfiling("0.11 m_repairStations.onFrame");
	m_repairStations.onFrame();
	StopProfiling("0.11 m_repairStations.onFrame");

	StartProfiling("0.12 m_combatAnalyzer.onFrame");
	m_combatAnalyzer.onFrame();
	StopProfiling("0.12 m_combatAnalyzer.onFrame");

	StartProfiling("0.10 m_gameCommander.onFrame");
	m_gameCommander.onFrame(executeMacro);
	StopProfiling("0.10 m_gameCommander.onFrame");

	updatePreviousFrameEnemyUnitPos();

	StopProfiling("0.0 OnStep");	//Do not remove

#ifndef PUBLIC_RELEASE
	if (Config().AllowDebug)
	{
		drawProfilingInfo();
		Debug()->SendDebug();
	}
#endif
	StartProfiling("0 Starcraft II");

	if (Config().TimeControl)
	{
		int speed = Util::GetTimeControlSpeed();
		if (speed != Util::GetTimeControlMaxSpeed())
		{
			long elapsedTime;
			do
			{
				elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_lastFrameEndTime).count() / 1000;
			} while (elapsedTime < (1000 / 22.4f) / (speed / 100.f));

			m_lastFrameEndTime = std::chrono::steady_clock::now();
			drawTimeControl();
		}
	}
}

#pragma optimize( "checkKeyState", off )
void CCBot::checkKeyState()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
	if (!m_config.AllowDebug || !m_config.AllowKeyControl)
	{
		return;
	}
#ifdef _WINDOWS
	if (GetAsyncKeyState(VK_INSERT))
	{
		printf("Pausing...");
	}

	if (GetAsyncKeyState(VK_DELETE))
	{
		keyDelete = true;
	}
	else if (keyDelete)
	{
		keyDelete = false;
	}

	if (GetAsyncKeyState(VK_END))
	{
		keyEnd = true;
	}
	else if (keyEnd)
	{
		keyEnd = false;
	}

	if (GetAsyncKeyState(VK_F1))
	{
		keyF1 = true;
	}
	else if (keyF1)
	{
		keyF1 = false;
		Util::TimeControlIncreaseSpeed();
	}

	if (GetAsyncKeyState(VK_F2))
	{
		keyF2 = true;
	}
	else if (keyF2)
	{
		keyF2 = false;
		Util::TimeControlDecreaseSpeed();
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

	if (GetAsyncKeyState('5'))
	{
		key5 = true;
	}
	else if (key5)
	{
		key5 = false;
		m_config.DrawBaseLocationInfo = !m_config.DrawBaseLocationInfo;
	}

	/*if (GetAsyncKeyState('6'))
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
#endif
}
#pragma optimize( "checkKeyState", on )


void CCBot::setUnits()
{
    m_allUnits.clear();
	m_allyUnitsPerType.clear();
	m_unitCount.clear();
	m_unitCompletedCount.clear();
	m_strategy.setEnemyCurrentlyHasInvisible(false);
	bool firstPhoenix = true;
	const bool zergEnemy = GetPlayerRace(Players::Enemy) == CCRace::Zerg;
	StartProfiling("0.2.1 loopAllUnits");
    for (auto & unitptr : Observation()->GetUnits())
    {
		Unit unit(unitptr, *this);
		if (!unit.isValid())
			continue;
		if (!unit.isAlive())
			continue;
		if (unitptr->alliance == sc2::Unit::Self || unitptr->alliance == sc2::Unit::Ally)
		{
			m_allyUnits[unitptr->tag] = unit;
			m_allyUnitsPerType[unitptr->unit_type].push_back(unit);
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
					m_KD8ChargesSpawnFrame[unitptr->tag] = GetGameLoop();
				}
				else
				{
					uint32_t & spawnFrame = it->second;
					if (GetGameLoop() - spawnFrame > 10)	// Will consider our KD8 Charges to be dangerous only after a few frames
						m_enemyUnits[unitptr->tag] = unit;
				}
			}
		}
		else if (unitptr->alliance == sc2::Unit::Enemy)
		{
			m_enemyUnits[unitptr->tag] = unit;
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
						Util::DebugLog(__FUNCTION__, "Metabolic Boost detected", *this);
					}
				}
			}
			if (!m_strategy.shouldProduceAntiAirDefense())
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
					case sc2::UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE:
						break;
					case sc2::UNIT_TYPEID::TERRAN_BANSHEE:
					case sc2::UNIT_TYPEID::PROTOSS_ORACLE:
					case sc2::UNIT_TYPEID::ZERG_MUTALISK:
						m_strategy.setShouldProduceAntiAirDefense(true);
						m_strategy.setShouldProduceAntiAirOffense(true);
						Actions()->SendChat("Planning on harassing with air units? That's MY strategy! >:(");
						Util::DebugLog(__FUNCTION__, "Air Harass detected: " + unit.getType().getName(), *this);
						break;
					case sc2::UNIT_TYPEID::PROTOSS_PHOENIX:
						if (unitptr->last_seen_game_loop != GetCurrentFrame())
							break;
						if (firstPhoenix)
						{
							if (!m_saidHallucinationLine)
							{
								Actions()->SendChat("Am I hallucinating?");
								m_saidHallucinationLine = true;
								Util::DebugLog(__FUNCTION__, "Hallucination (maybe) detected: " + unit.getType().getName(), *this);
							}
							firstPhoenix = false;
							break;
						}
						// no break because more than one Phoenix probably means that there is a real fleet
					default:
						if (unit.getType().isBuilding() && !m_strategy.enemyOnlyHasFlyingBuildings())
						{
							bool enemyHasGroundUnit = false;
							for(auto & knownEnemyTypes : m_enemyUnitsPerType)
							{
								if(!knownEnemyTypes.second.empty())
								{
									if(!knownEnemyTypes.second[0].isFlying())
									{
										enemyHasGroundUnit = true;
										break;
									}
								}
							}
							if(!enemyHasGroundUnit)
							{
								m_strategy.setEnemyOnlyHasFlyingBuildings(true);
								Actions()->SendChat("Lifting your buildings won't save them for long.");
								Util::DebugLog(__FUNCTION__, "Lifted building detected: " + unit.getType().getName(), *this);
							}
						}
						else if(!m_strategy.shouldProduceAntiAirOffense())
						{
							m_strategy.setShouldProduceAntiAirOffense(true);
							Actions()->SendChat("What!? Air units? I'm not ready! :s");
							Util::DebugLog(__FUNCTION__, "Air unit detected: " + unit.getType().getName(), *this);
						}
					}
				}

				// If the opponent has built a building that can produce flying units, we should produce Anti Air units
				if (!m_strategy.shouldProduceAntiAirOffense() && unit.getType().isBuilding())
				{
					switch (sc2::UNIT_TYPEID(unitptr->unit_type))
					{
					case sc2::UNIT_TYPEID::TERRAN_STARPORT:
					case sc2::UNIT_TYPEID::TERRAN_FUSIONCORE:
					case sc2::UNIT_TYPEID::PROTOSS_STARGATE:
					//case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
					case sc2::UNIT_TYPEID::PROTOSS_FLEETBEACON:
					case sc2::UNIT_TYPEID::ZERG_GREATERSPIRE:
					case sc2::UNIT_TYPEID::ZERG_SPIRE:
					case sc2::UNIT_TYPEID::ZERG_HIVE:
						m_strategy.setShouldProduceAntiAirOffense(true);
						if (GetPlayerRace(Players::Enemy) == sc2::Zerg)
							Actions()->SendChat("Going for air units? Your flock will be not match for my fleet!");
						else
							Actions()->SendChat("Going for air units? Your fleet will be not match for mine!");
						Util::DebugLog(__FUNCTION__, "Air production building detected: " + unit.getType().getName(), *this);
					default:
						break;
					}
				}
			}
			if (unitptr->cloak == sc2::Unit::Cloaked || unitptr->is_burrowed)//Handle whether the enemy has cloaked/burrowed units or not
			{
				if (!m_strategy.enemyHasInvisible())
				{
					m_strategy.setEnemyHasInvisible(true);
					Actions()->SendChat("I see you are also attracted to the dark arts of invisibility.");
					Util::DebugLog(__FUNCTION__, "Invisible unit detected: " + unit.getType().getName(), *this);
				}
				///TODO if we see at least 1 burrowed roach, we might want to consider all roaches as potentially invisible
				///TODO Should handle unit that CAN get invisible (banshee) and the Mothership (can turn others invisible).
				m_strategy.setEnemyCurrentlyHasInvisible(true);
			}
			else
			{
				switch (unitptr->unit_type.ToType())
				{
				case sc2::UNIT_TYPEID::TERRAN_BANSHEE:
				case sc2::UNIT_TYPEID::TERRAN_WIDOWMINE:
				case sc2::UNIT_TYPEID::TERRAN_GHOST:
					if (!m_strategy.enemyHasInvisible())
					{
						m_strategy.setEnemyHasInvisible(true);
						Actions()->SendChat("I guess I should prepare against cloaked or burrowed units...");
						Util::DebugLog(__FUNCTION__, "Potential invisible unit detected: " + unit.getType().getName(), *this);
					}
					m_strategy.setEnemyCurrentlyHasInvisible(true);
					break;
				default:
					break;
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
					case sc2::UNIT_TYPEID::TERRAN_GHOSTACADEMY:
						m_strategy.setEnemyHasInvisible(true);
						Actions()->SendChat("Planning on striking me with cloaked units?");
						Util::DebugLog(__FUNCTION__, "Invis production building detected: " + unit.getType().getName(), *this);
					default:
						break;
					}
				}
			}
			if(!m_strategy.enemyHasProtossHighTechAir())
			{
				switch (sc2::UNIT_TYPEID(unitptr->unit_type))
				{
				case sc2::UNIT_TYPEID::PROTOSS_FLEETBEACON:
				case sc2::UNIT_TYPEID::PROTOSS_TEMPEST:
				case sc2::UNIT_TYPEID::PROTOSS_CARRIER:
					m_strategy.setEnemyHasProtossHighTechAir(true);
					Actions()->SendChat("OP strat detected, panic mode activated");
					Util::DebugLog(__FUNCTION__, "High tech air strat detected: " + unit.getType().getName(), *this);
				default:
					break;
				}
			}
			if(!m_strategy.enemyHasNydusWorm())
			{
				switch (sc2::UNIT_TYPEID(unitptr->unit_type))
				{
				case sc2::UNIT_TYPEID::ZERG_NYDUSNETWORK:
					m_strategy.setEnemyHasNydusWorm(true);
					Actions()->SendChat("Worms. Really?");
					Util::DebugLog(__FUNCTION__, "Nydus Network detected", *this);
					break;
				case sc2::UNIT_TYPEID::ZERG_NYDUSCANAL:
					m_strategy.setEnemyHasNydusWorm(true);
					Actions()->SendChat("Oh shit oh shit oh shit");
					Util::DebugLog(__FUNCTION__, "Nydus Worm detected", *this);
				default:
					break;
				}
			}
			m_lastSeenPosUnits[unitptr->tag] = std::pair<CCPosition, uint32_t>(unitptr->pos, GetGameLoop());
		}
		else //if(unitptr->alliance == sc2::Unit::Neutral)
		{
			m_neutralUnits[unitptr->tag] = unit;
		}
        m_allUnits.push_back(unit);
    }
	StopProfiling("0.2.1 loopAllUnits");

	StartProfiling("0.2.2 clearDeadUnits");
	clearDeadUnits();
	StopProfiling("0.2.2 clearDeadUnits");
	StartProfiling("0.2.3 clearDuplicateUnits");
	clearDuplicateUnits();
	StopProfiling("0.2.3 clearDuplicateUnits");

	int armoredEnemies = 0;
	m_knownEnemyUnits.clear();
	m_enemyBuildingsUnderConstruction.clear();
	m_enemyUnitsPerType.clear();
	for(auto& enemyUnitPair : m_enemyUnits)
	{
		bool ignoreEnemyUnit = false;
		const Unit& enemyUnit = enemyUnitPair.second;
		const sc2::Unit* enemyUnitPtr = enemyUnit.getUnitPtr();
		const bool isBurrowedWidowMine = enemyUnitPtr->unit_type == sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED;
		const bool isSiegedSiegeTank = enemyUnitPtr->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED;

		m_enemyUnitsPerType[enemyUnitPtr->unit_type].push_back(enemyUnit);

		if (enemyUnit.getType().isBuilding() && enemyUnit.getBuildPercentage() < 1)
			m_enemyBuildingsUnderConstruction.push_back(enemyUnit);

		if (enemyUnit.isArmored() && !enemyUnit.getType().isOverlord() && !enemyUnit.getType().isBuilding())
			++armoredEnemies;

		// If the unit is not were we last saw it, ignore it
		//TODO when the unit is cloaked or burrowed, check if the tile is inside detection range, in that case, we should ignore the unit because it is not there anymore
		if (GetGameLoop() != enemyUnitPtr->last_seen_game_loop && Map().isVisible(enemyUnit.getPosition()) && !isBurrowedWidowMine)
		{
			ignoreEnemyUnit = true;
		}
		// If mobile unit is not seen for too long (around 7s, or 67s for burrowed widow mines and 45s for sieged tanks), ignore it
		else if (!enemyUnit.getType().isBuilding()
			&& (!isBurrowedWidowMine || enemyUnitPtr->last_seen_game_loop + 1500 < GetGameLoop())
			&& (!isSiegedSiegeTank || enemyUnitPtr->last_seen_game_loop + 1000 < GetGameLoop())
			&& enemyUnitPtr->last_seen_game_loop + 150 < GetGameLoop())
		{
			ignoreEnemyUnit = true;
		}
		if (!ignoreEnemyUnit)
		{
			m_knownEnemyUnits.push_back(enemyUnit);
		}
	}

	StartProfiling("0.2.1   identifyEnemyRepairingSCVs");
	identifyEnemyRepairingSCVs();
	StopProfiling("0.2.1   identifyEnemyRepairingSCVs");

	StartProfiling("0.2.2   identifyEnemySCVBuilders");
	identifyEnemySCVBuilders();
	StopProfiling("0.2.2   identifyEnemySCVBuilders");

	StartProfiling("0.2.3   identifyEnemyWorkersGoingIntoRefinery");
	identifyEnemyWorkersGoingIntoRefinery();
	StopProfiling("0.2.3   identifyEnemyWorkersGoingIntoRefinery");

	m_strategy.setEnemyHasMassZerglings(m_enemyUnitsPerType[sc2::UNIT_TYPEID::ZERG_ZERGLING].size() >= 10);
	m_strategy.setEnemyHasSeveralArmoredUnits(armoredEnemies >= 5);
}

void CCBot::identifyEnemyRepairingSCVs()
{
	m_enemyUnitsBeingRepaired.clear();
	m_enemyRepairingSCVs.clear();

	const auto & enemySCVs = m_enemyUnitsPerType[sc2::UNIT_TYPEID::TERRAN_SCV];
	if (enemySCVs.empty())
		return;

	for(const auto & enemyUnitTypePair : m_enemyUnitsPerType)
	{
		const auto unitType = UnitType(enemyUnitTypePair.first, *this);
		// if this type of unit is not repairable
		if (!unitType.isRepairable())
			continue;
		// we don't care about SCVs repairing passive buildings because we will always target the SCVs instead
		if (!unitType.isCombatUnit())
			continue;

		for(const auto & enemyUnit : enemyUnitTypePair.second)
		{
			// if the unit is not currently visible
			if (enemyUnit.getUnitPtr()->last_seen_game_loop != m_gameLoop)
				continue;
			// if the unit is not currently visible (not a snapshot)
			if (!enemyUnit.isVisible())
				continue;
			// if the unit is not injured
			if (enemyUnit.getHitPointsPercentage() >= 100.f)
				continue;
			// if the unit is a building under construction
			if (unitType.isBuilding() && !enemyUnit.isCompleted())
				continue;

			for(const auto & SCV : enemySCVs)
			{
				// if the SCV is not currently visible
				if (SCV.getUnitPtr()->last_seen_game_loop != m_gameLoop)
					continue;
				// if SCV is too far from unit
				const auto distSq = Util::DistSq(enemyUnit, SCV);
				const auto maxDist = enemyUnit.getUnitPtr()->radius + SCV.getUnitPtr()->radius + 1;
				if (distSq > maxDist * maxDist)
					continue;

				if (Util::isUnitFacingAnother(SCV.getUnitPtr(), enemyUnit.getUnitPtr()))
				{
					m_enemyUnitsBeingRepaired[enemyUnit.getUnitPtr()].insert(SCV.getUnitPtr());
					m_enemyRepairingSCVs.insert(SCV.getUnitPtr());
				}
			}
		}
	}
}

void CCBot::identifyEnemySCVBuilders()
{
	m_enemySCVBuilders.clear();

	const auto & enemySCVs = m_enemyUnitsPerType[sc2::UNIT_TYPEID::TERRAN_SCV];
	if (enemySCVs.empty())
		return;

	for (const auto & enemyBuildingUnderConstruction : m_enemyBuildingsUnderConstruction)
	{
		CCTilePosition bottomLeft, topRight;
		enemyBuildingUnderConstruction.getBuildingLimits(bottomLeft, topRight);
		for (const auto & SCV : enemySCVs)
		{
			// if the SCV is not currently visible
			if (SCV.getUnitPtr()->last_seen_game_loop != m_gameLoop)
				continue;

			const auto scvPosition = SCV.getPosition();
			if (scvPosition.x >= bottomLeft.x - 0.5f && scvPosition.x <= topRight.x + 0.5f && scvPosition.y >= bottomLeft.y - 0.5f && scvPosition.y <= topRight.y + 0.5f)
			{
				m_enemySCVBuilders.insert(SCV.getUnitPtr());
				break;
			}
		}
	}
}

void CCBot::identifyEnemyWorkersGoingIntoRefinery()
{
	m_enemyWorkersGoingInRefinery.clear();

	const auto & enemySCVs = m_enemyUnitsPerType[sc2::UNIT_TYPEID::TERRAN_SCV];
	if (enemySCVs.empty())
		return;

	const auto enemyRace = GetPlayerRace(Players::Enemy);
	const auto enemyRefineryType = UnitType::getEnemyRefineryType(enemyRace);
	for (auto & refinery : m_enemyUnitsPerType[enemyRefineryType])
	{
		for (const auto & SCV : enemySCVs)
		{
			const float refineryDist = Util::DistSq(refinery, SCV);
			if (refineryDist < pow(refinery.getUnitPtr()->radius + SCV.getUnitPtr()->radius + 1, 2))
			{
				if (Util::isUnitFacingAnother(SCV.getUnitPtr(), refinery.getUnitPtr()))
				{
					m_enemyWorkersGoingInRefinery.insert(SCV.getUnitPtr());
					break;
				}
			}
		}
	}

}

void CCBot::clearDeadUnits()
{
	std::vector<sc2::Tag> unitsToRemove;
	// Find dead ally units
	for (auto& pair : m_allyUnits)
	{
		auto tag = pair.first;
		auto& unit = pair.second;
		if (!unit.isAlive() ||
			unit.getPlayer() == Players::Enemy)	// In case of one of our units get neural parasited, its alliance will switch)
		{
			unitsToRemove.push_back(tag);
			if (unit.getUnitPtr()->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
				m_KD8ChargesSpawnFrame.erase(tag);
			if (unit.getPlayer() == Players::Enemy)
				m_parasitedUnits.insert(unit.getUnitPtr()->tag);
			if (m_deadAllyUnitsCount.find(unit.getAPIUnitType()) == m_deadAllyUnitsCount.end())
				m_deadAllyUnitsCount[unit.getAPIUnitType()] = 1;
			else
				m_deadAllyUnitsCount[unit.getAPIUnitType()] += 1;
		}
	}
	// Remove dead ally units
	for (auto tag : unitsToRemove)
	{
		m_allyUnits.erase(tag);
	}

	unitsToRemove.clear();
	// Find dead enemy units
	for (auto& pair : m_enemyUnits)
	{
		auto& unit = pair.second;
		// Remove dead unit or old snapshot
		if (!unit.isAlive() || 
			(unit.getPlayer() == Players::Self && unit.getAPIUnitType() != sc2::UNIT_TYPEID::TERRAN_KD8CHARGE) ||	// In case of one of our units get neural parasited, its alliance will switch
			(unit.getUnitPtr()->display_type == sc2::Unit::Snapshot
			&& m_map.isVisible(unit.getPosition())
			&& unit.getUnitPtr()->last_seen_game_loop < GetCurrentFrame()))
		{
			const auto unitPtr = unit.getUnitPtr();
			unitsToRemove.push_back(unitPtr->tag);
			this->Analyzer().increaseDeadEnemy(unitPtr->unit_type);
			if (unit.getPlayer() == Players::Self)
				m_parasitedUnits.erase(unitPtr->tag);
		}
	}
	// Remove dead enemy units
	for (auto tag : unitsToRemove)
	{
		m_enemyUnits.erase(tag);
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
	}
}

void CCBot::clearDuplicateUnits()
{
	std::vector<sc2::Tag> unitsToRemove;
	std::map<sc2::UNIT_TYPEID, std::map<float, std::set<const sc2::Unit*>>> units;

	// Classify enemy units
	for (auto& pair : m_enemyUnits)
	{
		auto& unit = pair.second;
		if (unit.getType().isBuilding())
			units[unit.getAPIUnitType()][unit.getPosition().x * 10000 + unit.getPosition().y].insert(unit.getUnitPtr());
	}
	// Find duplicate enemy units
	for (auto& buildingType : units)
	{
		auto& buildingPositions = buildingType.second;
		for (auto& buildingPosition : buildingPositions)
		{
			auto& buildings = buildingPosition.second;
			if (buildings.size() > 1)
			{
				// Find the real one
				const sc2::Unit* toKeep = nullptr;
				for (auto& building : buildings)
				{
					if (!toKeep || building->display_type == sc2::Unit::Visible || (toKeep->display_type == sc2::Unit::Snapshot && building->last_seen_game_loop > toKeep->last_seen_game_loop))
					{
						if (toKeep)
							unitsToRemove.push_back(toKeep->tag);
						toKeep = building;
					}
					else
						unitsToRemove.push_back(building->tag);
				}
			}
		}
	}
	// Remove duplicate enemy units
	for (auto tag : unitsToRemove)
	{
		m_enemyUnits.erase(tag);
	}

	unitsToRemove.clear();
	units.clear();
	// Classify neutral units
	for (auto& pair : m_neutralUnits)
	{
		auto& unit = pair.second;
		units[unit.getAPIUnitType()][unit.getPosition().x * 10000 + unit.getPosition().y].insert(unit.getUnitPtr());
	}
	// Find duplicate neutral units
	for (auto& unitType : units)
	{
		auto& unitPositions = unitType.second;
		for (auto& unitPosition : unitPositions)
		{
			auto& neutralUnits = unitPosition.second;
			if (neutralUnits.size() > 1)
			{
				// Find the real one
				const sc2::Unit* toKeep = nullptr;
				for (auto& unit : neutralUnits)
				{
					if (!toKeep || unit->last_seen_game_loop > toKeep->last_seen_game_loop)
					{
						if (toKeep)
							unitsToRemove.push_back(toKeep->tag);
						toKeep = unit;
					}
					else
						unitsToRemove.push_back(unit->tag);
				}
			}
		}
	}
	// Remove duplicate neutral units
	for (auto tag : unitsToRemove)
	{
		m_neutralUnits.erase(tag);
	}
}

void CCBot::updatePreviousFrameEnemyUnitPos()
{
	m_previousFrameEnemyPos.clear();
	for (auto & unit : UnitInfo().getUnits(Players::Enemy))
	{
		m_previousFrameEnemyPos[unit.getUnitPtr()->tag] = unit.getPosition();
	}
}

void CCBot::checkForConcede()
{
	if(!m_concede && GetCurrentFrame() > 2688)	// 2 min
	{
		Unit building;
		for (const auto allyUnit : m_allyUnits)
		{
			if(allyUnit.second.getType().isBuilding())
			{
				if (building.isValid())
					return;
				building = allyUnit.second;
			}
		}
		if (building.getHitPointsPercentage() < 50 && Util::PathFinding::GetTotalInfluenceOnTile(Util::GetTilePosition(building.getPosition()), building.getUnitPtr(), *this) > 0)
		{
			m_concede = true;
			//Actions()->SendChat("Pineapple");
			Util::Log(__FUNCTION__, "Concede", *this);
			m_strategy.onEnd(false);
		}
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

BaseLocationManager & CCBot::Bases()
{
    return m_bases;
}

CombatAnalyzer & CCBot::Analyzer()
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

void CCBot::IssueGameStartCheats()
{
	//IMPORTANT: Players::Enemy doesn't work with the cheats if the second player isn't human. We need to use the player id (player 1 vs player 2)
	//¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

	const int player1 = 1;
	const int player2 = 2;
	const auto mapCenter = Map().center();
	const auto towardsCenter = Util::Normalized(mapCenter - m_startLocation);
	const auto towardsCenterX = Util::Normalized(CCPosition(mapCenter.x - m_startLocation.x, 0));
	const auto towardsCenterY = Util::Normalized(CCPosition(0, mapCenter.y - m_startLocation.y));
	const auto offset = towardsCenter * 15;
	const auto enemyLocation = GetEnemyStartLocations()[0];

	//Strategy().setShouldProduceAntiAirOffense(true);
	//Debug()->DebugGiveAllTech();
	//Strategy().setUpgradeCompleted(sc2::UPGRADE_ID::BATTLECRUISERENABLESPECIALIZATIONS);
	//Strategy().setUpgradeCompleted(sc2::UPGRADE_ID::BANSHEECLOAK);

	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, m_startLocation + offset, player1, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_THORAP, m_startLocation, player2, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, mapCenter, player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED, m_startLocation, 2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, mapCenter, player2, 5);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_VOIDRAY, mapCenter, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PROBE, mapCenter, player2, 3);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, m_startLocation, player2, 3);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED, m_startLocation, player2, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_IMMORTAL, m_startLocation, player2, 6);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_TEMPEST, m_startLocation, player2, 3);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, m_startLocation, player1, 10);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, mapCenter, player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, m_startLocation + towardsCenter * 15, player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE, mapCenter, player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, mapCenter + offset, player2, 15);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, mapCenter - towardsCenter * 1, player1, 4);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_ZEALOT, mapCenter + towardsCenter * 5, player2, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, mapCenter + towardsCenter * 8, player2, 4);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_INFESTOR, m_startLocation, player2, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_CORRUPTOR, mapCenter, player2, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE, mapCenter, player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE, mapCenter, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_BANELING, m_startLocation, player1, 20);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR, m_startLocation + offset, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR, mapCenter, player2, 1);
	
	//for (const auto baseLocation : Bases().getBaseLocations())
	//	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ROACHBURROWED, Util::GetPosition(baseLocation->getDepotPosition()), player1, 1);
	//Debug()->DebugGiveAllTech();

	//Workers
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PROBE, m_startLocation, player1, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_FORGE, m_startLocation, Players::Enemy, 1);
	
	//Test for reproducing Reaper bug against Marine and SCV in DiscoBloodbathLE
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, m_startLocation + towardsCenterX * 20, player1, 2);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, m_startLocation, player2, 2);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE, m_startLocation + towardsCenterX * 25, player2, 1);*/
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, m_startLocation, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BARRACKS, m_startLocation + towardsCenterX * 21 + towardsCenterY * 2, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BARRACKS, m_startLocation + towardsCenterX * 25 + towardsCenterY * 2, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_SCV, m_startLocation + towardsCenterX * 22 + towardsCenterY * 1, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, m_startLocation + towardsCenterX * 25 + towardsCenterY * 2, player1, 1);*/

	// Test for reproducing bug where units would try to hit enemy units on top of cliffs with no vision
	/*CCPosition cliffPos;
	float currentDist = 0;
	const auto topHeight = Map().terrainHeight(enemyLocation);
	while (cliffPos == CCPosition())
	{
		currentDist += 1;
		auto currentPosition = enemyLocation - towardsCenter * currentDist;
		const auto currentHeight = Map().terrainHeight(currentPosition);
		if (currentHeight < topHeight)
		{
			cliffPos = currentPosition;
		}
	}
	const auto enemyStalkerLocation = cliffPos - Util::Normalized(cliffPos - enemyLocation) * 2;
	const auto marauderLocation = cliffPos + Util::Normalized(cliffPos - enemyLocation);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, enemyStalkerLocation, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, marauderLocation, player2, 1);*/

	// Test for reproducing bugs with Vikings against Tempests
	// Offensive
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_ZEALOT, mapCenter + towardsCenter * 5, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_TEMPEST, mapCenter + towardsCenter * 10, player2, 3);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_OBSERVER, mapCenter - towardsCenter * 5, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, mapCenter - towardsCenter * 5, player1, 3);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, mapCenter - towardsCenter * 10, player1, 2);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, mapCenter - towardsCenter * 3, player1, 1);*/
	// Defensive
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_ZEALOT, m_startLocation + towardsCenter * 5, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_TEMPEST, m_startLocation + towardsCenter * 10, player2, 3);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_OBSERVER, m_startLocation - towardsCenter * 5, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, m_startLocation - towardsCenter * 5, player1, 3);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, m_startLocation - towardsCenter * 10, player1, 2);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, m_startLocation - towardsCenter * 3, player1, 1);*/

	// Situation for testing the threat fighting morph of Vikings
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, mapCenter + towardsCenter * 4, player1, 4);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, mapCenter - towardsCenter * 4, player2, 3);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_TEMPEST, mapCenter - towardsCenter * 8, player2, 1);*/

	// Test for reproducing the bug of locked on burrowed widow mines (for use against human)
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_RAVEN, mapCenter, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, mapCenter, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED, mapCenter + towardsCenter * 7, player1, 1);*/

	// Test for reproducing the bug where units do not launch opportunistic attacks (and also where they hesitate to attack defensive buildings)
	/*Commander().Combat().setAllowEarlyBuildingAttack(true);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, mapCenter - towardsCenter * 5, player1, 5);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, mapCenter - towardsCenter * 5, player1, 17);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_FORGE, mapCenter + towardsCenter * 3, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PYLON, mapCenter + towardsCenter * 8 + towardsCenterX * 2, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, mapCenter + towardsCenter * 4, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, mapCenter + towardsCenter * 6, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, mapCenter + towardsCenter * 8, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, mapCenter + towardsCenter * 10, player2, 1);*/
	
	// Test for reproducing cliff bugs with Marauders
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, enemyLocation - towardsCenter * 20, player1, 4);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, enemyLocation, player2, 4);
		
	// Test for reproducing Cannon rush defense bugs
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PYLON, m_startLocation + towardsCenter * 10, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PYLON, m_startLocation + towardsCenter * 10 + towardsCenterY * 5, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PYLON, m_startLocation + towardsCenter * 10 + towardsCenterY * 7, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PROBE, m_startLocation + towardsCenter * 10, player2, 1);
	
	// Test for reproducing disabled units bug
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE, mapCenter, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, mapCenter, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_RAVEN, enemyLocation, player1, 1);*/

	// Test for checking out the Medivac micro
	/*Debug()->DebugGiveAllTech();
	Strategy().setUpgradeCompleted(sc2::UPGRADE_ID::STIMPACK);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, mapCenter - towardsCenter * 3, player1, 3);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, mapCenter - towardsCenter * 3, player1, 2);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC, mapCenter - towardsCenter * 3, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, mapCenter + towardsCenter * 3, player2, 3);*/

	// Test for Reaper trade
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, mapCenter - towardsCenter * 2.5, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, mapCenter + towardsCenter * 2.5, player2, 1);*/

	// Test for reproducing pathing bugs with Cyclones in CatalystLE
	/*const CCPosition leftLocation(53, 21);
	const CCPosition rightLocation(59, 23);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, rightLocation, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, rightLocation, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, leftLocation, player2, 3);*/

	// Test for reproducing bugs with units on top of cliffs on AcropolisLE
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, CCPosition(40, 117), player2, 6);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, CCPosition(40, 117), player1, 6);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, CCPosition(45, 124), player2, 4);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PYLON, CCPosition(46, 128), player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, CCPosition(40, 124), player2, 6);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_VOIDRAY, CCPosition(40, 124), player1, 1);

	// Test for reproducing bug where Reaper gets hit by zerglings on creep
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, enemyLocation, player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ZERGLING, enemyLocation, player2, 5);
	
	// Test for checking if BCs can teleport before dying
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, mapCenter, player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, mapCenter, player2, 7);

	// Test for reproducing bug with Cyclones not fleeing after acquiring a Lock-On target
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, mapCenter, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_RAVEN, mapCenter - towardsCenter * 5, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_MUTALISK, mapCenter, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, mapCenter + towardsCenter * 5, player2, 10);*/

	// Test for reproducing bug where Hellions are roaming the map with a Reaper instead of defending
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, mapCenter, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_HELLION, m_startLocation + towardsCenter * 40, player1, 2);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ZERGLING, m_startLocation + towardsCenter * 5, player2, 10);*/

	// Test for reproducing bug where Banshees move into detection zones when attacking cloaked
	/*Debug()->DebugGiveAllTech();
	Strategy().setUpgradeCompleted(sc2::UPGRADE_ID::BANSHEECLOAK);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE, mapCenter - towardsCenter * 5, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER, mapCenter, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET, mapCenter + towardsCenter * 8, player2, 1);*/

	// Test for checking if bases lift themselves when getting attacked by enemies that attack ground only
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER, mapCenter, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_ZEALOT, mapCenter, player1, 1);*/

	// Test for checking if workers flee DTs
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET, m_startLocation - towardsCenter * 4, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR, m_startLocation, player1, 2);*/
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ZERGLING, m_startLocation, player1, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_BANELING, m_startLocation, player1, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_ZEALOT, m_startLocation, player1, 2);
	
	// Test for checking the Lock-On priority of Cyclones
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, mapCenter - towardsCenter * 7, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, mapCenter - towardsCenter * 7, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, mapCenter + towardsCenter * 1, player1, 4);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK, mapCenter + towardsCenter * 3, player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_ADEPT, mapCenter + towardsCenter * 2, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, mapCenter + towardsCenter * 3, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_IMMORTAL, mapCenter + towardsCenter * 4, player1, 1);*/

	// Test to reproduce attack bug of BCs while moving
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, mapCenter - towardsCenter * 11, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PYLON, mapCenter, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, mapCenter + towardsCenter * 4, player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_QUEEN, mapCenter + towardsCenter * 4, player1, 1);*/

	// Test to reproduce bug where Medivac are moving through enemy influence
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, mapCenter, player1, 3);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC, mapCenter - towardsCenter * 30, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, mapCenter, player2, 2);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_TEMPEST, mapCenter - towardsCenter * 20, player2, 1);*/

	// Test to reproduce bug where an Overlord would trigger the cancel of the offensive
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, mapCenter, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_OVERLORD, mapCenter, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ZERGLING, mapCenter, player2, 1);*/

	// Test to reproduce bug where Vikings are selected to defend against ground units
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, m_startLocation, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_ZEALOT, m_startLocation, player2, 1);*/
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, mapCenter, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, mapCenter, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE, m_startLocation, player2, 1);*/

	// Test for reproducing bug where units would not prioritize Nydus worms
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE, m_startLocation, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, m_startLocation, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_NYDUSCANAL, m_startLocation + towardsCenter * 10, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ZERGLING, m_startLocation + towardsCenter * 5, player2, 10);*/

	// Test to make sure our harass units do not try to trade too much
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE, mapCenter - towardsCenter * 5, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, mapCenter + towardsCenter * 5, player1, 2);*/

	// Test to detect burrowing units (against human player)
	/*Debug()->DebugGiveAllTech();
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ROACH, m_startLocation + towardsCenter * 8, player1, 2);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_HELLION, m_startLocation + towardsCenter * 8, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND, m_startLocation + towardsCenter * 5, player2, 2);*/
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_OVERLORDTRANSPORT, m_startLocation + towardsCenter * 8, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_NYDUSCANAL, m_startLocation + towardsCenter * 8, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_NYDUSNETWORK, mapCenter, player1, 1);*/

	// Test to trigger the Barracks repositioning on WorldofSleepersLE
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ROACH, m_startLocation + towardsCenter * 10 + towardsCenterY * 2, player2, 1);
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ROACH, enemyLocation, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT, m_startLocation - towardsCenterX * 2 + towardsCenterY * 15, player1, 1);*/

	// Test to reproduce bug where burrowed Widow Mines are ignored by our units (against human player)
	/*Debug()->DebugGiveAllTech();
	Strategy().setUpgradeCompleted(sc2::UPGRADE_ID::BATTLECRUISERENABLESPECIALIZATIONS);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED, m_startLocation + towardsCenter * 10, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, m_startLocation, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, m_startLocation, player2, 1);*/

	// Test to check if our units can offensively kite through influence
	//Debug()->DebugGiveAllTech();
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER, mapCenter - towardsCenter * 4, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ROACH, mapCenter + towardsCenter * 3, player1, 5);*/
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_HELLION, m_startLocation + towardsCenter * 3, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ZERGLING, m_startLocation + towardsCenter * 15, player1, 10);*/
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, enemyLocation - towardsCenter * 15, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, enemyLocation - towardsCenter * 15, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ZERGLING, enemyLocation - towardsCenter * 3, player2, 5);*/

	// Test for reproducing bug where BCs are not kiting
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, mapCenter - towardsCenter * 4, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, mapCenter + towardsCenter * 3, player1, 8);*/

	// Test for reproducing the bug where morphing units would be detected as burrowing
	/*Debug()->DebugGiveAllResources();
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_BANELINGNEST, enemyLocation - towardsCenter * 5, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ZERGLING, m_startLocation + towardsCenter * 5, player1, 5);*/

	// Test to reproduce bug where Reaper scout would not avoid influence
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, mapCenter - towardsCenter * 2, player2, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, mapCenter, player1, 5);*/

	// Test to reproduce bug where Reaper cannot move around Bunker (on WorldOfSleepersLE)
	/*Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, mapCenter, player1, 1);
	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER, enemyLocation - towardsCenterY * 10, player2, 1);*/

	// Test to reproduce bug where Cyclone cannot Lock-On to a bunker on high ground (on WorldOfSleepersLE)
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER, enemyLocation - towardsCenterY * 15, player1, 1);
}

void CCBot::IssueCheats()
{
	//Kill all selected units
	if (keyDelete)
	{
		for (auto u : Observation()->GetUnits())
		{
			if (u->is_selected) {
				Debug()->DebugKillUnit(u);
			}
		}
		Util::ClearChat(*this);
	}
	if (keyEnd)
	{
		for (auto u : Observation()->GetUnits())
		{
			if (u->is_selected) {
				Debug()->DebugSetLife(10.0f, u);
			}
		}
		Util::ClearChat(*this);
	}
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

const TypeData & CCBot::Data(const sc2::UNIT_TYPEID & type)
{
	return Data(UnitType(type, *this));
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

int CCBot::GetReservedMinerals()
{
	return m_reservedMinerals;
}

int CCBot::GetReservedGas()
{
	return m_reservedGas;
}

void CCBot::ReserveMinerals(int reservedMinerals)
{
	m_reservedMinerals += reservedMinerals;
}

void CCBot::ReserveGas(int reservedGas)
{
	m_reservedGas += reservedGas;
}

void CCBot::FreeMinerals(int freedMinerals)
{
	m_reservedMinerals -= freedMinerals;
}

void CCBot::FreeGas(int freedGas)
{
	m_reservedGas -= freedGas;
}

int CCBot::GetFreeMinerals()
{
	return GetMinerals() - GetReservedMinerals();
}

int CCBot::GetFreeGas()
{
	return GetGas() - GetReservedGas();
}

Unit CCBot::GetUnit(const CCUnitID & tag) const
{
	return Unit(Observation()->GetUnit(tag), *(CCBot *)this);
}

const sc2::Unit * CCBot::GetUnitPtr(const CCUnitID & tag) const
{
	return Observation()->GetUnit(tag);
}

int CCBot::GetUnitCount(sc2::UNIT_TYPEID type, bool completed, bool underConstruction)
{ 
	auto completedCount = 0;
	if (m_unitCompletedCount.find(type) != m_unitCompletedCount.end())
		completedCount = m_unitCompletedCount.at(type);
	
	if (completed)
		return completedCount;

	int total = completedCount;

	auto unitType = UnitType(type, *this);
	if (unitType.isBuilding())
	{
		total += m_buildings.countBeingBuilt(unitType, underConstruction);
	}
	else //is unit
	{
		for (auto building : m_buildings.getFinishedBuildings())
		{
			if (building.isConstructing(unitType))
			{
				total++;
			}
		}
	}
	
	return total;
}

int CCBot::GetDeadAllyUnitsCount(sc2::UNIT_TYPEID type) const
{
	const auto it = m_deadAllyUnitsCount.find(type);
	if (it != m_deadAllyUnitsCount.end())
		return it->second;
	return 0;
}

std::map<sc2::Tag, Unit> & CCBot::GetAllyUnits()
{
	return m_allyUnits;
}

const std::vector<Unit> & CCBot::GetAllyUnits(sc2::UNIT_TYPEID type)
{
	return m_allyUnitsPerType[type];
}

const std::vector<Unit> CCBot::GetAllyDepotUnits()
{
	switch (this->GetSelfRace())
	{
		case CCRace::Protoss:
		{
			return GetAllyUnits(sc2::UNIT_TYPEID::PROTOSS_NEXUS);
		}
		case CCRace::Zerg:
		{
			auto hatchery = GetAllyUnits(sc2::UNIT_TYPEID::ZERG_HATCHERY);//cannot be by reference, because its modified
			auto& lair = GetAllyUnits(sc2::UNIT_TYPEID::ZERG_LAIR);
			auto& hive = GetAllyUnits(sc2::UNIT_TYPEID::ZERG_HIVE);
			hatchery.insert(hatchery.end(), lair.begin(), lair.end());
			hatchery.insert(hatchery.end(), hive.begin(), hive.end());
			return hatchery;
		}
		case CCRace::Terran:
		{
			auto commandcenter = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER);//cannot be by reference, because its modified
			auto& commandcenterFlying = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING);
			auto& orbital = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND);
			auto& orbitalFlying = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING);
			auto& planetary = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS);
			commandcenter.insert(commandcenter.end(), commandcenterFlying.begin(), commandcenterFlying.end());
			commandcenter.insert(commandcenter.end(), orbital.begin(), orbital.end());
			commandcenter.insert(commandcenter.end(), orbitalFlying.begin(), orbitalFlying.end());
			commandcenter.insert(commandcenter.end(), planetary.begin(), planetary.end());
			return commandcenter;
		}
		default://Random?
		{
			//TODO not sure it can happen
			assert(false);
		}
	}
}

const std::vector<Unit> CCBot::GetAllyGeyserUnits()
{
	//TODO protoss and zerg do not support rich geysers
	switch (this->GetSelfRace())
	{
		case CCRace::Protoss:
		{
			auto assimilator = GetAllyUnits(sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR);//cannot be by reference, because its modified
			auto& richAssimilator = GetAllyUnits(Util::GetRichAssimilatorId());
			assimilator.insert(assimilator.end(), richAssimilator.begin(), richAssimilator.end());
			return assimilator;
		}
		case CCRace::Zerg:
		{
			auto extractor = GetAllyUnits(sc2::UNIT_TYPEID::ZERG_EXTRACTOR);//cannot be by reference, because its modified
			auto& richExtractor = GetAllyUnits(Util::GetRichExtractorId());
			extractor.insert(extractor.end(), richExtractor.begin(), richExtractor.end());
			return extractor;
		}
		case CCRace::Terran:
		{
			auto refinery = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_REFINERY);//cannot be by reference, because its modified
			auto& richRefinery = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_REFINERYRICH);
			refinery.insert(refinery.end(), richRefinery.begin(), richRefinery.end());
			return refinery;
		}
		default://Random?
		{
			//TODO not sure it can happen
			assert(false);
		}
	}
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

const std::vector<Unit> & CCBot::GetEnemyUnits(sc2::UnitTypeID type)
{
	return m_enemyUnitsPerType[type];
}

std::map<sc2::Tag, Unit> & CCBot::GetNeutralUnits()
{
	return m_neutralUnits;
}

bool CCBot::IsParasited(const sc2::Unit * unit) const
{
	return Util::Contains(unit->tag, m_parasitedUnits);
}

const CCPosition CCBot::GetStartLocation() const
{
#ifdef SC2API
    return m_startLocation;
#else
    return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
#endif
}

const CCTilePosition CCBot::GetBuildingArea(MetaType buildingType)
{
	if (m_strategy.isEarlyRushed() || m_strategy.isWorkerRushed())
	{
		//Build on the opposite direction (generally behind the minerals) from the ramp so it is safer.
		auto wallPos = m_buildings.getWallPosition();
		auto safeLocation = CCTilePosition(m_buildingArea);
		safeLocation.x += (safeLocation.x - wallPos.x);
		safeLocation.y += (safeLocation.y - wallPos.y);

		return safeLocation;
	}

	const auto & bases = m_bases.getOccupiedBaseLocations(Players::Self);
	for (auto & base : bases)
	{
		if (base == nullptr || !base->isOccupiedByPlayer(Players::Self) || base->isUnderAttack())
			continue;

		if (buildingType == MetaTypeEnum::Starport && GetAllyUnits(buildingType.getUnitType().getAPIUnitType()).empty())
		{
			const auto towardsBehind = Util::Normalized(base->getPosition() - Util::GetPosition(base->getDepotPosition()));
			const auto behindBase = base->getPosition() + towardsBehind * 5;
			return Util::GetTilePosition(behindBase);
		}
		
		return base->getDepotPosition();
	}

	//If all bases are underattack, return the main base.
	return m_buildingArea;
}

const std::vector<CCPosition> & CCBot::GetEnemyStartLocations() const
{
    return m_enemyBaseLocations;
}

void CCBot::OnError(const std::vector<sc2::ClientError> & client_errors, const std::vector<std::string> & protocol_errors)
{
	for (const auto & clientError : client_errors)
	{
		const auto errorId = int(clientError);
		std::stringstream err;
		err << "Client error: " << std::to_string(errorId) << std::endl;
		Util::Log(__FUNCTION__, err.str(), *this);
	}
	
	for (const auto & protocolError : protocol_errors)
	{
		std::stringstream err;
		err << "Protocol error error: " << protocolError << std::endl;
		Util::Log(__FUNCTION__, err.str(), *this);
	}
}

void CCBot::StartProfiling(const std::string & profilerName)
{
	auto & profiler = m_profilingTimes[profilerName];	// Get the profiling queue tuple
	profiler.start = std::chrono::steady_clock::now();	// Set the start time (third element of the tuple) to now
}

void CCBot::StopProfiling(const std::string & profilerName)
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

void CCBot::drawProfilingInfo()
{
	const std::string stepString = "0.0 OnStep";
	long long stepTime = 0;
	long long currentStepTime = 0;
	const auto it = m_profilingTimes.find(stepString);
	if(it != m_profilingTimes.end())
	{
		const auto & profiler = (*it).second;
		stepTime = profiler.total / profiler.queue.size();
		currentStepTime = profiler.queue[0];
	}

	std::string profilingInfo = "Profiling info (ms)";
	if (m_config.IsRealTime)
	{
		int skipped = m_gameLoop - m_previousGameLoop - 1;
		m_skippedFrames += skipped;
		profilingInfo += "\nTotal skipped " + std::to_string(m_skippedFrames) + " frames.";
		profilingInfo += "\nSkipped " + std::to_string(skipped) + " frames since last loop.";
		m_previousGameLoop = m_gameLoop;
	}
	for (auto & mapPair : m_profilingTimes)
	{
		const std::string& key = mapPair.first;
		auto& profiler = m_profilingTimes.at(mapPair.first);
		auto& queue = profiler.queue;
		const int queueCount = queue.size();
		const long long time = profiler.total / std::max(queueCount, 1);
		if (key == stepString)
		{
			long long maxFrameTime = 0;
			for(auto frameTime : queue)
			{
				if (frameTime > maxFrameTime)
					maxFrameTime = frameTime;
			}
			profilingInfo += "\n Recent Frame Max: " + std::to_string(0.001f * maxFrameTime);
			if(maxFrameTime > 40900)	//limit for a frame in real time
			{
				profilingInfo += "!!!";
			}
			profilingInfo += "\n Recent Frame Avg: " + std::to_string(0.001f * time);
		}
		else if (time * 10 > stepTime)
		{
			profilingInfo += "\n" + mapPair.first + ": " + std::to_string(0.001f * time);
			profilingInfo += " !";
			if (time * 4 > stepTime)
			{
				profilingInfo += "!!";
				/*if(GetCurrentFrame() - m_lastProfilingLagOutput >= 25 && stepTime > 10000)	// >10ms
				{
					m_lastProfilingLagOutput = GetCurrentFrame();
					Util::Log(__FUNCTION__, mapPair.first + " took " + std::to_string(0.001f * time) + "ms", *this);
				}*/
			}
		}

		if (currentStepTime >= 1000000 && queue.size() > 0 && queue[0] > 0)	// 1s
		{
			Util::Log(__FUNCTION__, mapPair.first + " took " + std::to_string(0.001f * queue[0]) + "ms", *this);
		}

		if(queue.size() >= 50)
		{
			queue.push_front(0);
			profiler.total -= queue[50];
			queue.pop_back();
		}
	}
	if (m_config.DrawProfilingInfo)
	{
		m_map.drawTextScreen(0.72f, 0.1f, profilingInfo);
	}
}

void CCBot::drawTimeControl()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
	if (m_config.TimeControl)
	{
		std::stringstream ss;
		ss << "Speed: " << Util::GetTimeControlSpeed() << "%";
		m_map.drawTextScreen(0.5f, 0.75f, ss.str());
	}
}

std::mutex & CCBot::GetCommandMutex()
{
	return m_command_mutex;
}