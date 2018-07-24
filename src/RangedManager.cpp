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
#include "UCTConsideringDurations.h"
#include "UCTCDUnit.h"
#include "UCTCDMove.h"
#include "UCTCDAction.h"


RangedManager::RangedManager(CCBot & bot) : MicroManager(bot)
{ }

void RangedManager::setTargets(const std::vector<Unit> & targets)
{
    std::vector<Unit> rangedUnitTargets;
    for (auto & target : targets)
    {
        auto targetPtr = target.getUnitPtr();
        if (!targetPtr) { continue; }
        if (targetPtr->unit_type == sc2::UNIT_TYPEID::ZERG_EGG) { continue; }
        if (targetPtr->unit_type == sc2::UNIT_TYPEID::ZERG_LARVA) { continue; }
		if (m_harassMode && target.getType().isBuilding() && !target.getType().isCombatUnit()) { continue; }

        rangedUnitTargets.push_back(target);
    }
    m_targets = rangedUnitTargets;
}

void RangedManager::executeMicro()
{
    const std::vector<Unit> &units = getUnits();

    std::vector<const sc2::Unit *> rangedUnits;
    for (auto & unit : units)
    {
        const sc2::Unit * rangedUnit = unit.getUnitPtr();
        rangedUnits.push_back(rangedUnit);
    }

    std::vector<const sc2::Unit *> rangedUnitTargets;
    for (auto target : m_targets)
    {
        rangedUnitTargets.push_back(target.getUnitPtr());
    }

    // Use UCTCD
    if (m_bot.Config().UCTCD)
	{
        UCTCD(rangedUnits, rangedUnitTargets);
    }
    // Use alpha-beta (considering durations) for combat
    else if (m_bot.Config().AlphaBetaPruning)
	{
        AlphaBetaPruning(rangedUnits, rangedUnitTargets);
    }
	else if (m_harassMode)
	{
		HarassLogic(rangedUnits, rangedUnitTargets);
	}
    // use good-ol' BT
    else 
	{
        // for each rangedUnit
        for (auto rangedUnit : rangedUnits)
        {
            BOT_ASSERT(rangedUnit, "ranged unit is null");

            const sc2::Unit * target = nullptr;
            target = getTarget(rangedUnit, rangedUnitTargets);
            bool isEnemyInSightCondition = rangedUnitTargets.size() > 0 &&
                target != nullptr && Util::Dist(rangedUnit->pos, target->pos) <= m_bot.Config().MaxTargetDistance;

            ConditionAction isEnemyInSight(isEnemyInSightCondition);
            ConditionAction isEnemyRanged(target != nullptr && isTargetRanged(target));

            FocusFireAction focusFireAction(rangedUnit, target, &rangedUnitTargets, m_bot, m_focusFireStates, &rangedUnits, m_unitHealth);
            KiteAction kiteAction(rangedUnit, target, m_bot, m_kittingStates);
            GoToObjectiveAction goToObjectiveAction(rangedUnit, m_order.getPosition(), m_bot);

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

void RangedManager::HarassLogic(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets)
{
	// for each rangedUnit
	for (auto rangedUnit : rangedUnits)
	{
		long frame = lastCommandFrameForUnit.find(rangedUnit) != lastCommandFrameForUnit.end() ? lastCommandFrameForUnit[rangedUnit] : 0;
		if (frame + 2 > m_bot.Observation()->GetGameLoop())
			continue;

		const sc2::Unit * target = getTarget(rangedUnit, rangedUnitTargets);
		const std::vector<const sc2::Unit *> threats = getThreats(rangedUnit, rangedUnitTargets);

		// if there is no potential target or threat, move to objective
		if (target == nullptr && threats.empty())
		{
			Micro::SmartMove(rangedUnit, m_order.getPosition(), m_bot);
			lastCommandFrameForUnit[rangedUnit] = m_bot.Observation()->GetGameLoop();
			continue;
		}

		float dirX = 0.f, dirY = 0.f, dirLen = 0.f;

		if (target != nullptr)
		{
			bool targetInRange = Util::Dist(rangedUnit->pos, target->pos) <= Util::GetAttackRangeForTarget(rangedUnit, target, m_bot);

			// if target is in range and weapon is ready, attack target
			if (targetInRange && rangedUnit->weapon_cooldown <= 0.f) {
				Micro::SmartAttackUnit(rangedUnit, target, m_bot);
				lastCommandFrameForUnit[rangedUnit] = m_bot.Observation()->GetGameLoop();
				continue;
			}

			// TODO retreat if faster ranged unit is near

			// if not in range of target, add normalized vector towards target
			if (!targetInRange)
			{
				dirX = target->pos.x - rangedUnit->pos.x;
				dirY = target->pos.y - rangedUnit->pos.y;
			}
			else
			{
				dirX = rangedUnit->pos.x - target->pos.x;
				dirY = rangedUnit->pos.y - target->pos.y;
			}
			dirLen = abs(dirX) + abs(dirY);
			dirX /= dirLen;
			dirY /= dirLen;
		}
		else
		{
			// add normalized vector towards objective
			dirX = m_order.getPosition().x - rangedUnit->pos.x;
			dirY = m_order.getPosition().y - rangedUnit->pos.y;
			dirLen = abs(dirX) + abs(dirY);
			dirX /= dirLen;
			dirY /= dirLen;
		}

		// add normalied * 1.5 vector of potential threats (inside their range + 2)
		for (auto threat : threats)
		{
			float threatRange = Util::GetAttackRangeForTarget(threat, rangedUnit, m_bot);
			if (Util::Dist(rangedUnit->pos, threat->pos) < threatRange + 2)
			{
				m_bot.Map().drawCircle(threat->pos, threatRange + 2, CCColor(255, 0, 0));
				float fleeDirX = 0.f, fleeDirY = 0.f, fleeDirLen = 0.f;
				fleeDirX = rangedUnit->pos.x - threat->pos.x;
				fleeDirY = rangedUnit->pos.y - threat->pos.y;
				fleeDirLen = abs(fleeDirX) + abs(fleeDirY);
				fleeDirX /= fleeDirLen;
				fleeDirY /= fleeDirLen;
				//TODO reduce the multiplier the farther we are from it
				dirX += fleeDirX * 1.5f;
				dirY += fleeDirY * 1.5f;
			}
		}

		dirLen = abs(dirX) + abs(dirY);
		dirX /= dirLen;
		dirY /= dirLen;

		// move towards direction if pathable
		CCPosition moveTo(rangedUnit->pos.x + dirX * 5, rangedUnit->pos.y + dirY * 5);
		//TODO find another place to go if not pathable
		Micro::SmartMove(rangedUnit, moveTo, m_bot);
		lastCommandFrameForUnit[rangedUnit] = m_bot.Observation()->GetGameLoop();

		// TODO implement path finding that dodges threats
	}
}

void RangedManager::AlphaBetaPruning(std::vector<const sc2::Unit *> rangedUnits, std::vector<const sc2::Unit *> rangedUnitTargets) {
    std::vector<std::shared_ptr<AlphaBetaUnit>> minUnits;
    std::vector<std::shared_ptr<AlphaBetaUnit>> maxUnits;

    if (lastUnitCommand.size() >= rangedUnits.size()) {
        lastUnitCommand.clear();
    }

    for (auto unit : rangedUnits) {
        bool has_played = false;

        if (m_bot.Config().UnitOwnAgent) {
            // Update has_played value
            for (auto unitC : lastUnitCommand) {
                if (unitC == unit) {
                    has_played = true;
                    break;
                }
            }
        }

        maxUnits.push_back(std::make_shared<AlphaBetaUnit>(unit, &m_bot, has_played));
    }
    for (auto unit : rangedUnitTargets) {
        minUnits.push_back(std::make_shared<AlphaBetaUnit>(unit, &m_bot));
    }

    AlphaBetaConsideringDurations alphaBeta = AlphaBetaConsideringDurations(std::chrono::milliseconds(m_bot.Config().AlphaBetaMaxMilli), m_bot.Config().AlphaBetaDepth, m_bot.Config().UnitOwnAgent, m_bot.Config().ClosestEnemy, m_bot.Config().WeakestEnemy, m_bot.Config().HighestPriority);
    AlphaBetaValue value = alphaBeta.doSearch(maxUnits, minUnits, &m_bot);
    size_t nodes = alphaBeta.nodes_evaluated;
    m_bot.Map().drawTextScreen(0.005f, 0.005f, std::string("Nodes explored : ") + std::to_string(nodes));
    m_bot.Map().drawTextScreen(0.005f, 0.020f, std::string("Max depth : ") + std::to_string(m_bot.Config().AlphaBetaDepth));
    m_bot.Map().drawTextScreen(0.005f, 0.035f, std::string("AB value : ") + std::to_string(value.score));
    if (value.move != NULL) {
        for (auto action : value.move->actions) {
            lastUnitCommand.push_back(action->unit->actual_unit);
            if (action->type == AlphaBetaActionType::ATTACK) {
                Micro::SmartAttackUnit(action->unit->actual_unit, action->target->actual_unit, m_bot);
            }
            else if (action->type == AlphaBetaActionType::MOVE_BACK) {
                Micro::SmartMove(action->unit->actual_unit, action->position, m_bot);
            }
            else if (action->type == AlphaBetaActionType::MOVE_FORWARD) {
                Micro::SmartMove(action->unit->actual_unit, action->position, m_bot);
            }
        }
    }
}

void RangedManager::UCTCD(std::vector<const sc2::Unit *> rangedUnits, std::vector<const sc2::Unit *> rangedUnitTargets) {
    std::vector<UCTCDUnit> minUnits;
    std::vector<UCTCDUnit> maxUnits;
    
    if (m_bot.Config().SkipOneFrame && isCommandDone) {
        isCommandDone = false;

        return;
    }

    if (lastUnitCommand.size() >= rangedUnits.size()) {
        lastUnitCommand.clear();
    }

    for (auto unit : rangedUnits) {
        bool has_played = false;

        if (m_bot.Config().UnitOwnAgent && std::find(lastUnitCommand.begin(), lastUnitCommand.end(), unit) != lastUnitCommand.end()) {
            // Update has_played value
            has_played = true;
        }

        maxUnits.push_back(UCTCDUnit(unit, &m_bot, has_played));
    }
    for (auto unit : rangedUnitTargets) {
        minUnits.push_back(UCTCDUnit(unit, &m_bot));
    }
    UCTConsideringDurations uctcd = UCTConsideringDurations(m_bot.Config().UCTCDK, m_bot.Config().UCTCDMaxTraversals, m_bot.Config().UCTCDMaxMilli, command_for_unit);
    UCTCDMove move = uctcd.doSearch(maxUnits, minUnits, m_bot.Config().ClosestEnemy, m_bot.Config().WeakestEnemy, m_bot.Config().HighestPriority, m_bot.Config().UCTCDConsiderDistance, m_bot.Config().UnitOwnAgent);

    size_t nodes = uctcd.nodes_explored;
    size_t traversals = uctcd.traversals;
    long long time_spent = uctcd.time_spent.count();
    int win_value = uctcd.win_value;
    m_bot.Map().drawTextScreen(0.005f, 0.005f, std::string("Nodes explored : ") + std::to_string(nodes));
    m_bot.Map().drawTextScreen(0.005f, 0.020f, std::string("Root traversed : ") + std::to_string(traversals) + std::string(" times"));
    m_bot.Map().drawTextScreen(0.005f, 0.035f, std::string("Time spent : ") + std::to_string(time_spent));
    m_bot.Map().drawTextScreen(0.005f, 0.050f, std::string("Most value : ") + std::to_string(win_value));

    for (auto action : move.actions) {
        if (action.unit.has_played) {
            // reset priority
            lastUnitCommand.clear();
        }
        lastUnitCommand.push_back(action.unit.actual_unit);
        command_for_unit[action.unit.actual_unit] = action;

        // Select unit (visual info only)
        const sc2::ObservationInterface* obs = m_bot.Observation();
        sc2::Point2DI target = Util::ConvertWorldToCamera(obs->GetGameInfo(), obs->GetCameraPos(), action.unit.actual_unit->pos);
        m_bot.ActionsFeatureLayer()->Select(target, sc2::PointSelectionType::PtSelect);

        if (action.type == UCTCDActionType::ATTACK) {
            Micro::SmartAttackUnit(action.unit.actual_unit, action.target.actual_unit, m_bot);
        }
        else if (action.type == UCTCDActionType::MOVE_BACK) {
            Micro::SmartMove(action.unit.actual_unit, action.position, m_bot);
        }
        else if (action.type == UCTCDActionType::MOVE_FORWARD) {
            Micro::SmartMove(action.unit.actual_unit, action.position, m_bot);
        }

        isCommandDone = true;
    }
}

// get a target for the ranged unit to attack
const sc2::Unit * RangedManager::getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets)
{
    BOT_ASSERT(rangedUnit, "null ranged unit in getTarget");

    float highestPriority = 0.f;
    const sc2::Unit * bestTarget = nullptr;

    // for each possible target
    for (auto targetUnit : targets)
    {
        BOT_ASSERT(targetUnit, "null target unit in getTarget");

        float priority = getAttackPriority(rangedUnit, targetUnit);

        // if it's a higher priority, set it
        if (priority > highestPriority)
        {
            highestPriority = priority;
            bestTarget = targetUnit;
        }
    }

    return bestTarget;
}

// get threats to our harass unit
const std::vector<const sc2::Unit *> RangedManager::getThreats(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets)
{
	BOT_ASSERT(rangedUnit, "null ranged unit in getThreats");

	std::vector<const sc2::Unit *> threats;

	// for each possible threat
	for (auto targetUnit : targets)
	{
		BOT_ASSERT(targetUnit, "null target unit in getThreats");

		if (Util::Dist(rangedUnit->pos, targetUnit->pos) < Util::GetAttackRangeForTarget(targetUnit, rangedUnit, m_bot) + 2)
			threats.push_back(targetUnit);
	}

	return threats;
}

float RangedManager::getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * target)
{
    BOT_ASSERT(target, "null unit in getAttackPriority");
    
    Unit targetUnit(target, m_bot);

	if (m_harassMode)
	{
		float unitSpeed = Util::GetUnitTypeDataFromUnitTypeId(attacker->unit_type, m_bot).movement_speed;
		float targetSpeed = Util::GetUnitTypeDataFromUnitTypeId(target->unit_type, m_bot).movement_speed;
		if (unitSpeed > targetSpeed * 1.2f && isTargetRanged(target))
			return -1.f;
	}

	float healthValue = pow(target->health + target->shield, 0.4f);		//the more health a unit has, the less it is prioritized
	float distanceValue = 1 / Util::Dist(attacker->pos, target->pos);   //the more far a unit is, the less it is prioritized
	if (distanceValue > Util::GetAttackRangeForTarget(attacker, target, m_bot))
		distanceValue /= 2;

    if (targetUnit.getType().isCombatUnit() || targetUnit.getType().isWorker())
    {
		float unitDps = Util::GetDpsForTarget(attacker, target, m_bot);
        float targetDps = Util::GetDpsForTarget(target, attacker, m_bot);
        if (target->unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER)
        {
            //We manually reduce the dps of the bunker because it only serve as a shield, units will spawn out of it when destroyed
			targetDps = 5.f;
        }
        float workerBonus = targetUnit.getType().isWorker() ? 1.5f : 1.f;   //workers are important to kill

        return (targetDps + unitDps - healthValue + distanceValue * 50) * workerBonus;
    }

	return (healthValue + distanceValue * 50) / 100.f;		//we do not want non combat buildings to have a higher priority than other units
}

// according to http://wiki.teamliquid.net/starcraft2/Range
// the maximum range of a melee unit is 1 (ultralisk)
bool RangedManager::isTargetRanged(const sc2::Unit * target)
{
    BOT_ASSERT(target, "target is null");
    float maxRange = Util::GetMaxAttackRange(target->unit_type, m_bot);
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

