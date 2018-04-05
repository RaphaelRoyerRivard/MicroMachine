#include "RangedManager.h"
#include "Util.h"
#include "CCBot.h"
#include "BehaviorTreeBuilder.h"
#include "RangedActions.h"
#include <algorithm>
#include <string>
#include "AlphaBetaConsideringDurations.h"
#include "AlphaBetaUnit.h"
#include "AlphaBetaMove.h"
#include "AlphaBetaAction.h"


RangedManager::RangedManager(CCBot & bot) : MicroManager(bot)
{ }

void RangedManager::executeMicro(const std::vector<Unit> & targets)
{
    assignTargets(targets);
}

void RangedManager::assignTargets(const std::vector<Unit> & targets)
{
    const std::vector<Unit> &units = getUnits();

    std::vector<const sc2::Unit *> rangedUnits;
    for (auto unit : units)
    {
        const sc2::Unit * rangedUnit = unit.getUnitPtr();
        rangedUnits.push_back(rangedUnit);
    }

    // figure out targets
    std::vector<const sc2::Unit *> rangedUnitTargets;
    for (auto target : targets)
    {
        auto targetPtr = target.getUnitPtr();
        if (!targetPtr) { continue; }
        if (targetPtr->unit_type == sc2::UNIT_TYPEID::ZERG_EGG) { continue; }
        if (targetPtr->unit_type == sc2::UNIT_TYPEID::ZERG_LARVA) { continue; }

        rangedUnitTargets.push_back(targetPtr);
    }

    // Use alpha-beta (considering durations) for combat
    // TODO: Split theses into Combat managers and use IOC and dependecy injection or something instead of a vulgar if
    if (m_bot.Config().AlphaBetaPruning) {

        std::vector<AlphaBetaUnit *> minUnits;
        std::vector<AlphaBetaUnit *> maxUnits;

        for (auto unit : rangedUnits) {
            if (m_units_actions.find(unit->tag) != m_units_actions.end()) {
                AlphaBetaUnit * old = m_units_actions[unit->tag];
                maxUnits.push_back(new AlphaBetaUnit(unit, &m_bot, old->previous_action));
            }
            else maxUnits.push_back(new AlphaBetaUnit(unit, &m_bot, nullptr));
        }

        // TODO: Keep enemy unit's actions too ?
        for (auto unit : rangedUnitTargets) {
            minUnits.push_back(new AlphaBetaUnit(unit, &m_bot, nullptr));
        }
        size_t depth = 6;
        AlphaBetaConsideringDurations alphaBeta = AlphaBetaConsideringDurations(40, depth);
        AlphaBetaValue value = alphaBeta.doSearch(maxUnits, minUnits, &m_bot);
        size_t nodes = alphaBeta.nodes_evaluated;
        m_bot.Map().drawTextScreen(0.005, 0.005, std::string("Nodes explored : ") + std::to_string(nodes));
        m_bot.Map().drawTextScreen(0.005, 0.020, std::string("Max depth : ") + std::to_string(depth));
        m_bot.Map().drawTextScreen(0.005, 0.035, std::string("AB value : ") + std::to_string(value.score));
        if (value.move != NULL) {
            for (auto action : value.move->actions) {
                if (action->type == AlphaBetaActionType::ATTACK) {
                    Micro::SmartAttackUnit(action->unit->actual_unit, action->target->actual_unit, m_bot);
                }
                else if (action->type == AlphaBetaActionType::MOVE) {
                    Micro::SmartMove(action->unit->actual_unit, action->position, m_bot);
                }
                m_units_actions.insert_or_assign(action->unit->actual_unit->tag, action->unit);
            }
        }
    }
    // use good-ol' BT
    else {
        // for each rangedUnit
        for (auto rangedUnit : rangedUnits)
        {
            BOT_ASSERT(rangedUnit, "ranged unit is null");

            const sc2::Unit * target = nullptr;
            const sc2::Unit * mineral = nullptr;
            sc2::Point2D mineralPos;

            ConditionAction isEnemyInSight(rangedUnitTargets.size() > 0 && (target = getTarget(rangedUnit, rangedUnitTargets)) != nullptr),
                isEnemyRanged(target != nullptr && isTargetRanged(target));

            if (mineral == nullptr) mineralPos = sc2::Point2D(0, 0);
            else mineralPos = mineral->pos;

            FocusFireAction focusFireAction(rangedUnit, target, &rangedUnitTargets, m_bot, m_focusFireStates, &rangedUnits, m_unitHealth);
            KiteAction kiteAction(rangedUnit, target, m_bot, m_kittingStates);
            GoToObjectiveAction goToObjectiveAction(rangedUnit, order.getPosition(), m_bot);

            BehaviorTree* bt = BehaviorTreeBuilder()
                .selector()
                .sequence()
                .condition(&isEnemyInSight).end()
                .selector()
                .sequence()
                .condition(&isEnemyRanged).end()
                .action(&focusFireAction).end()
                .end()
                .action(&kiteAction).end()
                .end()
                .end()
                .action(&goToObjectiveAction).end()
                .end()
                .end();

            bt->tick();
        }
    }
}

// get a target for the ranged unit to attack
const sc2::Unit * RangedManager::getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets)
{
    BOT_ASSERT(rangedUnit, "null ranged unit in getTarget");

    float highestPriority = 0.f;
    const sc2::Unit * bestTarget = nullptr;

    // for each target possiblity
    for (auto targetUnit : targets)
    {
        BOT_ASSERT(targetUnit, "null target unit in getTarget");

        float priority = getAttackPriority(rangedUnit, targetUnit);

        // if it's a higher priority, set it
        if (!bestTarget || priority > highestPriority)
        {
            highestPriority = priority;
            bestTarget = targetUnit;
        }
    }

    return bestTarget;
}

float RangedManager::getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * target)
{
    BOT_ASSERT(target, "null unit in getAttackPriority");
    
    if (Unit(target, m_bot).getType().isCombatUnit())
    {
        float dps = Util::GetDpsForTarget(target, attacker, m_bot);
        if (dps == 0.f)
            dps = 15.f;
        float healthValue = 1 / (target->health + target->shield);
        float distanceValue = 1 / Util::Dist(attacker->pos, target->pos);
        if (distanceValue > Util::GetAttackRangeForTarget(attacker, target, m_bot))
            distanceValue /= 2;
        //TODO try to give different weights to each variables
        //return 5 + dps * healthValue * distanceValue;
        return dps + healthValue * 100 + distanceValue * 50;
    }

    if (Unit(target, m_bot).getType().isWorker())
    {
        return 2;
    }

    return 1;
}

// according to http://wiki.teamliquid.net/starcraft2/Range
// the maximum range of a melee unit is 1 (ultralisk)
bool RangedManager::isTargetRanged(const sc2::Unit * target)
{
    BOT_ASSERT(target, "target is null");
    sc2::UnitTypeData unitTypeData = Util::GetUnitTypeDataFromUnitTypeId(target->unit_type, m_bot);
    float maxRange = 0.f;

    for (sc2::Weapon & weapon : unitTypeData.weapons)
        maxRange = std::max(maxRange, weapon.range);
    return maxRange > 1.f;
}

const sc2::Unit * RangedManager::getClosestMineral(const sc2::Unit * unit) {
    const sc2::Unit * closestShard = nullptr;
    auto potentialMinerals = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Neutral);
    for (auto mineral : potentialMinerals)
    {
        if (closestShard == nullptr && mineral->is_on_screen) {
            closestShard = mineral;
        }
        else if (mineral->unit_type == 1680 && Util::Dist(mineral->pos, unit->pos) < Util::Dist(closestShard->pos, unit->pos) && mineral->is_on_screen) {
            closestShard = mineral;
        }
    }
    return closestShard;
}

