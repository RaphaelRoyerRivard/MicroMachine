#include "AttackFSMState.h"
#include "FleeFSMState.h"
#include "ShouldFleeTransition.h"
#include "CCBot.h"

AttackFSMState::AttackFSMState(const sc2::Unit * unit, const sc2::Unit * target)
{
    m_unit = unit;
    m_target = target;
	actionSent = false;
}

void AttackFSMState::onEnter()
{
    KitingFSMState* flee = new FleeFSMState(m_unit, m_target);
    KitingFSMTransition* shouldFlee = new ShouldFleeTransition(m_unit, m_target, flee);
    transitions = { shouldFlee };
}

void AttackFSMState::onExit() {
	actionSent = false;
}

std::vector<KitingFSMTransition*> AttackFSMState::getTransitions()
{
    return transitions;
}

void AttackFSMState::onUpdate(const sc2::Unit * target, CCBot* bot)
{
	if (bot->Config().DrawFSMStateInfo)
		bot->Map().drawLine(CCPosition(m_unit->pos), CCPosition(target->pos), CCColor(255, 0, 0));
	
	UnitType targetType(target->unit_type, *bot);
	//We want to trigger stimpack uses only against combat units
	if (targetType.isCombatUnit())
	{
		Unit unit(m_unit, *bot);
		//Use stimpack buff
		if (unit.useAbility(sc2::ABILITY_ID::EFFECT_STIM))
			return;
	}

	if (!actionSent)
	{
		m_target = target;
		bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::ATTACK_ATTACK, target);
		actionSent = true;
	}
}
