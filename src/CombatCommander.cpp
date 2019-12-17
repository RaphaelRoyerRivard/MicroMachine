#include "CombatCommander.h"
#include "Util.h"
#include "CCBot.h"
#include <list>

const size_t IdlePriority = 0;
const size_t WorkerFleePriority = 0;
const size_t BackupPriority = 1;
const size_t HarassPriority = 2;
const size_t AttackPriority = 2;
const size_t ClearExpandPriority = 3;
const size_t ScoutPriority = 3;
const size_t BaseDefensePriority = 4;
const size_t ScoutDefensePriority = 5;
const size_t DropPriority = 5;

const float DefaultOrderRadius = 25;			//Order radius is the threat awareness range of units in the squad
const float WorkerRushDefenseOrderRadius = 250;
const float MainAttackOrderRadius = 15;
const float HarassOrderRadius = 25;
const float ScoutOrderRadius = 6;				//Small number to prevent the scout from targeting far units instead of going to the next base location
const float MainAttackMaxDistance = 20;			//Distance from the center of the Main Attack Squad for a unit to be considered in it
const float MainAttackMaxRegroupDuration = 100; //Max number of frames allowed for a regroup order
const float MainAttackRegroupCooldown = 200;	//Min number of frames required to wait between regroup orders
const float MainAttackMinRetreatDuration = 50;	//Max number of frames allowed for a regroup order

const size_t BLOCKED_TILES_UPDATE_FREQUENCY = 24;
const uint32_t WORKER_RUSH_DETECTION_COOLDOWN = 30 * 24;
const size_t MAX_DISTANCE_FROM_CLOSEST_BASE_FOR_WORKER_FLEE = 15;

CombatCommander::CombatCommander(CCBot & bot)
    : m_bot(bot)
    , m_squadData(bot)
    , m_initialized(false)
    , m_attackStarted(false)
	, m_currentBaseExplorationIndex(0)
	, m_currentBaseScoutingIndex(0)
{
}

void CombatCommander::onStart()
{
	for (auto& ability : m_bot.Observation()->GetAbilityData())
	{
		m_abilityCastingRanges[ability.ability_id] = ability.cast_range;
	}

    m_squadData.clearSquadData();

	// the squad that consists of units waiting for the squad to be big enough to begin the main attack
	SquadOrder idleOrder(SquadOrderTypes::Idle, CCPosition(), DefaultOrderRadius, "Prepare for battle");
	m_squadData.addSquad("Idle", Squad("Idle", idleOrder, IdlePriority, m_bot));

	// the squad that consists of fleeing workers
	SquadOrder fleeOrder(SquadOrderTypes::Retreat, CCPosition(), DefaultOrderRadius, "Worker flee");
	m_squadData.addSquad("WorkerFlee", Squad("WorkerFlee", fleeOrder, WorkerFleePriority, m_bot));

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

	SquadOrder scoutOrder(SquadOrderTypes::Scout, CCPosition(), ScoutOrderRadius, "Scouting for new bases");
	m_squadData.addSquad("Scout", Squad("Scout", scoutOrder, ScoutPriority, m_bot));

	//The influence maps are initialised earlier so we can use the blocked tiles influence map to place the turrets
}

bool CombatCommander::isSquadUpdateFrame()
{
    return true;
}

void CombatCommander::clearYamatoTargets()
{
	for(auto it = m_yamatoTargets.begin(); it != m_yamatoTargets.end();)
	{
		auto & targetPair = *it;
		const auto targetTag = targetPair.first;
		const auto target = m_bot.Observation()->GetUnit(targetTag);
		if (!target || !target->is_alive)
		{
			it = m_yamatoTargets.erase(it);
			continue;
		}

		auto & battlecruiserPairs = targetPair.second;
		for (auto it2 = battlecruiserPairs.begin(); it2 != battlecruiserPairs.end();)
		{
			const auto & battlecruiserPair = *it2;
			const auto battlecruiserTag = battlecruiserPair.first;
			const auto battlecruiser = m_bot.Observation()->GetUnit(battlecruiserTag);
			const auto finishFrame = battlecruiserPair.second;
			if(!battlecruiser || !battlecruiser->is_alive || m_bot.GetCurrentFrame() >= finishFrame)
			{
				it2 = battlecruiserPairs.erase(it2);
				continue;
			}
			
			++it2;
		}

		if (battlecruiserPairs.empty())
		{
			it = m_yamatoTargets.erase(it);
			continue;
		}

		++it;
	}
}

void CombatCommander::onFrame(const std::vector<Unit> & combatUnits)
{
    if (!m_attackStarted)
    {
        m_attackStarted = shouldWeStartAttacking();
    }

	clearYamatoTargets();

    m_combatUnits = combatUnits;

	m_bot.StartProfiling("0.10.4.0    updateInfluenceMaps");
	updateInfluenceMaps();
	m_bot.StopProfiling("0.10.4.0    updateInfluenceMaps");

	m_bot.StartProfiling("0.10.4.2    updateSquads");
    if (isSquadUpdateFrame())
    {
		updateIdleSquad();
		updateWorkerFleeSquad();
        updateScoutDefenseSquad();
		m_bot.StartProfiling("0.10.4.2.1    updateDefenseBuildings");
		updateDefenseBuildings();
		m_bot.StopProfiling("0.10.4.2.1    updateDefenseBuildings");
		m_bot.StartProfiling("0.10.4.2.2    updateDefenseSquads");
        updateDefenseSquads();
		m_bot.StopProfiling("0.10.4.2.2    updateDefenseSquads");
		updateClearExpandSquads();
		updateScoutSquad();
		updateHarassSquads();
		updateAttackSquads();
        //updateBackupSquads();
    }
	drawCombatInformation();
	m_bot.StopProfiling("0.10.4.2    updateSquads");

	m_bot.StartProfiling("0.10.4.1    m_squadData.onFrame");
	m_squadData.onFrame();
	m_bot.StopProfiling("0.10.4.1    m_squadData.onFrame");

	m_bot.StartProfiling("0.10.4.3    lowPriorityCheck");
	lowPriorityCheck();
	m_bot.StopProfiling("0.10.4.3    lowPriorityCheck");
}

void CombatCommander::lowPriorityCheck()
{
	auto frame = m_bot.GetGameLoop();
	if (frame - m_lastLowPriorityFrame < 5)
	{
		return;
	}
	m_lastLowPriorityFrame = frame;

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
	m_groundFromGroundCombatInfluenceMap.resize(mapWidth);
	m_groundFromAirCombatInfluenceMap.resize(mapWidth);
	m_airFromGroundCombatInfluenceMap.resize(mapWidth);
	m_airFromAirCombatInfluenceMap.resize(mapWidth);
	m_groundEffectInfluenceMap.resize(mapWidth);
	m_airEffectInfluenceMap.resize(mapWidth);
	m_groundFromGroundCloakedCombatInfluenceMap.resize(mapWidth);
	m_blockedTiles.resize(mapWidth);
	for(size_t x = 0; x < mapWidth; ++x)
	{
		auto& groundFromGroundInfluenceMapRow = m_groundFromGroundCombatInfluenceMap[x];
		auto& groundFromAirInfluenceMapRow = m_groundFromAirCombatInfluenceMap[x];
		auto& airFromGroundInfluenceMapRow = m_airFromGroundCombatInfluenceMap[x];
		auto& airFromAirInfluenceMapRow = m_airFromAirCombatInfluenceMap[x];
		auto& groundEffectInfluenceMapRow = m_groundEffectInfluenceMap[x];
		auto& airEffectInfluenceMapRow = m_airEffectInfluenceMap[x];
		auto& groundFromGroundCloakedCombatInfluenceMapRow = m_groundFromGroundCloakedCombatInfluenceMap[x];
		auto& blockedTilesRow = m_blockedTiles[x];
		groundFromGroundInfluenceMapRow.resize(mapHeight);
		groundFromAirInfluenceMapRow.resize(mapHeight);
		airFromGroundInfluenceMapRow.resize(mapHeight);
		airFromAirInfluenceMapRow.resize(mapHeight);
		groundEffectInfluenceMapRow.resize(mapHeight);
		airEffectInfluenceMapRow.resize(mapHeight);
		groundFromGroundCloakedCombatInfluenceMapRow.resize(mapHeight);
		blockedTilesRow.resize(mapHeight);
		for (size_t y = 0; y < mapHeight; ++y)
		{
			groundFromGroundInfluenceMapRow[y] = 0;
			groundFromAirInfluenceMapRow[y] = 0;
			airFromGroundInfluenceMapRow[y] = 0;
			airFromAirInfluenceMapRow[y] = 0;
			groundEffectInfluenceMapRow[y] = 0;
			airEffectInfluenceMapRow[y] = 0;
			groundFromGroundCloakedCombatInfluenceMapRow[y] = 0;
			blockedTilesRow[y] = false;
		}
	}
}

void CombatCommander::resetInfluenceMaps()
{
	const size_t mapWidth = m_bot.Map().totalWidth();
	const size_t mapHeight = m_bot.Map().totalHeight();
	const bool resetBlockedTiles = m_bot.GetGameLoop() - m_lastBlockedTilesResetFrame >= BLOCKED_TILES_UPDATE_FREQUENCY;
	if (resetBlockedTiles)
		m_lastBlockedTilesResetFrame = m_bot.GetGameLoop();
	for (size_t x = 0; x < mapWidth; ++x)
	{
		std::vector<float> & groundFromGroundInfluenceMap = m_groundFromGroundCombatInfluenceMap[x];
		std::vector<float> & groundFromAirInfluenceMap = m_groundFromAirCombatInfluenceMap[x];
		std::vector<float> & airFromGroundInfluenceMap = m_airFromGroundCombatInfluenceMap[x];
		std::vector<float> & airFromAirInfluenceMap = m_airFromAirCombatInfluenceMap[x];
		std::vector<float> & groundEffectInfluenceMap = m_groundEffectInfluenceMap[x];
		std::vector<float> & airEffectInfluenceMap = m_airEffectInfluenceMap[x];
		std::vector<float> & groundFromGroundCloakedCombatInfluenceMap = m_groundFromGroundCloakedCombatInfluenceMap[x];
		std::fill(groundFromGroundInfluenceMap.begin(), groundFromGroundInfluenceMap.end(), 0.f);
		std::fill(groundFromAirInfluenceMap.begin(), groundFromAirInfluenceMap.end(), 0.f);
		std::fill(airFromGroundInfluenceMap.begin(), airFromGroundInfluenceMap.end(), 0.f);
		std::fill(airFromAirInfluenceMap.begin(), airFromAirInfluenceMap.end(), 0.f);
		std::fill(groundEffectInfluenceMap.begin(), groundEffectInfluenceMap.end(), 0.f);
		std::fill(airEffectInfluenceMap.begin(), airEffectInfluenceMap.end(), 0.f);
		std::fill(groundFromGroundCloakedCombatInfluenceMap.begin(), groundFromGroundCloakedCombatInfluenceMap.end(), 0.f);

		if (resetBlockedTiles)
		{
			auto& blockedTilesRow = m_blockedTiles[x];
			for (size_t y = 0; y < mapHeight; ++y)
			{
				blockedTilesRow[y] = false;
			}
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
	
	drawInfluenceMaps();	
	drawBlockedTiles();
}

void CombatCommander::updateInfluenceMapsWithUnits()
{
	const bool updateBlockedTiles = m_bot.GetGameLoop() - m_lastBlockedTilesUpdateFrame >= BLOCKED_TILES_UPDATE_FREQUENCY;
	if (updateBlockedTiles)
		m_lastBlockedTilesUpdateFrame = m_bot.GetGameLoop();
	for (auto& enemyUnit : m_bot.GetKnownEnemyUnits())
	{
		auto& enemyUnitType = enemyUnit.getType();
		if (enemyUnitType.isCombatUnit() || enemyUnitType.isWorker() || (enemyUnitType.isAttackingBuilding() && enemyUnit.getUnitPtr()->build_progress >= 1.f))
		{
			if(enemyUnit.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE || enemyUnit.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED)
			{
				const float dps = Util::GetSpecialCaseDps(enemyUnit.getUnitPtr(), m_bot, sc2::Weapon::TargetType::Ground);
				const float radius = Util::GetSpecialCaseRange(enemyUnit.getAPIUnitType(), sc2::Weapon::TargetType::Ground);
				updateInfluenceMap(dps, radius, 1.f, enemyUnit.getPosition(), true, true, true, false);
			}
			else
			{
				updateGroundInfluenceMapForUnit(enemyUnit);
				updateAirInfluenceMapForUnit(enemyUnit);
			}
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
		updateBlockedTilesWithNeutral();
	}
}

void CombatCommander::updateInfluenceMapsWithEffects()
{
	m_enemyScans.clear();
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
				for (const auto & pos : effect.positions)
					m_enemyScans.push_back(pos);
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
				updateInfluenceMap(dps, radius, 1.f, pos, false, true, true, false);
			if (targetType == sc2::Weapon::TargetType::Any || targetType == sc2::Weapon::TargetType::Ground)
				updateInfluenceMap(dps, radius, 1.f, pos, true, true, true, false);
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
	float range = ground ? Util::GetGroundAttackRange(enemyUnit.getUnitPtr(), m_bot) : Util::GetAirAttackRange(enemyUnit.getUnitPtr(), m_bot);
	if (range == 0.f)
		return;
	if (!ground && enemyUnit.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_TEMPEST)
		range += 2;
	const float speed = std::max(1.5f, Util::getSpeedOfUnit(enemyUnit.getUnitPtr(), m_bot));
	updateInfluenceMap(dps, range, speed, enemyUnit.getPosition(), ground, !enemyUnit.isFlying(), false, enemyUnit.getUnitPtr()->cloak == sc2::Unit::Cloaked);
}

void CombatCommander::updateInfluenceMap(float dps, float range, float speed, const CCPosition & position, bool ground, bool fromGround, bool effect, bool cloaked)
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
	auto& influenceMap = ground ? (effect ? m_groundEffectInfluenceMap : (fromGround ? m_groundFromGroundCombatInfluenceMap : m_groundFromAirCombatInfluenceMap)) : (effect ? m_airEffectInfluenceMap : (fromGround ? m_airFromGroundCombatInfluenceMap : m_airFromAirCombatInfluenceMap));
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
			if (fromGround && cloaked)
				m_groundFromGroundCloakedCombatInfluenceMap[x][y] += dps * multiplier;
		}
	}
}

void CombatCommander::updateBlockedTilesWithUnit(const Unit& unit)
{
	CCTilePosition bottomLeft;
	CCTilePosition topRight;
	unit.getBuildingLimits(bottomLeft, topRight);

	for(int x = bottomLeft.x; x < topRight.x; ++x)
	{
		for(int y = bottomLeft.y; y < topRight.y; ++y)
		{
			m_blockedTiles[x][y] = true;
		}
	}
}

void CombatCommander::updateBlockedTilesWithNeutral()
{
	for (auto& neutralUnitPair : m_bot.GetNeutralUnits())
	{
		auto& neutralUnit = neutralUnitPair.second;
		updateBlockedTilesWithUnit(neutralUnit);
	}
}

void CombatCommander::drawInfluenceMaps()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
	if (m_bot.Config().DrawInfluenceMaps)
	{
		m_bot.StartProfiling("0.10.4.0.4      drawInfluenceMaps");
		const size_t mapWidth = m_bot.Map().totalWidth();
		const size_t mapHeight = m_bot.Map().totalHeight();
		for (size_t x = 0; x < mapWidth; ++x)
		{
			auto& groundFromGroundInfluenceMapRow = m_groundFromGroundCombatInfluenceMap[x];
			auto& groundFromAirInfluenceMapRow = m_groundFromAirCombatInfluenceMap[x];
			auto& airFromGroundInfluenceMapRow = m_airFromGroundCombatInfluenceMap[x];
			auto& airFromAirInfluenceMapRow = m_airFromAirCombatInfluenceMap[x];
			auto& groundEffectInfluenceMapRow = m_groundEffectInfluenceMap[x];
			auto& airEffectInfluenceMapRow = m_airEffectInfluenceMap[x];
			for (size_t y = 0; y < mapHeight; ++y)
			{
				const float groundInfluence = groundFromGroundInfluenceMapRow[y] + groundFromAirInfluenceMapRow[y];
				const float airInfluence = airFromGroundInfluenceMapRow[y] + airFromAirInfluenceMapRow[y];
				if (groundInfluence > 0.f)
				{
					const float value = std::min(255.f, std::max(0.f, groundInfluence * 5));
					m_bot.Map().drawTile(x, y, CCColor(255, 255 - value, 0));	//yellow to red
				}
				if (airInfluence > 0.f)
				{
					const float value = std::min(255.f, std::max(0.f, airInfluence * 5));
					m_bot.Map().drawTile(x, y, CCColor(255, 255 - value, 0), 0.5f);	//yellow to red
				}
				if (groundEffectInfluenceMapRow[y] > 0.f)
				{
					const float value = std::min(255.f, std::max(0.f, groundEffectInfluenceMapRow[y] * 5));
					m_bot.Map().drawTile(x, y, CCColor(255 - value, value, 255), 0.7f);	//cyan to purple
				}
				if (airEffectInfluenceMapRow[y] > 0.f)
				{
					const float value = std::min(255.f, std::max(0.f, airEffectInfluenceMapRow[y] * 5));
					m_bot.Map().drawTile(x, y, CCColor(255 - value, value, 255), 0.4f);	//cyan to purple
				}
			}
		}
		m_bot.StopProfiling("0.10.4.0.4      drawInfluenceMaps");
	}
}

void CombatCommander::drawBlockedTiles()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
	if (m_bot.Config().DrawBlockedTiles)
	{
		m_bot.StartProfiling("0.10.4.0.5      drawBlockedTiles");
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
		m_bot.StopProfiling("0.10.4.0.5      drawBlockedTiles");
	}
}

void CombatCommander::updateIdleSquad()
{
    Squad & idleSquad = m_squadData.getSquad("Idle");
    for (auto & unit : m_combatUnits)
    {
		if (unit.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING)
			continue;
        // if it hasn't been assigned to a squad yet, put it in the low priority idle squad
        if (m_squadData.canAssignUnitToSquad(unit, idleSquad))
        {
            idleSquad.addUnit(unit);
        }
    }

	if (idleSquad.getUnits().empty())
		return;

	if (m_bot.GetCurrentFrame() - m_lastIdleSquadUpdateFrame >= 24)	// Every second
	{
		m_lastIdleSquadUpdateFrame = m_bot.GetCurrentFrame();
		auto idlePosition = m_bot.GetStartLocation();
		const BaseLocation* farthestBase = m_bot.Bases().getFarthestOccupiedBaseLocation();
		if(farthestBase)
		{
			const auto vectorAwayFromBase = Util::Normalized(farthestBase->getResourceDepot().getPosition() - Util::GetPosition(farthestBase->getCenterOfMinerals()));
			idlePosition = farthestBase->getResourceDepot().getPosition() + vectorAwayFromBase * 5.f;
		}
		
		for (auto & combatUnit : idleSquad.getUnits())
		{
			if (Util::DistSq(combatUnit, idlePosition) > 5.f * 5.f)
			{
				Micro::SmartMove(combatUnit.getUnitPtr(), idlePosition, m_bot);
			}
		}
	}
}

void CombatCommander::updateWorkerFleeSquad()
{
	Squad & workerFleeSquad = m_squadData.getSquad("WorkerFlee");
	for (auto & worker : m_bot.Workers().getWorkers())
	{
		const CCTilePosition tile = Util::GetTilePosition(worker.getPosition());
		const bool flyingThreat = Util::PathFinding::HasCombatInfluenceOnTile(tile, worker.isFlying(), false, m_bot);
		const bool groundCloakedThreat = Util::PathFinding::HasGroundFromGroundCloakedInfluenceOnTile(tile, m_bot);
		const bool groundThreat = Util::PathFinding::HasCombatInfluenceOnTile(tile, worker.isFlying(), true, m_bot);
		const bool injured = worker.getHitPointsPercentage() < m_bot.Workers().MIN_HP_PERCENTAGE_TO_FIGHT * 100;
		const auto job = m_bot.Workers().getWorkerData().getWorkerJob(worker);
		const auto isProxyWorker = m_bot.Workers().getWorkerData().isProxyWorker(worker);
		// Check if the worker needs to flee (the last part is bad because workers sometimes need to mineral walk)
		if ((((flyingThreat && !groundThreat) || groundCloakedThreat) && job != WorkerJobs::Build && job != WorkerJobs::Repair)
			|| Util::PathFinding::HasEffectInfluenceOnTile(tile, worker.isFlying(), m_bot)
			|| (groundThreat && (injured || isProxyWorker) && job != WorkerJobs::Build && Util::DistSq(worker, Util::GetPosition(m_bot.Bases().getClosestBasePosition(worker.getUnitPtr(), Players::Self))) < MAX_DISTANCE_FROM_CLOSEST_BASE_FOR_WORKER_FLEE * MAX_DISTANCE_FROM_CLOSEST_BASE_FOR_WORKER_FLEE))
		{
			// Put it in the squad if it is not defending or already in the squad
			if (m_squadData.canAssignUnitToSquad(worker, workerFleeSquad))
			{
				m_bot.Workers().setCombatWorker(worker);
				workerFleeSquad.addUnit(worker);
			}
		}
		else
		{
			const auto squad = m_squadData.getUnitSquad(worker);
			if(squad != nullptr && squad == &workerFleeSquad)
			{
				m_bot.Workers().finishedWithWorker(worker);
				workerFleeSquad.removeUnit(worker);
			}
		}
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

void CombatCommander::updateClearExpandSquads()
{
	// reset clear expand squads
	for (const auto & kv : m_squadData.getSquads())
	{
		const Squad & squad = kv.second;
		if (squad.getName().find("Clear Expand") != std::string::npos)
		{
			m_squadData.getSquad(squad.getName()).clear();
		}
	}

	for(const auto baseLocation : m_bot.Bases().getBaseLocations())
	{
		if(baseLocation->isBlocked())
		{
			const auto basePosition = baseLocation->getPosition();
			std::stringstream squadName;
			squadName << "Clear Expand " << basePosition.x << " " << basePosition.y;

			const SquadOrder clearExpand(SquadOrderTypes::Attack, basePosition, DefaultOrderRadius, "Clear Blocked Expand");
			// if we don't have a squad assigned to this blocked expand already, create one
			if (!m_squadData.squadExists(squadName.str()))
			{
				m_squadData.addSquad(squadName.str(), Squad(squadName.str(), clearExpand, ClearExpandPriority, m_bot));
			}

			// assign units to the squad
			Squad & clearExpandSquad = m_squadData.getSquad(squadName.str());
			clearExpandSquad.setSquadOrder(clearExpand);

			// add closest unit in squad
			Unit closestUnit;
			float distance = 0.f;
			for (auto & unitPair : m_bot.GetAllyUnits())
			{
				const auto & unit = unitPair.second;

				if (!unit.isValid())
					continue;
				if (unit.getType().isBuilding())
					continue;
				if (unit.getType().isWorker())
					continue;
				if (!m_squadData.canAssignUnitToSquad(unit, clearExpandSquad))
					continue;

				if(Util::CanUnitAttackGround(unit.getUnitPtr(), m_bot))
				{
					const float dist = Util::DistSq(unit, basePosition);
					if(!closestUnit.isValid() || dist < distance)
					{
						distance = dist;
						closestUnit = unit;
					}
				}
			}

			if(closestUnit.isValid())
			{
				m_squadData.assignUnitToSquad(closestUnit.getUnitPtr(), clearExpandSquad);
			}
		}
	}
}

void CombatCommander::updateScoutSquad()
{
	if (!m_bot.Strategy().enemyHasMassZerglings() && m_bot.GetCurrentFrame() < 4704)	//around 3:30, or as soon as enemy has a lot of lings
		return;

	Squad & scoutSquad = m_squadData.getSquad("Scout");
	if (scoutSquad.getUnits().empty())
	{
		Unit bestCandidate;
		float distanceFromBase = 0.f;
		for (auto & unit : m_combatUnits)
		{
			BOT_ASSERT(unit.isValid(), "null unit in combat units");
			if (unit.getUnitPtr()->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER)
			{
				const auto base = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
				if(base)
				{
					const float dist = Util::DistSq(unit, base->getPosition());
					if (!bestCandidate.isValid() || dist < distanceFromBase)
					{
						if (m_squadData.canAssignUnitToSquad(unit, scoutSquad))
						{
							bestCandidate = unit;
							distanceFromBase = dist;
						}
					}
				}
			}
		}
		if(bestCandidate.isValid())
		{
			m_squadData.assignUnitToSquad(bestCandidate, scoutSquad);
		}
	}

	if (scoutSquad.getUnits().empty())
	{
		return;
	}

	const SquadOrder scoutOrder(SquadOrderTypes::Scout, GetNextBaseLocationToScout(), ScoutOrderRadius, "Scout");
	scoutSquad.setSquadOrder(scoutOrder);
}

void CombatCommander::updateHarassSquads()
{
	Squad & harassSquad = m_squadData.getSquad("Harass");	
	std::vector<Unit*> idleHellions;
	std::vector<Unit*> idleMarines;
	std::vector<Unit*> idleVikings;
	std::vector<Unit*> idleCyclones;
	for (auto & unit : m_combatUnits)
	{
		BOT_ASSERT(unit.isValid(), "null unit in combat units");

		// put high mobility units in the harass squad
		const sc2::UnitTypeID unitTypeId = unit.getType().getAPIUnitType();
		if ((unitTypeId == sc2::UNIT_TYPEID::TERRAN_MARINE
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_REAPER
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_HELLION
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_CYCLONE
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_BANSHEE
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_RAVEN
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_THOR
			|| unitTypeId == sc2::UNIT_TYPEID::TERRAN_THORAP
			|| (unitTypeId == sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING && m_bot.Strategy().getStartingStrategy() == PROXY_CYCLONES && m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Reaper.getUnitType(), true) + m_bot.GetDeadAllyUnitsCount(sc2::UNIT_TYPEID::TERRAN_REAPER) >= 2))
			&& m_squadData.canAssignUnitToSquad(unit, harassSquad))
		{
			if (unitTypeId == sc2::UNIT_TYPEID::TERRAN_HELLION)
				idleHellions.push_back(&unit);
			else if (unitTypeId == sc2::UNIT_TYPEID::TERRAN_MARINE)
				idleMarines.push_back(&unit);
			else if (unitTypeId == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER || unitTypeId == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
				idleVikings.push_back(&unit);
			else if (unitTypeId == sc2::UNIT_TYPEID::TERRAN_CYCLONE)
				idleCyclones.push_back(&unit);
			else
				m_squadData.assignUnitToSquad(unit, harassSquad);
		}
	}
	if(idleHellions.size() >= Util::HELLION_SQUAD_COUNT)
	{
		for (auto hellion : idleHellions)
		{
			m_squadData.assignUnitToSquad(*hellion, harassSquad);
		}
	}
	const auto battlecruisers = m_bot.UnitInfo().getUnitTypeCount(Players::Self, MetaTypeEnum::Battlecruiser.getUnitType(), true, true);
	if (idleMarines.size() >= 10 && battlecruisers > 0)
	{
		for (auto marine : idleMarines)
		{
			m_squadData.assignUnitToSquad(*marine, harassSquad);
		}
	}
	const auto tempestCount = m_bot.GetKnownEnemyUnits(sc2::UNIT_TYPEID::PROTOSS_TEMPEST).size();
	const auto VIKING_TEMPEST_RATIO = 2.5f;
	if(idleVikings.size() >= tempestCount * VIKING_TEMPEST_RATIO)
	{
		for (auto viking : idleVikings)
		{
			m_squadData.assignUnitToSquad(*viking, harassSquad);
		}
		m_hasEnoughVikingsAgainstTempests = true;
	} 
	else
	{
		/*const auto harassVikings = harassSquad.getUnitCountOfType(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER) + harassSquad.getUnitCountOfType(sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT);
		// If we have enough Vikings overall we can send them to fight
		m_hasEnoughVikingsAgainstTempests = harassVikings + idleVikings.size() >= tempestCount * VIKING_TEMPEST_RATIO;
		if (m_hasEnoughVikingsAgainstTempests)
		{
			for (auto viking : idleVikings)
			{
				m_squadData.assignUnitToSquad(*viking, harassSquad);
			}
		}
		else*/
		{
			m_hasEnoughVikingsAgainstTempests = false;
			auto vikings = harassSquad.getUnitsOfType(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER);
			auto vikingsAssault = harassSquad.getUnitsOfType(sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT);
			vikings.insert(vikings.end(), vikingsAssault.begin(), vikingsAssault.end());
			// Otherwise we remove our Vikings from the Harass Squad when they are close to our base
			for (const auto & viking : vikings)
			{
				if(Util::DistSq(viking, m_bot.GetStartLocation()) < 10.f * 10.f)
				{
					harassSquad.removeUnit(viking);
				}
			}
		}
	}
	if(!idleCyclones.empty())
	{
		for (const auto & unit : harassSquad.getUnits())
		{
			if (unit.isFlying())
			{
				for (auto cyclone : idleCyclones)
				{
					m_squadData.assignUnitToSquad(*cyclone, harassSquad);
				}
				break;
			}
		}
	}

	if (harassSquad.getUnits().empty())
		return;

	sc2::Units allyUnits;
	auto allySupply = 0;
	for (const auto & unit : harassSquad.getUnits())
	{
		allyUnits.push_back(unit.getUnitPtr());
		allySupply += unit.getType().supplyRequired();
	}
	sc2::Units enemyUnits;
	auto enemySupply = 0;
	for (const auto & enemyUnitPair : m_bot.GetEnemyUnits())
	{
		const auto & enemyUnit = enemyUnitPair.second;
		if (enemyUnit.getType().isCombatUnit() && !enemyUnit.getType().isBuilding())
		{
			enemyUnits.push_back(enemyUnit.getUnitPtr());
			enemySupply += enemyUnit.getType().supplyRequired();
		}
	}
	m_winAttackSimulation = Util::SimulateCombat(allyUnits, enemyUnits, m_bot);
	m_biggerArmy = allySupply >= enemySupply;

	const SquadOrder harassOrder(SquadOrderTypes::Harass, GetClosestEnemyBaseLocation(), HarassOrderRadius, "Harass");
	harassSquad.setSquadOrder(harassOrder);
}

void CombatCommander::updateAttackSquads()
{
    /*if (!m_attackStarted)
    {
        return;
    }*/

    Squad & mainAttackSquad = m_squadData.getSquad("MainAttack");
	
	if (m_bot.Strategy().getStartingStrategy() == WORKER_RUSH && m_bot.GetCurrentFrame() >= 224)
	{
		for (auto & scv : m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_SCV))
		{
			if (!scv.isReturningCargo() && mainAttackSquad.getUnits().size() < 11 && m_squadData.canAssignUnitToSquad(scv, mainAttackSquad, true))
			{
				m_bot.Workers().getWorkerData().setWorkerJob(scv, WorkerJobs::Combat);
				m_squadData.assignUnitToSquad(scv, mainAttackSquad);
			}
		}
		
		const SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), MainAttackOrderRadius, "Attack");
		mainAttackSquad.setSquadOrder(mainAttackOrder);
	}

    /*for (auto & unit : m_combatUnits)
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
    }*/
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
	if (m_bot.GetCurrentFrame() < 120 * 24)	// No need to have a scout defense after 2 min
	{
		for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			if (myBaseLocation->containsPosition(unit.getPosition()) && unit.getType().isWorker())
			{
				enemyUnitsInRegion.push_back(unit);
				if (enemyUnitsInRegion.size() > 1)
					break;
			}
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
	handleWall();
	lowerSupplyDepots();
}

void CombatCommander::handleWall()
{
	int SUPPLYDEPOT_DISTANCE = 25;//5 tiles ^ 2, because we use DistSq

	auto wallCenter = m_bot.Buildings().getWallPosition();
	auto & enemies = m_bot.GetEnemyUnits();

	for (auto & enemy : enemies)
	{
		if (enemy.second.isFlying() || enemy.second.getType().isBuilding())
			continue;
		CCTilePosition enemyPosition = enemy.second.getTilePosition();
		int distance = Util::DistSq(enemyPosition, wallCenter);
		if (distance < SUPPLYDEPOT_DISTANCE)
		{//Raise wall
			for (auto & building : m_bot.Buildings().getWallBuildings())
			{
				if (building.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED)
				{
					building.useAbility(sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_RAISE);
				}
			}
			return;
		}
	}

	//Lower wall
	for (auto & building : m_bot.Buildings().getWallBuildings())
	{
		if (building.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)
		{
			building.useAbility(sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
		}
	}
}

void CombatCommander::lowerSupplyDepots()
{
	for(auto & supplyDepot : m_bot.GetAllyUnits(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT))
	{
		if(supplyDepot.isCompleted() && !Util::Contains(supplyDepot, m_bot.Buildings().getWallBuildings()))
		{
			supplyDepot.useAbility(sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
		}
	}
}

struct RegionArmyInformation
{
	const BaseLocation* baseLocation;
	CCBot& bot;
	std::vector<Unit> enemyUnits;
	std::vector<const sc2::Unit*> affectedAllyUnits;
	std::unordered_map<const sc2::Unit*, float> unitGroundScores;
	std::unordered_map<const sc2::Unit*, float> unitAirScores;
	std::unordered_map<const sc2::Unit*, float> unitDetectionScores;
	float airEnemyPower;
	float groundEnemyPower;
	bool invisEnemies;
	float antiAirAllyPower;
	float antiGroundAllyPower;
	bool antiInvis;
	Squad* squad;
	Unit closestEnemyUnit;

	RegionArmyInformation(const BaseLocation* baseLocation, CCBot& bot)
		: baseLocation(baseLocation)
		, bot(bot)
		, airEnemyPower(0)
		, groundEnemyPower(0)
		, invisEnemies(false)
		, antiAirAllyPower(0)
		, antiGroundAllyPower(0)
		, antiInvis(false)
		, squad(nullptr)
		, closestEnemyUnit({})
	{}

	std::unordered_map<const sc2::Unit*, float> & getScores(std::string type)
	{
		if (type == "detection")
			return unitDetectionScores;
		if (type == "ground")
			return unitGroundScores;
		return unitAirScores;
	}

	void calcEnemyPower()
	{
		airEnemyPower = 0;
		groundEnemyPower = 0;
		invisEnemies = false;
		for (auto& unit : enemyUnits)
		{
			auto power = Util::GetUnitPower(unit.getUnitPtr(), nullptr, bot);
			if (power == 0.f)
				power = Util::GetSpecialCasePower(unit);
			if (unit.isFlying())
				airEnemyPower += power;
			else
				groundEnemyPower += power;
			if (unit.isCloaked() || unit.isBurrowed())
				invisEnemies = true;
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
		const bool isDetector = UnitType(unit->unit_type, bot).isDetector();
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
		if (isDetector)
		{
			antiInvis = true;
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

	bool antiInvisNeeded() const
	{
		return invisEnemies && !antiInvis;
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
	const auto & ourBases = m_bot.Bases().getOccupiedBaseLocations(Players::Self);
	auto nextExpansion = m_bot.Bases().getNextExpansion(Players::Self, false, false);
	std::set<BaseLocation*> bases;
	bases.insert(ourBases.begin(), ourBases.end());
	if (nextExpansion)
		bases.insert(nextExpansion);
	for (BaseLocation * myBaseLocation : bases)
	{
		// don't defend inside the enemy region, this will end badly when we are stealing gas or cannon rushing
		if (myBaseLocation == enemyBaseLocation)
		{
			continue;
		}

		m_bot.StartProfiling("0.10.4.2.2.1      detectEnemiesInRegions");
		auto region = RegionArmyInformation(myBaseLocation, m_bot);

		const CCPosition basePosition = Util::GetPosition(myBaseLocation->getDepotPosition());

		// calculate how many units are flying / ground units
		bool unitOtherThanWorker = false;
		float minEnemyDistance = 0;
		Unit closestEnemy;
		int enemyWorkers = 0;
		for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			// if it's an overlord, don't worry about it for defense, we don't care what they see
			if (unit.getType().isOverlord())
			{
				continue;
			}

			// if the unit is not targetable, we do not need to defend against it (shade, kd8 charge, disruptor's ball, etc.)
			if (!UnitType::isTargetable(unit.getAPIUnitType()))
				continue;

			if (myBaseLocation->containsUnitApproximative(unit, m_bot.Strategy().isWorkerRushed() ? WorkerRushDefenseOrderRadius : 0))
			{
				//we can ignore the first enemy worker in our region since we assume it is a scout (handled by scout defense)
				if (!workerRushed && unit.getType().isWorker() && !unitOtherThanWorker && m_bot.GetGameLoop() < 4392 && myBaseLocation == m_bot.Bases().getPlayerStartingBaseLocation(Players::Self))	// first 3 minutes
				{
					// Need at least 3 workers for a worker rush (or 1 if the previous frame was a worker rush)
					if (!m_bot.Strategy().isWorkerRushed() && enemyWorkers < 3)
					{
						++enemyWorkers;
						continue;
					}
					workerRushed = true;
				}
				else if (!earlyRushed && m_bot.GetGameLoop() < 7320)	// first 5 minutes
				{
					earlyRushed = true;
				}

				if (!unit.getType().isWorker())
				{
					unitOtherThanWorker = true;
					workerRushed = false;
				}

				const float enemyDistance = Util::DistSq(unit.getPosition(), basePosition);
				if (!closestEnemy.isValid() || enemyDistance < minEnemyDistance)
				{
					minEnemyDistance = enemyDistance;
					closestEnemy = unit;
				}

				region.enemyUnits.push_back(unit);
			}
		}

		std::stringstream squadName;
		squadName << "Base Defense " << basePosition.x << " " << basePosition.y;

		myBaseLocation->setIsUnderAttack(!region.enemyUnits.empty());
		m_bot.StopProfiling("0.10.4.2.2.1      detectEnemiesInRegions");
		if (region.enemyUnits.empty())
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
		const SquadOrder defendRegion(SquadOrderTypes::Defend, closestEnemy.getPosition(), m_bot.Strategy().isWorkerRushed() ? WorkerRushDefenseOrderRadius : DefaultOrderRadius, "Defend Region!");
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
		if (Util::IsTerran(m_bot.GetSelfRace()))
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

	if (workerRushed)
	{
		m_lastWorkerRushDetectionFrame = m_bot.GetCurrentFrame();
		m_bot.Strategy().setIsWorkerRushed(true);
	}
	else if (m_bot.GetCurrentFrame() > m_lastWorkerRushDetectionFrame + WORKER_RUSH_DETECTION_COOLDOWN)
	{
		m_bot.Strategy().setIsWorkerRushed(false);
	}
	m_bot.Strategy().setIsEarlyRushed(earlyRushed);

	// Find our Reaper that is the closest to the enemy base
	const sc2::Unit * offensiveReaper = nullptr;
	if (earlyRushed)
	{
		if (enemyBaseLocation)
		{
			for (auto & unitPair : m_bot.GetAllyUnits())
			{
				float minDist = 0.f;
				if (unitPair.second.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_REAPER)
				{
					const auto dist = Util::DistSq(unitPair.second, enemyBaseLocation->getPosition());
					if (!offensiveReaper || dist < minDist)
					{
						minDist = dist;
						offensiveReaper = unitPair.second.getUnitPtr();
					}
				}
			}
		}
	}

	// If we have at least one region under attack
	if(!regions.empty())
	{
		m_bot.StartProfiling("0.10.4.2.2.5      calculateRegionsScores");
		// We sort them (the one with the strongest enemy force is first)
		regions.sort();

		// We check each of our units to determine how useful they would be for defending each of our attacked regions
		for (auto & unitPair : m_bot.GetAllyUnits())
		{
			auto & unit = unitPair.second;
			if (unit.getType().isWorker() || unit.getType().isBuilding())
				continue;	// We don't want to consider our workers and defensive buildings (they will automatically defend their region)

			if (unit.getUnitPtr() == offensiveReaper)
				continue;	// We want to keep at least one Reaper in the Harass squad (defined only when early rushed)

			// We check how useful our unit would be for anti ground and anti air for each of our regions
			for (auto & region : regions)
			{
				const float distance = Util::Dist(unit, region.baseLocation->getPosition());
				bool weakUnitAgainstOnlyBuildings = unit.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_REAPER;
				bool immune = true;
				bool detectionUseful = false;
				float maxGroundDps = 0.f;
				float maxAirDps = 0.f;
				for (auto & enemyUnit : region.enemyUnits)
				{
					// As soon as there is a non building unit that the weak unit can attack, we consider that the weak unit can be useful
					if (weakUnitAgainstOnlyBuildings)
					{
						if (enemyUnit.getType().isBuilding())
							continue;
						if (Util::GetDpsForTarget(unit.getUnitPtr(), enemyUnit.getUnitPtr(), m_bot) > 0.f)
							weakUnitAgainstOnlyBuildings = false;
					}
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
					if (!detectionUseful && unit.getType().isDetector() && (enemyUnit.isCloaked() || enemyUnit.isBurrowed()))
						detectionUseful = true;
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
				// The weak unit would not be useful against buildings, it should harass instead of defend
				if (weakUnitAgainstOnlyBuildings)
					continue;
				// If our unit would have a valid ground target, we calculate the score (usefulness in that region) and add it to the list
				if (maxGroundDps > 0.f)
				{
					float regionScore = immune * 50 + maxGroundDps - distance;
					region.unitGroundScores[unit.getUnitPtr()] = regionScore;
				}
				// If our unit would have a valid air target, we calculate the score (usefulness in that region) and add it to the list
				if (maxAirDps > 0.f)
				{
					float regionScore = immune * 50 + maxAirDps - distance;
					region.unitAirScores[unit.getUnitPtr()] = regionScore;
				}
				if (detectionUseful)
				{
					float regionScore = 100 - distance;
					region.unitDetectionScores[unit.getUnitPtr()] = regionScore;
				}
			}
		}
		m_bot.StopProfiling("0.10.4.2.2.5      calculateRegionsScores");

		m_bot.StartProfiling("0.10.4.2.2.6      affectUnits");
		while (true)
		{
			Unit unit;
			const sc2::Unit* unitptr = nullptr;

			// We take the region that needs the most support
			auto regionIterator = regions.begin();
			auto & squad = *regionIterator->squad;
			bool stopCheckingForGroundSupport = false;
			bool stopCheckingForAirSupport = false;
			bool stopCheckingForDetectionSupport = false;

			do
			{
				auto & region = *regionIterator;

				// We find the unit that is the most interested to defend that region
				float bestScore = 0.f;
				std::string support;
				if (region.antiInvisNeeded() && !stopCheckingForDetectionSupport)
				{
					support = "detection";
				}
				else if(region.groundEnemyPower > 0.f && (stopCheckingForAirSupport || region.airEnemyPower == 0.f))
				{
					support = "ground";
				}
				else if(region.airEnemyPower > 0.f && (stopCheckingForGroundSupport || region.groundEnemyPower == 0.f))
				{
					support = "air";
				}
				else
				{
					support = region.antiGroundPowerNeeded() >= region.antiAirPowerNeeded() ? "ground" : "air";
				}
				bool needsMoreSupport = true;
				if (support == "ground")
					needsMoreSupport = region.needsMoreAntiGround();
				else if (support == "air")
					needsMoreSupport = region.needsMoreAntiAir();
				auto& scores = region.getScores(support);
				for (auto & scorePair : scores)
				{
					// If the base already has enough defense
					if (!needsMoreSupport)
					{
						// We check if the unit is in an offensive squad
						const auto scoredUnit = Unit(scorePair.first, m_bot);
						const auto unitSquad = m_squadData.getUnitSquad(scoredUnit);
						if (unitSquad)
						{
							const auto & squadOrder = unitSquad->getSquadOrder();
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
				else if(support == "ground" && needsMoreSupport)
				{
					unit = findWorkerToAssignToSquad(*region.squad, region.baseLocation->getPosition(), region.closestEnemyUnit);
				}

				// If no support is available
				if (!unit.isValid())
				{
					// If we were checking for detection support, stop checking for detection support
					if (support == "detection" && !stopCheckingForDetectionSupport)
					{
						stopCheckingForDetectionSupport = true;
						continue;	// Continue to check if the same region needs the other type of support
					}
					// If we were checking for ground support, stop checking for ground support
					if(support == "ground" && !stopCheckingForGroundSupport)
					{
						stopCheckingForGroundSupport = true;
						continue;	// Continue to check if the same region needs the other type of support
					}
					// If we were checking for air support, stop checking for air support
					if(support == "air" && !stopCheckingForAirSupport)
					{
						stopCheckingForAirSupport = true;
						continue;	// Continue to check if the same region needs the other type of support
					}
					// Otherwise, check next region
					++regionIterator;
					// If this was the last region, exit
					if (regionIterator == regions.end())
						break;
					// Reset the support checks because it will now be for another region
					stopCheckingForGroundSupport = false;
					stopCheckingForAirSupport = false;
					stopCheckingForDetectionSupport = false;
				}
			} while (!unit.isValid());

			// BREAK CONDITION : there is no more unit to be affected to the defense squads
			if(!unit.isValid())
			{
				break;
			}

			// Assign it to the squad
			if(m_squadData.canAssignUnitToSquad(unit, squad))
			{
				// We affect that unit to the region
				regionIterator->affectAllyUnit(unit.getUnitPtr());
				m_squadData.assignUnitToSquad(unit.getUnitPtr(), squad);	// we cannot give a reference of the Unit because it doesn't have a big scope
			}
			else
			{
				Util::Log(__FUNCTION__, "Cannot assign unit of type " + unit.getType().getName() + " to squad " + squad.getName() + ", it is already in squad " + m_squadData.getUnitSquad(unit)->getName(), m_bot);
			}

			// We remove that unit from the score maps of all regions
			if (unitptr)
			{
				for (auto & regionToRemoveUnit : regions)
				{
					regionToRemoveUnit.unitGroundScores.erase(unitptr);
					regionToRemoveUnit.unitAirScores.erase(unitptr);
					regionToRemoveUnit.unitDetectionScores.erase(unitptr);
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

bool CombatCommander::ShouldWorkerDefend(const Unit & worker, const Squad & defenseSquad, const CCPosition & pos, Unit & closestEnemy) const
{
	if (!worker.isValid())
		return false;
	if (m_bot.Workers().getWorkerData().isProxyWorker(worker))
		return false;
	if (!m_squadData.canAssignUnitToSquad(worker, defenseSquad))
		return false;
	if (closestEnemy.isFlying())
		return false;
	// do not check distances if it is to protect against a scout
	if (defenseSquad.getName() == "ScoutDefense")
		return true;
	// do not check min distance if worker rushed
	if (m_bot.Strategy().isWorkerRushed())
		return true;
	// worker can fight buildings somewhat close to the base
	const auto isBuilding = closestEnemy.getType().isBuilding();
	const auto enemyDistanceToBase = Util::DistSq(closestEnemy, pos);
	const auto maxEnemyDistance = closestEnemy.getAPIUnitType() == sc2::UNIT_TYPEID::ZERG_NYDUSCANAL ? 30.f : 15.f;
	const auto enemyDistanceToWorker = Util::DistSq(worker, closestEnemy);
	if (isBuilding && enemyDistanceToBase < maxEnemyDistance * maxEnemyDistance && enemyDistanceToWorker < maxEnemyDistance * maxEnemyDistance)
		return true;
	// worker should not get too far from base and can fight only units close to it
	if (Util::DistSq(worker, pos) < 15.f * 15.f && enemyDistanceToWorker < 7.f * 7.f)
		return true;
	return false;
}

std::map<Unit, std::pair<CCPosition, uint32_t>> & CombatCommander::GetInvisibleSighting()
{
	return m_invisibleSighting;
}

float CombatCommander::getTotalGroundInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_groundFromGroundCombatInfluenceMap[tilePosition.x][tilePosition.y] + m_groundFromAirCombatInfluenceMap[tilePosition.x][tilePosition.y] + m_groundEffectInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getTotalAirInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_airFromGroundCombatInfluenceMap[tilePosition.x][tilePosition.y] + m_airFromAirCombatInfluenceMap[tilePosition.x][tilePosition.y] + m_airEffectInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getGroundCombatInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_groundFromGroundCombatInfluenceMap[tilePosition.x][tilePosition.y] + m_groundFromAirCombatInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getAirCombatInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_airFromGroundCombatInfluenceMap[tilePosition.x][tilePosition.y] + m_airFromAirCombatInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getGroundFromGroundCombatInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_groundFromGroundCombatInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getGroundFromAirCombatInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_groundFromAirCombatInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getAirFromGroundCombatInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_airFromGroundCombatInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getAirFromAirCombatInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_airFromAirCombatInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getGroundEffectInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_groundEffectInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getAirEffectInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_airEffectInfluenceMap[tilePosition.x][tilePosition.y];
}

float CombatCommander::getGroundFromGroundCloakedCombatInfluence(CCTilePosition tilePosition) const
{
	if (!m_bot.Map().isValidTile(tilePosition))
		return 0.f;
	return m_groundFromGroundCloakedCombatInfluenceMap[tilePosition.x][tilePosition.y];
}

bool CombatCommander::isTileBlocked(int x, int y)
{
	if (m_blockedTiles.size() <= 0)
	{
		return false;
	}
	return m_blockedTiles[x][y];
}

void CombatCommander::drawCombatInformation()
{
    if (m_bot.Config().DrawCombatInformation)
    {
		const auto str = "Bigger army: " + std::to_string(m_biggerArmy) + "\nWin simulation: " + std::to_string(m_winAttackSimulation);
		const auto color = m_biggerArmy && m_winAttackSimulation ? sc2::Colors::Green : m_biggerArmy || m_winAttackSimulation ? sc2::Colors::Yellow : sc2::Colors::Red;
		m_bot.Map().drawTextScreen(0.25f, 0.01f, str, color);
    }
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
                if (enemyUnit.getType().isBuilding() && Util::DistSq(enemyUnit, enemyBasePosition) < 6.f * 6.f)
                {
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
		return closestEnemyPosition;
	}

    // Fourth choice: We can't see anything so explore the map attacking along the way
	return exploreMap();
}

CCPosition CombatCommander::exploreMap()
{
	// Hack to prevent our units from hugging observers that they can't kill 
	if (!m_bot.Strategy().shouldProduceAntiAirOffense())
		m_bot.Strategy().setShouldProduceAntiAirOffense(true);

	const CCPosition basePosition = Util::GetPosition(m_bot.Bases().getBasePosition(Players::Enemy, m_currentBaseExplorationIndex));
	for (auto & unit : m_combatUnits)
	{
		if (Util::DistSq(unit.getPosition(), basePosition) < 3.f * 3.f)
		{
			m_currentBaseExplorationIndex = (m_currentBaseExplorationIndex + 1) % m_bot.Bases().getBaseLocations().size();
			return Util::GetPosition(m_bot.Bases().getBasePosition(Players::Enemy, m_currentBaseExplorationIndex));
		}
	}
	return basePosition;
}

CCPosition CombatCommander::GetClosestEnemyBaseLocation()
{
	const auto base = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
	if (!base)
		return getMainAttackLocation();

	BaseLocation * closestEnemyBase = nullptr;
	float closestDistance = 0.f;
	const auto & baseLocations = m_bot.Bases().getOccupiedBaseLocations(Players::Enemy);
	for(const auto baseLocation : baseLocations)
	{
		const auto dist = Util::DistSq(baseLocation->getPosition(), base->getPosition());
		if(!closestEnemyBase || dist < closestDistance)
		{
			closestEnemyBase = baseLocation;
			closestDistance = dist;
		}
	}

	if (!closestEnemyBase)
		return getMainAttackLocation();

	const auto depotPosition = Util::GetPosition(closestEnemyBase->getDepotPosition());
	const auto position = depotPosition + Util::Normalized(depotPosition - closestEnemyBase->getPosition()) * 3.f;
	return position;
}

CCPosition CombatCommander::GetNextBaseLocationToScout()
{
	const auto & baseLocations = m_bot.Bases().getBaseLocations();

	if(baseLocations.size() == m_visitedBaseLocations.size())
	{
		m_visitedBaseLocations.clear();
	}

	CCPosition targetBasePosition;
	auto & squad = m_squadData.getSquad("Scout");
	if (!squad.getUnits().empty())
	{
		float minDistance = 0.f;
		const auto & scoutUnit = squad.getUnits()[0];
		for(auto baseLocation : baseLocations)
		{
			const bool visited = Util::Contains(baseLocation, m_visitedBaseLocations);
			if(visited)
			{
				continue;
			}
			if (baseLocation->isOccupiedByPlayer(Players::Enemy) ||
				baseLocation->isOccupiedByPlayer(Players::Self) ||
				Util::DistSq(scoutUnit, baseLocation->getPosition()) < 5.f * 5.f)
			{
				m_visitedBaseLocations.push_back(baseLocation);
				continue;
			}
			const float distance = Util::DistSq(scoutUnit, baseLocation->getPosition());
			if(targetBasePosition == CCPosition() || distance < minDistance)
			{
				minDistance = distance;
				targetBasePosition = baseLocation->getPosition();
			}
		}
	}
	if(targetBasePosition == CCPosition())
	{
		targetBasePosition = GetClosestEnemyBaseLocation();
	}
	return targetBasePosition;
}