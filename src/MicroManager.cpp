#include "MicroManager.h"
#include "CCBot.h"

MicroManager::MicroManager(CCBot & bot)
    : m_bot(bot)
{
}

void MicroManager::setUnits(const std::vector<Unit> & u)
{
    m_units = u;
}

const std::vector<Unit> & MicroManager::getUnits() const
{
    return m_units;
}

void MicroManager::regroup(const CCPosition & regroupPosition) const
{
    // for each of the units we have
    for (auto & unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit in MicroManager regroup");

        if (Util::Dist(unit, regroupPosition) > 4)
        {
            // regroup it
			Micro::SmartMove(unit.getUnitPtr(), regroupPosition, m_bot);
        }
        else
        {
            //defend itself near the regroupPosition
			Micro::SmartAttackMove(unit.getUnitPtr(), regroupPosition, m_bot);
        }
    }
}

float MicroManager::getAverageSquadSpeed() const
{
	return Util::getAverageSpeedOfUnits(m_units, m_bot);
}

float MicroManager::getAverageTargetsSpeed() const
{
	return Util::getAverageSpeedOfUnits(m_targets, m_bot);
}

float MicroManager::getSquadPower() const
{
    float squadPower = 0;

    for (auto & unit : m_units)
    {
        Unit& closestTarget = Util::CalcClosestUnit(unit, m_targets);
        squadPower += getUnitPower(unit, closestTarget);
    }

    return squadPower;
}

float MicroManager::getTargetsPower(const std::vector<Unit>& units) const
{
    float enemyPower = 0;

    for (auto & target : m_targets)
    {
        Unit& closestUnit = Util::CalcClosestUnit(target, units);
        enemyPower += getUnitPower(target, closestUnit);
    }

    return enemyPower;
}

float MicroManager::getUnitPower(const Unit &unit, Unit& closestUnit) const
{
    ///////// HEALTH
	//even though a unit is low life, it is still worth more than close to nothing.
	float unitPower = std::max(5.f, sqrt(unit.getHitPoints() + unit.getShields()));

    ///////// DPS
	if(closestUnit.isValid())
		unitPower *= Util::GetDpsForTarget(unit.getUnitPtr(), closestUnit.getUnitPtr(), m_bot);
	else
		unitPower *= Util::GetDps(unit.getUnitPtr(), m_bot);

    ///////// RANGE
    const float unitRange = unit.getType().getAttackRange();
    if (unitRange >= 1.5f)
        unitPower *= 3; //ranged bonus (+200%)

    ///////// ARMOR
    unitPower *= 1 + Util::GetArmor(unit.getUnitPtr(), m_bot) * 0.1f;   //armor bonus (+10% per armor)

    ///////// SPLASH DAMAGE
    //TODO bonus for splash damage

    ///////// DISTANCE
    if (closestUnit.isValid())
    {
        float distance = Util::Dist(unit.getPosition(), closestUnit.getPosition());
        if (unitRange + 1 < distance)   //if the unit can't reach the closest unit (with a small buffer)
        {
            distance -= unitRange + 1;
            float distancePenalty = unit.getType().isBuilding() ? 0.2f : 0.1f;
            unitPower = fmax(0, unitPower - (unitPower * distance * distancePenalty));  //penalty for distance (-10% per meter)
        }
    }

    ///////// TERRAIN
    /*if (averageSquadHeight > 0)
    {
        float unitHeight = m_bot.Map().terrainHeight(unit.getTilePosition().x, unit.getTilePosition().y);
        if (unitHeight > averageSquadHeight + 1)
        {
            unitPower *= 1.25f;  //height bonus (+25%)
        }
        else if (unitHeight < averageSquadHeight + 1)
        {
            unitPower *= 0.75f; //height penalty (-25%)
        }
    }*/

    m_bot.Map().drawText(unit.getPosition(), "Power: " + std::to_string(unitPower));
    
    return unitPower;
}

void MicroManager::trainSubUnits(const Unit & unit) const
{
    // TODO: something here
}