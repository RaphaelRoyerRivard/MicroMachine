#include "CombatAnalyzer.h"
#include "CCBot.h"

const uint32_t AREA_UNDER_DETECTION_DURATION = 112;		// 5s

CombatAnalyzer::CombatAnalyzer(CCBot & bot) 
	: m_bot(bot)
{
}

void CombatAnalyzer::onStart()
{
	m_bot.Commander();
}

void CombatAnalyzer::onFrame()
{
	clearAreasUnderDetection();
	//Handle our units
	m_bot.StartProfiling("0.10.4.4    checkUnitsState");
	checkUnitsState();
	m_bot.StopProfiling("0.10.4.4    checkUnitsState");
	m_bot.StartProfiling("0.10.4.5    UpdateTotalHealthLoss");
	UpdateTotalHealthLoss();
	m_bot.StartProfiling("0.10.4.5    UpdateTotalHealthLoss");
	m_bot.StartProfiling("0.10.4.6    UpdateRatio");
	UpdateRatio();
	m_bot.StartProfiling("0.10.4.6    UpdateRatio");

	drawDamageHealthRatio();

	lowPriorityChecks();
//drawAreasUnderDetection();
}

void CombatAnalyzer::lowPriorityChecks()
{
	if (m_bot.GetGameLoop() - m_lastLowPriorityFrame < 10)
	{
		return;
	}
	m_lastLowPriorityFrame = m_bot.GetGameLoop();

	std::vector<CCTilePosition> buildingPositions;
	aliveEnemiesCountByType.clear();

	lightCountByType.clear();
	armoredCountByType.clear();
	bioCountByType.clear();
	mechCountByType.clear();
	psiCountByType.clear();
	massiveCountByType.clear();

	totalKnownWorkerCount = 0;
	totalAirUnitsCount = 0;
	totalGroundUnitsCount = 0;
	totalGroundLightCount = 0;
	totalAirLightCount = 0;
	totalGroundArmoredCount = 0;
	totalAirArmoredCount = 0;
	totalGroundBioCount = 0;
	totalAirBioCount = 0;
	totalGroundMechCount = 0;
	totalAirMechCount = 0;
	totalGroundPsiCount = 0;
	totalAirPsiCount = 0;
	totalGroundMassiveCount = 0;
	totalAirMassiveCount = 0;

	for (auto enemy : m_bot.GetEnemyUnits())
	{
		auto unit = enemy.second;
		sc2::UNIT_TYPEID type = (sc2::UNIT_TYPEID)unit.getAPIUnitType();
		
		if (m_bot.Data(unit).isBuilding)
		{
			if (std::find(buildingPositions.begin(), buildingPositions.end(), unit.getTilePosition()) == buildingPositions.end())
			{
				buildingPositions.push_back(unit.getTilePosition());
			}
		}
		aliveEnemiesCountByType[unit.getUnitPtr()->unit_type]++;

		if (unit.getType().isBuilding())
		{
			continue;
		}

		//Count units by types
		if (unit.getType().isWorker())
		{
			totalKnownWorkerCount++;
			continue;
		}

		//Ignored units
		switch (type)
		{
			case sc2::UNIT_TYPEID::TERRAN_AUTOTURRET:
			case sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR:
			case sc2::UNIT_TYPEID::ZERG_LARVA:
			case sc2::UNIT_TYPEID::ZERG_BROODLING:
			case sc2::UNIT_TYPEID::ZERG_CHANGELING:
			case sc2::UNIT_TYPEID::ZERG_INFESTEDTERRANSEGG:
			case sc2::UNIT_TYPEID::ZERG_INFESTORTERRAN:
			case sc2::UNIT_TYPEID::ZERG_EGG:
			case sc2::UNIT_TYPEID::ZERG_BANELINGCOCOON:
			case sc2::UNIT_TYPEID::ZERG_BROODLORDCOCOON:
			case sc2::UNIT_TYPEID::ZERG_RAVAGERCOCOON:
			case sc2::UNIT_TYPEID::ZERG_TRANSPORTOVERLORDCOCOON:
			continue;
		}

		if (unit.isFlying())
			totalAirUnitsCount++;
		else
			totalGroundUnitsCount++;


		if (unit.isLight())
		{
			lightCountByType[type]++;
			if (unit.isFlying())
				totalAirLightCount++;
			else
				totalGroundLightCount++;
		}

		//Check armored units
		if (unit.isArmored())
		{
			armoredCountByType[type]++;
			if (unit.isFlying())
				totalAirArmoredCount++;
			else
				totalGroundArmoredCount++;
		}

		//Check biological units
		if (unit.isBiological())
		{

			bioCountByType[type]++;
			if (unit.isFlying())
				totalAirBioCount++;
			else
				totalGroundBioCount++;
		}

		//Check mech units
		if (unit.isMechanical())
		{
			mechCountByType[type]++;
			if (unit.isFlying())
				totalAirMechCount++;
			else
				totalGroundMechCount++;
		}

		//Check psi units
		if (unit.isPsionic())
		{
			psiCountByType[type]++;
			if (unit.isFlying())
				totalAirPsiCount++;
			else
				totalGroundPsiCount++;
		}

		//Check massive units
		if (unit.isMassive())
		{
			massiveCountByType[type]++;
			if (unit.isFlying())
				totalAirMassiveCount++;
			else
				totalGroundMassiveCount++;
		}
	}
	
	//TODO handle dead ally units

	//Upgrades
	const auto combatVehicleCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Hellion.getUnitType(), true, true) +
		m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Cyclone.getUnitType(), true, true);
	const auto combatShipCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Banshee.getUnitType(), true, true) +
		m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Viking.getUnitType(), true, true) +
		m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Battlecruiser.getUnitType(), true, true);
	if (combatVehicleCount + combatShipCount >= 4)
	{
		auto & production = m_bot.Commander().Production();
		if (!production.isTechQueuedOrStarted(MetaTypeEnum::TerranShipWeaponsLevel1))
		{
			production.queueTech(MetaTypeEnum::TerranShipWeaponsLevel1);
		}
		else if (production.isTechFinished(MetaTypeEnum::TerranShipWeaponsLevel1) && !production.isTechQueuedOrStarted(MetaTypeEnum::TerranVehicleAndShipArmorsLevel1))
		{
			production.queueTech(MetaTypeEnum::TerranVehicleAndShipArmorsLevel1);
		}
		else if (production.isTechFinished(MetaTypeEnum::TerranVehicleAndShipArmorsLevel1) && !production.isTechQueuedOrStarted(MetaTypeEnum::TerranShipWeaponsLevel2))
		{
			production.queueTech(MetaTypeEnum::TerranShipWeaponsLevel2);
		}
		else if (production.isTechFinished(MetaTypeEnum::TerranShipWeaponsLevel2) && !production.isTechQueuedOrStarted(MetaTypeEnum::TerranVehicleAndShipArmorsLevel2))
		{
			production.queueTech(MetaTypeEnum::TerranVehicleAndShipArmorsLevel2);
		}
		else if (production.isTechFinished(MetaTypeEnum::TerranVehicleAndShipArmorsLevel2) && !production.isTechQueuedOrStarted(MetaTypeEnum::TerranShipWeaponsLevel3))
		{
			production.queueTech(MetaTypeEnum::TerranShipWeaponsLevel3);
		}
		else if (production.isTechFinished(MetaTypeEnum::TerranShipWeaponsLevel3) && !production.isTechQueuedOrStarted(MetaTypeEnum::TerranVehicleAndShipArmorsLevel3))
		{
			production.queueTech(MetaTypeEnum::TerranVehicleAndShipArmorsLevel3);
		}
	}
}

void CombatAnalyzer::clearAreasUnderDetection()
{
	m_areasUnderDetection.remove_if([this](auto & area) { return m_bot.GetGameLoop() > area.second + AREA_UNDER_DETECTION_DURATION; });
}

void CombatAnalyzer::drawAreasUnderDetection()
{
	const int areaUnderDetectionSize = m_bot.GetPlayerRace(Players::Enemy) == sc2::Protoss ? 20 : 10;
	for (const auto & area : m_areasUnderDetection)
	{
		m_bot.Map().drawCircle(area.first, areaUnderDetectionSize, sc2::Colors::Blue);
	}
}

void CombatAnalyzer::UpdateTotalHealthLoss()
{
	for (auto unitstate : m_unitStates)
	{
		increaseTotalHealthLoss(unitstate.second.GetDamageTaken(), unitstate.second.GetType());
	}
}

void CombatAnalyzer::increaseTotalDamage(float damageDealt, sc2::UNIT_TYPEID unittype)
{
	if (damageDealt <= 0)
	{
		return;
	}
	for(int i = 0; i < 2; ++i)
	{
		if (totalDamage.find(unittype) == totalDamage.end())
		{
			totalDamage[unittype] = 0;
		}
		totalDamage.at(unittype) += damageDealt;
		// for total
		unittype = sc2::UNIT_TYPEID(0);
	}
}

void CombatAnalyzer::increaseTotalHealthLoss(float healthLoss, sc2::UNIT_TYPEID unittype)
{
	if (healthLoss <= 0)
	{
		return;
	}
	for (int i = 0; i < 2; ++i)
	{
		if (totalhealthLoss.find(unittype) == totalhealthLoss.end())
		{
			totalhealthLoss[unittype] = 0;
		}
		totalhealthLoss.at(unittype) += healthLoss;
		// for total
		unittype = sc2::UNIT_TYPEID(0);
	}
}

float CombatAnalyzer::GetRatio(sc2::UNIT_TYPEID type)
{
	if (ratio.find(type) != ratio.end())
	{
		return ratio[type];
	}
	return std::numeric_limits<float>::max();
}

void CombatAnalyzer::UpdateRatio()
{
	float overrallDamage = 0;
	float overrallhealthLoss = 0;

	std::vector<std::pair<sc2::UNIT_TYPEID, std::string>> checkedTypes = {
		std::pair<sc2::UNIT_TYPEID, std::string>(sc2::UNIT_TYPEID::TERRAN_REAPER, "Reaper"),
		std::pair<sc2::UNIT_TYPEID, std::string>(sc2::UNIT_TYPEID::TERRAN_BANSHEE, "Banshee"),
		std::pair<sc2::UNIT_TYPEID, std::string>(sc2::UNIT_TYPEID::TERRAN_CYCLONE, "Cyclone"),
		std::pair<sc2::UNIT_TYPEID, std::string>(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, "VikingFighter"),
		std::pair<sc2::UNIT_TYPEID, std::string>(sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT, "VikingAssault"),
		std::pair<sc2::UNIT_TYPEID, std::string>(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, "Battlecruiser"),
		std::pair<sc2::UNIT_TYPEID, std::string>(sc2::UNIT_TYPEID::TERRAN_HELLION, "Hellion")
	};

	for (auto type : checkedTypes)
	{
		if (totalDamage.find(type.first) != totalDamage.end())
		{
			if (totalhealthLoss.find(type.first) != totalhealthLoss.end())
			{
				ratio[type.first] = totalhealthLoss.at(type.first) > 0 ? totalDamage.at(type.first) / totalhealthLoss.at(type.first) : 0;
				overrallhealthLoss += totalhealthLoss.at(type.first);
			}
			else
			{
				ratio[type.first] = totalDamage.at(type.first);
			}
			overrallDamage += totalDamage.at(type.first);
		}
	}
	overallRatio = overrallhealthLoss > 0 ? overrallDamage / overrallhealthLoss : 0;
}

void CombatAnalyzer::drawDamageHealthRatio()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
	if (!m_bot.Config().DrawDamageHealthRatio)
	{
		return;
	}

	std::stringstream ss;
	for (auto unitRatio : ratio)
	{
		ss << UnitTypeToName(unitRatio.first) << ": " << unitRatio.second << "\n";
	}
	ss << "Overrall: " << overallRatio << "\n";

	m_bot.Map().drawTextScreen(0.68f, 0.68f, std::string("Damage HealhLoss Ratio : \n") + ss.str().c_str());
}

void CombatAnalyzer::checkUnitsState()
{
	m_bot.StartProfiling("0.10.4.4.1      resetStates");
	for (auto & state : m_unitStates)
	{
		state.second.Reset();
	}
	m_bot.StopProfiling("0.10.4.4.1      resetStates");

	m_bot.StartProfiling("0.10.4.4.2      updateStates");
	for (auto & unit : m_bot.Commander().getValidUnits())
	{
		checkUnitState(unit);
	}
	for (auto & building : m_bot.Buildings().getBuildings())
	{
		checkUnitState(building.buildingUnit);
	}
	m_bot.StopProfiling("0.10.4.4.2      updateStates");

	m_bot.StartProfiling("0.10.4.4.3      removeStates");
	std::vector<CCUnitID> toRemove;
	for (auto & state : m_unitStates)
	{
		if (!state.second.WasUpdated())
		{
			//Unit died
			toRemove.push_back(state.first);

			deadCountByType[state.second.GetType()]++;
		}
	}

	//Remove dead units from the map
	for (auto & tag : toRemove)
	{
		m_unitStates.erase(tag);
	}
	m_bot.StopProfiling("0.10.4.4.3      removeStates");
}

void CombatAnalyzer::checkUnitState(Unit unit)
{
	if (!unit.isValid())
	{
		return;
	}

	m_bot.StartProfiling("0.10.4.4.2.1        addState");
	auto tag = unit.getTag();

	auto it = m_unitStates.find(tag);
	if (it == m_unitStates.end())
	{
		UnitState state = UnitState(unit.getHitPoints(), unit.getShields(), unit.getEnergy(), unit.getUnitPtr());
		state.Update();
		m_unitStates[tag] = state;
		m_bot.StopProfiling("0.10.4.4.2.1        addState");
		return;
	}
	m_bot.StopProfiling("0.10.4.4.2.1        addState");

	m_bot.StartProfiling("0.10.4.4.2.2        updateState");
	UnitState & state = it->second;
	state.Update(unit.getHitPoints(), unit.getShields(), unit.getEnergy());
	m_bot.StopProfiling("0.10.4.4.2.2        updateState");
	if (state.WasAttacked())
	{
		m_bot.StartProfiling("0.10.4.4.2.1        saveDetectedArea");
		if (unit.getUnitPtr()->cloak == sc2::Unit::CloakedAllied && !Util::IsPositionUnderDetection(unit.getPosition(), m_bot))
		{
			m_areasUnderDetection.push_back({ unit.getPosition(), m_bot.GetGameLoop() });
		}
		m_bot.StopProfiling("0.10.4.4.2.1        saveDetectedArea");

		//Is building underconstruction. Cancel building
		if (unit.isBeingConstructed())
		{
			//If the building could die, cancel it.
			if (state.GetRecentDamageTaken() >= 2 * unit.getHitPoints())
			{
				unit.useAbility(sc2::ABILITY_ID::CANCEL);
			}
		}
		
		//TODO Temporarily commented out since the logic isn't finishes
		/*m_bot.StartProfiling("0.10.4.4.2.3        detectUpgrades");
		detectUpgrades(unit, state);
		m_bot.StopProfiling("0.10.4.4.2.3        detectUpgrades");

		m_bot.StartProfiling("0.10.4.4.2.3        detectTechs");
		detectTechs(unit, state);
		m_bot.StopProfiling("0.10.4.4.2.3        detectTechs");*/
	}
}

void CombatAnalyzer::increaseDeadEnemy(sc2::UNIT_TYPEID type)
{
	deadEnemiesCountByType[type]++;
}

bool CombatAnalyzer::shouldProduceGroundAntiGround()
{
	return false;
}

bool CombatAnalyzer::shouldProduceGroundAntiAir()
{
	return false;
}

bool CombatAnalyzer::shouldProduceAirAntiGround()
{
	return false;
}

bool CombatAnalyzer::shouldProduceAirAntiAir()
{
	return false;
}

int CombatAnalyzer::getUnitUpgradeArmor(const sc2::Unit* unit)
{
	bool isEnemy = unit->owner != Players::Self;

	switch ((sc2::UNIT_TYPEID)unit->unit_type)
	{
		case sc2::UNIT_TYPEID::TERRAN_MARINE:
		case sc2::UNIT_TYPEID::TERRAN_MARAUDER:
		case sc2::UNIT_TYPEID::TERRAN_SCV:
		case sc2::UNIT_TYPEID::TERRAN_MULE:
		case sc2::UNIT_TYPEID::TERRAN_REAPER:
		case sc2::UNIT_TYPEID::TERRAN_GHOST:
			return isEnemy ? opponentTerranBioArmor : selfTerranBioArmor;
		case sc2::UNIT_TYPEID::TERRAN_HELLION:
		case sc2::UNIT_TYPEID::TERRAN_SIEGETANK:
		case sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED:
		case sc2::UNIT_TYPEID::TERRAN_HELLIONTANK:
		case sc2::UNIT_TYPEID::TERRAN_THOR:
		case sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT:
		case sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER:
		case sc2::UNIT_TYPEID::TERRAN_RAVEN:
		case sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER:
		case sc2::UNIT_TYPEID::TERRAN_BANSHEE:
		case sc2::UNIT_TYPEID::TERRAN_WIDOWMINE:
		case sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED:
		case sc2::UNIT_TYPEID::TERRAN_CYCLONE:
		case sc2::UNIT_TYPEID::TERRAN_LIBERATOR:
		case sc2::UNIT_TYPEID::TERRAN_LIBERATORAG:
			return isEnemy ? opponentTerranMechArmor : selfTerranMechArmor;
		case sc2::UNIT_TYPEID::PROTOSS_ADEPT:
		case sc2::UNIT_TYPEID::PROTOSS_ARCHON:
		case sc2::UNIT_TYPEID::PROTOSS_COLOSSUS:
		case sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR:
		case sc2::UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_IMMORTAL:
		case sc2::UNIT_TYPEID::PROTOSS_PROBE:
		case sc2::UNIT_TYPEID::PROTOSS_SENTRY:
		case sc2::UNIT_TYPEID::PROTOSS_STALKER:
		case sc2::UNIT_TYPEID::PROTOSS_ZEALOT:
			return isEnemy ? opponentProtossGroundArmor : selfProtossGroundArmor;
		case sc2::UNIT_TYPEID::PROTOSS_CARRIER:
		case sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR:
		case sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIP:
		case sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIPCORE:
		case sc2::UNIT_TYPEID::PROTOSS_OBSERVER:
		case sc2::UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE:
		case sc2::UNIT_TYPEID::PROTOSS_ORACLE:
		case sc2::UNIT_TYPEID::PROTOSS_PHOENIX:
		case sc2::UNIT_TYPEID::PROTOSS_TEMPEST:
		case sc2::UNIT_TYPEID::PROTOSS_VOIDRAY:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISM:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISMPHASING:
			return isEnemy ? opponentProtossAirArmor : selfProtossAirArmor;
		case sc2::UNIT_TYPEID::ZERG_BANELING:
		case sc2::UNIT_TYPEID::ZERG_BANELINGBURROWED:
		case sc2::UNIT_TYPEID::ZERG_BROODLING:
		case sc2::UNIT_TYPEID::ZERG_DRONE:
		case sc2::UNIT_TYPEID::ZERG_DRONEBURROWED:
		case sc2::UNIT_TYPEID::ZERG_HYDRALISK:
		case sc2::UNIT_TYPEID::ZERG_HYDRALISKBURROWED:
		case sc2::UNIT_TYPEID::ZERG_INFESTOR:
		case sc2::UNIT_TYPEID::ZERG_INFESTORBURROWED:
		case sc2::UNIT_TYPEID::ZERG_INFESTORTERRAN:
		case sc2::UNIT_TYPEID::ZERG_LOCUSTMP:
		case sc2::UNIT_TYPEID::ZERG_LURKERMP:
		case sc2::UNIT_TYPEID::ZERG_LURKERMPBURROWED:
		case sc2::UNIT_TYPEID::ZERG_QUEEN:
		case sc2::UNIT_TYPEID::ZERG_QUEENBURROWED:
		case sc2::UNIT_TYPEID::ZERG_RAVAGER:
		case sc2::UNIT_TYPEID::ZERG_RAVAGERCOCOON:
		case sc2::UNIT_TYPEID::ZERG_ROACH:
		case sc2::UNIT_TYPEID::ZERG_ROACHBURROWED:
		case sc2::UNIT_TYPEID::ZERG_SWARMHOSTBURROWEDMP:
		case sc2::UNIT_TYPEID::ZERG_SWARMHOSTMP:
		case sc2::UNIT_TYPEID::ZERG_ULTRALISK:
			//TODO Missing burrowed Ultralisk
			//TODO Might want to check for chininous plating (+2 armor on Ultras), would have to change the switch a little.
		case sc2::UNIT_TYPEID::ZERG_ZERGLING:
		case sc2::UNIT_TYPEID::ZERG_ZERGLINGBURROWED:
			return isEnemy ? opponentZergGroundArmor : selfZergGroundArmor;
		case sc2::UNIT_TYPEID::ZERG_BROODLORD:
		case sc2::UNIT_TYPEID::ZERG_CORRUPTOR:
		case sc2::UNIT_TYPEID::ZERG_LOCUSTMPFLYING:
		case sc2::UNIT_TYPEID::ZERG_MUTALISK:
		case sc2::UNIT_TYPEID::ZERG_OVERLORD:
		case sc2::UNIT_TYPEID::ZERG_OVERSEER:
		case sc2::UNIT_TYPEID::ZERG_VIPER:
			return isEnemy ? opponentZergAirArmor : selfZergAirArmor;
		default:
			return 0;
	}
}

int CombatAnalyzer::getUnitUpgradeWeapon(const sc2::Unit* unit)
{
	bool isEnemy = unit->owner != Players::Self;

	switch ((sc2::UNIT_TYPEID)unit->unit_type)
	{
		case sc2::UNIT_TYPEID::TERRAN_MARINE:
		case sc2::UNIT_TYPEID::TERRAN_MARAUDER:
		case sc2::UNIT_TYPEID::TERRAN_SCV:
		case sc2::UNIT_TYPEID::TERRAN_MULE:
		case sc2::UNIT_TYPEID::TERRAN_REAPER:
		case sc2::UNIT_TYPEID::TERRAN_GHOST:
			return isEnemy ? opponentTerranBioWeapon : selfTerranBioWeapon;
		case sc2::UNIT_TYPEID::TERRAN_HELLION:
		case sc2::UNIT_TYPEID::TERRAN_SIEGETANK:
		case sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED:
		case sc2::UNIT_TYPEID::TERRAN_HELLIONTANK:
		case sc2::UNIT_TYPEID::TERRAN_THOR:
		case sc2::UNIT_TYPEID::TERRAN_WIDOWMINE:
		case sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED:
		case sc2::UNIT_TYPEID::TERRAN_CYCLONE:
			return isEnemy ? opponentTerranGroundMechWeapon : selfTerranGroundMechWeapon;
		case sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT:
		case sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER:
		case sc2::UNIT_TYPEID::TERRAN_RAVEN:
		case sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER:
		case sc2::UNIT_TYPEID::TERRAN_BANSHEE:
		case sc2::UNIT_TYPEID::TERRAN_LIBERATOR:
		case sc2::UNIT_TYPEID::TERRAN_LIBERATORAG:
			return isEnemy ? opponentTerranAirMechWeapon : selfTerranAirMechWeapon;
		case sc2::UNIT_TYPEID::PROTOSS_ADEPT:
		case sc2::UNIT_TYPEID::PROTOSS_ARCHON:
		case sc2::UNIT_TYPEID::PROTOSS_COLOSSUS:
		case sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR:
		case sc2::UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_IMMORTAL:
		case sc2::UNIT_TYPEID::PROTOSS_PROBE:
		case sc2::UNIT_TYPEID::PROTOSS_SENTRY:
		case sc2::UNIT_TYPEID::PROTOSS_STALKER:
		case sc2::UNIT_TYPEID::PROTOSS_ZEALOT:
			return isEnemy ? opponentProtossGroundWeapon : selfProtossGroundWeapon;
		case sc2::UNIT_TYPEID::PROTOSS_CARRIER:
		case sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR:
		case sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIP:
		case sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIPCORE:
		case sc2::UNIT_TYPEID::PROTOSS_OBSERVER:
		case sc2::UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE:
		case sc2::UNIT_TYPEID::PROTOSS_ORACLE:
		case sc2::UNIT_TYPEID::PROTOSS_PHOENIX:
		case sc2::UNIT_TYPEID::PROTOSS_TEMPEST:
		case sc2::UNIT_TYPEID::PROTOSS_VOIDRAY:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISM:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISMPHASING:
			return isEnemy ? opponentProtossAirWeapon : selfProtossAirWeapon;
		case sc2::UNIT_TYPEID::ZERG_ZERGLING:
		case sc2::UNIT_TYPEID::ZERG_ZERGLINGBURROWED:
		case sc2::UNIT_TYPEID::ZERG_BANELING:
		case sc2::UNIT_TYPEID::ZERG_BANELINGBURROWED:
		case sc2::UNIT_TYPEID::ZERG_BROODLING:
		case sc2::UNIT_TYPEID::ZERG_DRONE:
		case sc2::UNIT_TYPEID::ZERG_DRONEBURROWED:
		case sc2::UNIT_TYPEID::ZERG_ULTRALISK:
			//TODO Missing burrowed Ultralisk
			return isEnemy ? opponentZergGroundMeleeWeapon : selfZergGroundMeleeWeapon;
		case sc2::UNIT_TYPEID::ZERG_HYDRALISK:
		case sc2::UNIT_TYPEID::ZERG_HYDRALISKBURROWED:
		case sc2::UNIT_TYPEID::ZERG_INFESTOR:
		case sc2::UNIT_TYPEID::ZERG_INFESTORBURROWED:
		case sc2::UNIT_TYPEID::ZERG_INFESTORTERRAN:
		case sc2::UNIT_TYPEID::ZERG_LOCUSTMP:
		case sc2::UNIT_TYPEID::ZERG_LURKERMP:
		case sc2::UNIT_TYPEID::ZERG_LURKERMPBURROWED:
		case sc2::UNIT_TYPEID::ZERG_QUEEN:
		case sc2::UNIT_TYPEID::ZERG_QUEENBURROWED:
		case sc2::UNIT_TYPEID::ZERG_RAVAGER:
		case sc2::UNIT_TYPEID::ZERG_RAVAGERCOCOON:
		case sc2::UNIT_TYPEID::ZERG_ROACH:
		case sc2::UNIT_TYPEID::ZERG_ROACHBURROWED:
		case sc2::UNIT_TYPEID::ZERG_SWARMHOSTBURROWEDMP:
		case sc2::UNIT_TYPEID::ZERG_SWARMHOSTMP:
			return isEnemy ? opponentZergGroundMeleeWeapon : selfZergGroundMeleeWeapon;
		case sc2::UNIT_TYPEID::ZERG_BROODLORD:
		case sc2::UNIT_TYPEID::ZERG_CORRUPTOR:
		case sc2::UNIT_TYPEID::ZERG_LOCUSTMPFLYING:
		case sc2::UNIT_TYPEID::ZERG_MUTALISK:
		case sc2::UNIT_TYPEID::ZERG_OVERLORD:
		case sc2::UNIT_TYPEID::ZERG_OVERSEER:
		case sc2::UNIT_TYPEID::ZERG_VIPER:
			return isEnemy ? opponentZergAirWeapon : selfZergAirWeapon;
		default:
			return 0;
	}
}

void CombatAnalyzer::detectUpgrades(Unit & unit, UnitState & state)
{
	int healthLost = state.GetDamageTaken();
	sc2::UnitTypeData unitTypeData = Util::GetUnitTypeDataFromUnitTypeId(unit.getAPIUnitType(), m_bot);
	UnitType type = UnitType(unit.getAPIUnitType(), m_bot);
	int unitUpgradeArmor = getUnitUpgradeArmor(unit.getUnitPtr());
	
	const sc2::Weapon::TargetType expectedWeaponType = unit.isFlying() ? sc2::Weapon::TargetType::Air : sc2::Weapon::TargetType::Ground;
	auto& threats = Util::getThreats(unit.getUnitPtr(), m_bot.GetKnownEnemyUnits(), m_bot);
	for each (auto threat in threats)
	{
		//TODO validate unit is looking towards the unit

		sc2::UnitTypeData threatTypeData = Util::GetUnitTypeDataFromUnitTypeId(threat->unit_type, m_bot);
		auto range = Util::GetAttackRangeForTarget(threat, unit.getUnitPtr(), m_bot, true);
		auto distSq = Util::DistSq(unit.getPosition(), threat->pos) - type.radius() - threat->radius;
		// If the threat is too far and cant have dealt damage (doesn't consider projectil travel time)
		if (distSq > range * range)
		{
			//This threat cant hit the unit
			continue;
		}

		float weaponDamage = Util::GetDamageForTarget(threat, unit.getUnitPtr(), m_bot);
		
		//Get the weapon, even if we have the range and damage, to see if there is another bonus damage that applies to the unit, if there is, apply the + on it.
		sc2::UnitTypeData unitTypeData = Util::GetUnitTypeDataFromUnitTypeId(threat->unit_type, m_bot);
		sc2::UnitTypeData targetTypeData = Util::GetUnitTypeDataFromUnitTypeId(threat->unit_type, m_bot);
		for (auto & weapon : unitTypeData.weapons)
		{
			if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == expectedWeaponType)
			{
				int weaponLevel = getUnitUpgradeWeapon(threat);
				for (int level = weaponLevel; level <= 3; level++)//We still try to detect level 3 upgrades, even if detecting it has no purpose, this way we can stop looping on all the threats.
				{
					weaponDamage -= (unitTypeData.armor + unitUpgradeArmor);
					//TODO Attempt to figure out the upgrade level of the enemy
					if (weaponDamage == healthLost)
					{
						//TODO Can detect range upgrades here
						return;
					}
				}
				break;//Only one weapon can hit air or ground (or both), there cannot be multiple that hit ground or air on a single unit.
				//TODO validate this statement with the Thor, since it switches between two "stances"
			}
		}
	}


	switch (m_bot.GetPlayerRace(Players::Enemy))
	{
		case CCRace::Protoss:
		{

		}
		case CCRace::Terran:
		{

		}
		case CCRace::Zerg:
		{

		}
	}
}

void CombatAnalyzer::detectTechs(Unit & unit, UnitState & state)
{
	m_bot.StartProfiling("0.10.4.4.2.3.1      checkForRangeUpgrade");
	const CCTilePosition tilePosition = Util::GetTilePosition(unit.getPosition());
	if (Util::PathFinding::HasCombatInfluenceOnTile(tilePosition, unit.isFlying(), m_bot))
	{
		if (unit.isFlying() && !m_bot.Strategy().enemyHasHiSecAutoTracking())
		{
			for (const auto & enemyMissileTurret : m_bot.GetKnownEnemyUnits(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET))
			{
				if (Util::DistSq(unit, enemyMissileTurret) < 10 * 10)
				{
					m_bot.Strategy().setEnemyHasHiSecAutoTracking(true);
					Util::Log(__FUNCTION__, unit.getType().getName() + " got hit near by a missile turret at a distance of " + std::to_string(Util::Dist(unit, enemyMissileTurret)), m_bot);
					m_bot.Actions()->SendChat("Is that a range upgrade on your missile turrets? I ain't gonna fall for it again!");
					break;
				}
			}
		}
	}
	m_bot.StopProfiling("0.10.4.4.2.3.1      checkForRangeUpgrade");
}