#pragma once

#include "Common.h"
#include "sc2api/sc2_api.h"
#include "FocusFireFiniteStateMachine.h"
#include "KitingFiniteStateMachine.h"

class CCBot;

enum MicroActionType
{
	Stop,
	AttackUnit,
	AttackMove,
	Move,
	Smart,
	Ability,
	AbilityPosition,
	AbilityTarget,
	FSMKite,
	FSMFocusFire,
};

namespace Micro
{   
	MicroActionType SmartStop          (const sc2::Unit * attacker,  CCBot & bot);
	MicroActionType SmartAttackUnit    (const sc2::Unit * attacker,  const sc2::Unit * target, CCBot & bot);
	MicroActionType SmartAttackMove    (const sc2::Unit * attacker,  const sc2::Point2D & targetPosition, CCBot & bot);
	MicroActionType SmartMove          (const sc2::Unit * attacker,  const sc2::Point2D & targetPosition, CCBot & bot);
	MicroActionType SmartRightClick    (const sc2::Unit * unit,      const sc2::Unit * target, CCBot & bot);
	MicroActionType SmartRepair        (const sc2::Unit * unit,      const sc2::Unit * target, CCBot & bot);
	MicroActionType SmartKiteTarget    (const sc2::Unit * rangedUnit,const sc2::Unit * target, CCBot & bot, std::unordered_map<sc2::Tag, KitingFiniteStateMachine*> &states);
	MicroActionType SmartFocusFire     (
        const sc2::Unit * rangedUnit,
        const sc2::Unit * target, 
        const std::vector<const sc2::Unit *> * targets, 
        CCBot & bot, 
        std::unordered_map<sc2::Tag, FocusFireFiniteStateMachine*> &states,
        const std::vector<const sc2::Unit *> * units,
        std::unordered_map<sc2::Tag, float> &unitHealth
    );
	MicroActionType SmartAbility(const sc2::Unit * unit, const sc2::AbilityID & abilityID, CCBot & bot);
	MicroActionType SmartAbility(const sc2::Unit * unit, const sc2::AbilityID & abilityID, CCPosition position, CCBot & bot);
	MicroActionType SmartAbility(const sc2::Unit * unit, const sc2::AbilityID & abilityID, const sc2::Unit * target, CCBot & bot);
};