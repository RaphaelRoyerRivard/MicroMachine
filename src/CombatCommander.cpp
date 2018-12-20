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
    SquadOrder idleOrder(SquadOrderTypes::Idle, CCPosition(), DefaultOrderRadius, "Prepare for battle");
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
	// the -5 is to prevent enemy workers (during worker rush) to get outside the base defense range
    SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, m_bot.GetStartLocation(), DefaultOrderRadius - 5, "Chase scout");
    m_squadData.addSquad("ScoutDefense", Squad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority, m_bot));

	initInfluenceMaps();
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

	m_bot.StartProfiling("0.10.4.0    updateInfluenceMaps");
	updateInfluenceMaps();
	m_bot.StopProfiling("0.10.4.0    updateInfluenceMaps");

	m_bot.StartProfiling("0.10.4.1    m_squadData.onFrame");
    m_squadData.onFrame();
	m_bot.StopProfiling("0.10.4.1    m_squadData.onFrame");

	m_bot.StartProfiling("0.10.4.2    updateSquads");
    if (isSquadUpdateFrame())
    {
        updateIdleSquad();
        updateScoutDefenseSquad();
        updateDefenseSquads();
		updateHarassSquads();
		updateAttackSquads();
        updateBackupSquads();
    }
	m_bot.StopProfiling("0.10.4.2    updateSquads");

	m_bot.StartProfiling("0.10.4.3    lowPriorityCheck");
	lowPriorityCheck();
	m_bot.StopProfiling("0.10.4.3    lowPriorityCheck");
	/*m_bot.StartProfiling("0.10.4.4    checkUnitsState");
	checkUnitsState();
	m_bot.StopProfiling("0.10.4.4    checkUnitsState");*/
}

void CombatCommander::lowPriorityCheck()
{
	auto frame = m_bot.GetGameLoop();
	if (frame % 5)
	{
		return;
	}

	std::vector<Unit> toRemove;
	for (auto sighting : m_invisibleSighting)
	{
		if (frame + FRAME_BEFORE_SIGHTING_INVALIDATED < sighting.second.second)
		{
			toRemove.push_back(sighting.first);
		}
	}
	for (auto unit : toRemove)
	{
		m_invisibleSighting.erase(unit);
	}
}

bool CombatCommander::shouldWeStartAttacking()
{
    return m_bot.Strategy().getCurrentStrategy().m_attackCondition.eval();
}

void CombatCommander::initInfluenceMaps()
{
	const size_t mapWidth = m_bot.Map().width();
	const size_t mapHeight = m_bot.Map().height();
	m_groundInfluenceMap.resize(mapWidth);
	m_airInfluenceMap.resize(mapWidth);
	for(size_t x = 0; x < mapWidth; ++x)
	{
		auto& groundInfluenceMapRow = m_groundInfluenceMap[x];
		auto& airInfluenceMapRow = m_airInfluenceMap[x];
		groundInfluenceMapRow.resize(mapHeight);
		airInfluenceMapRow.resize(mapHeight);
		for (size_t y = 0; y < mapHeight; ++y)
		{
			groundInfluenceMapRow[y] = 0;
			airInfluenceMapRow[y] = 0;
		}
	}
}

void CombatCommander::resetInfluenceMaps()
{
	const size_t mapWidth = m_bot.Map().width();
	const size_t mapHeight = m_bot.Map().height();
	for (size_t x = 0; x < mapWidth; ++x)
	{
		auto& groundInfluenceMapRow = m_groundInfluenceMap[x];
		auto& airInfluenceMapRow = m_airInfluenceMap[x];
		for (size_t y = 0; y < mapHeight; ++y)
		{
			groundInfluenceMapRow[y] = 0;
			airInfluenceMapRow[y] = 0;
		}
	}
}

void CombatCommander::updateInfluenceMaps()
{
	m_bot.StartProfiling("0.10.4.0.1      resetInfluenceMaps");
	resetInfluenceMaps();
	m_bot.StopProfiling("0.10.4.0.1      resetInfluenceMaps");
	m_bot.StartProfiling("0.10.4.0.2      updateInfluenceMapsWithUnits");
	updateInfluenceMapsWithUnits();
	m_bot.StopProfiling("0.10.4.0.2      updateInfluenceMapsWithUnits");
	m_bot.StartProfiling("0.10.4.0.3      updateInfluenceMapsWithEffects");
	updateInfluenceMapsWithEffects();
	m_bot.StopProfiling("0.10.4.0.3      updateInfluenceMapsWithEffects");
	if (m_bot.Config().DrawInfluenceMaps)
	{
		m_bot.StartProfiling("0.10.4.0.4      drawInfluenceMaps");
		drawInfluenceMaps();
	}
	m_bot.StopProfiling("0.10.4.0.4      drawInfluenceMaps");
}

void CombatCommander::updateInfluenceMapsWithUnits()
{
	for (auto& enemyUnit : m_bot.GetKnownEnemyUnits())
	{
		auto& enemyUnitType = enemyUnit.getType();
		if (enemyUnitType.isCombatUnit() || enemyUnitType.isWorker() || (enemyUnitType.isAttackingBuilding() && enemyUnit.getUnitPtr()->build_progress >= 1.f))
		{
			updateGroundInfluenceMapForUnit(enemyUnit);
			updateAirInfluenceMapForUnit(enemyUnit);
		}
		else
		{
			//TODO ground units cannot move through non flying buildings
		}
	}
}

void CombatCommander::updateInfluenceMapsWithEffects()
{
	auto & effectDataVector = m_bot.Observation()->GetEffectData();
	for (auto & effect : m_bot.Observation()->GetEffects())
	{
		float radius, dps;
		sc2::Weapon::TargetType targetType;
		auto & effectData = effectDataVector[effect.effect_id];
		switch(effect.effect_id)
		{
			case 0: // Nothing
				continue;
			case 1:	// Psi Storm
				radius = effectData.radius;
				dps = 28.07f;	// 80 dmg over 2.85 sec
				targetType = sc2::Weapon::TargetType::Any;
				break;
			case 2:	// Guardian Shield
				radius = effectData.radius;
				//TODO consider it in power calculation
				continue;
			case 3:	// Temporal Field Growing (doesn't exist anymore)
			case 4:	// Temporal Field (doesn't exist anymore)
				continue;
			case 5:	// Thermal Lance (Colossus beams)
				radius = effectData.radius;
				dps = 18.7f;
				targetType = sc2::Weapon::TargetType::Ground;
				break;
			case 6:	// Scanner Sweep
				continue;
			case 7: // Nuke Dot
				radius = 8.f;
				dps = 300.f;	// 300 dmg one shot
				targetType = sc2::Weapon::TargetType::Any;
				break;
			case 8: // Liberator Defender Zone Setup
			case 9: // Liberator Defender Zone
				radius = effectData.radius;
				dps = 65.8f;
				targetType = sc2::Weapon::TargetType::Ground;
				break;
			case 10: // Blinding Cloud
				radius = effectData.radius;
				dps = 25.f;		// this effect does no damage, but we still want to go avoid it so we set a high dps
				targetType = sc2::Weapon::TargetType::Ground;
				break;
			case 11: // Corrosive Bile
				radius = effectData.radius;
				dps = 60.f;		// 60 dmg one shot
				targetType = sc2::Weapon::TargetType::Any;
				break;
			case 12: // Lurker Spines
				radius = effectData.radius;
				dps = 20.f;		// 20 dmg one shot
				targetType = sc2::Weapon::TargetType::Ground;
				break;
			default:
				continue;
			//TODO The following effects are not part of the list and should be managed elsewhere if possible
			/*case static_cast<const unsigned>(sc2::ABILITY_ID::EFFECT_PARASITICBOMB) :
				radius = 3.f;
				dps = 17.14f;	// 120 dmg over 7 sec
				targetType = sc2::Weapon::TargetType::Air;
				break;
			case static_cast<const unsigned>(sc2::ABILITY_ID::EFFECT_PURIFICATIONNOVA) :
				radius = 1.5f;
				dps = 145.f;	// 145 dmg on shot
				targetType = sc2::Weapon::TargetType::Ground;
				break;
			case static_cast<const unsigned>(sc2::ABILITY_ID::EFFECT_TIMEWARP) :
				radius = 3.5f;
				dps = 25.f;		// this effect does no damage, but we still want to go avoid it so we set a high dps
				targetType = sc2::Weapon::TargetType::Ground;
				break;
			case static_cast<const unsigned>(sc2::ABILITY_ID::EFFECT_WIDOWMINEATTACK) :
				radius = 1.5f;
				dps = 40.f;
				targetType = sc2::Weapon::TargetType::Any;
				break;*/
		}
		for(auto & pos : effect.positions)
		{
			if (targetType == sc2::Weapon::TargetType::Any || targetType == sc2::Weapon::TargetType::Air)
				updateInfluenceMap(dps, radius, 1.f, pos, false);
			if (targetType == sc2::Weapon::TargetType::Any || targetType == sc2::Weapon::TargetType::Ground)
				updateInfluenceMap(dps, radius, 1.f, pos, true);
		}
	}
}

void CombatCommander::updateGroundInfluenceMapForUnit(const Unit& enemyUnit)
{
	updateInfluenceMapForUnit(enemyUnit, true);
}

void CombatCommander::updateAirInfluenceMapForUnit(const Unit& enemyUnit)
{
	updateInfluenceMapForUnit(enemyUnit, false);
}

void CombatCommander::updateInfluenceMapForUnit(const Unit& enemyUnit, const bool ground)
{
	const float dps = ground ? Util::GetGroundDps(enemyUnit.getUnitPtr(), m_bot) : Util::GetAirDps(enemyUnit.getUnitPtr(), m_bot);
	if (dps == 0.f)
		return;
	const float range = ground ? Util::GetGroundAttackRange(enemyUnit.getUnitPtr(), m_bot) : Util::GetAirAttackRange(enemyUnit.getUnitPtr(), m_bot);
	if (range == 0.f)
		return;
	const float speed = std::max(1.f, Util::getSpeedOfUnit(enemyUnit.getUnitPtr(), m_bot));
	updateInfluenceMap(dps, range, speed, enemyUnit.getPosition(), ground);
}

void CombatCommander::updateInfluenceMap(const float dps, const float range, const float speed, const CCPosition & position, const bool ground)
{
	const float totalRange = range + speed;

	const float fminX = floor(position.x - totalRange);
	const float fmaxX = ceil(position.x + totalRange);
	const float fminY = floor(position.y - totalRange);
	const float fmaxY = ceil(position.y + totalRange);
	const float maxMapX = m_bot.Map().width() - 1.f;
	const float maxMapY = m_bot.Map().height() - 1.f;
	const int minX = std::max(0.f, fminX);
	const int maxX = std::min(maxMapX, fmaxX);
	const int minY = std::max(0.f, fminY);
	const int maxY = std::min(maxMapY, fmaxY);
	auto& influenceMap = ground ? m_groundInfluenceMap : m_airInfluenceMap;
	//loop for a square of size equal to the diameter of the influence circle
	for (int x = minX; x < maxX; ++x)
	{
		for (int y = minY; y < maxY; ++y)
		{
			const float distance = Util::Dist(position, CCPosition(x + 0.5f, y + 0.5f));
			float multiplier = 1.f;
			if (distance > range)
				multiplier = std::max(0.f, (speed - (distance - range)) / speed);	//value is linearly interpolated in the speed buffer zone
			influenceMap[x][y] += dps * multiplier;
		}
	}
}

void CombatCommander::drawInfluenceMaps()
{
	const size_t mapWidth = m_bot.Map().width();
	const size_t mapHeight = m_bot.Map().height();
	for (size_t x = 0; x < mapWidth; ++x)
	{
		auto& groundInfluenceMapRow = m_groundInfluenceMap[x];
		auto& airInfluenceMapRow = m_airInfluenceMap[x];
		for (size_t y = 0; y < mapHeight; ++y)
		{
			if (groundInfluenceMapRow[y] > 0.f)
				m_bot.Map().drawTile(x, y, CCColor(255, 255 - std::min(255.f, std::max(0.f, groundInfluenceMapRow[y] * 5)), 0));
			if (airInfluenceMapRow[y] > 0.f)
				m_bot.Map().drawCircle(CCPosition(x + 0.5f, y + 0.5f), 0.5f, CCColor(255, 255 - std::min(255.f, std::max(0.f, airInfluenceMapRow[y] * 5)), 0));
		}
	}
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
		const BaseLocation * nextExpansion = m_bot.Bases().getNextExpansion(Players::Self);

		CCPosition idlePosition = CCPosition(nextExpansion->getDepotPosition().x, nextExpansion->getDepotPosition().y);
		idlePosition.x += nextExpansion->getDepotPosition().x - nextExpansion->getCenterOfMinerals().x;
		idlePosition.y += nextExpansion->getDepotPosition().y - nextExpansion->getCenterOfMinerals().y;

		SquadOrder idleOrder(SquadOrderTypes::Attack, idlePosition, DefaultOrderRadius, "Prepare for battle");
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
	std::vector<Unit*> idleHellions;
	std::vector<Unit*> idleBanshees;
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
			if (unit.getType().getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_HELLION)
				idleHellions.push_back(&unit);
			/*else if (unit.getType().getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_BANSHEE)
				idleBanshees.push_back(&unit);*/
			else
				m_squadData.assignUnitToSquad(unit, harassSquad);
		}
	}
	if(idleHellions.size() >= 10)
	{
		for (auto hellion : idleHellions)
		{
			m_squadData.assignUnitToSquad(*hellion, harassSquad);
		}
	}
	/*if (idleBanshees.size() >= 3)
	{
		for (auto banshee : idleBanshees)
		{
			m_squadData.assignUnitToSquad(*banshee, harassSquad);
		}
	}*/

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
				Unit workerDefender = findWorkerToAssignToSquad(scoutDefenseSquad, enemyWorkerUnit.getPosition(), enemyWorkerUnit);
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
		int numDefendersPerEnemyResourceDepot = 6; // This is a minimum
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

				const float enemyDistance = Util::DistSq(unit.getPosition(), basePosition);
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
        for (auto & unit : enemyUnitsInRegion)
        {
            BOT_ASSERT(unit.isValid(), "null enemy unit in region");

            if (unit.isFlying())
            {
                numEnemyFlyingInRegion++;
            }
            else
            {
				// Canon rush dangerous
				if (unit.getType().isAttackingBuilding())
				{
					numEnemyGroundInRegion += numDefendersPerEnemyCanon;
				}
				// Hatcheries are tanky
				else if (unit.getType().isResourceDepot())
				{
					numEnemyGroundInRegion += numDefendersPerEnemyResourceDepot;
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

			if (Util::IsTerran(m_bot.GetSelfRace()))
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
				//SquadOrder defendRegion(SquadOrderTypes::Defend, basePosition, DefaultOrderRadius, "Defend Region!");
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
            int groundDefensersNeeded = numDefendersPerEnemyUnit * numEnemyGroundInRegion;

            updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefensersNeeded, closestEnemy);
        }
        else
        {
            BOT_ASSERT(false, "Squad should have existed: %s", squadName.str().c_str());
        }

		//Protect our SCVs and lift our base
		if(Util::IsTerran(m_bot.GetSelfRace()))
		{
			Unit base = m_bot.Buildings().getClosestResourceDepot(basePosition);
			if (base.isValid())
			{
				if(base.getUnitPtr()->cargo_space_taken == 0 && m_bot.Workers().getNumWorkers() > 0)
				{
					// Hide our last SCVs (should be 5, but is higher because some workers may end up dying on the way)
					if(m_bot.Workers().getNumWorkers() <= 7)
						Micro::SmartAbility(base.getUnitPtr(), sc2::ABILITY_ID::LOADALL, m_bot);
				}
				else if(!base.isFlying() && base.getUnitPtr()->health < base.getUnitPtr()->health_max * 0.5f)
					Micro::SmartAbility(base.getUnitPtr(), sc2::ABILITY_ID::LIFT, m_bot);	//TODO do not lift if we actually have no combat unit nor in production (otherwise it's BM)
			}
		}
    }

	m_bot.Strategy().setIsWorkerRushed(workerRushed);

    // for each of our defense squads, if there aren't any enemy units near the position, clear the squad
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
            if (Util::DistSq(unit, order.getPosition()) < order.getRadius() * order.getRadius())
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

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, size_t flyingDefendersNeeded, size_t groundDefendersNeeded, Unit & closestEnemy)
{
    auto & squadUnits = defenseSquad.getUnits();

    // if there's nothing left to defend, clear the squad
    if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
    {
        defenseSquad.clear();
        return;
    }

    for (auto & unit : squadUnits)
    {
		// Let injured worker return mining, no need to sacrifice it
		if (unit.getType().isWorker())
		{
			if (unit.getUnitPtr()->health < unit.getUnitPtr()->health_max * m_bot.Workers().MIN_HP_PERCENTAGE_TO_FIGHT ||
				!ShouldWorkerDefend(unit, defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemy))
			{
				m_bot.Workers().finishedWithWorker(unit);
				defenseSquad.removeUnit(unit);
			}
		}
        else if (unit.isAlive())
        {
			bool isUseful = (flyingDefendersNeeded > 0 && unit.canAttackAir()) || (groundDefendersNeeded > 0 && unit.canAttackGround());
			if(!isUseful)
				defenseSquad.removeUnit(unit);
        }
    }

	if (flyingDefendersNeeded > 0)
	{
		Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemy, "air");

		while(defenderToAdd.isValid())
		{
			m_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemy, "air");
		}
	}

	if (groundDefendersNeeded > 0)
	{
		Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemy, "ground");

		while (defenderToAdd.isValid())
		{
			m_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemy, "ground");
		}
	}
}

void CombatCommander::checkUnitsState()
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
			UnitState state = UnitState(unit.getHitPoints(), unit.getShields(), unit.getEnergy());
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
			m_bot.StartProfiling("0.10.4.4.2.3        checkForInvis");
			auto& threats = Util::getThreats(unit.getUnitPtr(), m_bot.GetKnownEnemyUnits(), m_bot);
			if (threats.empty() && state.HadRecentTreats())
			{
				//Invisible unit detected
				m_bot.Strategy().setEnemyHasInvisible(true);
				m_invisibleSighting[unit] = std::pair<CCPosition, uint32_t>(unit.getPosition(), m_bot.GetGameLoop());
			}
			m_bot.StopProfiling("0.10.4.4.2.3        checkForInvis");
		}
		else if (m_bot.GetGameLoop() % 5)
		{
			m_bot.StartProfiling("0.10.4.4.2.4        updateThreats");
			auto& threats = Util::getThreats(unit.getUnitPtr(), m_bot.GetKnownEnemyUnits(), m_bot);
			state.UpdateThreat(!threats.empty());
			m_bot.StopProfiling("0.10.4.4.2.4        updateThreats");
		}
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

Unit CombatCommander::findClosestDefender(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy, std::string type)
{
    Unit closestDefender;
    float minDistance = std::numeric_limits<float>::max();

    for (auto & unit : m_combatUnits)
    {
        BOT_ASSERT(unit.isValid(), "null combat unit");

		if (type == "air" && !unit.canAttackAir())
			continue;
		if (type == "ground" && !unit.canAttackGround())
			continue;

        if (!m_squadData.canAssignUnitToSquad(unit, defenseSquad))
        {
            continue;
        }

        const float dist = Util::DistSq(unit, closestEnemy);
        Squad *unitSquad = m_squadData.getUnitSquad(unit);
        if (unitSquad && (unitSquad->getName() == "MainAttack" || unitSquad->getName() == "Harass") && Util::DistSq(unit.getPosition(), unitSquad->getSquadOrder().getPosition()) < dist)
        {
            //We do not want to bring back the main attackers when they are closer to their objective than our base
            continue;
        }

        if (!closestDefender.isValid() || dist < minDistance)
        {
            closestDefender = unit;
            minDistance = dist;
        }
    }

    if (!closestDefender.isValid() && type == "ground")
    {
        // we search for worker to defend.
        closestDefender = findWorkerToAssignToSquad(defenseSquad, pos, closestEnemy);
    }

    return closestDefender;
}

Unit CombatCommander::findWorkerToAssignToSquad(const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy)
{
    // get our worker unit that is mining that is closest to it
    Unit workerDefender = m_bot.Workers().getClosestMineralWorkerTo(closestEnemy.getPosition(), m_bot.Workers().MIN_HP_PERCENTAGE_TO_FIGHT);

	if(ShouldWorkerDefend(workerDefender, defenseSquad, pos, closestEnemy))
	{
        m_bot.Workers().setCombatWorker(workerDefender);
    }
    else
    {
        workerDefender = {};
    }
    return workerDefender;
}

bool CombatCommander::ShouldWorkerDefend(const Unit & woker, const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy)
{
	// grab it from the worker manager and put it in the squad, but only if it meets the distance requirements
	if (woker.isValid() && 
		m_squadData.canAssignUnitToSquad(woker, defenseSquad) &&
		!closestEnemy.isFlying() &&
		Util::DistSq(woker, pos) < 15.f * 15.f &&	// worker should not get too far from base
		(Util::DistSq(woker, closestEnemy) < 7.f * 7.f ||	// worker can fight only units close to it
		(closestEnemy.getType().isBuilding() && Util::DistSq(closestEnemy, pos) < 12.f * 12.f)))	// worker can fight buildings somewhat close to the base
		return true;
	return false;
}

std::map<Unit, std::pair<CCPosition, uint32_t>> & CombatCommander::GetInvisibleSighting()
{
	return m_invisibleSighting;
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
			if (m_bot.GetCurrentFrame() % 25 == 0)
				std::cout << m_bot.GetCurrentFrame() << ": Unexplored enemy base" << std::endl;
            return enemyBasePosition;
        }
        else
        {
            // if it has been explored, go there if there are any visible enemy units there
            for (auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
            {
                if (enemyUnit.getType().isBuilding() && Util::Dist(enemyUnit, enemyBasePosition) < 15)
                {
					if (m_bot.GetCurrentFrame() % 25 == 0)
						std::cout << m_bot.GetCurrentFrame() << ": Visible enemy building" << std::endl;
                    return enemyBasePosition;
                }
            }
        }
    }

	if(!m_bot.Strategy().shouldFocusBuildings())
	{
		m_bot.Actions()->SendChat("Looks like you lost your main base, time to conceed? :)");
		m_bot.Actions()->SendChat("Entering Derp mode... sorry for the glitches to come");
		m_bot.Strategy().setFocusBuildings(true);
	}

	CCPosition harassSquadCenter = m_squadData.getSquad("Harass").calcCenter();
	float lowestDistance = -1.f;
	CCPosition closestEnemyPosition;

    // Second choice: Attack known enemy buildings
	Squad& harassSquad = m_squadData.getSquad("Harass");
    for (const auto & enemyUnit : harassSquad.getTargets())
    {
        if (enemyUnit.getType().isBuilding() && enemyUnit.isAlive() && enemyUnit.getUnitPtr()->display_type != sc2::Unit::Hidden)
        {
			if (enemyUnit.getType().isCreepTumor())
				continue;
			float dist = Util::DistSq(enemyUnit, harassSquadCenter);
			if(lowestDistance < 0 || dist < lowestDistance)
			{
				lowestDistance = dist;
				closestEnemyPosition = enemyUnit.getPosition();
			}
        }
    }
	if (lowestDistance >= 0.f)
	{
		if (m_bot.GetCurrentFrame() % 25 == 0)
			std::cout << m_bot.GetCurrentFrame() << ": Memory enemy building" << std::endl;
		return closestEnemyPosition;
	}

    // Third choice: Attack visible enemy units that aren't overlords
	for (const auto & enemyUnit : harassSquad.getTargets())
	{
        if (!enemyUnit.getType().isOverlord() && enemyUnit.isAlive() && enemyUnit.getUnitPtr()->display_type != sc2::Unit::Hidden)
        {
			if (enemyUnit.getType().isCreepTumor())
				continue;
			float dist = Util::DistSq(enemyUnit, harassSquadCenter);
			if (lowestDistance < 0 || dist < lowestDistance)
			{
				lowestDistance = dist;
				closestEnemyPosition = enemyUnit.getPosition();
			}
        }
    }
	if (lowestDistance >= 0.f)
	{
		if (m_bot.GetCurrentFrame() % 25 == 0)
			std::cout << m_bot.GetCurrentFrame() << ": Memory enemy unit" << std::endl;
		return closestEnemyPosition;
	}

    // Fourth choice: We can't see anything so explore the map attacking along the way
	return exploreMap();
}

CCPosition CombatCommander::exploreMap()
{
	CCPosition basePosition = Util::GetPosition(m_bot.Bases().getBasePosition(Players::Enemy, m_currentBaseExplorationIndex));
	for (auto & unit : m_combatUnits)
	{
		if (Util::DistSq(unit.getPosition(), basePosition) < 3.f * 3.f)
		{
			m_currentBaseExplorationIndex = (m_currentBaseExplorationIndex + 1) % m_bot.Bases().getBaseLocations().size();
			if (m_bot.GetCurrentFrame() % 25 == 0)
				std::cout << m_bot.GetCurrentFrame() << ": Explore map, base index increased to " << m_currentBaseExplorationIndex << std::endl;
			return Util::GetPosition(m_bot.Bases().getBasePosition(Players::Enemy, m_currentBaseExplorationIndex));
		}
	}
	if (m_bot.GetCurrentFrame() % 25 == 0)
		std::cout << m_bot.GetCurrentFrame() << ": Explore map, base index " << m_currentBaseExplorationIndex << std::endl;
	return basePosition;
}