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

void GameCommander::onFrame(bool executeMacro)
{
	m_bot.StartProfiling("0.10.1   handleUnitAssignments");
    handleUnitAssignments();
	m_bot.StopProfiling("0.10.1   handleUnitAssignments");

	m_bot.StartProfiling("0.10.2   m_productionManager.onFrame");
	m_productionManager.onFrame(executeMacro);
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

	// check for unit being carried and units carrying others
	setCarryingAndCarried();
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
                Unit workerScout = m_bot.Workers().getClosestAvailableWorkerTo(m_bot.GetStartLocation());

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

void GameCommander::setCarryingAndCarried()
{
	//Reset lists
	m_unitInside.clear();
	m_unitCarrier.clear();

	for (auto & carrier : m_validUnits)
	{
		if (carrier.getUnitPtr()->cargo_space_max == 0 || carrier.getUnitPtr()->cargo_space_taken == 0)
		{
			continue;
		}

		for (auto & carried : carrier.getUnitPtr()->passengers)
		{
			setInside(carried.tag);
			setCarrierForUnit(carried.tag, &carrier);
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


bool GameCommander::isInside(sc2::Tag unit)
{
	return std::find(m_unitInside.begin(), m_unitInside.end(), unit) != m_unitInside.end();
}

void GameCommander::setInside(sc2::Tag unit)
{
	m_unitInside.push_back(unit);
}

Unit* GameCommander::getCarrierForUnit(sc2::Tag unitTag)
{
	return m_unitCarrier[unitTag];
}

Unit* GameCommander::setCarrierForUnit(sc2::Tag unitTag, Unit* carrier)
{
	return m_unitCarrier[unitTag] = carrier;
}

void GameCommander::AddDelayedSmartAbility(Unit unit, sc2::AbilityID ability, CCPosition position)
{
	m_delayedSmartAbility.push_back(std::pair<Unit, std::pair<sc2::AbilityID, CCPosition>>(unit, std::pair<sc2::AbilityID, CCPosition>(ability, position)));
}

void GameCommander::GiveDelayedSmarAbilityOrders()
{
	for (auto command : m_delayedSmartAbility)
	{
		if (command.second.first == 0)
		{
			command.first.rightClick(command.second.second);
		}
		else
		{
			Micro::SmartAbility(command.first.getUnitPtr(), command.second.first, command.second.second, m_bot);
		}
	}
}