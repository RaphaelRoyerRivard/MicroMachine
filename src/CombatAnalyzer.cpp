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
	UpdateTotalHealthLoss();
	UpdateRatio();

	drawDamageHealthRatio();
	//drawAreasUnderDetection();

	//Handle enemy units
	/*auto currentEnemies = m_bot.GetEnemyUnits();
	for (auto enemy : currentEnemies)
	{
		if (enemies.find(enemy.first) == enemies.end())
		{
			enemies[enemy.first] = enemy.second;
		}
	}
	for (auto enemy : enemies)
	{
		if (!enemy.second.isValid() || !enemy.second.isAlive())
		{
			deadEnemies[enemy.first] = enemy.second;
			enemies.erase(enemy.first);
		}
	}
	for (auto enemy : currentEnemies)
	{
		if (deadEnemies.find(enemy.first) != deadEnemies.end())
		{
			auto b = 2;
		}
	}
	auto a = 1;*/
	//previousFrameEnemies = m_bot.GetEnemyUnits();
	//auto b = m_bot.GetKnownEnemyUnits();
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
		m_bot.StartProfiling("0.10.4.4.2.1        addState");
		auto tag = unit.getTag();

		auto it = m_unitStates.find(tag);
		if (it == m_unitStates.end())
		{
			UnitState state = UnitState(unit.getHitPoints(), unit.getShields(), unit.getEnergy(), unit.getUnitPtr());
			state.Update();
			m_unitStates[tag] = state;
			m_bot.StopProfiling("0.10.4.4.2.1        addState");
			continue;
		}
		m_bot.StopProfiling("0.10.4.4.2.1        addState");

		m_bot.StartProfiling("0.10.4.4.2.2        updateState");
		UnitState & state = it->second;
		state.Update(unit.getHitPoints(), unit.getShields(), unit.getEnergy());
		m_bot.StopProfiling("0.10.4.4.2.2        updateState");
		if (state.WasAttacked())
		{
			m_bot.StartProfiling("0.10.4.4.2.1        checkForRangeUpgrade");
			const auto & influenceMap = unit.isFlying() ? m_bot.Commander().Combat().getAirInfluenceMap() : m_bot.Commander().Combat().getGroundInfluenceMap();
			const auto & effectInfluenceMap = unit.isFlying() ? m_bot.Commander().Combat().getAirEffectInfluenceMap() : m_bot.Commander().Combat().getGroundEffectInfluenceMap();
			const CCTilePosition tilePosition = Util::GetTilePosition(unit.getPosition());
			const float influence = influenceMap.at(tilePosition.x).at(tilePosition.y) + effectInfluenceMap.at(tilePosition.x).at(tilePosition.y);
			if(influence == 0.f)
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
			if(unit.getUnitPtr()->cloak == sc2::Unit::Cloaked && !Util::IsPositionUnderDetection(unit.getPosition(), m_bot))
			{
				m_areasUnderDetection.push_back({ unit.getPosition(), m_bot.GetGameLoop() });
			}
			m_bot.StopProfiling("0.10.4.4.2.2        saveDetectedArea");
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
	m_bot.StopProfiling("0.10.4.4.2      updateStates");

	m_bot.StartProfiling("0.10.4.4.3      removeStates");
	std::vector<CCUnitID> toRemove;
	for (auto & state : m_unitStates)
	{
		if (!state.second.WasUpdated())
		{
			//Unit died
			toRemove.push_back(state.first);
		}
	}

	//Remove dead units from the map
	for (auto & tag : toRemove)
	{
		m_unitStates.erase(tag);
	}
	m_bot.StopProfiling("0.10.4.4.3      removeStates");
}