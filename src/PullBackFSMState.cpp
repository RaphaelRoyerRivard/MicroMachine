#include "PullBackFSMState.h"
#include "FireClosestFSMState.h"
#include "DonePullBackTransition.h"
#include "CCBot.h"
#include "Util.h"
#include <algorithm>

PullBackFSMState::PullBackFSMState(const sc2::Unit * unit)
{
    m_unit = unit;
}

void PullBackFSMState::onEnter(const std::vector<const sc2::Unit*> * targets, CCBot* bot)
{
    const sc2::Unit* closestTarget = targets->at(0);
    float minDist = Util::DistSq(closestTarget->pos, m_unit->pos);
    for (auto target : *targets)
    {
        const float targetDist = Util::DistSq(target->pos, m_unit->pos);
        if (targetDist < minDist)
        {
            closestTarget = target;
            minDist = targetDist;
        }
    }

    const float targetRange = Util::GetAttackRangeForTarget(closestTarget, m_unit, *bot);

    sc2::Point2D direction = m_unit->pos - closestTarget->pos;
    Util::Normalize(direction);
    m_position = (direction * (targetRange + 1.5f)) + closestTarget->pos;
    if (!bot->Map().isWalkable(Util::GetTilePosition(m_position)))
        m_position = m_unit->pos;

    FocusFireFSMState* fireClosest = new FireClosestFSMState(m_unit);
    FocusFireFSMTransition* donePull = new DonePullBackTransition(m_unit, m_position, fireClosest);
    this->transitions = { donePull };

	bot->GetCommandMutex().lock();
	Micro::SmartMove(m_unit, m_position, *bot);
	bot->GetCommandMutex().unlock();
	if (bot->Config().DrawFSMStateInfo)
		bot->Map().drawLine(CCPosition(m_unit->pos), CCPosition(m_position), CCColor(0, 0, 255));
}

void PullBackFSMState::onExit() {}

std::vector<FocusFireFSMTransition*> PullBackFSMState::getTransitions()
{
    return transitions;
}

void PullBackFSMState::onUpdate(const sc2::Unit * target, CCBot* bot)
{
    //bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::MOVE, m_position);
}
