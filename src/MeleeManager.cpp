#include "MeleeManager.h"
#include "Util.h"
#include "CCBot.h"

MeleeManager::MeleeManager(CCBot & bot)
    : MicroManager(bot)
{

}

void MeleeManager::setTargets(const std::vector<Unit> & targets)
{
    std::vector<Unit> meleeUnitTargets;
    for (auto & target : targets)
    {
        if (!target.isValid()) { continue; }
        if (target.isFlying()) { continue; }
        if (target.getType().isEgg()) { continue; }
        if (target.getType().isLarva()) { continue; }

        meleeUnitTargets.push_back(target);
    }
    m_targets = meleeUnitTargets;
}

void MeleeManager::executeMicro()
{
    const std::vector<Unit> & meleeUnits = getUnits();

    // for each meleeUnit
    for (auto & meleeUnit : meleeUnits)
    {
        BOT_ASSERT(meleeUnit.isValid(), "melee unit is null");

        // if the order is to attack or defend
        if (m_order.getType() == SquadOrderTypes::Attack || m_order.getType() == SquadOrderTypes::Defend)
        {
            // run away if we meet the retreat critereon
            if (meleeUnitShouldRetreat(meleeUnit, m_targets))
            {
                CCPosition fleeTo(Util::GetPosition(m_bot.Bases().getClosestBasePosition(meleeUnit.getUnitPtr())));

                meleeUnit.move(fleeTo);
            }
            // if there are targets
            else if (!m_targets.empty())
            {
                // find the best target for this meleeUnit
                Unit target = getTarget(meleeUnit, m_targets);

                // attack it
				Micro::SmartAttackUnit(meleeUnit.getUnitPtr(), target.getUnitPtr(), m_bot);
            }
            // if there are no targets
            else
            {
                // if we're not near the order position
                if (Util::DistSq(meleeUnit, m_order.getPosition()) > 4 * 4)
                {
                    // move to it
                    meleeUnit.move(m_order.getPosition());
                }
            }
        }
		else if(m_order.getType() == SquadOrderTypes::Retreat)
		{
			const BaseLocation* closestBaseLocation = m_bot.Bases().getClosestOccupiedBaseLocationForUnit(meleeUnit);
			if (closestBaseLocation)
			{
				CCPosition fleePosition = Util::PathFinding::FindOptimalPathToSafety(meleeUnit.getUnitPtr(), closestBaseLocation->getPosition(), m_bot);
				if (fleePosition != CCPosition())
				{
					Micro::SmartMove(meleeUnit.getUnitPtr(), fleePosition, m_bot);
				}
			}
		}

        /*if (m_bot.Config().DrawUnitTargetInfo)
        {
            // TODO: draw the line to the unit's target
        }*/
    }
}

// get a target for the meleeUnit to attack
Unit MeleeManager::getTarget(Unit meleeUnit, const std::vector<Unit> & targets)
{
    BOT_ASSERT(meleeUnit.isValid(), "null melee unit in getTarget");

    int highPriority = 0;
    double closestDist = std::numeric_limits<double>::max();
    Unit closestTarget;

    // for each target possiblity
    for (auto & targetUnit : targets)
    {
        BOT_ASSERT(targetUnit.isValid(), "null target unit in getTarget");

        const int priority = getAttackPriority(meleeUnit, targetUnit);
        const float distance = Util::DistSq(meleeUnit, targetUnit);

        // if it's a higher priority, or it's closer, set it
        if (!closestTarget.isValid() || (priority > highPriority) || (priority == highPriority && distance < closestDist))
        {
            closestDist = distance;
            highPriority = priority;
            closestTarget = targetUnit;
        }
    }

    return closestTarget;
}

// get the attack priority of a type in relation to a zergling
int MeleeManager::getAttackPriority(Unit attacker, const Unit & unit)
{
    BOT_ASSERT(unit.isValid(), "null unit in getAttackPriority");

    if (unit.getType().isCombatUnit())
    {
        return 10;
    }

    if (unit.getType().isWorker())
    {
        return 9;
    }

    return 1;
}

bool MeleeManager::meleeUnitShouldRetreat(Unit meleeUnit, const std::vector<Unit> & targets)
{
    // TODO: should melee units ever retreat?
    return false;
}
