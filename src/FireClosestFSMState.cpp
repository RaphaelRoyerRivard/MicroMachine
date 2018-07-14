#include "FireClosestFSMState.h"
#include "PullBackFSMState.h"
#include "ShouldPullBackTransition.h"
#include "CCBot.h"

FireClosestFSMState::FireClosestFSMState(const sc2::Unit * unit)
{
    m_unit = unit;
}

void FireClosestFSMState::onEnter(const std::vector<const sc2::Unit*> *, CCBot*)
{
    FocusFireFSMState* pullBack = new PullBackFSMState(m_unit);
    FocusFireFSMTransition* shouldPull = new ShouldPullBackTransition(m_unit, pullBack);
    transitions = { shouldPull };
    m_target = nullptr;
}
void FireClosestFSMState::onExit() {}
std::vector<FocusFireFSMTransition*> FireClosestFSMState::getTransitions()
{
    return transitions;
}
void FireClosestFSMState::onUpdate(const sc2::Unit * target, CCBot* bot) 
{
	if(bot->Config().DrawFSMStateInfo)
		bot->Map().drawLine(CCPosition(m_unit->pos), CCPosition(target->pos), CCColor(255, 0, 0));

    //it seems that submitting an attack command as soon as the weapon cooldown is finished increases the dps dealth
    //we submit an attack command on a new target even though the cooldown is not finished because it may also move our unit
    if (m_unit->weapon_cooldown == 0 || target != m_target)
    {
		UnitType targetType(target->unit_type, *bot);
		//We want to trigger stimpack uses only against combat units
		if (targetType.isCombatUnit())
		{
			Unit unit(m_unit, *bot);
			//Use stimpack buff
			if (unit.useAbility(sc2::ABILITY_ID::EFFECT_STIM))
				return;
		}

		m_target = target;
		bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::ATTACK_ATTACK, target);
    }
}
