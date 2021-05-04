#pragma once

#include "Common.h"
#include "sc2api/sc2_api.h"

class CCBot;

enum MicroActionType
{
	Stop,
	AttackUnit,
	AttackMove,
	Move,
	RightClick,
	Ability,
	AbilityPosition,
	AbilityTarget,
	ToggleAbility,
	FSMKite,
	FSMFocusFire,
};

const std::string MicroActionTypeAccronyms[] = {"S", "AU", "AM", "M", "RC", "A", "AP", "AT", "K", "FF"};

namespace Micro
{
	MicroActionType SmartStop          (const sc2::Unit * attacker,  CCBot & bot);
	MicroActionType SmartAttackUnit    (const sc2::Unit * attacker,  const sc2::Unit * target, CCBot & bot);
	MicroActionType SmartAttackMove    (const sc2::Unit * attacker,  const sc2::Point2D & targetPosition, CCBot & bot);
	MicroActionType SmartMove          (const sc2::Unit * unit,  const sc2::Point2D & targetPosition, CCBot & bot);
	MicroActionType SmartHold          (const sc2::Unit * unit, bool queued, CCBot & bot);
	MicroActionType SmartRightClick    (const sc2::Unit * unit,      const sc2::Unit * target, CCBot & bot);
	MicroActionType SmartRightClick    (const sc2::Units & units, const sc2::Unit * target, CCBot & bot);
	MicroActionType SmartRepair        (const sc2::Unit * unit,      const sc2::Unit * target, CCBot & bot);
	MicroActionType SmartAbility	   (const sc2::Unit * unit, const sc2::AbilityID & abilityID, CCBot & bot);
	MicroActionType SmartAbility	   (const sc2::Unit * unit, const sc2::AbilityID & abilityID, CCPosition position, CCBot & bot);
	MicroActionType SmartAbility	   (const sc2::Unit * unit, const sc2::AbilityID & abilityID, const sc2::Unit * target, CCBot & bot);
	MicroActionType SmartToggleAutoCast(const sc2::Unit * unit, const sc2::AbilityID & abilityID, CCBot & bot);
};