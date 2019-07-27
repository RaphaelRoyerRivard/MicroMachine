#include "GameCommander.h"
#include "CCBot.h"
#include "Util.h"

GameCommander::GameCommander(CCBot & bot)
    : m_bot                 (bot)
    , m_productionManager   (bot)
    , m_scoutManager        (bot)
    , m_combatCommander     (bot)
    , m_initialScoutSet     (false)
{

}

void GameCommander::onStart()
{
    m_productionManager.onStart();
    m_scoutManager.onStart();
    m_combatCommander.onStart();
}

void GameCommander::onFrame()
{
	m_bot.StartProfiling("0.10.1   handleUnitAssignments");
    handleUnitAssignments();
	m_bot.StopProfiling("0.10.1   handleUnitAssignments");

	m_bot.StartProfiling("0.10.2   m_productionManager.onFrame");
    m_productionManager.onFrame();
	m_bot.StopProfiling("0.10.2   m_productionManager.onFrame");
	m_bot.StartProfiling("0.10.3   m_scoutManager.onFrame");
    m_scoutManager.onFrame();
	m_bot.StopProfiling("0.10.3   m_scoutManager.onFrame");
	m_bot.StartProfiling("0.10.4   m_combatCommander.onFrame");
    m_combatCommander.onFrame(m_combatUnits);
	m_bot.StopProfiling("0.10.4   m_combatCommander.onFrame");
}

ProductionManager& GameCommander::Production()
{
	return m_productionManager;
}

CombatCommander& GameCommander::Combat()
{
	return m_combatCommander;
}

// assigns units to various managers
void GameCommander::handleUnitAssignments()
{
    m_validUnits.clear();
    m_combatUnits.clear();

    // filter our units for those which are valid and usable
    setValidUnits();

    // set each type of unit
    setScoutUnits();
    setCombatUnits();
}

bool GameCommander::isAssigned(const Unit & unit) const
{
    return     (std::find(m_combatUnits.begin(), m_combatUnits.end(), unit) != m_combatUnits.end())
        || (std::find(m_scoutUnits.begin(), m_scoutUnits.end(), unit) != m_scoutUnits.end());
}

// validates units as usable for distribution to various managers
void GameCommander::setValidUnits()
{
    // make sure the unit is completed and alive and usable
    //for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
	for (auto & mapUnit : m_bot.GetAllyUnits())
    {
		auto &unit = mapUnit.second;
		if (!unit.isValid())
			continue;
		if (!unit.isCompleted())
			continue;
		if (!unit.isAlive())
			continue;
        m_validUnits.push_back(unit);
    }
}

std::vector<Unit> GameCommander::getValidUnits() const
{
	return m_validUnits;
}

void GameCommander::setScoutUnits()
{
    // if we haven't set a scout unit, do it
    if (m_scoutUnits.empty())
    {
        const BaseLocation * enemyBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);

		if (m_bot.Config().NoScoutOn2PlayersMap && enemyBaseLocation != nullptr)
			return;

        // We need to find the enemy base to do something
        if (!m_initialScoutSet || enemyBaseLocation == nullptr)
        {
            // if it exists
            if (shouldSendInitialScout())
            {
                // grab the closest worker to the supply provider to send to scout
                Unit workerScout = m_bot.Workers().getClosestMineralWorkerTo(m_bot.GetStartLocation());

                // if we find a worker (which we should) add it to the scout units
                if (workerScout.isValid())
                {
                    m_scoutManager.setWorkerScout(workerScout);
                    assignUnit(workerScout, m_scoutUnits);
                    m_initialScoutSet = true;
                }
            }
        }
    }
}

bool GameCommander::shouldSendInitialScout()
{
    return m_bot.Strategy().scoutConditionIsMet();
}

// sets combat units to be passed to CombatCommander
void GameCommander::setCombatUnits()
{
    for (auto & unit : m_validUnits)
    {
        BOT_ASSERT(unit.isValid(), "Have a null unit in our valid units\n");

        if (!isAssigned(unit) && unit.getType().isCombatUnit())
        {
			if (unit.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
				continue;

            assignUnit(unit, m_combatUnits);
        }
    }
}

void GameCommander::onUnitCreate(const Unit & unit)
{

}

void GameCommander::onUnitDestroy(const Unit & unit)
{
    //_productionManager.onUnitDestroy(unit);
}


void GameCommander::assignUnit(const Unit & unit, std::vector<Unit> & units)
{
    if (std::find(m_scoutUnits.begin(), m_scoutUnits.end(), unit) != m_scoutUnits.end())
    {
        m_scoutUnits.erase(std::remove(m_scoutUnits.begin(), m_scoutUnits.end(), unit), m_scoutUnits.end());
    }
    else if (std::find(m_combatUnits.begin(), m_combatUnits.end(), unit) != m_combatUnits.end())
    {
        m_combatUnits.erase(std::remove(m_combatUnits.begin(), m_combatUnits.end(), unit), m_combatUnits.end());
    }

    units.push_back(unit);
}
