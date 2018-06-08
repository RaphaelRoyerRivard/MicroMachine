#include "Squad.h"
#include "CCBot.h"
#include "Util.h"

Squad::Squad(CCBot & bot)
    : m_bot(bot)
    , m_regroupStartFrame(0)
    , m_maxRegroupDuration(0)
    , m_regroupCooldown(0)
    , m_maxDistanceFromCenter(0)
    , m_priority(0)
    , m_name("Default")
    , m_meleeManager(bot)
    , m_rangedManager(bot)
{

}

Squad::Squad(const std::string & name, const SquadOrder & order, size_t priority, CCBot & bot)
    : m_bot(bot)
    , m_name(name)
    , m_order(order)
    , m_regroupStartFrame(0)
    , m_maxRegroupDuration(0)
    , m_regroupCooldown(0)
    , m_maxDistanceFromCenter(0)
    , m_priority(priority)
    , m_meleeManager(bot)
    , m_rangedManager(bot)
{
}

Squad::Squad(const std::string & name, const SquadOrder & order, int maxRegroupDuration, int regroupCooldown, float maxDistanceFromCenter, size_t priority, CCBot & bot)
    : m_bot(bot)
    , m_name(name)
    , m_order(order)
    , m_regroupStartFrame(0)
    , m_maxRegroupDuration(maxRegroupDuration)
    , m_regroupCooldown(regroupCooldown)
    , m_maxDistanceFromCenter(maxDistanceFromCenter)
    , m_priority(priority)
    , m_meleeManager(bot)
    , m_rangedManager(bot)
{
}

void Squad::onFrame()
{
    // update all necessary unit information within this squad
    updateUnits();

    if (m_order.getType() == SquadOrderTypes::Retreat)
    {
        CCPosition retreatPosition = calcRetreatPosition();

        m_bot.Map().drawCircle(retreatPosition, 3, CCColor(255, 0, 255));

        m_meleeManager.regroup(retreatPosition);
        m_rangedManager.regroup(retreatPosition);
    }
    else if (m_order.getType() == SquadOrderTypes::Regroup)
    {
        CCPosition regroupPosition = calcCenter();

        m_bot.Map().drawCircle(regroupPosition, 3, CCColor(0, 0, 255));

        m_meleeManager.regroup(regroupPosition);
        m_rangedManager.regroup(regroupPosition);
    }
    else // otherwise, execute micro
    {
        // Nothing to do if we have no units
        if (!m_units.empty() && (m_order.getType() == SquadOrderTypes::Attack || m_order.getType() == SquadOrderTypes::Defend))
        {
            std::vector<Unit> targets = calcTargets();

            m_meleeManager.setTargets(targets);
            m_rangedManager.setTargets(targets);

            //TODO remove the order dependancy
            m_meleeManager.setOrder(m_order);
            m_rangedManager.setOrder(m_order);

            m_meleeManager.executeMicro();
            m_rangedManager.executeMicro();
        }
    }
}

std::vector<Unit> Squad::calcTargets() const
{
    // Discover enemies within region of interest
    std::vector<Unit> nearbyEnemies;

    // if the order is to defend, we only care about units in the radius of the defense
    if (m_order.getType() == SquadOrderTypes::Defend)
    {
        for (auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
        {
            if (Util::Dist(enemyUnit, m_order.getPosition()) < m_order.getRadius())
            {
                nearbyEnemies.push_back(enemyUnit);
            }
        }

    } // otherwise we want to see everything on the way as well
    else if (m_order.getType() == SquadOrderTypes::Attack)
    {
        CCPosition center = calcCenter();
        for (auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
        {
            if (Util::Dist(enemyUnit, center) < m_order.getRadius())
            {
                nearbyEnemies.push_back(enemyUnit);
            }
        }
    }
    
    return nearbyEnemies;
}

bool Squad::isEmpty() const
{
    return m_units.empty();
}

float Squad::getMaxDistanceFromCenter() const
{
    return m_maxDistanceFromCenter;
}

void Squad::setMaxDistanceFromCenter(const float &maxDistanceFromCenter)
{
    m_maxDistanceFromCenter = maxDistanceFromCenter;
}

size_t Squad::getPriority() const
{
    return m_priority;
}

void Squad::setPriority(const size_t & priority)
{
    m_priority = priority;
}

void Squad::updateUnits()
{
    setAllUnits();
    //setNearEnemyUnits();
    addUnitsToMicroManagers();
}

void Squad::setAllUnits()
{
    // clean up the _units vector just in case one of them died
    std::vector<Unit> goodUnits;
    for (auto & unit : m_units)
    {
        if (!unit.isValid()) { continue; }
        if (unit.isBeingConstructed()) { continue; }
        if (unit.getHitPoints() <= 0) { continue; }
        if (!unit.isAlive()) { continue; }
        
        goodUnits.push_back(unit);
    }

    m_units = goodUnits;

    CCPosition center = calcCenter();
    m_bot.Map().drawCircle(center, 1.f);
    for (auto & unit : m_units)
        m_bot.Map().drawLine(unit.getPosition(), center);
}

void Squad::setNearEnemyUnits()
{
    m_nearEnemy.clear();
    for (auto & unit : m_units)
    {
        m_nearEnemy[unit] = isUnitNearEnemy(unit);

        CCColor color = m_nearEnemy[unit] ? m_bot.Config().ColorUnitNearEnemy : m_bot.Config().ColorUnitNotNearEnemy;
        //m_bot.Map().drawCircleAroundUnit(unitTag, color);
    }
}

void Squad::addUnitsToMicroManagers()
{
    std::vector<Unit> meleeUnits;
    std::vector<Unit> rangedUnits;
    std::vector<Unit> detectorUnits;
    std::vector<Unit> transportUnits;
    std::vector<Unit> tankUnits;

    // add _units to micro managers
    for (auto & unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit in addUnitsToMicroManagers()");

        if (unit.getType().isTank())
        {
            tankUnits.push_back(unit);
        }
        // TODO: detectors
        else if (unit.getType().isDetector() && !unit.getType().isBuilding())
        {
            detectorUnits.push_back(unit);
        }
        // select ranged _units
        else if (unit.getType().getAttackRange() >= 1.5f)
        {
            rangedUnits.push_back(unit);
        }
        // select melee _units
        else if (unit.getType().getAttackRange() < 1.5f)
        {
            meleeUnits.push_back(unit);
        }
    }

    m_meleeManager.setUnits(meleeUnits);
    m_rangedManager.setUnits(rangedUnits);
    //m_tankManager.setUnits(tankUnits);
}

bool Squad::needsToRetreat() const
{
    //Only the main attack can retreat
    if (m_name != "MainAttack")
        return false;

	float averageSpeed = (m_meleeManager.getAverageSquadSpeed() + m_rangedManager.getAverageSquadSpeed()) / 2.f;
	float averageTargetsSpeed = m_rangedManager.getAverageTargetsSpeed();
	//TODO also consider the range (if targets are not in range, we should still back)
	if (averageSpeed < averageTargetsSpeed * 0.90f)	//Even though the enemy units are a little bit faster, maybe we should still back
		return false;

    float meleePower = m_meleeManager.getSquadPower();
    float rangedPower = m_rangedManager.getSquadPower();
    //float averageHeight = calcAverageHeight();
    float targetsPower = m_rangedManager.getTargetsPower(m_units);
    return meleePower + rangedPower < targetsPower * 0.90f; //We believe we can beat a slightly more powerful army with our good micro
}

bool Squad::needsToRegroup() const
{
    //Only the main attack can regroup
    if (m_name != "MainAttack")
        return false;

    int currentFrame = m_bot.GetCurrentFrame();

    //Is last regroup too recent?
    if (m_order.getType() != SquadOrderTypes::Regroup && currentFrame - m_regroupStartFrame < m_regroupCooldown)
        return false;
    
    //Is current regroup taking too long?
    if (m_order.getType() == SquadOrderTypes::Regroup && currentFrame - m_regroupStartFrame > m_maxRegroupDuration)
        return false;

    //do not regroup if targets are nearby (nor fleeing, since the targets are updated only whan having an Attack order)
    std::vector<Unit>& targets = calcTargets();
    if (!targets.empty())
        return false;

    //Regroup only if center is walkable
    CCPosition squadCenter = calcCenter();
    if (!m_bot.Map().isWalkable(squadCenter))
        return false;

    float averageDistance = 0;
    for (auto & unit : m_units)
    {
        averageDistance += Util::Dist(unit.getPosition(), squadCenter);
    }
    averageDistance /= m_units.size();

    bool shouldRegroup = false;
    if(m_order.getType() == SquadOrderTypes::Regroup)
        shouldRegroup = averageDistance > sqrt(m_units.size()) * 0.3f;
    else
        shouldRegroup = averageDistance > sqrt(m_units.size()) * 0.5f;
    return shouldRegroup;
}

void Squad::setSquadOrder(const SquadOrder & so)
{
    if (so.getType() == SquadOrderTypes::Regroup && m_order.getType() != SquadOrderTypes::Regroup)
        m_regroupStartFrame = m_bot.GetCurrentFrame();
    m_order = so;
}

bool Squad::containsUnit(const Unit & unit) const
{
    return std::find(m_units.begin(), m_units.end(), unit) != m_units.end();
}

void Squad::clear()
{
    for (auto & unit : getUnits())
    {
        BOT_ASSERT(unit.isValid(), "null unit in squad clear");

        if (unit.getType().isWorker())
        {
            m_bot.Workers().finishedWithWorker(unit);
        }
    }

    m_units.clear();
}

bool Squad::isUnitNearEnemy(const Unit & unit) const
{
    BOT_ASSERT(unit.isValid(), "null unit in squad");

    for (auto & u : m_bot.GetUnits())
    {
        if ((u.getPlayer() == Players::Enemy) && (Util::Dist(unit, u) < 20))
        {
            return true;
        }
    }

    return false;
}

CCPosition Squad::calcCenter() const
{
    //TODO keep a reference to calc only once per frame
    return Util::CalcCenter(m_units);
}

float Squad::calcAverageHeight() const
{
    float averageHeight = 0;
    for (auto & unit : m_units)
    {
        averageHeight += m_bot.Map().terrainHeight(unit.getTilePosition().x, unit.getTilePosition().y);
    }
    averageHeight /= m_units.size();
    return averageHeight;
}

CCPosition Squad::calcRetreatPosition() const
{
    CCPosition retreatPosition(0, 0);
    if(auto startingBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self))
        retreatPosition = startingBase->getPosition();
    return retreatPosition;

    /*CCPosition regroup(0, 0);

    float minDist = std::numeric_limits<float>::max();

    for (auto & unit : m_units)
    {
        if (!m_nearEnemy.at(unit))
        {
            float dist = Util::Dist(m_order.getPosition(), unit.getPosition());
            if (dist < minDist)
            {
                minDist = dist;
                regroup = unit.getPosition();
            }
        }
    }

    if (regroup.x == 0.0f && regroup.y == 0.0f)
    {
        return m_bot.GetStartLocation();
    }
    else
    {
        return regroup;
    }*/
}

Unit Squad::unitClosestToEnemy() const
{
    Unit closest;
    float closestDist = std::numeric_limits<float>::max();

    for (auto & unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit");

        // the distance to the order position
        int dist = m_bot.Map().getGroundDistance(unit.getPosition(), m_order.getPosition());

        if (dist != -1 && (!closest.isValid() || dist < closestDist))
        {
            closest = unit;
            closestDist = (float)dist;
        }
    }

    return closest;
}

int Squad::squadUnitsNear(const CCPosition & p) const
{
    int numUnits = 0;

    for (auto & unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit");

        if (Util::Dist(unit, p) < 20.0f)
        {
            numUnits++;
        }
    }

    return numUnits;
}

const std::vector<Unit> & Squad::getUnits() const
{
    return m_units;
}

const SquadOrder & Squad::getSquadOrder()	const
{
    return m_order;
}

void Squad::addUnit(const Unit & unit)
{
    m_units.push_back(unit);
}

void Squad::removeUnit(const Unit & unit)
{
    // this is O(n) but whatever
    for (size_t i = 0; i < m_units.size(); ++i)
    {
        if (unit == m_units[i])
        {
            m_units.erase(m_units.begin() + i);
            break;
        }
    }
}

void Squad::giveBackWorkers()
{
    for (auto & unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit");

        if (unit.getType().isWorker())
        {
            m_bot.Workers().finishedWithWorker(unit);
        }
    }
}

const std::string & Squad::getName() const
{
    return m_name;
}