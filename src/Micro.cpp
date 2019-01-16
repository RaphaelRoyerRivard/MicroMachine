#include "Micro.h"
#include "Util.h"
#include "CCBot.h"
#include "sc2api/sc2_api.h"

const float dotRadius = 0.1f;

MicroActionType Micro::SmartStop(const sc2::Unit * attacker, CCBot & bot)
{
    BOT_ASSERT(attacker != nullptr, "Attacker is null");
    bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::STOP);
    bot.Map().drawCircle(CCPosition(attacker->pos), 2, CCColor(255, 0, 255));
	return MicroActionType::Stop;
}

MicroActionType Micro::SmartAttackUnit(const sc2::Unit * attacker, const sc2::Unit * target, CCBot & bot)
{
    BOT_ASSERT(attacker != nullptr, "Attacker is null");
    BOT_ASSERT(target != nullptr, "Target is null");
    bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::ATTACK, target);
	return MicroActionType::AttackUnit;
}

MicroActionType Micro::SmartAttackMove(const sc2::Unit * attacker, const sc2::Point2D & targetPosition, CCBot & bot)
{
    BOT_ASSERT(attacker != nullptr, "Attacker is null");
    bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::ATTACK, targetPosition);
	return MicroActionType::AttackMove;
}

MicroActionType Micro::SmartMove(const sc2::Unit * attacker, const sc2::Point2D & targetPosition, CCBot & bot)
{
    BOT_ASSERT(attacker != nullptr, "Attacker is null");
    bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::MOVE, targetPosition);
	return MicroActionType::Move;
}

MicroActionType Micro::SmartRightClick(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot)
{
    BOT_ASSERT(unit != nullptr, "Unit is null");
    bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, target);
	return MicroActionType::RightClick;
}

MicroActionType Micro::SmartRepair(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot)
{
    BOT_ASSERT(unit != nullptr, "Unit is null");
    bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, target);
	return MicroActionType::RightClick;
}

MicroActionType Micro::SmartKiteTarget(const sc2::Unit * rangedUnit, const sc2::Unit * target, CCBot & bot, std::unordered_map<sc2::Tag, KitingFiniteStateMachine*> &state)
{
    BOT_ASSERT(rangedUnit != nullptr, "RangedUnit is null");
    BOT_ASSERT(target != nullptr, "Target is null");

    KitingFiniteStateMachine* stateMachine;
    if (state.find(rangedUnit->tag) == state.end()) {
        stateMachine = new KitingFiniteStateMachine(rangedUnit, target);
    }
    else
        stateMachine = state[rangedUnit->tag];

    stateMachine->update(target, &bot);
    state.insert_or_assign(rangedUnit->tag, stateMachine);
	return MicroActionType::FSMKite;
}

MicroActionType Micro::SmartFocusFire(const sc2::Unit * rangedUnit, const sc2::Unit * target, const std::vector<const sc2::Unit *> * targets, CCBot & bot, std::unordered_map<sc2::Tag, FocusFireFiniteStateMachine*> &state, const std::vector<const sc2::Unit *> * units, std::unordered_map<sc2::Tag, float> &unitHealth)
{
    BOT_ASSERT(rangedUnit != nullptr, "RangedUnit is null");
    BOT_ASSERT(target != nullptr, "Target is null");

    FocusFireFiniteStateMachine* stateMachine;
    if (state.find(rangedUnit->tag) == state.end())
        stateMachine = new FocusFireFiniteStateMachine(rangedUnit, target, targets, &bot);
    else stateMachine = state[rangedUnit->tag];
    
    stateMachine->update(target, targets, units, &unitHealth, &bot);
    state.insert_or_assign(rangedUnit->tag, stateMachine);
	return MicroActionType::FSMFocusFire;
}

MicroActionType Micro::SmartAbility(const sc2::Unit * unit, const sc2::AbilityID & abilityID, CCBot & bot)
{
	BOT_ASSERT(unit != nullptr, "Unit using smart ability is null");
	bot.Actions()->UnitCommand(unit, abilityID);
	return MicroActionType::Ability;
}

MicroActionType Micro::SmartAbility(const sc2::Unit * unit, const sc2::AbilityID & abilityID, CCPosition position, CCBot & bot)
{
	BOT_ASSERT(unit != nullptr, "Unit using smart ability is null");
	bot.Actions()->UnitCommand(unit, abilityID, position);
	return MicroActionType::AbilityPosition;
}

MicroActionType Micro::SmartAbility(const sc2::Unit * unit, const sc2::AbilityID & abilityID, const sc2::Unit * target, CCBot & bot)
{
	BOT_ASSERT(unit != nullptr, "Unit using smart ability is null");
	BOT_ASSERT(target != nullptr, "Target of smart ability is null");
	bot.Actions()->UnitCommand(unit, abilityID, target);
	return MicroActionType::AbilityTarget;
}