#pragma once

#include "Common.h"
#include "BuildOrder.h"
#include "Condition.h"

class Building;
typedef std::pair<UnitType, size_t> UnitPair;
typedef std::vector<UnitPair>       UnitPairVector;

class CCBot;

struct Strategy
{
    std::string m_name;
    CCRace      m_race;
    int         m_wins;
    int         m_losses;
    MM::BuildOrder  m_buildOrder;
    Condition   m_scoutCondition;
    Condition   m_attackCondition;

    Strategy();
    Strategy(const std::string & name, const CCRace & race, const MM::BuildOrder & buildOrder, const Condition & scoutCondition, const Condition & attackCondition);
};

enum StrategyPostBuildOrder {
	NO_STRATEGY = -1,
	TERRAN_CLASSIC = 0,
	TERRAN_VS_PROTOSS = 1,
	MARINE_MARAUDER = 2,
	WORKER_RUSH_DEFENSE = 3
};

enum StartingStrategy
{
	// Always add new strategies at the end of the enum (before COUNT), otherwise the saved data will be wrong
	PROXY_CYCLONES = 0,
	EARLY_EXPAND = 1,
	STANDARD = 2,
	WORKER_RUSH = 3,
	PROXY_MARAUDERS = 4,
	FAST_PF = 5,
	COUNT = 6
};

class StrategyManager
{
	std::vector<std::string> STRATEGY_NAMES = {
		"PROXY_CYCLONES",
		"EARLY_EXPAND",
		"STANDARD",
		"WORKER_RUSH",	// removed
		"PROXY_MARAUDERS",
		"FAST_PF"
	};

	std::map<StartingStrategy, sc2::Race> RACE_SPECIFIC_STRATEGIES = {
		//{ PROXY_MARAUDERS, sc2::Race::Protoss }
	};

	// Only strategies in this list and in the race specific list can be chosen
	std::vector<std::string> STRATEGY_ORDER = {
		"STANDARD",
		"EARLY_EXPAND",
		"FAST_PF",
		"PROXY_CYCLONES",
		"PROXY_MARAUDERS"
	};
	
    CCBot & m_bot;

    CCRace                          m_selfRace;
    CCRace                          m_enemyRace;
    std::map<std::string, Strategy> m_strategies;
	StartingStrategy m_initialStartingStrategy;
	StartingStrategy m_startingStrategy;
    int                             m_totalGamesPlayed;
    const MM::BuildOrder            m_emptyBuildOrder;
	bool m_workerRushed = false;
	bool m_earlyRushed = false;
	bool m_shouldProduceAntiAirOffense = false;
	bool m_shouldProduceAntiAirDefense = false;
	bool m_enemyHasProtossHighTechAir = false;
	bool m_enemyHasNydusWorm = false;
	bool m_enemyHasInvisible = false;
	bool m_enemyHasMovingInvisible = false;
	bool m_enemyCurrentlyHasInvisible = false;
	bool m_enemyHasMetabolicBoost = false;
	bool m_enemyHasProxyHatchery = false;
	bool m_enemyHasProxyCombatBuildings = false;
	bool m_enemyHasBurrowingUpgrade = false;
	bool m_enemyHasMassZerglings = false;
	bool m_enemyHasHiSecAutoTracking = false;
	bool m_enemyOnlyHasFlyingBuildings = false;
	bool m_enemyHasSeveralArmoredUnits = false;
	bool m_enemyHasWorkerHiddingInOurMain = false;
	bool m_focusBuildings = false;
	std::set<sc2::UPGRADE_ID> m_completedUpgrades;
	std::stringstream m_greetingMessage;
	std::stringstream m_opponentHistory;
	std::stringstream m_strategyMessage;
	bool m_greetingMessageSent = false;
	bool m_opponentHistorySent = false;
	bool m_strategyMessageSent = false;

    const UnitPairVector getProtossBuildOrderGoal() const;
    const UnitPairVector getTerranBuildOrderGoal() const;
    const UnitPairVector getZergBuildOrderGoal() const;

public:

    StrategyManager(CCBot & bot);

    const Strategy & getCurrentStrategy() const;
	StartingStrategy getStartingStrategy() const { return m_startingStrategy; }
	void setStartingStrategy(StartingStrategy startingStrategy);
	void checkForStrategyChange();
	StartingStrategy getInitialStartingStrategy() const { return m_initialStartingStrategy; }
	bool shouldProxyBuilderFinishSafely(const Building & building, bool onlyInjuredWorkers = false) const;
	bool isProxyStartingStrategy() const;
	bool isProxyFactoryStartingStrategy() const;
	bool wasProxyStartingStrategy() const;
	StrategyPostBuildOrder getCurrentStrategyPostBuildOrder() const;
    bool scoutConditionIsMet() const;
    bool attackConditionIsMet() const;
    void onStart();
    void onFrame(bool executeMacro);
    void onEnd(std::string result);
    void addStrategy(const std::string & name, const Strategy & strategy);
    const UnitPairVector getBuildOrderGoal() const;
    const MM::BuildOrder & getOpeningBookBuildOrder() const;
    void readStrategyFile(const std::string & str);
	bool isWorkerRushed() const { return m_workerRushed; }
	void setIsWorkerRushed(bool workerRushed) { m_workerRushed = workerRushed; }
	bool isEarlyRushed() const { return m_earlyRushed; }
	void setIsEarlyRushed(bool earlyRushed) { m_earlyRushed = earlyRushed; }
	bool shouldProduceAntiAirOffense() const { return m_shouldProduceAntiAirOffense; }
	void setShouldProduceAntiAirOffense(bool shouldProduceAntiAirOffense) { m_shouldProduceAntiAirOffense = shouldProduceAntiAirOffense; }
	bool shouldProduceAntiAirDefense() const { return m_shouldProduceAntiAirDefense; }
	void setShouldProduceAntiAirDefense(bool shouldProduceAntiAirDefense) { m_shouldProduceAntiAirDefense = shouldProduceAntiAirDefense; }
	bool enemyHasProtossHighTechAir() const { return m_enemyHasProtossHighTechAir; }
	void setEnemyHasProtossHighTechAir(bool enemyHasHighTechAir) { m_enemyHasProtossHighTechAir = enemyHasHighTechAir; }
	bool enemyHasNydusWorm() const { return m_enemyHasNydusWorm; }
	void setEnemyHasNydusWorm(bool enemyHasNydusWorm) { m_enemyHasNydusWorm = enemyHasNydusWorm; }
	bool enemyHasInvisible() const { return m_enemyHasInvisible; }
	bool enemyHasMovingInvisible() const { return m_enemyHasMovingInvisible; }
	bool enemyCurrentlyHasInvisible() const { return m_enemyCurrentlyHasInvisible; }
	void setEnemyHasInvisible(bool enemyHasInvisible) { m_enemyHasInvisible = enemyHasInvisible; }
	void setEnemyHasMovingInvisible(bool enemyHasMovingInvisible) { m_enemyHasMovingInvisible = enemyHasMovingInvisible; }
	void setEnemyCurrentlyHasInvisible(bool enemyCurrentlyHasInvisible) { m_enemyCurrentlyHasInvisible = enemyCurrentlyHasInvisible; }
	bool enemyHasMetabolicBoost() const { return m_enemyHasMetabolicBoost; }
	void setEnemyHasMetabolicBoost(bool enemyHasMetabolicBoost) { m_enemyHasMetabolicBoost = enemyHasMetabolicBoost; }
	bool enemyHasProxyHatchery() const { return m_enemyHasProxyHatchery; }
	void setEnemyHasProxyHatchery(bool enemyHasProxyHatchery) { m_enemyHasProxyHatchery = enemyHasProxyHatchery; }
	bool enemyHasProxyCombatBuildings() const { return m_enemyHasProxyCombatBuildings; }
	void setEnemyHasProxyCombatBuildings(bool enemyHasProxyCombatBuildings) { m_enemyHasProxyCombatBuildings = enemyHasProxyCombatBuildings; }
	bool enemyHasBurrowingUpgrade() const { return m_enemyHasBurrowingUpgrade; }
	void setEnemyHasBurrowingUpgrade(bool enemyHasBurrowingUpgrade) { m_enemyHasBurrowingUpgrade = enemyHasBurrowingUpgrade; }
	bool enemyHasMassZerglings() const { return m_enemyHasMassZerglings; }
	void setEnemyHasMassZerglings(bool enemyHasMassZerglings) { m_enemyHasMassZerglings = enemyHasMassZerglings; }
	bool enemyHasHiSecAutoTracking() const { return m_enemyHasHiSecAutoTracking; }
	void setEnemyHasHiSecAutoTracking(bool enemyHasHiSecAutoTracking) { m_enemyHasHiSecAutoTracking = enemyHasHiSecAutoTracking; }
	bool enemyOnlyHasFlyingBuildings() const { return m_enemyOnlyHasFlyingBuildings; }
	void setEnemyOnlyHasFlyingBuildings(bool enemyOnlyHasFlyingBuildings) { m_enemyOnlyHasFlyingBuildings = enemyOnlyHasFlyingBuildings; }
	bool enemyHasSeveralArmoredUnits() const { return m_enemyHasSeveralArmoredUnits; }
	void setEnemyHasSeveralArmoredUnits(bool enemyHasSeveralArmoredUnits) { m_enemyHasSeveralArmoredUnits = enemyHasSeveralArmoredUnits; }
	bool enemyHasWorkerHiddingInOurMain() const { return m_enemyHasWorkerHiddingInOurMain; }
	void setEnemyHasWorkerHiddingInOurMain(bool enemyHasWorkerHiddingInOurMain) { m_enemyHasWorkerHiddingInOurMain = enemyHasWorkerHiddingInOurMain; }
	bool shouldFocusBuildings() const { return m_focusBuildings; }
	void setFocusBuildings(bool focusBuildings) { m_focusBuildings = focusBuildings; }
	const std::set<sc2::UPGRADE_ID> & getCompletedUpgrades() const { return m_completedUpgrades; };
	bool isUpgradeCompleted(sc2::UPGRADE_ID upgradeId) const { return m_completedUpgrades.find(upgradeId) != m_completedUpgrades.end(); }
	void setUpgradeCompleted(sc2::UPGRADE_ID upgradeId) { m_completedUpgrades.insert(upgradeId); }
};
