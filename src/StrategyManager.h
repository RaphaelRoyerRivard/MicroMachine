#pragma once

#include "Common.h"
#include "BuildOrder.h"
#include "Condition.h"

typedef std::pair<UnitType, size_t> UnitPair;
typedef std::vector<UnitPair>       UnitPairVector;

class CCBot;

struct Strategy
{
    std::string m_name;
    CCRace      m_race;
    int         m_wins;
    int         m_losses;
    BuildOrder  m_buildOrder;
    Condition   m_scoutCondition;
    Condition   m_attackCondition;

    Strategy();
    Strategy(const std::string & name, const CCRace & race, const BuildOrder & buildOrder, const Condition & scoutCondition, const Condition & attackCondition);
};

enum StartingStrategy
{
	STANDARD,
	PROXY_REAPERS,
	PROXY_CYCLONES
};

enum StrategyPostBuildOrder {
	NO_STRATEGY = -1,
	TERRAN_CLASSIC = 0,
	TERRAN_VS_PROTOSS = 1,
	WORKER_RUSH_DEFENSE = 3
};

class StrategyManager
{
    CCBot & m_bot;

    CCRace                          m_selfRace;
    CCRace                          m_enemyRace;
    std::map<std::string, Strategy> m_strategies;
	StartingStrategy m_startingStrategy;
    int                             m_totalGamesPlayed;
    const BuildOrder                m_emptyBuildOrder;
	bool m_workerRushed = false;
	bool m_earlyRushed = false;
	bool m_shouldProduceAntiAirOffense = false;
	bool m_shouldProduceAntiAirDefense = false;
	bool m_enemyHasProtossHighTechAir = false;
	bool m_enemyHasInvisible = false;
	bool m_enemyHasMetabolicBoost = false;
	bool m_enemyHasMassZerglings = false;
	bool m_enemyHasHiSecAutoTracking = false;
	bool m_enemyOnlyHasFlyingBuildings = false;
	bool m_enemyHasSeveralArmoredUnits = false;
	bool m_focusBuildings = false;
	std::set<sc2::UPGRADE_ID> m_completedUpgrades;

    const UnitPairVector getProtossBuildOrderGoal() const;
    const UnitPairVector getTerranBuildOrderGoal() const;
    const UnitPairVector getZergBuildOrderGoal() const;

public:

    StrategyManager(CCBot & bot);

    const Strategy & getCurrentStrategy() const;
	StartingStrategy getStartingStrategy() const { return m_startingStrategy; }
	StrategyPostBuildOrder getCurrentStrategyPostBuildOrder() const;
    bool scoutConditionIsMet() const;
    bool attackConditionIsMet() const;
    void onStart();
    void onFrame(bool executeMacro);
    void onEnd(const bool isWinner);
    void addStrategy(const std::string & name, const Strategy & strategy);
    const UnitPairVector getBuildOrderGoal() const;
    const BuildOrder & getOpeningBookBuildOrder() const;
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
	bool enemyHasInvisible() const { return m_enemyHasInvisible; }
	void setEnemyHasInvisible(bool enemyHasInvisible) { m_enemyHasInvisible = enemyHasInvisible; }
	bool enemyHasMetabolicBoost() const { return m_enemyHasMetabolicBoost; }
	void setEnemyHasMetabolicBoost(bool enemyHasMetabolicBoost) { m_enemyHasMetabolicBoost = enemyHasMetabolicBoost; }
	bool enemyHasMassZerglings() const { return m_enemyHasMassZerglings; }
	void setEnemyHasMassZerglings(bool enemyHasMassZerglings) { m_enemyHasMassZerglings = enemyHasMassZerglings; }
	bool enemyHasHiSecAutoTracking() const { return m_enemyHasHiSecAutoTracking; }
	void setEnemyHasHiSecAutoTracking(bool enemyHasHiSecAutoTracking) { m_enemyHasHiSecAutoTracking = enemyHasHiSecAutoTracking; }
	bool enemyOnlyHasFlyingBuildings() const { return m_enemyOnlyHasFlyingBuildings; }
	void setEnemyOnlyHasFlyingBuildings(bool enemyOnlyHasFlyingBuildings) { m_enemyOnlyHasFlyingBuildings = enemyOnlyHasFlyingBuildings; }
	bool enemyHasSeveralArmoredUnits() const { return m_enemyHasSeveralArmoredUnits; }
	void setEnemyHasSeveralArmoredUnits(bool enemyHasSeveralArmoredUnits) { m_enemyHasSeveralArmoredUnits = enemyHasSeveralArmoredUnits; }
	bool shouldFocusBuildings() const { return m_focusBuildings; }
	void setFocusBuildings(bool focusBuildings) { m_focusBuildings = focusBuildings; }
	const std::set<sc2::UPGRADE_ID> & getCompletedUpgrades() const { return m_completedUpgrades; };
	bool isUpgradeCompleted(sc2::UPGRADE_ID upgradeId) const { return m_completedUpgrades.find(upgradeId) != m_completedUpgrades.end(); }
	void setUpgradeCompleted(sc2::UPGRADE_ID upgradeId) { m_completedUpgrades.insert(upgradeId); }
};
