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

const size_t BLOCKED_TILES_UPDATE_FREQUENCY = 24;

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
		m_bot.StartProfiling("0.10.4.2.1    updateDefenseBuildings");
		updateDefenseBuildings();
		m_bot.StopProfiling("0.10.4.2.1    updateDefenseBuildings");
		m_bot.StartProfiling("0.10.4.2.2    updateDefenseSquads");
        updateDefenseSquads();
		m_bot.StopProfiling("0.10.4.2.2    updateDefenseSquads");
		updateHarassSquads();
		updateAttackSquads();
        updateBackupSquads();
    }
	m_bot.StopProfiling("0.10.4.2    updateSquads");

	m_bot.StartProfiling("0.10.4.3    lowPriorityCheck");
	lowPriorityCheck();
	m_bot.StopProfiling("0.10.4.3    lowPriorityCheck");
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
	const size_t mapWidth = m_bot.Map().totalWidth();
	const size_t mapHeight = m_bot.Map().totalHeight();
	m_groundInfluenceMap.resize(mapWidth);
	m_airInfluenceMap.resize(mapWidth);
	m_blockedTiles.resize(mapWidth);
	for(size_t x = 0; x < mapWidth; ++x)
	{
		auto& groundInfluenceMapRow = m_groundInfluenceMap[x];
		auto& airInfluenceMapRow = m_airInfluenceMap[x];
		auto& blockedTilesRow = m_blockedTiles[x];
		groundInfluenceMapRow.resize(mapHeight);
		airInfluenceMapRow.resize(mapHeight);
		blockedTilesRow.resize(mapHeight);
		for (size_t y = 0; y < mapHeight; ++y)
		{
			groundInfluenceMapRow[y] = 0;
			airInfluenceMapRow[y] = 0;
			blockedTilesRow[y] = false;
		}
	}
}

void CombatCommander::resetInfluenceMaps()
{
	const size_t mapWidth = m_bot.Map().totalWidth();
	const size_t mapHeight = m_bot.Map().totalHeight();
	const bool resetBlockedTiles = m_bot.GetGameLoop() % BLOCKED_TILES_UPDATE_FREQUENCY == 0;
	for (size_t x = 0; x < mapWidth; ++x)
	{
		auto& groundInfluenceMapRow = m_groundInfluenceMap[x];
		auto& airInfluenceMapRow = m_airInfluenceMap[x];
		auto& blockedTilesRow = m_blockedTiles[x];
		for (size_t y = 0; y < mapHeight; ++y)
		{
			groundInfluenceMapRow[y] = 0;
			airInfluenceMapRow[y] = 0;
			if(resetBlockedTiles)
				blockedTilesRow[y] = false;
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
		m_bot.StopProfiling("0.10.4.0.4      drawInfluenceMaps");
	}
	if (m_bot.Config().DrawBlockedTiles)
	{
		m_bot.StartProfiling("0.10.4.0.5      drawBlockedTiles");
		drawBlockedTiles();
		m_bot.StopProfiling("0.10.4.0.5      drawBlockedTiles");
	}
}

void CombatCommander::updateInfluenceMapsWithUnits()
{
	const bool updateBlockedTiles = m_bot.GetGameLoop() % BLOCKED_TILES_UPDATE_FREQUENCY == 0;
	for (auto& enemyUnit : m_bot.GetKnownEnemyUnits())
	{
		auto& enemyUnitType = enemyUnit.getType();
		if (enemyUnitType.isCombatUnit() || enemyUnitType.isWorker() || (enemyUnitType.isAttackingBuilding() && enemyUnit.getUnitPtr()->build_progress >= 1.f))
		{
			updateGroundInfluenceMapForUnit(enemyUnit);
			updateAirInfluenceMapForUnit(enemyUnit);
		}
		if(updateBlockedTiles && enemyUnitType.isBuilding() && !enemyUnit.isFlying() && enemyUnit.getUnitPtr()->unit_type != sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED)
		{
			updateBlockedTilesWithUnit(enemyUnit);
		}
	}
	if(updateBlockedTiles)
	{
		for (auto& allyUnitPair : m_bot.GetAllyUnits())
		{
			auto& allyUnit = allyUnitPair.second;
			if (allyUnit.getType().isBuilding() && !allyUnit.isFlying() && allyUnit.getUnitPtr()->unit_type != sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED)
			{
				updateBlockedTilesWithUnit(allyUnit);
			}
		}
		for (auto& neutralUnitPair : m_bot.GetNeutralUnits())
		{
			auto& neutralUnit = neutralUnitPair.second;
			updateBlockedTilesWithUnit(neutralUnit);
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
	const float minMapX = m_bot.Map().mapMin().x;
	const float minMapY = m_bot.Map().mapMin().y;
	const float maxMapX = m_bot.Map().mapMax().x;
	const float maxMapY = m_bot.Map().mapMax().y;
	const int minX = std::max(minMapX, fminX);
	const int maxX = std::min(maxMapX, fmaxX);
	const int minY = std::max(minMapY, fminY);
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

void CombatCommander::updateBlockedTilesWithUnit(const Unit& unit)
{
	const CCTilePosition centerTile = Util::GetTilePosition(unit.getPosition());
	const int size = floor(unit.getUnitPtr()->radius * 2);
	const int flooredHalfSize = floor(size / 2.f);
	const int ceiledHalfSize = ceil(size / 2.f);
	int minX = centerTile.x - flooredHalfSize;
	const int maxX = centerTile.x + ceiledHalfSize;
	int minY = centerTile.y - flooredHalfSize;
	const int maxY = centerTile.y + ceiledHalfSize;

	//special cases
	if (unit.getType().isAttackingBuilding())
	{
		//attacking buildings have a smaller radius, so we must increase the min to cover more tiles
		minX = centerTile.x - ceiledHalfSize;
		minY = centerTile.y - ceiledHalfSize;
	}
	else if (unit.getType().isMineral())
	{
		//minerals are rectangles instead of squares
		minY = centerTile.y;
	}

	for(int x = minX; x < maxX; ++x)
	{
		for(int y = minY; y < maxY; ++y)
		{
			m_blockedTiles[x][y] = true;
		}
	}
}

void CombatCommander::drawInfluenceMaps()
{
	const size_t mapWidth = m_bot.Map().totalWidth();
	const size_t mapHeight = m_bot.Map().totalHeight();
	for (size_t x = 0; x < mapWidth; ++x)
	{
		auto& groundInfluenceMapRow = m_groundInfluenceMap[x];
		auto& airInfluenceMapRow = m_airInfluenceMap[x];
		for (size_t y = 0; y < mapHeight; ++y)
		{
			if (groundInfluenceMapRow[y] > 0.f)
				m_bot.Map().drawTile(x, y, CCColor(255, 255 - std::min(255.f, std::max(0.f, groundInfluenceMapRow[y] * 5)), 0));
			if (airInfluenceMapRow[y] > 0.f)
				m_bot.Map().drawTile(x, y, CCColor(255, 255 - std::min(255.f, std::max(0.f, airInfluenceMapRow[y] * 5)), 0), 0.5f);
		}
	}
}

void CombatCommander::drawBlockedTiles()
{
	const size_t mapWidth = m_bot.Map().totalWidth();
	const size_t mapHeight = m_bot.Map().totalHeight();
	for (size_t x = 0; x < mapWidth; ++x)
	{
		auto& blockedTilesRow = m_blockedTiles[x];
		for (size_t y = 0; y < mapHeight; ++y)
		{
			if (blockedTilesRow[y])
				m_bot.Map().drawTile(x, y, sc2::Colors::Red);
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

		const CCPosition idlePosition = Util::GetPosition(nextExpansion ? nextExpansion->getCenterOfMinerals() : m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getCenterOfMinerals());

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
		const sc2::UnitTypeID unitTypeId = unit.getType().getAPIUnitType();
		if ((unitTypeId == sc2::UNIT_TYPEID::TERRAN_REAPER
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_HELLION
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_BANSHEE)
			&& m_squadData.canAssignUnitToSquad(unit, harassSquad))
		{
			if (unitTypeId == sc2::UNIT_TYPEID::TERRAN_HELLION)
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
			if (enemyUnitsInRegion.size() > 1)
				break;
        }
    }

    // if there's an enemy worker in our region
    if (enemyUnitsInRegion.size() == 1)
    {
		// and there is an injured worker in the squad, remove it
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

		// if our the squad is empty, assign a worker
		if(scoutDefenseSquad.isEmpty())
		{
			// the enemy worker that is attacking us
			Unit enemyWorkerUnit = *enemyUnitsInRegion.begin();
			BOT_ASSERT(enemyWorkerUnit.isValid(), "null enemy worker unit");

			Unit workerDefender = findWorkerToAssignToSquad(scoutDefenseSquad, enemyWorkerUnit.getPosition(), enemyWorkerUnit);
			if (workerDefender.isValid())
			{
				m_squadData.assignUnitToSquad(workerDefender, scoutDefenseSquad);
			}
		}
    }
    // if our squad is not empty and we shouldn't have a worker chasing then take him out of the squad
    else if (!scoutDefenseSquad.isEmpty())
    {
        for (auto & unit : scoutDefenseSquad.getUnits())
        {
            BOT_ASSERT(unit.isValid(), "null unit in scoutDefenseSquad");

            if (unit.getType().isWorker())
            {
                m_bot.Workers().finishedWithWorker(unit);
            }
        }

        scoutDefenseSquad.clear();
    }
}

void CombatCommander::updateDefenseBuildings()
{
	int SUPPLYDEPOT_DISTANCE = 5;

	auto enemies = m_bot.GetEnemyUnits();
	auto buildings = m_bot.Buildings().getFinishedBuildings();
	for (auto building : buildings)
	{
		sc2::UNIT_TYPEID buildingType = building.getType().getAPIUnitType();
		switch (buildingType)
		{
			case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED:
				for (auto enemy : enemies)
				{
					CCTilePosition enemyPosition = enemy.second.getTilePosition();
					CCTilePosition buildingPosition = building.getTilePosition();
					int distance = abs(enemyPosition.x - buildingPosition.x) + abs(enemyPosition.y - buildingPosition.y);
					if (distance < SUPPLYDEPOT_DISTANCE)
					{//Raise
						building.useAbility(sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_RAISE);
						break;
					}
				}
				break;
			case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT:
				{//Extra bracket needed to compile
					bool canLower = true;
					for (auto enemy : enemies)
					{
						CCTilePosition enemyPosition = enemy.second.getTilePosition();
						CCTilePosition buildingPosition = building.getTilePosition();
						int distance = abs(enemyPosition.x - buildingPosition.x) + abs(enemyPosition.y - buildingPosition.y);
						if (distance < SUPPLYDEPOT_DISTANCE)
						{
							canLower = false;
							break;
						}
					}
					if (canLower)
					{
						building.useAbility(sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
					}
				}
				break;
			default:
				break;
		}
	}
}

struct RegionArmyInformation
{
	BaseLocation* baseLocation;
	CCBot& bot;
	std::vector<Unit> enemyUnits;
	std::vector<const sc2::Unit*> affectedAllyUnits;
	std::unordered_map<const sc2::Unit*, float> unitGroundScores;
	std::unordered_map<const sc2::Unit*, float> unitAirScores;
	float airEnemyPower;
	float groundEnemyPower;
	float antiAirAllyPower;
	float antiGroundAllyPower;
	Squad* squad;
	Unit closestEnemyUnit;

	RegionArmyInformation(BaseLocation* baseLocation, CCBot& bot)
		: baseLocation(baseLocation)
		, bot(bot)
		, airEnemyPower(0)
		, groundEnemyPower(0)
		, antiAirAllyPower(0)
		, antiGroundAllyPower(0)
		, squad(nullptr)
		, closestEnemyUnit({})
	{}

	void calcEnemyPower()
	{
		airEnemyPower = 0;
		groundEnemyPower = 0;
		for(auto& unit : enemyUnits)
		{
			if (unit.isFlying())
				airEnemyPower += Util::GetUnitPower(unit.getUnitPtr(), nullptr, bot);
			else
				groundEnemyPower += Util::GetUnitPower(unit.getUnitPtr(), nullptr, bot);
		}
	}

	void calcClosestEnemy()
	{
		float minDist = 0.f;
		for(auto & enemyUnit : enemyUnits)
		{
			const float dist = Util::DistSq(enemyUnit, baseLocation->getPosition());
			if(!closestEnemyUnit.isValid() || dist < minDist)
			{
				minDist = dist;
				closestEnemyUnit = enemyUnit;
			}
		}
	}

	void affectAllyUnit(const sc2::Unit* unit)
	{
		affectedAllyUnits.push_back(unit);
		const float power = Util::GetUnitPower(unit, nullptr, bot);
		const bool canAttackGround = Util::CanUnitAttackGround(unit, bot);
		const bool canAttackAir = Util::CanUnitAttackAir(unit, bot);
		if(canAttackGround)
		{
			if(canAttackAir)
			{
				if(airEnemyPower - antiAirAllyPower > groundEnemyPower - antiGroundAllyPower)
				{
					antiAirAllyPower += power;
				}
				else
				{
					antiGroundAllyPower += power;
				}
			}
			else
			{
				antiGroundAllyPower += power;
			}
		}
		else
		{
			antiAirAllyPower += power;
		}
	}

	float antiGroundPowerNeeded() const
	{
		return groundEnemyPower * 1.5f - antiGroundAllyPower;
	}

	float antiAirPowerNeeded() const
	{
		return airEnemyPower * 1.5f - antiAirAllyPower;
	}

	bool needsMoreAntiGround() const
	{
		return antiGroundPowerNeeded() > 0;
	}

	bool needsMoreAntiAir() const
	{
		return antiAirPowerNeeded() > 0;
	}

	bool needsMoreSupport() const
	{
		return needsMoreAntiGround() || needsMoreAntiAir();
	}

	float getTotalPowerNeeded() const
	{
		return antiGroundPowerNeeded() + antiAirPowerNeeded();
	}

	bool operator<(const RegionArmyInformation& ref) const
	{
		return getTotalPowerNeeded() > ref.getTotalPowerNeeded();	// greater is used to have a decreasing order
	}
};

void CombatCommander::updateDefenseSquads()
{
	// reset defense squads
	for (const auto & kv : m_squadData.getSquads())
	{
		const Squad & squad = kv.second;
		const SquadOrder & order = squad.getSquadOrder();

		if (order.getType() != SquadOrderTypes::Defend || squad.getName() == "ScoutDefense")
		{
			continue;
		}

		m_squadData.getSquad(squad.getName()).clear();
	}

	bool workerRushed = false;
	bool earlyRushed = false;
	// TODO instead of separing by bases, we should separate by clusters
	std::list<RegionArmyInformation> regions;
    // for each of our occupied regions
    const BaseLocation * enemyBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
    for (BaseLocation * myBaseLocation : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
    {
        // don't defend inside the enemy region, this will end badly when we are stealing gas or cannon rushing
        if (myBaseLocation == enemyBaseLocation)
        {
            continue;
        }

		m_bot.StartProfiling("0.10.4.2.2.1      detectEnemiesInRegions");
		auto region = RegionArmyInformation(myBaseLocation, m_bot);

        const CCPosition basePosition = Util::GetPosition(myBaseLocation->getDepotPosition());

		const int numDefendersPerEnemyResourceDepot = 6; // This is a minimum
		const int numDefendersPerEnemyCanon = 4; // This is a minimum

		// calculate how many units are flying / ground units
		int numEnemyFlyingInRegion = 0;
		int numEnemyGroundInRegion = 0;
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
                if (!workerRushed && unit.getType().isWorker())
                {
					if (firstWorker)
					{
						firstWorker = false;
						continue;
					}
					workerRushed = true;
                }
				else if(!earlyRushed && m_bot.GetGameLoop() < 7320)	// first 5 minutes
				{
					earlyRushed = true;
				}

				const float enemyDistance = Util::DistSq(unit.getPosition(), basePosition);
				if(!closestEnemy.isValid() || enemyDistance < minEnemyDistance)
				{
					minEnemyDistance = enemyDistance;
					closestEnemy = unit;
				}

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

				region.enemyUnits.push_back(unit);
            }
        }

		std::stringstream squadName;
		squadName << "Base Defense " << basePosition.x << " " << basePosition.y;

		myBaseLocation->setIsUnderAttack(!region.enemyUnits.empty());
		m_bot.StopProfiling("0.10.4.2.2.1      detectEnemiesInRegions");
		if(region.enemyUnits.empty())
        {
			m_bot.StartProfiling("0.10.4.2.2.3      clearRegion");
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
						for (auto & worker : m_bot.Workers().getWorkers())
						{
							if (m_bot.Workers().getWorkerData().getWorkerJob(worker) != WorkerJobs::Scout)
							{
								m_bot.Workers().finishedWithWorker(worker);
							}
						}
					}
				}
			}
			m_bot.StopProfiling("0.10.4.2.2.3      clearRegion");

            // and return, nothing to defend here
            continue;
        }

		m_bot.StartProfiling("0.10.4.2.2.3      createSquad");
		const SquadOrder defendRegion(SquadOrderTypes::Defend, closestEnemy.getPosition(), DefaultOrderRadius, "Defend Region!");
        // if we don't have a squad assigned to this region already, create one
        if (!m_squadData.squadExists(squadName.str()))
        {
			m_squadData.addSquad(squadName.str(), Squad(squadName.str(), defendRegion, BaseDefensePriority, m_bot));
        }

        // assign units to the squad
        if (m_squadData.squadExists(squadName.str()))
        {
            Squad & defenseSquad = m_squadData.getSquad(squadName.str());
			defenseSquad.setSquadOrder(defendRegion);
			region.squad = &defenseSquad;
        }
        else
        {
            BOT_ASSERT(false, "Squad should have existed: %s", squadName.str().c_str());
        }
		m_bot.StopProfiling("0.10.4.2.2.3      createSquad");

		m_bot.StartProfiling("0.10.4.2.2.4      calculateRegionInformation");
		region.calcEnemyPower();
		region.calcClosestEnemy();
		regions.push_back(region);
		m_bot.StopProfiling("0.10.4.2.2.4      calculateRegionInformation");

		//Protect our SCVs and lift our base
		if(Util::IsTerran(m_bot.GetSelfRace()))
		{
			const Unit& base = myBaseLocation->getResourceDepot();
			if (base.isValid())
			{
				if (base.getUnitPtr()->cargo_space_taken == 0 && m_bot.Workers().getNumWorkers() > 0)
				{
					// Hide our last SCVs (should be 5, but is higher because some workers may end up dying on the way)
					if (m_bot.Workers().getNumWorkers() <= 7)
						Micro::SmartAbility(base.getUnitPtr(), sc2::ABILITY_ID::LOADALL, m_bot);
				}
				else if (!base.isFlying() && base.getUnitPtr()->health < base.getUnitPtr()->health_max * 0.5f)
					Micro::SmartAbility(base.getUnitPtr(), sc2::ABILITY_ID::LIFT, m_bot);
			}
		}
    }

	m_bot.Strategy().setIsWorkerRushed(workerRushed);
	m_bot.Strategy().setIsEarlyRushed(earlyRushed);

	// If we have at least one region under attack
	if(!regions.empty())
	{
		m_bot.StartProfiling("0.10.4.2.2.5      calculateRegionsScores");
		// We sort them (the one with the strongest enemy force is first)
		regions.sort();

		// We check each of our units to determine how useful they would be for defending each of our attacked regions
		for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
		{
			if (unit.getType().isWorker() || unit.getType().isBuilding())
				continue;	// We don't want to consider our workers and defensive buildings (they will automatically defend their region)

			// We check how useful our unit would be for anti ground and anti air for each of our regions
			for (auto & region : regions)
			{
				
				const float distance = Util::Dist(unit, region.baseLocation->getPosition());
				bool immune = true;
				float maxGroundDps = 0.f;
				float maxAirDps = 0.f;
				for (auto & enemyUnit : region.enemyUnits)
				{
					// We check if our unit is immune to the enemy unit (as soon as one enemy unit can attack our unit, we stop checking)
					if (immune)
					{
						if (unit.isFlying())
						{
							immune = !Util::CanUnitAttackAir(enemyUnit.getUnitPtr(), m_bot);
						}
						else
						{
							immune = !Util::CanUnitAttackGround(enemyUnit.getUnitPtr(), m_bot);
						}
					}
					// We check the max ground and air dps that our unit would do in that region
					const float dps = Util::GetDpsForTarget(unit.getUnitPtr(), enemyUnit.getUnitPtr(), m_bot);
					if (enemyUnit.isFlying())
					{
						if (dps > maxAirDps)
						{
							maxAirDps = dps;
						}
					}
					else
					{
						if (dps > maxGroundDps)
						{
							maxGroundDps = dps;
						}
					}
				}
				// If our unit would have a valid ground target, we calculate the score (usefulness in that region) and add it to the list
				if (maxGroundDps > 0.f)
				{
					float regionScore = immune * 50 + maxGroundDps - distance;
					region.unitGroundScores.insert_or_assign(unit.getUnitPtr(), regionScore);
				}
				// If our unit would have a valid air target, we calculate the score (usefulness in that region) and add it to the list
				if (maxAirDps > 0.f)
				{
					float regionScore = immune * 50 + maxAirDps - distance;
					region.unitAirScores.insert_or_assign(unit.getUnitPtr(), regionScore);
				}
			}
		}
		m_bot.StopProfiling("0.10.4.2.2.5      calculateRegionsScores");

		m_bot.StartProfiling("0.10.4.2.2.6      affectUnits");
		while (true)	// The conditions to exit are if enough units are protecting our regions enough or if 
		{
			Unit unit;
			const sc2::Unit* unitptr = nullptr;

			// We take the region that needs the most support
			auto regionIterator = regions.begin();

			do
			{
				auto & region = *regionIterator;
				const bool needsMoreSupport = region.needsMoreSupport();

				// We find the unit that is the most interested to defend that region
				float bestScore = 0.f;
				auto& scores = region.antiGroundPowerNeeded() > region.antiAirPowerNeeded() ? region.unitGroundScores : region.unitAirScores;
				for (auto & scorePair : scores)
				{
					// If the base already has enough defense
					if (!needsMoreSupport)
					{
						// We check if the unit is in an offensive squad
						const auto scoredUnit = Unit(scorePair.first, m_bot);
						const auto squad = m_squadData.getUnitSquad(scoredUnit);
						if (squad)
						{
							const auto & squadOrder = squad->getSquadOrder();
							if (squadOrder.getType() == SquadOrderTypes::Attack || squadOrder.getType() == SquadOrderTypes::Harass)
							{
								// If the unit is closer to its squad order objective than the base to defend, we won't send back that unit to defend
								if (Util::DistSq(scoredUnit, squadOrder.getPosition()) < Util::DistSq(scoredUnit, region.squad->getSquadOrder().getPosition()))
								{
									continue;
								}
							}
						}
					}
					if (!unitptr || scorePair.second > bestScore)
					{
						bestScore = scorePair.second;
						unitptr = scorePair.first;
					}
				}

				// If we have a unit that can defend
				if (unitptr)
				{
					unit = Unit(unitptr, m_bot);
				}
				// If we have no more unit to defend we check for the workers
				else
				{
					unit = findWorkerToAssignToSquad(*region.squad, region.baseLocation->getPosition(), region.closestEnemyUnit);
				}

				if (!unit.isValid())
				{
					++regionIterator;
					if (regionIterator == regions.end())
						break;
				}
			} while (!unit.isValid());

			// BREAK CONDITION : there is no more unit to be affected to the defense squads
			if(!unit.isValid())
			{
				break;
			}

			// Assign it to the squad
			auto & squad = *regionIterator->squad;
			if(m_squadData.canAssignUnitToSquad(unit, squad))
			{
				// We affect that unit to the region
				regionIterator->affectAllyUnit(unit.getUnitPtr());
				m_squadData.assignUnitToSquad(unit.getUnitPtr(), squad);	// we cannot give a reference of the Unit because it doesn't have a big scope
			}
			else
			{
				Util::Log(__FUNCTION__, "Cannot assign unit of type " + unit.getType().getName() + " to squad " + squad.getName(), m_bot);
			}

			// We remove that unit from the score maps of all regions
			if (unitptr)
			{
				for (auto & regionToRemoveUnit : regions)
				{
					regionToRemoveUnit.unitGroundScores.erase(unitptr);
					regionToRemoveUnit.unitAirScores.erase(unitptr);
				}
			}

			// We sort the regions so the one that needs the most support comes back first
			regions.sort();
		}
		m_bot.StopProfiling("0.10.4.2.2.6      affectUnits");
	}
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, bool flyingDefendersNeeded, bool groundDefendersNeeded, Unit & closestEnemy)
{
    auto & squadUnits = defenseSquad.getUnits();

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
			bool isUseful = (flyingDefendersNeeded && unit.canAttackAir()) || (groundDefendersNeeded && unit.canAttackGround());
			if(!isUseful)
				defenseSquad.removeUnit(unit);
        }
    }

	if (flyingDefendersNeeded)
	{
		Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemy, "air");

		while(defenderToAdd.isValid())
		{
			m_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemy, "air");
		}
	}

	if (groundDefendersNeeded)
	{
		Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemy, "ground");

		while (defenderToAdd.isValid())
		{
			m_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), closestEnemy, "ground");
		}
	}
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
		(defenseSquad.getName() == "ScoutDefense" ||  // do not check distances if it is to protect against a scout
		Util::DistSq(woker, pos) < 15.f * 15.f &&	// worker should not get too far from base
		(m_bot.Strategy().isWorkerRushed() ||		// do not check min distance if worker rushed
		Util::DistSq(woker, closestEnemy) < 7.f * 7.f ||	// worker can fight only units close to it
		(closestEnemy.getType().isBuilding() && Util::DistSq(closestEnemy, pos) < 12.f * 12.f))))	// worker can fight buildings somewhat close to the base
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

		if (!m_bot.Strategy().shouldFocusBuildings())
		{
			m_bot.Actions()->SendChat("Looks like you lost your main base, time to concede? :)");
			m_bot.Strategy().setFocusBuildings(true);
		}
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