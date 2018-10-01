#include "CombatCommander.h"
#include "Util.h"
#include "CCBot.h"

const size_t IdlePriority = 0;
const size_t BackupPriority = 1;
const size_t HarassPriority = 2;
const size_t AttackPriority = 2;
const size_t BaseDefensePriority = 3;
const size_t ScoutDefensePriority = 4;
const size_t DropPriority = 4;

const float DefaultOrderRadius = 25;			//Order radius is the threat awareness range of units in the squad
const float MainAttackOrderRadius = 15;
const float HarassOrderRadius = 15;
const float MainAttackMaxDistance = 20;			//Distance from the center of the Main Attack Squad for a unit to be considered in it
const float MainAttackMaxRegroupDuration = 100; //Max number of frames allowed for a regroup order
const float MainAttackRegroupCooldown = 200;	//Min number of frames required to wait between regroup orders
const float MainAttackMinRetreatDuration = 50;	//Max number of frames allowed for a regroup order

CombatCommander::CombatCommander(CCBot & bot)
    : m_bot(bot)
    , m_squadData(bot)
    , m_initialized(false)
    , m_attackStarted(false)
	, m_currentBaseExplorationIndex(0)
{

}

void CombatCommander::onStart()
{
    m_squadData.clearSquadData();

    // the squad that consists of units waiting for the squad to be big enough to begin the main attack
    CCTilePosition nextExpansionPosition = m_bot.Bases().getNextExpansion(Players::Self);
    SquadOrder idleOrder(SquadOrderTypes::Attack, CCPosition(nextExpansionPosition.x, nextExpansionPosition.y), DefaultOrderRadius, "Prepare for battle");
    m_squadData.addSquad("Idle", Squad("Idle", idleOrder, IdlePriority, m_bot));

	// the harass attack squad that will pressure the enemy's main base workers
	SquadOrder harassOrder(SquadOrderTypes::Harass, CCPosition(0, 0), HarassOrderRadius, "Harass");
	m_squadData.addSquad("Harass", Squad("Harass", harassOrder, HarassPriority, m_bot));

    // the main attack squad that will pressure the enemy's closest base location
    SquadOrder mainAttackOrder(SquadOrderTypes::Attack, CCPosition(0, 0), MainAttackOrderRadius, "Attack");
    m_squadData.addSquad("MainAttack", Squad("MainAttack", mainAttackOrder, MainAttackMaxRegroupDuration, MainAttackRegroupCooldown, MainAttackMinRetreatDuration, MainAttackMaxDistance, AttackPriority, m_bot));

    // the backup squad that will send reinforcements to the main attack squad
    SquadOrder backupSquadOrder(SquadOrderTypes::Attack, CCPosition(0, 0), DefaultOrderRadius, "Send backups");
    m_squadData.addSquad("Backup1", Squad("Backup1", backupSquadOrder, BackupPriority, m_bot));

    // the scout defense squad will handle chasing the enemy worker scout
    SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, m_bot.GetStartLocation(), DefaultOrderRadius, "Chase scout");
    m_squadData.addSquad("ScoutDefense", Squad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority, m_bot));
}

bool CombatCommander::isSquadUpdateFrame()
{
    return true;
}

void CombatCommander::onFrame(const std::vector<Unit> & combatUnits)
{
    if (!m_attackStarted)
    {
        m_attackStarted = shouldWeStartAttacking();
    }

    m_combatUnits = combatUnits;

    m_squadData.onFrame();

    if (isSquadUpdateFrame())
    {
        updateIdleSquad();
        updateScoutDefenseSquad();
        updateDefenseSquads();
		updateHarassSquads();
		updateAttackSquads();
        updateBackupSquads();
    }
}

bool CombatCommander::shouldWeStartAttacking()
{
    return m_bot.Strategy().getCurrentStrategy().m_attackCondition.eval();
}

void CombatCommander::updateIdleSquad()
{
    Squad & idleSquad = m_squadData.getSquad("Idle");
    for (auto & unit : m_combatUnits)
    {
        // if it hasn't been assigned to a squad yet, put it in the low priority idle squad
        if (m_squadData.canAssignUnitToSquad(unit, idleSquad))
        {
            idleSquad.addUnit(unit);
        }
    }

	if (idleSquad.getUnits().empty())
		return;

	if (idleSquad.needsToRetreat())
	{
		SquadOrder retreatOrder(SquadOrderTypes::Retreat, getMainAttackLocation(), DefaultOrderRadius, "Retreat!!");
		idleSquad.setSquadOrder(retreatOrder);
	}
	//regroup only after retreat
	else if (idleSquad.needsToRegroup())
	{
		SquadOrder regroupOrder(SquadOrderTypes::Regroup, getMainAttackLocation(), DefaultOrderRadius, "Regroup");
		idleSquad.setSquadOrder(regroupOrder);
	}
	else
	{
		CCTilePosition nextExpansionPosition = m_bot.Bases().getNextExpansion(Players::Self);
		SquadOrder idleOrder(SquadOrderTypes::Attack, CCPosition(nextExpansionPosition.x, nextExpansionPosition.y), DefaultOrderRadius, "Prepare for battle");
		m_squadData.addSquad("Idle", Squad("Idle", idleOrder, IdlePriority, m_bot));
	}
}

void CombatCommander::updateBackupSquads()
{
    if (!m_attackStarted)
    {
        return;
    }

    Squad & mainAttackSquad = m_squadData.getSquad("MainAttack");

    int backupNo = 1;
    while (m_squadData.squadExists("Backup" + std::to_string(backupNo)))
    {
        Squad & backupSquad = m_squadData.getSquad("Backup" + std::to_string(backupNo));

        for (auto & unit : m_combatUnits)
        {
            BOT_ASSERT(unit.isValid(), "null unit in combat units");

            // get every unit of a lower priority and put it into the attack squad
            if (!unit.getType().isWorker()
                && !(unit.getType().isOverlord())
                && !(unit.getType().isQueen())
                && m_squadData.canAssignUnitToSquad(unit, backupSquad))
                //TODO validate that the unit is near enough the backup squad, otherwise create another one
            {
                m_squadData.assignUnitToSquad(unit, backupSquad);
            }
        }

		if (mainAttackSquad.isSuiciding())
		{
			SquadOrder retreatOrder(SquadOrderTypes::Retreat, CCPosition(0, 0), 25, "Retreat");
			backupSquad.setSquadOrder(retreatOrder);
		}
		else
		{
			SquadOrder sendBackupsOrder(SquadOrderTypes::Attack, mainAttackSquad.calcCenter(), 25, "Send backups");
			backupSquad.setSquadOrder(sendBackupsOrder);
		}

        ++backupNo;
    }
}

void CombatCommander::updateHarassSquads()
{
	Squad & harassSquad = m_squadData.getSquad("Harass");

	for (auto & unit : m_combatUnits)
	{
		BOT_ASSERT(unit.isValid(), "null unit in combat units");

		// put high mobility units in the harass squad
		if ((unit.getType().getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_REAPER
			|| unit.getType().getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_HELLION
			|| unit.getType().getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER
			|| unit.getType().getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_BANSHEE)
			&& m_squadData.canAssignUnitToSquad(unit, harassSquad))
		{
			m_squadData.assignUnitToSquad(unit, harassSquad);
		}
	}

	if (harassSquad.getUnits().empty())
		return;

	SquadOrder harassOrder(SquadOrderTypes::Harass, getMainAttackLocation(), HarassOrderRadius, "Harass");
	harassSquad.setSquadOrder(harassOrder);
}

void CombatCommander::updateAttackSquads()
{
    if (!m_attackStarted)
    {
        return;
    }

    Squad & mainAttackSquad = m_squadData.getSquad("MainAttack");

    for (auto & unit : m_combatUnits)
    {   
        BOT_ASSERT(unit.isValid(), "null unit in combat units");

        // get every unit of a lower priority and put it into the attack squad
        if (!unit.getType().isWorker()
            && !(unit.getType().isOverlord()) 
            && !(unit.getType().isQueen()) 
            && m_squadData.canAssignUnitToSquad(unit, mainAttackSquad))
        {
            m_squadData.assignUnitToSquad(unit, mainAttackSquad);
        }
    }

    if (mainAttackSquad.getUnits().empty())
        return;

    if (mainAttackSquad.needsToRetreat())
    {
        SquadOrder retreatOrder(SquadOrderTypes::Retreat, getMainAttackLocation(), DefaultOrderRadius, "Retreat!!");
        mainAttackSquad.setSquadOrder(retreatOrder);
    }
    //regroup only after retreat
    else if (mainAttackSquad.needsToRegroup())
    {
        SquadOrder regroupOrder(SquadOrderTypes::Regroup, getMainAttackLocation(), DefaultOrderRadius, "Regroup");
        mainAttackSquad.setSquadOrder(regroupOrder);
    }
    else
    {
        SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), MainAttackOrderRadius, "Attack");
        mainAttackSquad.setSquadOrder(mainAttackOrder);
    }
}

void CombatCommander::updateScoutDefenseSquad()
{
    // if the current squad has units in it then we can ignore this
    Squad & scoutDefenseSquad = m_squadData.getSquad("ScoutDefense");

    // get the region that our base is located in
    const BaseLocation * myBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
    if (!myBaseLocation)
        return;

    // get all of the enemy units in this region
    std::vector<Unit> enemyUnitsInRegion;
    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
    {
        if (myBaseLocation->containsPosition(unit.getPosition()) && unit.getType().isWorker())
        {
            enemyUnitsInRegion.push_back(unit);
        }
    }

    // if there's an enemy worker in our region then assign someone to chase him
    bool assignScoutDefender = !enemyUnitsInRegion.empty();

    // if our current squad is empty and we should assign a worker, do it
    if (assignScoutDefender)
    {
		if (!scoutDefenseSquad.isEmpty())
		{
			auto & units = scoutDefenseSquad.getUnits();
			for (auto & unit : units)
			{
				if (unit.getUnitPtr()->health < unit.getUnitPtr()->health_max * m_bot.Workers().MIN_HP_PERCENTAGE_TO_FIGHT)
				{
					m_bot.Workers().finishedWithWorker(unit);
					scoutDefenseSquad.removeUnit(unit);
				}
			}
		}

		if(scoutDefenseSquad.isEmpty())
		{
			// the enemy worker that is attacking us
			Unit enemyWorkerUnit = *enemyUnitsInRegion.begin();
			BOT_ASSERT(enemyWorkerUnit.isValid(), "null enemy worker unit");

			if (enemyWorkerUnit.isValid())
			{
				Unit workerDefender = findWorkerToAssignToSquad(scoutDefenseSquad, enemyWorkerUnit.getPosition(), enemyWorkerUnit.getPosition());
				if (workerDefender.isValid())
				{
					m_squadData.assignUnitToSquad(workerDefender, scoutDefenseSquad);
				}
			}
		}
    }
    // if our squad is not empty and we shouldn't have a worker chasing then take him out of the squad
    else if (!scoutDefenseSquad.isEmpty())
    {
        for (auto & unit : scoutDefenseSquad.getUnits())
        {
            BOT_ASSERT(unit.isValid(), "null unit in scoutDefenseSquad");

            unit.stop();
            if (unit.getType().isWorker())
            {
                m_bot.Workers().finishedWithWorker(unit);
            }
        }

        scoutDefenseSquad.clear();
    }
}

void CombatCommander::updateDefenseSquads()
{
	bool workerRushed = false;
    // for each of our occupied regions
    const BaseLocation * enemyBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
    for (const BaseLocation * myBaseLocation : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
    {
        // don't defend inside the enemy region, this will end badly when we are stealing gas or cannon rushing
        if (myBaseLocation == enemyBaseLocation)
        {
            continue;
        }

        CCPosition basePosition = Util::GetPosition(myBaseLocation->getDepotPosition());

        // start off assuming all enemy units in region are just workers
        int numDefendersPerEnemyUnit = 2; // 2 might be too much if we consider them workers...
        int numDefendersPerEnemyCanon = 4; // This is a minimum

        // all of the enemy units in this region
        std::vector<Unit> enemyUnitsInRegion;
		float minEnemyDistance = 0;
		Unit closestEnemy;
        bool firstWorker = true;
        for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
        {
            // if it's an overlord, don't worry about it for defense, we don't care what they see
            if (unit.getType().isOverlord())
            {
                continue;
            }

            if (myBaseLocation->containsPosition(unit.getPosition()))
            {
                //we can ignore the first enemy worker in our region since we assume it is a scout (handled by scout defense)
                if (unit.getType().isWorker())
                {
					if (firstWorker)
					{
						firstWorker = false;
						continue;
					}
					workerRushed = true;
                }

				float enemyDistance = Util::Dist(unit.getPosition(), basePosition);
				if(!closestEnemy.isValid() || enemyDistance < minEnemyDistance)
				{
					minEnemyDistance = enemyDistance;
					closestEnemy = unit;
				}
                enemyUnitsInRegion.push_back(unit);
            }
        }

        // calculate how many units are flying / ground units
        int numEnemyFlyingInRegion = 0;
        int numEnemyGroundInRegion = 0;
        int numForCanonRush = 0;
        for (auto & unit : enemyUnitsInRegion)
        {
            BOT_ASSERT(unit.isValid(), "null enemy unit in region");

            if (unit.isFlying())
            {
                numEnemyFlyingInRegion++;
            }
            else
            {
                // Canon rush are dangerous
                if (unit.getType().isAttackingBuilding())
                {
                    numForCanonRush++;
                }
                else
                {
                    numEnemyGroundInRegion++;
                }
            }
        }

        std::stringstream squadName;
        squadName << "Base Defense " << basePosition.x << " " << basePosition.y;

        // if there's nothing in this region to worry about
        if (enemyUnitsInRegion.empty())
        {
            // if a defense squad for this region exists, remove it
            if (m_squadData.squadExists(squadName.str()))
            {
                m_squadData.getSquad(squadName.str()).clear();
            }

			if (Util::IsTerran(m_bot.GetPlayerRace(Players::Self)))
			{
				Unit base = m_bot.Buildings().getClosestResourceDepot(basePosition);
				if (base.isValid())
				{
					if (base.isFlying())
					{
						Micro::SmartAbility(base.getUnitPtr(), sc2::ABILITY_ID::LAND, basePosition, m_bot);
					}
					else if (base.getUnitPtr()->cargo_space_taken > 0)
					{
						Micro::SmartAbility(base.getUnitPtr(), sc2::ABILITY_ID::UNLOADALL, m_bot);

						//Remove builder and gas jobs.
						for (auto worker : m_bot.Workers().getWorkers())
						{
							if (m_bot.Workers().getWorkerData().getWorkerJob(worker) != WorkerJobs::Scout)
							{
								m_bot.Workers().finishedWithWorker(worker);
							}
						}
					}
				}
			}

            // and return, nothing to defend here
            continue;
        }
        else
        {
            // if we don't have a squad assigned to this region already, create one
            if (!m_squadData.squadExists(squadName.str()))
            {
                SquadOrder defendRegion(SquadOrderTypes::Defend, basePosition, DefaultOrderRadius, "Defend Region!");
                m_squadData.addSquad(squadName.str(), Squad(squadName.str(), defendRegion, BaseDefensePriority, m_bot));
            }
        }

        // assign units to the squad
        if (m_squadData.squadExists(squadName.str()))
        {
            Squad & defenseSquad = m_squadData.getSquad(squadName.str());

            // figure out how many units we need on defense
            int flyingDefendersNeeded = numDefendersPerEnemyUnit * numEnemyFlyingInRegion;
            int groundDefensersNeeded = numDefendersPerEnemyUnit * numEnemyGroundInRegion + numForCanonRush * numDefendersPerEnemyCanon;

            updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefensersNeeded, closestEnemy.getPosition());
        }
        else
        {
            BOT_ASSERT(false, "Squad should have existed: %s", squadName.str().c_str());
        }

		// Hide our last SCVs
		if(Util::IsTerran(m_bot.GetPlayerRace(Players::Self)) && m_bot.Workers().getNumWorkers() <= 7)//Should be 5, but is higher because some workers will end up dying on the way.
		{
			Unit base = m_bot.Buildings().getClosestResourceDepot(basePosition);
			if (base.isValid())
			{
				if(base.getUnitPtr()->cargo_space_taken == 0)
					Micro::SmartAbility(base.getUnitPtr(), sc2::ABILITY_ID::LOADALL, m_bot);
				else if(!base.isFlying() && base.getUnitPtr()->health < base.getUnitPtr()->health_max * 0.4f)
					Micro::SmartAbility(base.getUnitPtr(), sc2::ABILITY_ID::LIFT, m_bot);
			}
		}
    }

	m_bot.Strategy().setIsWorkerRushed(workerRushed);

    // for each of our defense squads, if there aren't any enemy units near the position, clear the squad
    std::set<std::string> uselessDefenseSquads;
    for (const auto & kv : m_squadData.getSquads())
    {
        const Squad & squad = kv.second;
        const SquadOrder & order = squad.getSquadOrder();

        if (order.getType() != SquadOrderTypes::Defend)
        {
            continue;
        }

        bool enemyUnitInRange = false;
        for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
        {
            if (Util::Dist(unit, order.getPosition()) < order.getRadius())
            {
                enemyUnitInRange = true;
                break;
            }
        }

        if (!enemyUnitInRange)
        {
            m_squadData.getSquad(squad.getName()).clear();
        }
    }
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, const CCPosition & closestEnemyPosition)
{
    auto & squadUnits = defenseSquad.getUnits();

    // TODO: right now this will assign arbitrary defenders, change this so that we make sure they can attack air/ground

    // if there's nothing left to defend, clear the squad
    if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
    {
        defenseSquad.clear();
        return;
    }

    size_t defendersNeeded = flyingDefendersNeeded + groundDefendersNeeded;
    for (auto & unit : squadUnits)
    {
		// Let injured worker return mining, no need to sacrifice it
		if (unit.getType().isWorker() && unit.getUnitPtr()->health < unit.getUnitPtr()->health_max * m_bot.Workers().MIN_HP_PERCENTAGE_TO_FIGHT)
		{
			m_bot.Workers().finishedWithWorker(unit);
			defenseSquad.removeUnit(unit);
		}
        else if (unit.isAlive())
        {
            defendersNeeded--;
        }
    }

    size_t defendersAdded = 0;
    while (defendersNeeded > defendersAdded)
    {
        Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemyPosition);

        if (defenderToAdd.isValid())
        {
            m_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
            defendersAdded++;
        }
        else
        {
            break;
        }
    }
}

Unit CombatCommander::findClosestDefender(const Squad & defenseSquad, const CCPosition & pos, const CCPosition & closestEnemyPosition)
{
    Unit closestDefender;
    float minDistance = std::numeric_limits<float>::max();

    // TODO: add back special case of zergling rush defense

    for (auto & unit : m_combatUnits)
    {
        BOT_ASSERT(unit.isValid(), "null combat unit");

        if (!m_squadData.canAssignUnitToSquad(unit, defenseSquad))
        {
            continue;
        }

        float dist = Util::Dist(unit, pos);
        Squad *unitSquad = m_squadData.getUnitSquad(unit);
        if (unitSquad && (unitSquad->getName() == "MainAttack" || unitSquad->getName() == "Harass") && Util::Dist(unit.getPosition(), unitSquad->getSquadOrder().getPosition()) < dist)
        {
            //We do not want to bring back the main attackers when they are closer to their objective than our base
            continue;
        }

        if (!closestDefender.isValid() || (dist < minDistance))
        {
            closestDefender = unit;
            minDistance = dist;
        }
    }

    if (!closestDefender.isValid())
    {
        // we search for worker to defend.
        closestDefender = findWorkerToAssignToSquad(defenseSquad, pos, closestEnemyPosition);
    }

    return closestDefender;
}

Unit CombatCommander::findWorkerToAssignToSquad(const Squad & defenseSquad, const CCPosition & pos, const CCPosition & closestEnemyPosition)
{
    // get our worker unit that is mining that is closest to it
    Unit workerDefender = m_bot.Workers().getClosestMineralWorkerTo(pos, m_bot.Workers().MIN_HP_PERCENTAGE_TO_FIGHT);

    if (workerDefender.isValid())
    {
        // grab it from the worker manager and put it in the squad
        if (m_squadData.canAssignUnitToSquad(workerDefender, defenseSquad) && Util::Dist(workerDefender, closestEnemyPosition) < 8.f)
        {
            m_bot.Workers().setCombatWorker(workerDefender);
        }
        else
        {
            workerDefender = {};
        }
    }
    return workerDefender;
}

void CombatCommander::drawSquadInformation()
{
    m_squadData.drawSquadInformation();
}

CCPosition CombatCommander::getMainAttackLocation()
{
    const BaseLocation * enemyBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);

    // First choice: Attack an enemy region if we can see units inside it
    if (enemyBaseLocation)
    {
        CCPosition enemyBasePosition = enemyBaseLocation->getPosition();

        // If the enemy base hasn't been seen yet, go there.
        if (!m_bot.Map().isExplored(enemyBasePosition))
        {
            return enemyBasePosition;
        }
        else
        {
            // if it has been explored, go there if there are any visible enemy units there
            for (auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
            {
                if (enemyUnit.getType().isBuilding() && Util::Dist(enemyUnit, enemyBasePosition) < 15)
                {
                    return enemyBasePosition;
                }
            }
        }
    }

    // Second choice: Attack known enemy buildings
    for (const auto & kv : m_bot.UnitInfo().getUnitInfoMap(Players::Enemy))
    {
        const UnitInfo & ui = kv.second;

        if (m_bot.Data(ui.type).isBuilding && ui.lastHealth > 0.0f && ui.unit.isAlive() && !(ui.lastPosition.x == 0.0f && ui.lastPosition.y == 0.0f))
        {
			CCPosition mainAttackSquadCenter = m_squadData.getSquad("MainAttack").calcCenter();
			if(Util::Dist(mainAttackSquadCenter, ui.lastPosition) > 3)
				return ui.lastPosition;
        }
    }

    // Third choice: Attack visible enemy units that aren't overlords
    for (auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
    {
        if (!enemyUnit.getType().isOverlord())
        {
            return enemyUnit.getPosition();
        }
    }

    // Fourth choice: We can't see anything so explore the map attacking along the way
	return exploreMap();
}

CCPosition CombatCommander::exploreMap()
{
	CCPosition basePosition = Util::GetPosition(m_bot.Bases().getBasePosition(Players::Enemy, m_currentBaseExplorationIndex));
	for (auto & unit : m_combatUnits)
	{
		if (Util::Dist(unit.getPosition(), basePosition) < 3.f)
		{
			m_currentBaseExplorationIndex = m_currentBaseExplorationIndex + 1 % m_bot.Bases().getBaseLocations().size();
			return Util::GetPosition(m_bot.Bases().getBasePosition(Players::Enemy, m_currentBaseExplorationIndex));
		}
	}
	return basePosition;
}