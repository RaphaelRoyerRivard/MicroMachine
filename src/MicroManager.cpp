#include "MicroManager.h"
#include "CCBot.h"
#include "Util.h"

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
            unit.move(regroupPosition);
        }
        else
        {
            //defend itself near the regroupPosition
            unit.attackMove(regroupPosition);
        }
    }
}

float MicroManager::getSquadPower() const
{
    float squadPower = 0;

    for (auto & unit : m_units)
    {
        squadPower += getUnitPower(unit);
    }

    return squadPower;
}

float MicroManager::getTargetsPower(float averageSquadHeight, Unit& closestUnit) const
{
    float enemyPower = 0;

    for (auto & target : m_targets)
    {
        enemyPower += getUnitPower(target, averageSquadHeight, &closestUnit);
    }

    return enemyPower;
}

float MicroManager::getUnitPower(const Unit &unit, float averageSquadHeight, Unit* closestUnit) const
{
    ///////// HEALTH
    float unitPower = sqrt(unit.getHitPoints() + unit.getShields());

    ///////// DPS
    unitPower *= Util::GetDps(unit.getUnitPtr(), m_bot);

    ///////// RANGE
    const float unitRange = unit.getType().getAttackRange();
    if (unit.getType().getAttackRange() >= 1.5f)
        unitPower *= 3; //ranged bonus (+200%)

    ///////// ARMOR
    unitPower *= 1 + Util::GetArmor(unit.getUnitPtr(), m_bot) * 0.1f;   //armor bonus (+10% per armor)

    ///////// SPLASH DAMAGE
    //TODO bonus for splash damage

    ///////// DISTANCE
    if (closestUnit != nullptr)
    {
        float distance = Util::Dist(unit.getPosition(), closestUnit->getPosition());
        if (unitRange + 1 < distance)
        {
            distance -= unitRange + 1;
            unitPower -= fmax(0, unitPower * distance * 0.1f);  //penalty for distance (-10% per meter)
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

    return unitPower;
}

void MicroManager::trainSubUnits(const Unit & unit) const
{
    // TODO: something here
}