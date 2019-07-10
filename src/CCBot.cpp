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
	, m_concede(false)
	, m_saidHallucinationLine(false)
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
	Util::DebugLog(__FUNCTION__, "Upgrade " + upgrade.to_string() + " completed", *this);
	m_strategy.setUpgradeCompleted(upgrade);
}
void CCBot::OnBuildingConstructionComplete(const sc2::Unit*) {}
void CCBot::OnNydusDetected() {}
void CCBot::OnUnitEnterVision(const sc2::Unit*) {}
void CCBot::OnNuclearLaunchDetected() {}

void CCBot::OnGameStart() //full start
{
    m_config.readConfigFile();
	Util::Initialize(*this, GetPlayerRace(Players::Self), Observation()->GetGameInfo());

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

	if (Config().AllowDebug)
	{
		IssueCheats();
	}

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
#ifndef PUBLIC_RELEASE
	if (Config().AllowDebug)
	{
		drawProfilingInfo();
		Debug()->SendDebug();
	}
#endif
#endif
	StartProfiling("0 Starcraft II");
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
	m_allyUnitsPerType.clear();
	m_unitCount.clear();
	m_unitCompletedCount.clear();
#ifdef SC2API
	bool firstPhoenix = true;
	const bool zergEnemy = GetPlayerRace(Players::Enemy) == CCRace::Zerg;
    for (auto & unitptr : Observation()->GetUnits())
    {
		Unit unit(unitptr, *this);
		if (!unit.isValid())
			continue;
		if (!unit.isAlive())
			continue;
		if (unitptr->alliance == sc2::Unit::Self || unitptr->alliance == sc2::Unit::Ally)
		{
			m_allyUnits.insert_or_assign(unitptr->tag, unit);
			m_allyUnitsPerType[unitptr->unit_type].push_back(unit);
			bool isMorphingResourceDepot = false;
			auto type = unit.getType();
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
					if (GetGameLoop() - spawnFrame > 10)	// Will consider our KD8 Charges to be dangerous only after a few frames
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
							}
							firstPhoenix = false;
							break;
						}
						// no break because more than one Phoenix probably means that there is a real fleet
					case sc2::UNIT_TYPEID::TERRAN_BANSHEE:
					case sc2::UNIT_TYPEID::PROTOSS_ORACLE:
					case sc2::UNIT_TYPEID::ZERG_MUTALISK:
						m_strategy.setShouldProduceAntiAirDefense(true);
						m_strategy.setShouldProduceAntiAirOffense(true);
						Actions()->SendChat("Planning on harassing with air units? That's MY strategy! >:(");
						break;
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
							}
						}
						else if(!m_strategy.shouldProduceAntiAirOffense())
						{
							m_strategy.setShouldProduceAntiAirOffense(true);
							Actions()->SendChat("What!? Air units? I'm not ready! :s");
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
						Actions()->SendChat("Going for air units? Your fleet will be not match for mine!");
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

		if (enemyUnit.isArmored() && !enemyUnit.getType().isBuilding())
			++armoredEnemies;

		// If the unit is not were we last saw it, ignore it
		//TODO when the unit is cloaked or burrowed, check if the tile is inside detection range, in that case, we should ignore the unit because it is not there anymore
		if (GetGameLoop() != enemyUnitPtr->last_seen_game_loop && Map().isVisible(enemyUnit.getPosition()) && !isBurrowedWidowMine)
		{
			ignoreEnemyUnit = true;
		}
		// If mobile unit is not seen for too long (around 4s, or 67s for burrowed widow mines and 22s for sieged tanks), ignore it
		else if (!enemyUnit.getType().isBuilding()
			&& (!isBurrowedWidowMine || enemyUnitPtr->last_seen_game_loop + 1500 < GetGameLoop())
			&& (!isSiegedSiegeTank || enemyUnitPtr->last_seen_game_loop + 500 < GetGameLoop())
			&& enemyUnitPtr->last_seen_game_loop + 100 < GetGameLoop())
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
#else
    for (auto & unit : BWAPI::Broodwar->getAllUnits())
    {
        m_allUnits.push_back(Unit(unit, *this));
    }
#endif
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
		if (!unit.isAlive() || 
			unit.getPlayer() == Players::Self ||	// In case of one of our units get neural parasited, its alliance will switch
			(unit.getUnitPtr()->display_type == sc2::Unit::Snapshot
			&& m_map.isVisible(unit.getPosition())
			&& unit.getUnitPtr()->last_seen_game_loop < GetCurrentFrame()))
		{
			const auto unitPtr = unit.getUnitPtr();
			unitsToRemove.push_back(unitPtr->tag);
			this->CombatAnalyzer().increaseDeadEnemy(unitPtr->unit_type);
			if (unit.getPlayer() == Players::Self)
				m_parasitedUnits.erase(unitPtr->tag);
		}
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
	if(!m_concede && m_allyUnits.size() == 1)
	{
		m_concede = true;
		Actions()->SendChat("Pineapple");
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

void CCBot::IssueCheats()
{
	//IMPORTANT: Players::Enemy doesn't work with the cheats if the second player isn't human. We need to use the player id (player 1 vs player 2)
	//¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

	const int player1 = 1;
	const int player2 = 2;
	const auto mapCenter = Map().center();
	//Debug()->DebugGiveAllTech();

	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, m_startLocation, player1, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, m_startLocation, player2, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED, m_startLocation, 2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_STALKER, m_startLocation, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_VOIDRAY, m_startLocation, 2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PROBE, mapCenter, player2, 3);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_ZEALOT, mapCenter, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_REAPER, mapCenter + CCPosition(3, 3), player1, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_CYCLONE, m_startLocation, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_MARINE, m_startLocation, player2, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_INFESTOR, m_startLocation, player1, 2);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE, m_startLocation, player2, 1);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_ZERGLING, m_startLocation, player1, 10);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_BANELING, m_startLocation, player1, 20);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::TERRAN_HELLION, m_startLocation, 1, 8);

	//Workers
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PROBE, m_startLocation, Players::Enemy, 10);
	//Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_FORGE, m_startLocation, Players::Enemy, 1);
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
#ifdef SC2API
    return Unit(Observation()->GetUnit(tag), *(CCBot *)this);
#else
    return Unit(BWAPI::Broodwar->getUnit(tag), *(CCBot *)this);
#endif
}

int CCBot::GetUnitCount(sc2::UNIT_TYPEID type, bool completed)
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
		total += m_buildings.countBeingBuilt(unitType);
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
			auto& richAssimilator = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_RICHREFINERY);//TODO wrong
			assimilator.insert(assimilator.end(), richAssimilator.begin(), richAssimilator.end());
			return assimilator;
		}
		case CCRace::Zerg:
		{
			auto extractor = GetAllyUnits(sc2::UNIT_TYPEID::ZERG_EXTRACTOR);//cannot be by reference, because its modified
			auto& richExtractor = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_RICHREFINERY);//TODO wrong
			extractor.insert(extractor.end(), richExtractor.begin(), richExtractor.end());
			return extractor;
		}
		case CCRace::Terran:
		{
			auto refinery = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_REFINERY);//cannot be by reference, because its modified
			auto& richRefinery = GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_RICHREFINERY);
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

const std::vector<Unit> & CCBot::GetKnownEnemyUnits(sc2::UnitTypeID type)
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
#ifndef PUBLIC_RELEASE
	if (m_config.DrawProfilingInfo)
	{
		auto & profiler = m_profilingTimes[profilerName];	// Get the profiling queue tuple
		profiler.start = std::chrono::steady_clock::now();	// Set the start time (third element of the tuple) to now
	}
#endif
}

void CCBot::StopProfiling(const std::string & profilerName)
{
#ifndef PUBLIC_RELEASE
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
#endif
}

void CCBot::drawProfilingInfo()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
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
					if(GetCurrentFrame() % 25 == 0 && stepTime > 10000)	// >10ms
					{
						Util::DebugLog(__FUNCTION__, mapPair.first + " took " + std::to_string(0.001f * time) + "ms", *this);
					}
				}
			}

			if(queue.size() >= 50)
			{
				queue.push_front(0);
				profiler.total -= queue[50];
				queue.pop_back();
			}
		}
		m_map.drawTextScreen(0.72f, 0.1f, profilingInfo);
	}
}

std::mutex & CCBot::GetCommandMutex()
{
	return m_command_mutex;
}