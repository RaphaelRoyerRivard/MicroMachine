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

const float HARASS_THREAT_RANGE_BUFFER = 1.f;
const float HARASS_THREAT_RANGE_HEIGHT_BONUS = 2.f;
const float HARASS_THREAT_MIN_HEIGHT_DIFF = 2.f;
const float HARASS_THREAT_MIN_RANGE = 4.f;
const float HARASS_UNIT_MIN_MOVE_DISTANCE = 1.5f;

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
    else 
	{
		// use good-ol' BT
		RunBehaviorTree(rangedUnits, rangedUnitTargets);
    }
}

void RangedManager::RunBehaviorTree(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets)
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

void RangedManager::HarassLogic(sc2::Units &rangedUnits, sc2::Units &rangedUnitTargets)
{
	// for each rangedUnit
	for (auto rangedUnit : rangedUnits)
	{
		// We want to give an action only every 5 frames to allow double attack and cliff jumps
		uint32_t frame = lastCommandFrameForUnit.find(rangedUnit) != lastCommandFrameForUnit.end() ? lastCommandFrameForUnit[rangedUnit] : 0;
		if (frame + 4 > m_bot.Observation()->GetGameLoop())
			continue;

		const sc2::Unit * target = getTarget(rangedUnit, rangedUnitTargets);
		const std::vector<const sc2::Unit *> threats = getThreats(rangedUnit, rangedUnitTargets);

		// if there is no potential target or threat, move to objective
		if ((target == nullptr || Util::Dist(rangedUnit->pos, target->pos) > m_order.getRadius()) && threats.empty())
		{
			Micro::SmartMove(rangedUnit, m_order.getPosition(), m_bot);
			lastCommandFrameForUnit[rangedUnit] = m_bot.Observation()->GetGameLoop();
			continue;
		}

		// Check if unit can use KD8Charge
		Unit harassUnit(rangedUnit, m_bot);
		sc2::AvailableAbilities abilities = harassUnit.getAbilities();
		bool canUseKD8Charge = false;
		for (const auto & ability : abilities.abilities)
		{
			if (ability.ability_id == sc2::ABILITY_ID::EFFECT_KD8CHARGE)
			{
				canUseKD8Charge = true;
				break;
			}
		}

		float dirX = 0.f, dirY = 0.f, dirLen = 0.f;

		if (target != nullptr)
		{
			bool targetInAttackRange = Util::Dist(rangedUnit->pos, target->pos) <= Util::GetAttackRangeForTarget(rangedUnit, target, m_bot);
			bool inRangeOfThreat = false;
			for (auto & threat : threats)
			{
				float threatRange = Util::GetAttackRangeForTarget(threat, rangedUnit, m_bot);
				float threatSpeed = Util::getSpeedOfUnit(threat, m_bot);
				if (Util::Dist(rangedUnit->pos, threat->pos) <= threatRange + threatSpeed)
				{
					inRangeOfThreat = true;
					break;
				}
			}

			// if our unit is not in range of threat and target is in range and weapon is ready, attack target
			if (!inRangeOfThreat && targetInAttackRange && rangedUnit->weapon_cooldown <= 0.f) {
				Micro::SmartAttackUnit(rangedUnit, target, m_bot);
				lastCommandFrameForUnit[rangedUnit] = m_bot.Observation()->GetGameLoop();
				continue;
			}

			// Check if we have enough reach to throw a mine at our target
			/*if (canUseKD8Charge && targetInAttackRange)
			{
				//TODO find a group of targets
				Micro::SmartAbility(rangedUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE, target->pos, m_bot);
				lastCommandFrameForUnit[rangedUnit] = m_bot.Observation()->GetGameLoop();
				continue;
			}*/

			// TODO retreat if faster ranged unit is near

			// if not in range of target, add normalized vector towards target
			if (!targetInAttackRange)
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

		bool useInfluenceMap = false;
		float rangedUnitSpeed = Util::getSpeedOfUnit(rangedUnit, m_bot);
		bool madeAction = false;
		// add normalied * 1.5 vector of potential threats
		for (auto threat : threats)
		{
			float rangedUnitRange = Util::GetAttackRangeForTarget(rangedUnit, threat, m_bot);
			float threatRange = Util::GetAttackRangeForTarget(threat, rangedUnit, m_bot);
			float threatSpeed = Util::getSpeedOfUnit(threat, m_bot);
			float fleeDirX = 0.f, fleeDirY = 0.f, fleeDirLen = 0.f;
			fleeDirX = rangedUnit->pos.x - threat->pos.x;
			fleeDirY = rangedUnit->pos.y - threat->pos.y;
			fleeDirLen = abs(fleeDirX) + abs(fleeDirY);
			fleeDirX /= fleeDirLen;
			fleeDirY /= fleeDirLen;
			// The expected threat position will be used to decide where to throw the mine
			// (between the unit and the enemy or on the enemy if it is a worker)
			CCPosition expectedThreatPosition(threat->pos.x + fleeDirX * threatSpeed * 0.5f, threat->pos.y + fleeDirY * threatSpeed * 0.5f);
			if (Unit(threat, m_bot).getType().isWorker())
				expectedThreatPosition = threat->pos;
			float dist = Util::Dist(rangedUnit->pos, threat->pos);
			float totalRange = getThreatRange(rangedUnit, threat);
			float distToExpectedPosition = Util::Dist(rangedUnit->pos, expectedThreatPosition);
			float minChargeDistance = totalRange - HARASS_THREAT_RANGE_BUFFER;
			// Check if we have enough reach to throw at the threat
			if (canUseKD8Charge && distToExpectedPosition <= rangedUnitRange && distToExpectedPosition >= minChargeDistance)
			{
				//TODO find a group of threat
				Micro::SmartAbility(rangedUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE, expectedThreatPosition, m_bot);
				lastCommandFrameForUnit[rangedUnit] = m_bot.Observation()->GetGameLoop();
				madeAction = true;
				break;
			}
			if (dist < threatRange + 0.5f)
				useInfluenceMap = true;
			m_bot.Map().drawCircle(threat->pos, Util::GetAttackRangeForTarget(threat, rangedUnit, m_bot), CCColor(255, 0, 0));
			m_bot.Map().drawCircle(threat->pos, totalRange, CCColor(128, 0, 0));
			//TODO reduce the multiplier the farther we are from it
			dirX += fleeDirX * 1.5f;
			dirY += fleeDirY * 1.5f;
		}
		if (madeAction)
			continue;

		dirLen = abs(dirX) + abs(dirY);
		dirX /= dirLen;
		dirY /= dirLen;

		// Average estimate of the distance between a ledge and the other side, in increment of mineral chunck size, also based on interloperLE.
		const int initialMoveDistance = 9;
		int moveDistance = initialMoveDistance;
		CCPosition moveTo(rangedUnit->pos.x + dirX * moveDistance, rangedUnit->pos.y + dirY * moveDistance);

		// If not using influence maps, check if we can move in the direction of the vector
		if (!useInfluenceMap)
		{
			// We check if we are moving towards and close to an unpathable position
			while (!m_bot.Observation()->IsPathable(moveTo))
			{
				++moveDistance;
				moveTo = CCPosition(rangedUnit->pos.x + dirX * moveDistance, rangedUnit->pos.y + dirY * moveDistance);
				// If moveTo is out of the map, stop checking farther and switch to influence map navigation
				if (m_bot.Map().width() < moveTo.x || moveTo.x < 0 || m_bot.Map().height() < moveTo.y || moveTo.y < 0)
				{
					useInfluenceMap = true;
					break;
				}
			}
		}

		// If close to an unpathable position or in danger
		if (useInfluenceMap)
		{
			// Use influence map to find safest path
			moveTo = Util::GetPosition(FindSafestPathWithInfluenceMap(rangedUnit, threats));
		}

		Micro::SmartMove(rangedUnit, moveTo, m_bot);
		lastCommandFrameForUnit[rangedUnit] = m_bot.Observation()->GetGameLoop();
	}
}

struct Node {
	Node(CCTilePosition position) :
		position(position),
		parent(nullptr)
	{
	}
	Node(CCTilePosition position, Node* parent) :
		position(position),
		parent(parent)
	{
	}
	CCTilePosition position;
	Node* parent;
};

bool isPositionInNodeSet(CCTilePosition& position, std::set<Node*> & set)
{
	for (Node* node : set)
	{
		if (node->position == position)
			return true;
	}
	return false;
}

Node* getLowestCostNode(std::map<Node*, float> & costs)
{
	std::pair<Node*, float> lowestCost = *(costs.begin());
	for (std::pair<Node*, float> node : costs)
	{
		if (node.second < lowestCost.second)
			lowestCost = node;
	}
	return lowestCost.first;
}

CCTilePosition RangedManager::FindSafestPathWithInfluenceMap(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & threats)
{
	m_bot.Map().drawText(rangedUnit->pos, "FLEE!", CCColor(255, 0, 0));
	CCTilePosition centerPos(25, 25);
	const int mapWidth = 50;
	const int mapHeight = 50;
	float map[mapWidth][mapHeight];
	for (int x = 0; x < mapWidth; ++x)
	{
		for (int y = 0; y < mapHeight; ++y)
		{
			bool pathable = m_bot.Observation()->IsPathable(sc2::Point2D(rangedUnit->pos.x - centerPos.x + x, rangedUnit->pos.y - centerPos.y + y));
			map[x][y] = pathable ? 0.f : 9999.f;
		}
	}
	for (auto threat : threats)
	{
		CCPosition threatRelativePosition = threat->pos - rangedUnit->pos + Util::GetPosition(centerPos);
		if ((int)threatRelativePosition.x < 0 || (int)threatRelativePosition.y < 0 || (int)threatRelativePosition.x >= mapWidth || (int)threatRelativePosition.y >= mapHeight)
			continue;

		float radius = getThreatRange(rangedUnit, threat);
		float intensity = Util::GetDpsForTarget(threat, rangedUnit, m_bot);
		int minX = std::max(0, (int)floor(threatRelativePosition.x - radius));
		int maxX = std::min((int)sizeof(map) - 1, (int)ceil(threatRelativePosition.x + radius));
		int minY = std::max(0, (int)floor(threatRelativePosition.y - radius));
		int maxY = std::min((int)sizeof(map[0]) - 1, (int)ceil(threatRelativePosition.y + radius));
		//loop for a square of size equal to the diameter of the influence circle
		for (int x = minX; x < maxX; ++x)
		{
			for (int y = minY; y < maxY; ++y)
			{
				//value is linear interpolation
				map[x][y] += intensity * std::max(0.f, radius - Util::Dist(threatRelativePosition, CCPosition(x, y)));
			}
		}
	}

	std::set<Node*> closedSet;
	std::set<Node*> openSet;
	std::map<Node*, float> costs;
	Node* start = new Node(centerPos);
	openSet.insert(start);
	costs[start] = 0;

	while (!openSet.empty())
	{
		Node* current = getLowestCostNode(costs);
		//Return condition: No threat influence on this tile
		if (map[current->position.x][current->position.y] == 0)
		{
			CCPosition currentPos = Util::GetPosition(current->position) - Util::GetPosition(centerPos);
			CCPosition returnPos = currentPos;
			while (current->parent != nullptr)
			{
				CCPosition parentPos = Util::GetPosition(current->parent->position) - Util::GetPosition(centerPos);
				m_bot.Map().drawCircle(currentPos + rangedUnit->pos, 1.f, CCColor(0, 255, 255));
				//we want to retun a node close to the current position
				if (Util::Dist(parentPos, currentPos) <= 3.f && returnPos == currentPos)
					returnPos = parentPos;
				current = current->parent;
			}
			for (Node* node : openSet)
			{
				delete(node);
			}
			for (Node* node : closedSet)
			{
				delete(node);
			}
			// Move to at least 1.5f distance, otherwise the unit might stop when reaching it's move command position
			float dist = Util::Dist(rangedUnit->pos, rangedUnit->pos + returnPos);
			if (dist < HARASS_UNIT_MIN_MOVE_DISTANCE)
			{
				float x = HARASS_UNIT_MIN_MOVE_DISTANCE / dist * returnPos.x;
				float y = HARASS_UNIT_MIN_MOVE_DISTANCE / dist * returnPos.y;
				returnPos = CCPosition(x, y);
			}
			return Util::GetTilePosition(rangedUnit->pos + returnPos);
		}
		float currentCost = costs[current];
		openSet.erase(current);
		costs.erase(current);
		closedSet.insert(current);
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				CCTilePosition neighborPosition(current->position.x + x, current->position.y + y);
				if (neighborPosition.x < 0 || neighborPosition.y < 0 || neighborPosition.x >= mapWidth || neighborPosition.y >= mapHeight)
					continue;
				if (isPositionInNodeSet(neighborPosition, closedSet))
					continue;
				if (!isPositionInNodeSet(neighborPosition, openSet))
				{
					Node* neighbor = new Node(neighborPosition, current);
					openSet.insert(neighbor);
					float neighborInfluence = map[neighborPosition.x][neighborPosition.y] + currentCost;
					costs[neighbor] = neighborInfluence;
				}
			}
		}
	}
	for (Node* node : closedSet)
	{
		delete(node);
	}
	return Util::GetTilePosition(m_order.getPosition());
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
		if (Util::GetDpsForTarget(targetUnit, rangedUnit, m_bot) == 0.f)
			continue;
		//We consider a unit as a threat if the sum of its range and speed is bigger than the distance to our unit
		//But this is not working so well for melee units, we keep every units in a radius of min threat range
		float threatRange = getThreatRange(rangedUnit, targetUnit);
		if (Util::Dist(rangedUnit->pos, targetUnit->pos) < std::max(HARASS_THREAT_MIN_RANGE, threatRange))
			threats.push_back(targetUnit);
	}

	return threats;
}

//calculate radius max(min range, range + speed + height bonus + small buffer)
float RangedManager::getThreatRange(const sc2::Unit * rangedUnit, const sc2::Unit * threat)
{
	sc2::GameInfo gameInfo = m_bot.Observation()->GetGameInfo();
	float heightBonus = Util::TerainHeight(gameInfo, threat->pos) > Util::TerainHeight(gameInfo, rangedUnit->pos) + HARASS_THREAT_MIN_HEIGHT_DIFF ? HARASS_THREAT_RANGE_HEIGHT_BONUS : 0.f;
	float threatRange = Util::GetAttackRangeForTarget(threat, rangedUnit, m_bot) + Util::getSpeedOfUnit(threat, m_bot) + heightBonus + HARASS_THREAT_RANGE_BUFFER;
	return threatRange;
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