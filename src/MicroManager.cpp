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

void MicroManager::setTargets(const std::vector<Unit> & t)
{
    m_targets = t;
}

void MicroManager::execute(const SquadOrder & inputOrder)
{
    // Nothing to do if we have no units
    if (m_units.empty() || !(inputOrder.getType() == SquadOrderTypes::Attack || inputOrder.getType() == SquadOrderTypes::Defend))
    {
        return;
    }

    order = inputOrder;

    // Discover enemies within region of interest
    std::set<Unit> nearbyEnemies;

    // if the order is to defend, we only care about units in the radius of the defense
    if (order.getType() == SquadOrderTypes::Defend)
    {
        for (auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
        {
            if (Util::Dist(enemyUnit, order.getPosition()) < order.getRadius())
            {
                nearbyEnemies.insert(enemyUnit);
            }
        }

    } // otherwise we want to see everything on the way as well
    else if (order.getType() == SquadOrderTypes::Attack)
    {
        for (auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
        {
            if (Util::Dist(enemyUnit, order.getPosition()) < order.getRadius())
            {
                nearbyEnemies.insert(enemyUnit);
            }
        }

        for (auto & unit : m_units)
        {
            BOT_ASSERT(unit.isValid(), "null unit in attack");

            for (auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
            {
                if (Util::Dist(enemyUnit, unit) < order.getRadius())
                {
                    nearbyEnemies.insert(enemyUnit);
                }
            }
        }
    }

    std::vector<Unit> targetUnitTags;
    std::copy(nearbyEnemies.begin(), nearbyEnemies.end(), std::back_inserter(targetUnitTags));
    m_targets = targetUnitTags;

    // the following block of code attacks all units on the way to the order position
    // we want to do this if the order is attack, defend, or harass
    if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend)
    {
        // if this is a defense squad then we care about all units in the area
        if (order.getType() == SquadOrderTypes::Defend)
        {
            executeMicro(targetUnitTags);
        }
        // otherwise we only care about workers if they are in their own region
        // this is because their scout can run around and drag our units around the map
        else
        {
            // Allow micromanager to handle enemies
            executeMicro(targetUnitTags);
        }
    }
}

const std::vector<Unit> & MicroManager::getUnits() const
{
    return m_units;
}

void MicroManager::regroup(const CCPosition & regroupPosition) const
{
    //CCPosition ourBasePosition = m_bot.GetStartLocation();
    //int regroupDistanceFromBase = m_bot.Map().getGroundDistance(regroupPosition, ourBasePosition);

    // for each of the units we have
    for (auto & unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit in MicroManager regroup");

        /*int unitDistanceFromBase = m_bot.Map().getGroundDistance(unit.getPosition(), ourBasePosition);

        // if the unit is outside the regroup area
        if (unitDistanceFromBase > regroupDistanceFromBase)
        {
            unit.move(ourBasePosition);
        }
        else*/ if (Util::Dist(unit, regroupPosition) > 4)
        {
            // regroup it
            unit.move(regroupPosition);
        }
        else
        {
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

float MicroManager::getTargetsPower() const
{
    float enemyPower = 0;

    for (auto & target : m_targets)
    {
        enemyPower += getUnitPower(target);
    }

    return enemyPower;
}

float MicroManager::getUnitPower(const Unit &unit) const
{
    float unitPower = sqrt(unit.getHitPoints() + unit.getShields());
    unitPower *= Util::GetDps(unit.getUnitPtr(), m_bot);

    if (unit.getType().getAttackRange() >= 1.5f)
        unitPower *= 2; //ranged bonus

    unitPower *= 1 + Util::GetArmor(unit.getUnitPtr(), m_bot) * 0.1f;

    //TODO bonus for splash damage
    //TODO bonus for terrain

    return unitPower;
}

void MicroManager::trainSubUnits(const Unit & unit) const
{
    // TODO: something here
}