#include "SquadData.h"
#include "CCBot.h"
#include "Util.h"

SquadData::SquadData(CCBot & bot)
    : m_bot(bot)
{

}

void SquadData::onFrame()
{
    updateAllSquads();
    verifySquadUniqueMembership();
    drawSquadInformation();
}

void SquadData::clearSquadData()
{
    // give back workers who were in squads
    for (auto & kv : m_squads)
    {
        Squad & squad = kv.second;
        squad.giveBackWorkers();//TODO 99% sure its useless since similar code is in .clear()
    }

    m_squads.clear();
}

void SquadData::removeSquad(const std::string & squadName)
{
    const auto & squadPtr = m_squads.find(squadName);

    BOT_ASSERT(squadPtr != m_squads.end(), "Trying to clear a squad that didn't exist: %s", squadName.c_str());
    if (squadPtr == m_squads.end())
    {
        return;
    }

    m_squads.erase(squadName);
}

const std::map<std::string, Squad> & SquadData::getSquads() const
{
    return m_squads;
}

bool SquadData::squadExists(const std::string & squadName)
{
    return m_squads.find(squadName) != m_squads.end();
}

void SquadData::addSquad(const std::string & squadName, const Squad & squad)
{
    m_squads.insert(std::pair<std::string, Squad>(squadName, squad));
}

void SquadData::updateAllSquads()
{
    for (auto & kv : m_squads)
    {
        kv.second.onFrame();
    }
}

void SquadData::drawSquadInformation()
{
#ifdef PUBLIC_RELEASE
	return;
#endif
    if (!m_bot.Config().DrawSquadInfo)
    {
        return;
    }

    std::stringstream ss;
    ss << "Squad Data\n\n";

    for (auto & kv : m_squads)
    {
        const Squad & squad = kv.second;

        auto & units = squad.getUnits();
        const SquadOrder & order = squad.getSquadOrder();

		if(units.size() > 0)
			ss << squad.getName() << ": [units: " << units.size() << ", status: " << order.getStatus() << "]\n";

        CCPosition squadCenter = squad.calcCenter();
        float terrainHeight = m_bot.Map().terrainHeight(squadCenter.x, squadCenter.y);
        m_bot.Debug()->DebugSphereOut(sc2::Point3D(squadCenter.x, squadCenter.y, terrainHeight), squad.getMaxDistanceFromCenter(), CCColor(0, 255, 0));
        m_bot.Map().drawCircle(order.getPosition(), 5, CCColor(255, 0, 0));
        m_bot.Map().drawText(order.getPosition(), squad.getName(), CCColor(255, 0, 0));

        for (auto & unit : units)
        {
            BOT_ASSERT(unit.isValid(), "null unit");

            m_bot.Map().drawText(unit.getPosition(), squad.getName(), CCColor(0, 255, 0));
        }
    }

    m_bot.Map().drawTextScreen(0.5f, 0.2f, ss.str(), CCColor(255, 0, 0));
}

void SquadData::verifySquadUniqueMembership()
{
    std::map<Unit, std::string> assigned;

    for (const auto & kv : m_squads)
    {
        for (auto & unit : kv.second.getUnits())
        {
			const auto it = assigned.find(unit);
			if(it != assigned.end())
            {
                std::cout << "Warning: A " << unit.getType().getName() << "(" << unit.getTag() << ") is in at least two squads: " << kv.second.getName() << " and " << it->second << "\n";
            }

            assigned[unit] = kv.second.getName();
        }
    }
}

bool SquadData::unitIsInSquad(const Unit & unit) const
{
    return getUnitSquad(unit) != nullptr;
}

const Squad * SquadData::getUnitSquad(const Unit & unit) const
{
    for (const auto & kv : m_squads)
    {
        if (kv.second.containsUnit(unit))
        {
            return &kv.second;
        }
    }

    return nullptr;
}

Squad * SquadData::getUnitSquad(const Unit & unit)
{
    for (auto & kv : m_squads)
    {
        if (kv.second.containsUnit(unit))
        {
            return &kv.second;
        }
    }

    return nullptr;
}

void SquadData::assignUnitToSquad(const sc2::Unit* unitptr, Squad & squad)
{
	const Unit unit(unitptr, m_bot);

	BOT_ASSERT(canAssignUnitToSquad(unit, squad), "We shouldn't be re-assigning this unit!");

	Squad * previousSquad = getUnitSquad(unit);

	if (previousSquad)
	{
		previousSquad->removeUnit(unit);
	}

	squad.addUnit(unit);
}

void SquadData::assignUnitToSquad(const Unit & unit, Squad & squad)
{
    Squad * previousSquad = getUnitSquad(unit);

    if (previousSquad)
    {
        previousSquad->removeUnit(unit);
    }

    squad.addUnit(unit);
}

bool SquadData::canAssignUnitToSquad(const Unit & unit, const Squad & newSquad, bool considerMaxSquadDistance) const
{
    const Squad * currentSquad = getUnitSquad(unit);

	if (currentSquad && currentSquad->getName() == newSquad.getName())
		return false;

    // make sure strictly less than so we don't reassign to the same newSquad etc
    bool canAssign = !currentSquad || currentSquad->getPriority() < newSquad.getPriority();
    if (!canAssign)
        return false;

    if (considerMaxSquadDistance && newSquad.getMaxDistanceFromCenter() > 0.f && !newSquad.isEmpty())
    {
        const float distance = Util::DistSq(unit.getPosition(), newSquad.calcCenter());
        const bool closeEnough = distance < newSquad.getMaxDistanceFromCenter() * newSquad.getMaxDistanceFromCenter();
        return closeEnough;
    }
    return true;
}

Squad & SquadData::getSquad(const std::string & squadName)
{
    BOT_ASSERT(squadExists(squadName), "Trying to access squad that doesn't exist: %s", squadName.c_str());
    if (!squadExists(squadName))
    {
        int a = 10;
    }

    return m_squads.at(squadName);
}