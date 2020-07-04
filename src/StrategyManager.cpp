#include "StrategyManager.h"
#include "CCBot.h"
#include "JSONTools.h"
#include "Util.h"
#include "MetaType.h"

Strategy::Strategy()
{

}

Strategy::Strategy(const std::string & name, const CCRace & race, const MM::BuildOrder & buildOrder, const Condition & scoutCondition, const Condition & attackCondition)
    : m_name            (name)
    , m_race            (race)
    , m_buildOrder      (buildOrder)
    , m_wins            (0)
    , m_losses          (0)
    , m_scoutCondition  (scoutCondition)
    , m_attackCondition (attackCondition)
{

}

// constructor
StrategyManager::StrategyManager(CCBot & bot)
    : m_bot(bot)
{

}

void StrategyManager::onStart()
{
	const auto opponentId = m_bot.GetOpponentId();
	Util::DebugLog(__FUNCTION__, "Playing against " + opponentId, m_bot);
    readStrategyFile(m_bot.Config().ConfigFileLocation);
	if (m_bot.Config().SelectStartingBuildBasedOnHistory)
	{
		const auto enemyPlayerRace = m_bot.GetPlayerRace(Players::Enemy);
		std::stringstream ss;
		ss << "data/opponents/" << opponentId << ".json";
		std::string path = ss.str();
		json j;
		std::ifstream file(path);
		if (file.good())
		{
			file >> j;
			file.close();
			auto jStrats = j["strategies"];
			int totalWins = 0;
			int totalLosses = 0;
			float bestScore = 0;
			int bestScoreGames = 0;
			int bestStrat = -1;
			for (auto stratIndex = 0; stratIndex < StartingStrategy::COUNT; ++stratIndex)
			{
				std::string strategyName = STRATEGY_NAMES[stratIndex];
				int wins = 0;
				int losses = 0;
				JSONTools::ReadInt("wins", jStrats[stratIndex], wins);
				JSONTools::ReadInt("losses", jStrats[stratIndex], losses);
				totalWins += wins;
				totalLosses += losses;
				auto games = wins + losses;
				float winPercentage = games > 0 ? wins / float(games) : 1;
				// We make sure the opponent has the appropriate race to pick the race specific strategy 
				const auto it = RACE_SPECIFIC_STRATEGIES.find(StartingStrategy(stratIndex));
				bool raceSpecificStrategy = it != RACE_SPECIFIC_STRATEGIES.end();
				bool validRaceSpecificStrategy = raceSpecificStrategy && enemyPlayerRace == it->second;
				if (bestStrat < 0 || winPercentage > bestScore || (winPercentage == bestScore && games >= bestScoreGames))
				{
					if ((!raceSpecificStrategy || validRaceSpecificStrategy) && Util::Contains(strategyName, STRATEGY_ORDER))
					{
						bool currentStratHasPriority = true;
						if (bestStrat >= 0 && winPercentage == bestScore)
						{
							std::string bestStrategyName = STRATEGY_NAMES[bestStrat];
							auto currentStratOrderIndex = std::distance(STRATEGY_ORDER.begin(), std::find(STRATEGY_ORDER.begin(), STRATEGY_ORDER.end(), strategyName));
							auto bestStratOrderIndex = std::distance(STRATEGY_ORDER.begin(), std::find(STRATEGY_ORDER.begin(), STRATEGY_ORDER.end(), bestStrategyName));
							if (currentStratOrderIndex > bestStratOrderIndex)
								currentStratHasPriority = false;
						}
						if (currentStratHasPriority)
						{
							bestScore = winPercentage;
							bestStrat = stratIndex;
							bestScoreGames = games;
						}
					}
				}
				m_opponentHistory << strategyName << " (" << wins << "-" << losses << ")";
				if (stratIndex < StartingStrategy::COUNT - 1)
					m_opponentHistory << ", ";
			}
			m_startingStrategy = StartingStrategy(bestStrat);
			if (m_bot.Config().PrintGreetingMessage)
			{
				const auto winPercentage = totalWins + totalLosses > 0 ? round(totalWins * 100 / (totalWins + totalLosses)) : 100;
				m_greetingMessage << "Greetings " << opponentId << ", my rudimentary database is telling me that I've won " << winPercentage << "% of our encounters. ";
				if (winPercentage >= 95)
					m_greetingMessage << "Prepare to get crushed.";
				else if (winPercentage >= 50)
					m_greetingMessage << "Do your best, as I won't spare you!";
				else if (winPercentage >= 10)
					m_greetingMessage << "Let's see if I can be lucky this time around!";
				else
					m_greetingMessage << "Ouch...";
			}
			Util::Log(__FUNCTION__, m_opponentHistory.str(), m_bot);
			std::cout << m_opponentHistory.str() << std::endl;
		}
		else
		{
			std::ofstream outFile(path);
			for (int i = 0; i < StartingStrategy::COUNT; ++i)
			{
				j["strategies"][i]["wins"] = 0;
				j["strategies"][i]["losses"] = 0;
			}
			outFile << j.dump();
			outFile.close();
			m_startingStrategy = m_bot.GetPlayerRace(Players::Enemy) == sc2::Protoss ? PROXY_MARAUDERS : EARLY_EXPAND;
			if (m_bot.Config().PrintGreetingMessage)
			{
				m_greetingMessage << "Greetings stranger. I shall call you " << opponentId << " from now on. GLHF!";
			}
		}
		m_strategyMessage << "Chosen strategy: " << STRATEGY_NAMES[m_startingStrategy];
		Util::Log(__FUNCTION__, m_strategyMessage.str(), m_bot);
		std::cout << m_strategyMessage.str() << std::endl;
		std::ofstream outFile(path);
		int wins = 0;
		int losses = 0;
		JSONTools::ReadInt("wins", j["strategies"][int(m_startingStrategy)], wins);
		JSONTools::ReadInt("losses", j["strategies"][int(m_startingStrategy)], losses);
		j["strategies"][int(m_startingStrategy)]["wins"] = wins + 1;
		j["strategies"][int(m_startingStrategy)]["losses"] = losses;
		outFile << j.dump();
		outFile.close();
	}
	else
	{
		m_startingStrategy = STANDARD;
	}
	m_initialStartingStrategy = m_startingStrategy;
}

void StrategyManager::onFrame(bool executeMacro)
{
	if (m_bot.Config().PrintGreetingMessage && m_bot.GetCurrentFrame() >= 5)
	{
		if (!m_greetingMessageSent)
		{
			m_bot.Actions()->SendChat(m_greetingMessage.str());
			std::cout << "Greeting message sent" << std::endl;
			m_greetingMessageSent = true;
		}
		if (m_bot.GetCurrentFrame() >= 30 && !m_opponentHistorySent)
		{
			m_bot.Actions()->SendChat(m_opponentHistory.str(), sc2::ChatChannel::Team);
			std::cout << "Opponent history sent" << std::endl;
			m_opponentHistorySent = true;
		}
		if (m_bot.GetCurrentFrame() >= 55 && !m_strategyMessageSent)
		{
			m_bot.Actions()->SendChat(m_strategyMessage.str(), sc2::ChatChannel::Team);
			std::cout << "Strategy message sent" << std::endl;
			m_strategyMessageSent = true;
		}
	}
	if (executeMacro)
	{
		if (isProxyStartingStrategy())
		{
			const auto completedBarracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), true, true);
			const auto completedSupplyDepotsCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::SupplyDepot.getUnitType(), true, true);

			if (m_bot.GetGameLoop() >= 448 && m_bot.Workers().getWorkerData().getProxyWorkers().empty())	// after 20s
			{
				if (isWorkerRushed())
				{
					m_bot.Workers().getWorkerData().clearProxyWorkers();
					m_startingStrategy = STANDARD;
					return;
				}
				
				const auto hasFactory = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Factory.getUnitType(), false, true) > 0;
				if (completedBarracksCount == 0)
				{
					m_startingStrategy = STANDARD;
					m_bot.Commander().Production().clearQueue();
					m_bot.Commander().Production().queueAsHighestPriority(MetaTypeEnum::Barracks, false);
				}
				else if (completedBarracksCount == 1)
				{
					const auto & buildings = m_bot.Buildings().getBuildings();
					for (const auto & building : buildings)
					{
						if (Util::DistSq(m_bot.Buildings().getProxyLocation(), Util::GetPosition(building.finalPosition)) <= 15 * 15)
						{
							m_bot.Buildings().CancelBuilding(building);
						}
					}
					m_startingStrategy = STANDARD;
					m_bot.Commander().Production().clearQueue();
				}
				else if (!hasFactory && m_startingStrategy == PROXY_CYCLONES)
				{
					m_startingStrategy = STANDARD;
					m_bot.Commander().Production().clearQueue();
				}

				// If our Marauders are getting overwhelmed, check if we should cancel our PROXY_MARAUDERS strategy
				if (m_startingStrategy == PROXY_MARAUDERS && !m_bot.Commander().Combat().winAttackSimulation())
				{
					bool cancelProxy = false;
					// Cancel PROXY_MARAUDERS if the opponent has too much static defenses
					int activeStaticDefenseUnits = 0;
					const auto staticDefenseTypes = { sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY };
					for (const auto staticDefenseType : staticDefenseTypes)
					{
						const auto & staticDefenseUnits = m_bot.GetEnemyUnits(staticDefenseType);
						for (const auto & staticDefenseUnit : staticDefenseUnits)
						{
							if (staticDefenseUnit.isPowered() && staticDefenseUnit.isCompleted())
								++activeStaticDefenseUnits;
						}
					}
					if (activeStaticDefenseUnits >= 2)
					{
						cancelProxy = true;
					}
					else
					{
						// Cancel PROXY_MARAUDERS if the opponent has an immortal or air units
						for (const auto enemyUnit : m_bot.GetKnownEnemyUnits())
						{
							// TODO do not cancel if the enemy is an hallucination
							if (enemyUnit.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_IMMORTAL || (enemyUnit.isFlying() && Util::GetGroundDps(enemyUnit.getUnitPtr(), m_bot) > 0))
							{
								cancelProxy = true;
							}
						}
					}
					if (cancelProxy)
					{
						m_startingStrategy = STANDARD;
						m_bot.Commander().Production().clearQueue();
					}
				}
			}
			else if (m_startingStrategy == PROXY_MARAUDERS && completedBarracksCount == 1)
			{
				// Remove proxy worker that just finished its Barracks
				const auto & proxyWorkers = m_bot.Workers().getWorkerData().getProxyWorkers();
				Unit proxyWorkerToRemove;
				for (auto & proxyWorker : proxyWorkers)
				{
					if (!proxyWorker.isConstructing(MetaTypeEnum::Barracks.getUnitType()))
					{
						proxyWorkerToRemove = proxyWorker;
						break;
					}
				}
				if (proxyWorkerToRemove.isValid())
				{
					m_bot.Workers().getWorkerData().removeProxyWorker(proxyWorkerToRemove);
					proxyWorkerToRemove.move(m_bot.GetStartLocation());
				}
			}
			else if (completedBarracksCount >= 2 && m_startingStrategy != PROXY_CYCLONES)
			{
				// Remove last proxy worker that finished its Barracks
				const auto & proxyWorkers = m_bot.Workers().getWorkerData().getProxyWorkers();
				for (const auto & proxyWorker : proxyWorkers)
				{
					proxyWorker.move(m_bot.GetStartLocation());
				}
				m_bot.Workers().getWorkerData().clearProxyWorkers();
			}
			else if (completedBarracksCount == 0)
			{
				bool cancelProxy = false;
				const auto barracksUnderConstructionCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), false, true, true);
				// We want to cancel our proxy strategy if the opponent has vision of our proxy location
				if (barracksUnderConstructionCount == 0)
				{
					const auto & buildings = m_bot.Buildings().getBuildings();
					for (const auto & building : buildings)
					{
						if (building.type.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_BARRACKS)
						{
							if (Util::DistSq(building.builderUnit, Util::GetPosition(building.finalPosition)) <= 3 * 3)
							{
								for (const auto & enemy : m_bot.GetKnownEnemyUnits())
								{
									const auto dist = Util::DistSq(building.builderUnit, enemy);
									const auto builderTerrainHeight = m_bot.Map().terrainHeight(building.builderUnit.getPosition());
									const auto enemyTerrainHeight = m_bot.Map().terrainHeight(enemy.getPosition());
									if (dist <= 8 * 8 && builderTerrainHeight <= enemyTerrainHeight)
									{
										// We want to cancel both the proxy Marauders strategy and the Barracks in the Building Manager
										cancelProxy = true;
										break;
									}
								}
								if (cancelProxy)
									break;
							}
						}
					}
				}
				else if (barracksUnderConstructionCount == 1)
				{
					// We want to cancel the proxy if the enemy can kill our proxy worker before it finishes its building
					const auto & buildings = m_bot.Buildings().getBuildings();
					for (const auto & building : buildings)
					{
						if (building.type.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_BARRACKS)
						{
							const auto & buildingUnit = building.buildingUnit;
							if (buildingUnit.isValid())
							{
								// Will also return false if the proxy worker died
								if (!shouldProxyBuilderFinishSafely(building, true))
								{
									cancelProxy = true;
									break;
								}
							}
						}
					}
				}
				// If we still don't want to cancel the proxy
				if (!cancelProxy)
				{
					// We want to cancel the proxy if one of our two proxy workers of the proxy Marauders strategy died
					if (m_startingStrategy == PROXY_MARAUDERS && completedSupplyDepotsCount > 0)
					{
						const auto & proxyWorkers = m_bot.Workers().getWorkerData().getProxyWorkers();
						if (proxyWorkers.size() < 2)
						{
							cancelProxy = true;
						}
					}
				}
				if (cancelProxy)
				{
					m_bot.Actions()->SendChat("FINE! No cheesing. Maybe next game :)");
					const auto & buildings = m_bot.Buildings().getBuildings();
					for (const auto & building : buildings)
					{
						if (building.type.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_BARRACKS)
						{
							bool cancel = true;
							if (building.buildingUnit.isValid())
							{
								cancel = !shouldProxyBuilderFinishSafely(building);
							}
							if (cancel)
							{
								m_bot.Buildings().CancelBuilding(building);
							}
						}
					}
					m_bot.Workers().getWorkerData().clearProxyWorkers();
					m_bot.Strategy().setStartingStrategy(STANDARD);
					m_bot.Commander().Production().clearQueue();
				}
			}
		}
		else if (m_startingStrategy == WORKER_RUSH)
		{
			const auto & enemyUnits = m_bot.GetKnownEnemyUnits();
			if (!enemyUnits.empty())
			{
				bool groundUnit = false;
				for (const auto & enemyUnit : enemyUnits)
				{
					if (!enemyUnit.isFlying())
					{
						groundUnit = true;
						break;
					}
				}
				if (!groundUnit)
				{
					m_startingStrategy = STANDARD;
				}
			}
		}
	}
}

bool StrategyManager::shouldProxyBuilderFinishSafely(const Building & building, bool onlyInjuredWorkers) const
{
	const auto & builder = building.builderUnit;
	if (builder.isValid() && builder.isAlive() && Util::DistSq(builder, building.buildingUnit) < 3 * 3)
	{
		if (onlyInjuredWorkers && builder.getHitPointsPercentage() == 100.f)
			return true;
		
		float totalPossibleDamageDealt = 0.f;
		const float secondsUntilFinished = (1 - building.buildingUnit.getUnitPtr()->build_progress) * 46.f;
		for (const auto & enemyUnit : m_bot.GetKnownEnemyUnits())
		{
			const auto tilesPerSecond = Util::getRealMovementSpeedOfUnit(enemyUnit.getUnitPtr(), m_bot);
			if (tilesPerSecond > 0.f)
			{
				const auto distance = Util::Dist(enemyUnit, builder);
				const auto timeToGetThere = distance / tilesPerSecond;
				const auto remainingTime = secondsUntilFinished - timeToGetThere;
				if (remainingTime > 0)
				{
					const auto dps = Util::GetDpsForTarget(enemyUnit.getUnitPtr(), builder.getUnitPtr(), m_bot);
					const auto damageDealt = remainingTime * dps;
					totalPossibleDamageDealt += damageDealt;
				}
			}
		}
		if (totalPossibleDamageDealt < builder.getHitPoints())
			return true;
	}
	return false;
}

bool StrategyManager::isProxyStartingStrategy() const
{
	return m_startingStrategy == PROXY_CYCLONES || m_startingStrategy == PROXY_MARAUDERS;
}

bool StrategyManager::isProxyFactoryStartingStrategy() const
{
	return m_startingStrategy == PROXY_CYCLONES;
}

bool StrategyManager::wasProxyStartingStrategy() const
{
	return m_initialStartingStrategy == PROXY_CYCLONES || m_initialStartingStrategy == PROXY_MARAUDERS;
}

const Strategy & StrategyManager::getCurrentStrategy() const
{
    auto strategy = m_strategies.find(m_bot.Config().StrategyName);

    BOT_ASSERT(strategy != m_strategies.end(), "Couldn't find Strategy corresponding to strategy name: %s", m_bot.Config().StrategyName.c_str());

    return (*strategy).second;
}

StrategyPostBuildOrder StrategyManager::getCurrentStrategyPostBuildOrder() const
{
	if (m_bot.Strategy().isWorkerRushed())
	{
		return WORKER_RUSH_DEFENSE;
	}
	if (m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Protoss)
	{
		//return TERRAN_VS_PROTOSS;
	}
	return TERRAN_CLASSIC;//MARINE_MARAUDER;
}

const MM::BuildOrder & StrategyManager::getOpeningBookBuildOrder() const
{
    return getCurrentStrategy().m_buildOrder;
}

bool StrategyManager::scoutConditionIsMet() const
{
    return getCurrentStrategy().m_scoutCondition.eval();
}

bool StrategyManager::attackConditionIsMet() const
{
    return getCurrentStrategy().m_attackCondition.eval();
}

void StrategyManager::addStrategy(const std::string & name, const Strategy & strategy)
{
    m_strategies[name] = strategy;
}

const UnitPairVector StrategyManager::getBuildOrderGoal() const
{
    return std::vector<UnitPair>();
}

const UnitPairVector StrategyManager::getProtossBuildOrderGoal() const
{
    return std::vector<UnitPair>();
}

const UnitPairVector StrategyManager::getTerranBuildOrderGoal() const
{
    return std::vector<UnitPair>();
}

const UnitPairVector StrategyManager::getZergBuildOrderGoal() const
{
    return std::vector<UnitPair>();
}


void StrategyManager::onEnd(const bool isWinner)
{
	if (isWinner || !m_bot.Config().SelectStartingBuildBasedOnHistory)
		return;

	std::stringstream ss;
	ss << "data/opponents/" << m_bot.GetOpponentId() << ".json";
	std::string path = ss.str();
	int wins;
	int losses;
	std::ifstream file(path);
	json j;
	if (file.good())
	{
		file >> j;
		file.close();
	}
	JSONTools::ReadInt("wins", j["strategies"][int(m_initialStartingStrategy)], wins);
	JSONTools::ReadInt("losses", j["strategies"][int(m_initialStartingStrategy)], losses);
	j["strategies"][int(m_initialStartingStrategy)]["wins"] = wins - 1;
	j["strategies"][int(m_initialStartingStrategy)]["losses"] = losses + 1;
	std::ofstream outFile(path);
	outFile << j.dump();
	outFile.close();
}

void StrategyManager::readStrategyFile(const std::string & filename)
{
    std::string ourRace = Util::GetStringFromRace(m_bot.GetSelfRace());

    std::ifstream file(filename);
    json j;
    file >> j;

#ifdef SC2API
    const char * strategyObject = "SC2API Strategy";
#else
    const char * strategyObject = "BWAPI Strategy";
#endif

    // Parse the Strategy Options
    if (j.count(strategyObject) && j[strategyObject].is_object())
    {
        const json & strategy = j[strategyObject];

        // read in the various strategic elements
        JSONTools::ReadBool("ScoutHarassEnemy", strategy, m_bot.Config().ScoutHarassEnemy);
		JSONTools::ReadBool("AutoCompleteBuildOrder", strategy, m_bot.Config().AutoCompleteBuildOrder);
		JSONTools::ReadBool("NoScoutOn2PlayersMap", strategy, m_bot.Config().NoScoutOn2PlayersMap);
        JSONTools::ReadString("ReadDirectory", strategy, m_bot.Config().ReadDir);
        JSONTools::ReadString("WriteDirectory", strategy, m_bot.Config().WriteDir);

        // if we have set a strategy for the current race, use it
        if (strategy.count(ourRace.c_str()) && strategy[ourRace.c_str()].is_string())
        {
            m_bot.Config().StrategyName = strategy[ourRace.c_str()].get<std::string>();
        }

        // check if we are using an enemy specific strategy
        JSONTools::ReadBool("UseEnemySpecificStrategy", strategy, m_bot.Config().UseEnemySpecificStrategy);
        if (m_bot.Config().UseEnemySpecificStrategy && strategy.count("EnemySpecificStrategy") && strategy["EnemySpecificStrategy"].is_object())
        {
            // TODO: Figure out enemy name
            const std::string enemyName = "ENEMY NAME";
            const json & specific = strategy["EnemySpecificStrategy"];

            // check to see if our current enemy name is listed anywhere in the specific strategies
            if (specific.count(enemyName.c_str()) && specific[enemyName.c_str()].is_object())
            {
                const json & enemyStrategies = specific[enemyName.c_str()];

                // if that enemy has a strategy listed for our current race, use it
                if (enemyStrategies.count(ourRace.c_str()) && enemyStrategies[ourRace.c_str()].is_string())
                {
                    m_bot.Config().StrategyName = enemyStrategies[ourRace.c_str()].get<std::string>();
                    m_bot.Config().FoundEnemySpecificStrategy = true;
                }
            }
        }

        // Parse all the Strategies
        if (strategy.count("Strategies") && strategy["Strategies"].is_object())
        {
            const json & strategies = strategy["Strategies"];
            for (auto it = strategies.begin(); it != strategies.end(); ++it) 
            {
                const std::string & name = it.key();
                const json & val = it.value();              
                
                BOT_ASSERT(val.count("Race") && val["Race"].is_string(), "Strategy is missing a Race string");
                CCRace strategyRace = Util::GetRaceFromString(val["Race"].get<std::string>());
                
                BOT_ASSERT(val.count("OpeningBuildOrder") && val["OpeningBuildOrder"].is_array(), "Strategy is missing an OpeningBuildOrder arrau");
				MM::BuildOrder buildOrder;
                const json & build = val["OpeningBuildOrder"];
                for (size_t b(0); b < build.size(); b++)
                {
                    if (build[b].is_string())
                    {
                        MetaType MetaType(build[b].get<std::string>(), m_bot);
                        buildOrder.add(MetaType);
                    }
                    else
                    {
                        BOT_ASSERT(false, "Build order item must be a string %s", name.c_str());
                        continue;
                    }
                }

                BOT_ASSERT(val.count("ScoutCondition") && val["ScoutCondition"].is_array(), "Strategy is missing a ScoutCondition array");
                Condition scoutCondition(val["ScoutCondition"], m_bot);
                
                BOT_ASSERT(val.count("AttackCondition") && val["AttackCondition"].is_array(), "Strategy is missing an AttackCondition array");
                Condition attackCondition(val["AttackCondition"], m_bot);

                addStrategy(name, Strategy(name, strategyRace, buildOrder, scoutCondition, attackCondition));
            }
        }
    }
}
