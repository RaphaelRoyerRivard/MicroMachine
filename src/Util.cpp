#include "Util.h"
#include "CCBot.h"
#include <iostream>

const float EPSILON = 1e-5;

std::string Util::GetStringFromRace(const CCRace & race)
{
#ifdef SC2API
    if      (race == sc2::Race::Terran)  { return "Terran"; }
    else if (race == sc2::Race::Protoss) { return "Protoss"; }
    else if (race == sc2::Race::Zerg)    { return "Zerg"; }
    else if (race == sc2::Race::Random)  { return "Random"; }
#else
    if      (race == BWAPI::Races::Terran)  { return "Terran"; }
    else if (race == BWAPI::Races::Protoss) { return "Protoss"; }
    else if (race == BWAPI::Races::Zerg)    { return "Zerg"; }
    else if (race == BWAPI::Races::Unknown) { return "Unknown"; }
#endif
    BOT_ASSERT(false, "Unknown Race");
    return "Error";
}

CCRace Util::GetRaceFromString(const std::string & raceIn)
{
    std::string race(raceIn);
    std::transform(race.begin(), race.end(), race.begin(), ::tolower);

#ifdef SC2API
    if      (race == "terran")  { return sc2::Race::Terran; }
    else if (race == "protoss") { return sc2::Race::Protoss; }
    else if (race == "zerg")    { return sc2::Race::Zerg; }
    else if (race == "random")  { return sc2::Race::Random; }
    
    BOT_ASSERT(false, "Unknown Race: ", race.c_str());
    return sc2::Race::Random;
#else
    if      (race == "terran")  { return BWAPI::Races::Terran; }
    else if (race == "protoss") { return BWAPI::Races::Protoss; }
    else if (race == "zerg")    { return BWAPI::Races::Zerg; }
    else if (race == "random")  { return BWAPI::Races::Unknown; }

    BOT_ASSERT(false, "Unknown Race: ", race.c_str());
    return BWAPI::Races::Unknown;
#endif
}

CCPositionType Util::TileToPosition(float tile)
{
#ifdef SC2API
    return tile;
#else
    return (int)(tile * 32);
#endif
}

UnitType Util::GetSupplyProvider(const CCRace & race, CCBot & bot)
{
#ifdef SC2API
    switch (race) 
    {
        case sc2::Race::Terran: return UnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT, bot);
        case sc2::Race::Protoss: return UnitType(sc2::UNIT_TYPEID::PROTOSS_PYLON, bot);
        case sc2::Race::Zerg: return UnitType(sc2::UNIT_TYPEID::ZERG_OVERLORD, bot);
        default: return UnitType();
    }
#else
    return UnitType(race.getSupplyProvider(), bot);
#endif
}

UnitType Util::GetWorkerType(const CCRace & race, CCBot & bot)
{
	switch (race)
	{
		case sc2::Race::Terran: return UnitType(sc2::UNIT_TYPEID::TERRAN_SCV, bot);
		case sc2::Race::Protoss: return UnitType(sc2::UNIT_TYPEID::PROTOSS_PROBE, bot);
		case sc2::Race::Zerg: return UnitType(sc2::UNIT_TYPEID::ZERG_DRONE, bot);
		default: return UnitType();
	}
}

UnitType Util::GetTownHall(const CCRace & race, CCBot & bot)
{
#ifdef SC2API
    switch (race) 
    {
        case sc2::Race::Terran: return UnitType(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER, bot);
        case sc2::Race::Protoss: return UnitType(sc2::UNIT_TYPEID::PROTOSS_NEXUS, bot);
        case sc2::Race::Zerg: return UnitType(sc2::UNIT_TYPEID::ZERG_HATCHERY, bot);
        default: return UnitType();
    }
#else
    return UnitType(race.getResourceDepot(), bot);
#endif
}

UnitType Util::GetRefinery(const CCRace & race, CCBot & bot)
{
#ifdef SC2API
    switch (race) 
    {
        case sc2::Race::Terran: return UnitType(sc2::UNIT_TYPEID::TERRAN_REFINERY, bot);
        case sc2::Race::Protoss: return UnitType(sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR, bot);
        case sc2::Race::Zerg: return UnitType(sc2::UNIT_TYPEID::ZERG_EXTRACTOR, bot);
        default: return UnitType();
    }
#else
    return UnitType(race.getRefinery(), bot);
#endif
}

CCPosition Util::CalcCenter(const std::vector<const sc2::Unit*> & units)
{
	if (units.empty())
	{
		return CCPosition(0, 0);
	}

	CCPositionType cx = 0;
	CCPositionType cy = 0;

	for (auto & unit : units)
	{
		BOT_ASSERT(unit, "Unit pointer was null");
		cx += unit->pos.x;
		cy += unit->pos.y;
	}

	return CCPosition(cx / units.size(), cy / units.size());
}

CCPosition Util::CalcCenter(const std::vector<Unit> & units)
{
    if (units.empty())
    {
        return CCPosition(0, 0);
    }

    CCPositionType cx = 0;
    CCPositionType cy = 0;

    for (auto & unit : units)
    {
        BOT_ASSERT(unit.isValid(), "Unit pointer was null");
        cx += unit.getPosition().x;
        cy += unit.getPosition().y;
    }

    return CCPosition(cx / units.size(), cy / units.size());
}

void Util::CCUnitsToSc2Units(const std::vector<Unit> & units, sc2::Units & outUnits)
{
	for (auto unit : units)
	{
		outUnits.push_back(unit.getUnitPtr());
	}
}

void Util::Sc2UnitsToCCUnits(const sc2::Units & units, std::vector<Unit> & outUnits, CCBot & bot)
{
	for (auto unit : units)
	{
		outUnits.push_back(Unit(unit, bot));
	}
}

const sc2::Unit* Util::CalcClosestUnit(const sc2::Unit* unit, const sc2::Units & targets)
{
	float minDistance = 0;
	const sc2::Unit* closestUnit = nullptr;
	for (auto target : targets)
	{
		const float distance = Dist(target->pos, unit->pos);
		if (minDistance == 0 || distance < minDistance)
		{
			minDistance = distance;
			closestUnit = target;
		}
	}
	return closestUnit;
}

float Util::GetUnitsPower(const sc2::Units & units, const sc2::Units & targets, CCBot& bot)
{
	float unitsPower = 0;

	for (auto unit : units)
	{
		const sc2::Unit* closestTarget = CalcClosestUnit(unit, targets);
		unitsPower += GetUnitPower(unit, closestTarget, bot);
	}

	return unitsPower;
}

float Util::GetUnitsPower(const std::vector<Unit> & units, const std::vector<Unit> & targets, CCBot& bot)
{
	float unitsPower = 0;
	sc2::Units sc2Targets;
	CCUnitsToSc2Units(targets, sc2Targets);

	for (auto & unit : units)
	{
		const sc2::Unit* closestTarget = CalcClosestUnit(unit.getUnitPtr(), sc2Targets);
		unitsPower += GetUnitPower(unit, Unit(closestTarget, bot), bot);
	}

	return unitsPower;
}

float Util::GetUnitPower(const sc2::Unit* unit, const sc2::Unit* target, CCBot& bot)
{
	Unit targetUnit = target ? Unit(target, bot) : Unit();
	return GetUnitPower(Unit(unit, bot), targetUnit, bot);
}

float Util::GetUnitPower(const Unit &unit, const Unit& target, CCBot& bot)
{
	const float unitRange = target.isValid() ? GetAttackRangeForTarget(unit.getUnitPtr(), target.getUnitPtr(), bot) : GetMaxAttackRange(unit.getUnitPtr(), bot);
	///////// HEALTH
	float unitPower = pow(unit.getHitPoints() + unit.getShields(), 0.5f);
	///////// DPS
	if (target.isValid())
		unitPower *= Util::GetDpsForTarget(unit.getUnitPtr(), target.getUnitPtr(), bot);
	else
		unitPower *= Util::GetDps(unit.getUnitPtr(), bot);
	///////// DISTANCE
	if (target.isValid())
	{
		float distance = Util::Dist(unit.getPosition(), target.getPosition());
		if (unitRange < distance)
		{
			distance -= unitRange;
			const float distancePenalty = pow(0.5f, distance);
			unitPower *= distancePenalty;
		}
	}

	/*bool isRanged = unitRange >= 1.5f;

	///////// HEALTH
	float unitPower = pow(unit.getHitPoints() + unit.getShields(), 0.15f);

	///////// DPS
	if (target.isValid())
		unitPower *= Util::GetDpsForTarget(unit.getUnitPtr(), target.getUnitPtr(), bot);
	else
		unitPower *= Util::GetDps(unit.getUnitPtr(), bot);

	///////// RANGE
	if (isRanged)
		unitPower *= 3; //ranged bonus (+200%)

	///////// ARMOR
	unitPower *= 1 + Util::GetArmor(unit.getUnitPtr(), bot) * 0.1f;   //armor bonus (+10% per armor)

	///////// SPLASH DAMAGE
	//TODO bonus for splash damage

	///////// DISTANCE
	if (target.isValid())
	{
		float distance = Util::Dist(unit.getPosition(), target.getPosition());
		if (unitRange + 1 < distance)   //if the unit can't reach the closest unit (with a small buffer)
		{
			distance -= unitRange + 1;
			//float distancePenalty = unit.getType().isBuilding() ? 0.9f : 0.95f;
			float distancePenalty = pow(0.95f, distance);
			unitPower *= distancePenalty;	//penalty for distance (very fast for building but slow for units)
		}
	}*/

	if (bot.Config().DrawUnitPowerInfo)
		bot.Map().drawText(unit.getPosition(), "Power: " + std::to_string(unitPower));

	return unitPower;
}

void Util::Normalize(sc2::Point2D& point)
{
    float norm = sqrt(pow(point.x, 2) + pow(point.y, 2));
	if(norm > EPSILON)
		point /= norm;
}

sc2::Point2D Util::Normalized(const sc2::Point2D& point)
{
    float norm = sqrt(pow(point.x, 2) + pow(point.y, 2));
	if(norm > EPSILON)
		return sc2::Point2D(point.x / norm, point.y / norm);
    return sc2::Point2D(point.x, point.y);
}

float Util::GetDotProduct(const sc2::Point2D& v1, const sc2::Point2D& v2)
{
    sc2::Point2D v1n = Normalized(v1);
    sc2::Point2D v2n = Normalized(v2);
    return v1n.x * v2n.x + v1n.y * v2n.y;
}

bool Util::IsZerg(const CCRace & race)
{
#ifdef SC2API
    return race == sc2::Race::Zerg;
#else
    return race == BWAPI::Races::Zerg;
#endif
}

bool Util::IsProtoss(const CCRace & race)
{
#ifdef SC2API
    return race == sc2::Race::Protoss;
#else
    return race == BWAPI::Races::Protoss;
#endif
}

bool Util::CanUnitAttackAir(const sc2::Unit * unit, CCBot & bot)
{
	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);
	for (auto & weapon : unitTypeData.weapons)
	{
		if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Air)
			return true;
	}
	return GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Air) > 0.f;
}

bool Util::CanUnitAttackGround(const sc2::Unit * unit, CCBot & bot)
{
	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);
	for (auto & weapon : unitTypeData.weapons)
	{
		if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Ground)
			return true;
	}
	return GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Ground) > 0.f;
}

float Util::GetSpecialCaseRange(const sc2::UNIT_TYPEID unitType, sc2::Weapon::TargetType where)
{
	float range = 0.f;

	if (unitType == sc2::UNIT_TYPEID::ZERG_BANELING || unitType == sc2::UNIT_TYPEID::ZERG_BANELINGCOCOON)
	{
		if(where != sc2::Weapon::TargetType::Air)
			range = 2.2f;
	}
	else if (unitType == sc2::UNIT_TYPEID::TERRAN_BUNKER)
	{
		range = 7.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
	{
		if (where != sc2::Weapon::TargetType::Air)
			range = 3.0f;
	}
	else if (unitType == sc2::UNIT_TYPEID::PROTOSS_ADEPTPHASESHIFT)
	{
		if (where != sc2::Weapon::TargetType::Air)
			range = 4.f;
	}

	return range;
}

float Util::GetGroundAttackRange(const sc2::Unit * unit, CCBot & bot)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);

	float maxRange = 0.0f;
	for (auto & weapon : unitTypeData.weapons)
	{
		// can attack target with a weapon
		if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Ground)
			maxRange = weapon.range;
	}

	if (maxRange == 0.f)
		maxRange = GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Ground);

	if (maxRange > 0.f)
		maxRange += unit->radius;
	return maxRange;
}

float Util::GetAirAttackRange(const sc2::Unit * unit, CCBot & bot)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);

	float maxRange = 0.0f;
	for (auto & weapon : unitTypeData.weapons)
	{
		// can attack target with a weapon
		if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Air)
			maxRange = weapon.range;
	}

	if (maxRange == 0.f)
		maxRange = GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Air);

	if (maxRange > 0.f)
		maxRange += unit->radius;
	return maxRange;
}

float Util::GetAttackRangeForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);
	const sc2::Weapon::TargetType expectedWeaponType = target->is_flying ? sc2::Weapon::TargetType::Air : sc2::Weapon::TargetType::Ground;
	
	float maxRange = 0.0f;
	for (auto & weapon : unitTypeData.weapons)
	{
		// can attack target with a weapon
		if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == expectedWeaponType)
			maxRange = weapon.range;
	}

	if (maxRange == 0.f)
		maxRange = GetSpecialCaseRange(unit->unit_type, expectedWeaponType);

	if (maxRange > 0.f)
		maxRange += unit->radius + target->radius;
	return maxRange; 
}

float Util::GetMaxAttackRangeForTargets(const sc2::Unit * unit, const std::vector<const sc2::Unit *> & targets, CCBot & bot)
{
    float maxRange = 0.f;
    for (const sc2::Unit * target : targets)
    {
        const float range = GetAttackRangeForTarget(unit, target, bot);
        if (range > maxRange)
            maxRange = range;
    }
    return maxRange;
}

float Util::GetMaxAttackRange(const sc2::Unit * unit, CCBot & bot)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	const sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);

	float maxRange = GetMaxAttackRange(unitTypeData);

	if (maxRange > 0.f)
		maxRange += unit->radius;
	return maxRange;
}

float Util::GetMaxAttackRange(const sc2::UnitTypeID unitType, CCBot & bot)
{
    const sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unitType]);
    return GetMaxAttackRange(unitTypeData);
}

float Util::GetMaxAttackRange(sc2::UnitTypeData unitTypeData)
{
    float maxRange = 0.0f;
    for (auto & weapon : unitTypeData.weapons)
    {
        // can attack target with a weapon
        if (weapon.range > maxRange)
            maxRange = weapon.range;
    }

	if (maxRange == 0.f)
		maxRange = GetSpecialCaseRange(unitTypeData.unit_type_id);

    return maxRange;
}

float Util::GetArmor(const sc2::Unit * unit, CCBot & bot)
{
    sc2::UnitTypeData unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
    return unitTypeData.armor;
}

float Util::GetGroundDps(const sc2::Unit * unit, CCBot & bot)
{
	return GetDps(unit, sc2::Weapon::TargetType::Ground, bot);
}

float Util::GetAirDps(const sc2::Unit * unit, CCBot & bot)
{
	return GetDps(unit, sc2::Weapon::TargetType::Air, bot);
}

float Util::GetDps(const sc2::Unit * unit, CCBot & bot)
{
	return GetDps(unit, sc2::Weapon::TargetType::Any, bot);
}

float Util::GetDps(const sc2::Unit * unit, const sc2::Weapon::TargetType targetType, CCBot & bot)
{
	float dps = GetSpecialCaseDps(unit, bot, targetType);
	if (dps == 0.f)
	{
		sc2::UnitTypeData unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
		for (auto & weapon : unitTypeData.weapons)
		{
			if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == targetType)
			{
				float weaponDps = weapon.damage_;
				weaponDps *= weapon.attacks / weapon.speed;
				if (weaponDps > dps)
					dps = weaponDps;
			}
		}
	}
	return dps;
}

float Util::GetDpsForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot)
{
    const sc2::Weapon::TargetType expectedWeaponType = target->is_flying ? sc2::Weapon::TargetType::Air : sc2::Weapon::TargetType::Ground;
    float dps = GetSpecialCaseDps(unit, bot, expectedWeaponType);
    if (dps == 0.f)
    {
		sc2::UnitTypeData unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
		sc2::UnitTypeData targetTypeData = GetUnitTypeDataFromUnitTypeId(target->unit_type, bot);
        for (auto & weapon : unitTypeData.weapons)
        {
            if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == expectedWeaponType)
            {
                float weaponDps = weapon.damage_;
                for (auto & damageBonus : weapon.damage_bonus)
                {
                    if (std::find(targetTypeData.attributes.begin(), targetTypeData.attributes.end(), damageBonus.attribute) != targetTypeData.attributes.end())
                        weaponDps += damageBonus.bonus;
                }
                weaponDps -= targetTypeData.armor;
                weaponDps *= weapon.attacks / weapon.speed;
                if (weaponDps > dps)
                    dps = weaponDps;
            }
        }
    }

    return dps;
}

float Util::GetSpecialCaseDps(const sc2::Unit * unit, CCBot & bot, sc2::Weapon::TargetType where)
{
    float dps = 0.f;

    if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_BANELING || unit->unit_type == sc2::UNIT_TYPEID::ZERG_BANELINGCOCOON)
    {
		if(where != sc2::Weapon::TargetType::Air)
			dps = 15.f;
    }
    else if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
    {
        dps = 0.f;
    }
    else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER)
    {
        //A special case must be done for bunkers since they have no weapon and the cargo space is not available
        dps = 30.f;
    }
	else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
	{
		if (where != sc2::Weapon::TargetType::Air)
			dps = 5.0f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_ADEPTPHASESHIFT)
	{
		if (where != sc2::Weapon::TargetType::Air)
			dps = 13.7f;
	}

    return dps;
}

// get threats to our harass unit
const std::vector<const sc2::Unit *> Util::getThreats(const sc2::Unit * unit, const std::vector<const sc2::Unit *> & targets, CCBot & bot)
{
	BOT_ASSERT(unit, "null ranged unit in getThreats");

	std::vector<const sc2::Unit *> threats;

	// for each possible threat
	for (auto targetUnit : targets)
	{
		BOT_ASSERT(targetUnit, "null target unit in getThreats");
		if (Util::GetDpsForTarget(targetUnit, unit, bot) == 0.f)
			continue;
		//We consider a unit as a threat if the sum of its range and speed is bigger than the distance to our unit
		//But this is not working so well for melee units, we keep every units in a radius of min threat range
		const float threatRange = getThreatRange(unit, targetUnit, bot);
		if (Util::DistSq(unit->pos, targetUnit->pos) < threatRange * threatRange)
			threats.push_back(targetUnit);
	}

	return threats;
}

// get threats to our harass unit
const std::vector<const sc2::Unit *> Util::getThreats(const sc2::Unit * unit, const std::vector<Unit> & targets, CCBot & bot)
{
	BOT_ASSERT(unit, "null ranged unit in getThreats");

	std::vector<const sc2::Unit *> targetsPtrs(targets.size());
	for (auto& targetUnit : targets)
		targetsPtrs.push_back(targetUnit.getUnitPtr());

	return getThreats(unit, targetsPtrs, bot);
}

//calculate radius max(min range, range + speed + height bonus + small buffer)
float Util::getThreatRange(const sc2::Unit * unit, const sc2::Unit * threat, CCBot & m_bot)
{
	const float HARASS_THREAT_MIN_HEIGHT_DIFF = 2.f;
	const float HARASS_THREAT_RANGE_BUFFER = 1.f;
	const float HARASS_THREAT_RANGE_HEIGHT_BONUS = 4.f;
	const sc2::GameInfo gameInfo = m_bot.Observation()->GetGameInfo();
	const float heightBonus = Util::TerainHeight(gameInfo, threat->pos) > Util::TerainHeight(gameInfo, unit->pos) + HARASS_THREAT_MIN_HEIGHT_DIFF ? HARASS_THREAT_RANGE_HEIGHT_BONUS : 0.f;
	const float threatRange = Util::GetAttackRangeForTarget(threat, unit, m_bot) + Util::getSpeedOfUnit(threat, m_bot) + heightBonus + HARASS_THREAT_RANGE_BUFFER;
	return threatRange;
}

float Util::getAverageSpeedOfUnits(const std::vector<Unit>& units, CCBot & bot)
{
	if (units.empty())
		return 0.f;

	float squadSpeed = 0;

	for (auto & unit : units)
	{
		squadSpeed += getSpeedOfUnit(unit.getUnitPtr(), bot);
	}

	return squadSpeed / (float)units.size();
}

float Util::getSpeedOfUnit(const sc2::Unit * unit, CCBot & bot)
{
	float zergBonus = 1.f;
	if(Unit(unit, bot).getType().getRace() == CCRace::Zerg && !unit->is_burrowed && !unit->is_flying)
	{
		/* From https://liquipedia.net/starcraft2/Creep
		 * All Zerg ground units move faster when traveling on Creep, with the exception of burrowed units, Drones, Broodlings, and Changelings not disguised as Zerglings. 
		 * The increase is 30% for most units. Queens move 166.67% faster on creep, Hydralisks move 50% faster, and Spine Crawlers and Spore Crawlers move 150% faster. */
		if(bot.Observation()->HasCreep(unit->pos))
		{
			if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_QUEEN)
				zergBonus = 2.6667f;
			else if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_HYDRALISK)
				zergBonus = 1.5f;
			else if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_SPINECRAWLERUPROOTED || unit->unit_type == sc2::UNIT_TYPEID::ZERG_SPORECRAWLERUPROOTED)
				zergBonus = 2.5f;
			else if (unit->unit_type != sc2::UNIT_TYPEID::ZERG_DRONE && unit->unit_type != sc2::UNIT_TYPEID::ZERG_BROODLING && unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELING
				&& unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELINGMARINE && unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELINGMARINESHIELD && unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELINGZEALOT)
				zergBonus = 1.3f;
		}
		if (bot.Strategy().enemyHasMetabolicBoost() && unit->unit_type == sc2::UNIT_TYPEID::ZERG_ZERGLING)
			zergBonus *= 1.6f;
	}
	return GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot).movement_speed * zergBonus;
}

bool Util::IsTerran(const CCRace & race)
{
#ifdef SC2API
    return race == sc2::Race::Terran;
#else
    return race == BWAPI::Races::Terran;
#endif
}

CCTilePosition Util::GetTilePosition(const CCPosition & pos)
{
#ifdef SC2API
    return CCTilePosition((int)std::floor(pos.x), (int)std::floor(pos.y));
#else
    return CCTilePosition(pos);
#endif
}

CCPosition Util::GetPosition(const CCTilePosition & tile)
{
#ifdef SC2API
    return CCPosition((float)tile.x, (float)tile.y);
#else
    return CCPosition(tile);
#endif
}

float Util::Dist(const CCPosition & p1, const CCPosition & p2)
{
    return sqrtf((float)Util::DistSq(p1,p2));
}

float Util::Dist(const CCTilePosition & p1, const CCTilePosition & p2)
{
	return sqrtf((float)Util::DistSq(p1, p2));
}

float Util::Dist(const Unit & unit, const CCPosition & p2)
{
    return Dist(unit.getPosition(), p2);
}

float Util::Dist(const Unit & unit1, const Unit & unit2)
{
    return Dist(unit1.getPosition(), unit2.getPosition());
}

CCPositionType Util::DistSq(const CCTilePosition & p1, const CCTilePosition & p2)
{
	CCPositionType dx = p1.x - p2.x;
	CCPositionType dy = p1.y - p2.y;

	return dx * dx + dy * dy;
}

CCPositionType Util::DistSq(const Unit & unit, const CCPosition & p2)
{
	return DistSq(unit.getPosition(), p2);
}

CCPositionType Util::DistSq(const Unit & unit, const Unit & unit2)
{
	return DistSq(unit.getPosition(), unit2.getPosition());
}

CCPositionType Util::DistSq(const CCPosition & p1, const CCPosition & p2)
{
    CCPositionType dx = p1.x - p2.x;
    CCPositionType dy = p1.y - p2.y;

    return dx*dx + dy*dy;
}

sc2::Point2D Util::CalcLinearRegression(const std::vector<const sc2::Unit *> & units)
{
    float sumX = 0, sumY = 0, sumXSqr = 0, sumXY = 0, avgX, avgY, numerator, denominator, slope;
    size_t size = units.size();
    for (auto unit : units)
    {
        sumX += unit->pos.x;
        sumY += unit->pos.y;
        sumXSqr += pow(unit->pos.x, 2);
        sumXY += unit->pos.x * unit->pos.y;
    }
    avgX = sumX / size;
    avgY = sumY / size;
    denominator = size * sumXSqr - pow(sumX, 2);
    if (denominator == 0.f)
        return sc2::Point2D(0, 1);
    numerator = size * sumXY - sumX * sumY;
    slope = numerator / denominator;
    return Util::Normalized(sc2::Point2D(1, slope));
}

sc2::Point2D Util::CalcPerpendicularVector(const sc2::Point2D & vector)
{
    return sc2::Point2D(vector.y, -vector.x);
}

bool Util::Pathable(const sc2::GameInfo & info, const sc2::Point2D & point) 
{
    sc2::Point2DI pointI((int)point.x, (int)point.y);
    if (pointI.x < 0 || pointI.x >= info.width || pointI.y < 0 || pointI.y >= info.width)
    {
        return false;
    }

    assert(info.pathing_grid.data.size() == info.width * info.height);
    unsigned char encodedPlacement = info.pathing_grid.data[pointI.x + ((info.height - 1) - pointI.y) * info.width];
    bool decodedPlacement = encodedPlacement == 255 ? false : true;
    return decodedPlacement;
}

bool Util::Placement(const sc2::GameInfo & info, const sc2::Point2D & point) 
{
    sc2::Point2DI pointI((int)point.x, (int)point.y);
    if (pointI.x < 0 || pointI.x >= info.width || pointI.y < 0 || pointI.y >= info.width)
    {
        return false;
    }

    assert(info.placement_grid.data.size() == info.width * info.height);
    unsigned char encodedPlacement = info.placement_grid.data[pointI.x + ((info.height - 1) - pointI.y) * info.width];
    bool decodedPlacement = encodedPlacement == 255 ? true : false;
    return decodedPlacement;
}

float Util::TerainHeight(const sc2::GameInfo & info, const sc2::Point2D & point) 
{
    sc2::Point2DI pointI((int)point.x, (int)point.y);
    if (pointI.x < 0 || pointI.x >= info.width || pointI.y < 0 || pointI.y >= info.width)
    {
        return 0.0f;
    }

    assert(info.terrain_height.data.size() == info.width * info.height);
    unsigned char encodedHeight = info.terrain_height.data[pointI.x + ((info.height - 1) - pointI.y) * info.width];
    float decodedHeight = -100.0f + 200.0f * float(encodedHeight) / 255.0f;
    return decodedHeight;
}

void Util::VisualizeGrids(const sc2::ObservationInterface * obs, sc2::DebugInterface * debug) 
{
    const sc2::GameInfo& info = obs->GetGameInfo();

    sc2::Point2D camera = obs->GetCameraPos();
    for (float x = camera.x - 8.0f; x < camera.x + 8.0f; ++x) 
    {
        for (float y = camera.y - 8.0f; y < camera.y + 8.0f; ++y) 
        {
            // Draw in the center of each 1x1 cell
            sc2::Point2D point(x + 0.5f, y + 0.5f);

            float height = TerainHeight(info, sc2::Point2D(x, y));
            bool placable = Placement(info, sc2::Point2D(x, y));
            //bool pathable = Pathable(info, sc2::Point2D(x, y));

            sc2::Color color = placable ? sc2::Colors::Green : sc2::Colors::Red;
            debug->DebugSphereOut(sc2::Point3D(point.x, point.y, height + 0.5f), 0.4f, color);
        }
    }

    debug->SendDebug();
}

sc2::UnitTypeID Util::GetUnitTypeIDFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::UnitTypeData & data : bot.Observation()->GetUnitTypeData())
    {
        if (name == data.name)
        {
            return data.unit_type_id;
        }
    }

    return 0;
}

sc2::UnitTypeData Util::GetUnitTypeDataFromUnitTypeId(const sc2::UnitTypeID unitTypeId, CCBot & bot)
{
    return bot.Observation()->GetUnitTypeData()[unitTypeId];
}

sc2::UpgradeID Util::GetUpgradeIDFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::UpgradeData & data : bot.Observation()->GetUpgradeData())
    {
        if (name == data.name)
        {
            return data.upgrade_id;
        }
    }

    return 0;
}

#ifdef SC2API
sc2::BuffID Util::GetBuffFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::BuffData & data : bot.Observation()->GetBuffData())
    {
        if (name == data.name)
        {
            return data.buff_id;
        }
    }

    return 0;
}

sc2::AbilityID Util::GetAbilityFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::AbilityData & data : bot.Observation()->GetAbilityData())
    {
        if (name == data.link_name)
        {
            return data.ability_id;
        }
    }

    return 0;
}

// To select unit: From https://github.com/Blizzard/s2client-api/blob/master/tests/feature_layers_shared.cc
sc2::Point2DI Util::ConvertWorldToCamera(const sc2::GameInfo& game_info, const sc2::Point2D camera_world, const sc2::Point2D& world) {
    float camera_size = game_info.options.feature_layer.camera_width;
    int image_width = game_info.options.feature_layer.map_resolution_x;
    int image_height = game_info.options.feature_layer.map_resolution_y;

    // Pixels always cover a square amount of world space. The scale is determined
    // by making the shortest axis of the camera match the requested camera_size.
    float pixel_size = camera_size / std::min(image_width, image_height);
    float image_width_world = pixel_size * image_width;
    float image_height_world = pixel_size * image_height;

    // Origin of world space is bottom left. Origin of image space is top left.
    // The feature layer is centered around the camera target position.
    float image_origin_x = camera_world.x - image_width_world / 2.0f;
    float image_origin_y = camera_world.y + image_height_world / 2.0f;
    float image_relative_x = world.x - image_origin_x;
    float image_relative_y = image_origin_y - world.y;

    int image_x = static_cast<int>(image_relative_x / pixel_size);
    int image_y = static_cast<int>(image_relative_y / pixel_size);

    return sc2::Point2DI(image_x, image_y);
}
#endif

// checks where a given unit can make a given unit type now
// this is done by iterating its legal abilities for the build command to make the unit
bool Util::UnitCanMetaTypeNow(const Unit & unit, const UnitType & type, CCBot & m_bot)
{
#ifdef SC2API
    BOT_ASSERT(unit.isValid(), "Unit pointer was null");
    sc2::AvailableAbilities available_abilities = m_bot.Query()->GetAbilitiesForUnit(unit.getUnitPtr());
    
    // quick check if the unit can't do anything it certainly can't build the thing we want
    if (available_abilities.abilities.empty()) 
    {
        return false;
    }
    else 
    {
        // check to see if one of the unit's available abilities matches the build ability type
        sc2::AbilityID MetaTypeAbility = m_bot.Data(type).buildAbility;
        for (const sc2::AvailableAbility & available_ability : available_abilities.abilities) 
        {
            if (available_ability.ability_id == MetaTypeAbility)
            {
                return true;
            }
        }
    }
#else

#endif
    return false;
}
