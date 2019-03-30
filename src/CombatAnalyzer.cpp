#include "CombatAnalyzer.h"
#include "CCBot.h"

uint32_t AREA_UNDER_DETECTION_DURATION = 732;		// around 30s

CombatAnalyzer::CombatAnalyzer(CCBot & bot) 
	: m_bot(bot)
{
}

void CombatAnalyzer::onStart()
{
	m_bot.Commander();
	if (m_bot.GetPlayerRace(Players::Enemy) == sc2::Protoss)
		AREA_UNDER_DETECTION_DURATION *= 2;
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
	if (m_bot.GetGameLoop() % 10)
	{
		return;
	}

	std::vector<CCTilePosition> buildingPositions;
	aliveEnemiesCountByType.clear();
	for (auto enemy : m_bot.GetEnemyUnits())
	{
		auto unit = enemy.second;
		if (m_bot.Data(unit).isBuilding)
		{
			if (std::find(buildingPositions.begin(), buildingPositions.end(), unit.getTilePosition()) != buildingPositions.end())
			{
				continue;
			}
			buildingPositions.push_back(unit.getTilePosition());
		}
		aliveEnemiesCountByType[unit.getUnitPtr()->unit_type]++;
	}
	
	//TODO handle dead ally units

	//Upgrades
	auto combatAirUnitCount = m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_BANSHEE, true) +
		m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT, true) + 
		m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, true);
	if (combatAirUnitCount >= 5)
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
			totalDamage.insert_or_assign(unittype, 0);
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
			totalhealthLoss.insert_or_assign(unittype, 0);
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
		std::pair<sc2::UNIT_TYPEID, std::string>(sc2::UNIT_TYPEID::TERRAN_CYCLONE, "Cyclone")
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
		m_bot.StartProfiling("0.10.4.4.2.1        checkForRangeUpgrade");
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
			//TODO detect invis
		}
		m_bot.StopProfiling("0.10.4.4.2.1        checkForRangeUpgrade");
		m_bot.StartProfiling("0.10.4.4.2.2        saveDetectedArea");
		if (unit.getUnitPtr()->cloak == sc2::Unit::Cloaked && !Util::IsPositionUnderDetection(unit.getPosition(), m_bot))
		{
			m_areasUnderDetection.push_back({ unit.getPosition(), m_bot.GetGameLoop() });
		}
		m_bot.StopProfiling("0.10.4.4.2.2        saveDetectedArea");

		//Is building underconstruction. Cancel building
		if (unit.isBeingConstructed())
		{
			//If the building could die, cancel it.
			if (state.GetRecentDamageTaken() >= 2 * unit.getHitPoints())
			{
				unit.useAbility(sc2::ABILITY_ID::CANCEL);
			}
		}
		/*auto& threats = Util::getThreats(unit.getUnitPtr(), m_bot.GetKnownEnemyUnits(), m_bot);
		if (threats.empty() && state.HadRecentTreats())
		{
		//Invisible unit detected
		m_bot.Strategy().setEnemyHasInvisible(true);
		m_invisibleSighting[unit] = std::pair<CCPosition, uint32_t>(unit.getPosition(), m_bot.GetGameLoop());
		}*/
	}
	/*else if (m_bot.GetGameLoop() % 5)
	{
	m_bot.StartProfiling("0.10.4.4.2.4        updateThreats");
	auto& threats = Util::getThreats(unit.getUnitPtr(), m_bot.GetKnownEnemyUnits(), m_bot);
	state.UpdateThreat(!threats.empty());
	m_bot.StopProfiling("0.10.4.4.2.4        updateThreats");
	}*/
}

void CombatAnalyzer::increaseDeadEnemy(sc2::UNIT_TYPEID type)
{
	deadEnemiesCountByType[type]++;
}