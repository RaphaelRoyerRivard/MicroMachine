#include "StrategyManager.h"
#include "CCBot.h"
#include "JSONTools.h"
#include "Util.h"
#include "MetaType.h"

Strategy::Strategy()
{

}

Strategy::Strategy(const std::string & name, const CCRace & race, const BuildOrder & buildOrder, const Condition & scoutCondition, const Condition & attackCondition)
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
				bool validRaceSpecificStrategy = raceSpecificStrategy && m_bot.GetPlayerRace(Players::Enemy) == it->second;
				if (bestStrat < 0 || winPercentage > bestScore || (winPercentage == bestScore && (validRaceSpecificStrategy || (games > 0 && games < bestScoreGames))))
				{
					if (!raceSpecificStrategy || validRaceSpecificStrategy)
					{
						bestScore = winPercentage;
						bestStrat = stratIndex;
						bestScoreGames = games;
					}
				}
				m_opponentHistory << STRATEGY_NAMES[stratIndex] << " (" << wins << "-" << losses << ")";
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
			m_startingStrategy = PROXY_CYCLONES;
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
		if (!m_greetingMessage.str().empty())
		{
			m_bot.Actions()->SendChat(m_greetingMessage.str());
			m_greetingMessage.str("");
			m_greetingMessage.clear();
		}
		if (!m_opponentHistory.str().empty())
		{
			m_bot.Actions()->SendChat(m_opponentHistory.str(), sc2::ChatChannel::Team);
			m_opponentHistory.str("");
			m_opponentHistory.clear();
		}
		if (!m_strategyMessage.str().empty())
		{
			m_bot.Actions()->SendChat(m_strategyMessage.str(), sc2::ChatChannel::Team);
			m_strategyMessage.str("");
			m_strategyMessage.clear();
		}
	}
	if (executeMacro)
	{
		if (isProxyStartingStrategy())
		{
			const auto barracksCount = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Barracks.getUnitType(), true, true);

			if (m_bot.GetGameLoop() >= 448 && m_bot.Workers().getWorkerData().getProxyWorkers().empty())	// after 20s
			{
				if (isWorkerRushed())
				{
					m_bot.Workers().getWorkerData().clearProxyWorkers();
					m_startingStrategy = STANDARD;
					return;
				}
				
				const auto hasFactory = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Factory.getUnitType(), false, true) > 0;
				if (barracksCount == 0)
				{
					m_startingStrategy = STANDARD;
					m_bot.Commander().Production().clearQueue();
					m_bot.Commander().Production().queueAsHighestPriority(MetaTypeEnum::Barracks, false);
				}
				else if (!hasFactory && m_startingStrategy == PROXY_CYCLONES)
				{
					m_startingStrategy = STANDARD;
					m_bot.Commander().Production().clearQueue();
				}
			}
			else if (barracksCount >= 2 && m_startingStrategy != PROXY_CYCLONES)
			{
				const auto & proxyWorkers = m_bot.Workers().getWorkerData().getProxyWorkers();
				for (const auto & proxyWorker : proxyWorkers)
				{
					proxyWorker.move(m_bot.GetStartLocation());
				}
				m_bot.Workers().getWorkerData().clearProxyWorkers();
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
	return MARINE_MARAUDER;//TERRAN_CLASSIC;
}

const BuildOrder & StrategyManager::getOpeningBookBuildOrder() const
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
                BuildOrder buildOrder;
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
