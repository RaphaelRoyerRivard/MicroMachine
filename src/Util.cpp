#include "Util.h"
#include "CCBot.h"
#include "libvoxelbot/combat/combat_upgrades.h"

const float EPSILON = 1e-5;
const float CLIFF_MIN_HEIGHT_DIFFERENCE = 1.f;
const float CLIFF_MAX_HEIGHT_DIFFERENCE = 2.5f;
const int HARASS_PATHFINDING_MAX_EXPLORED_NODE = 500;
const float HARASS_PATHFINDING_TILE_BASE_COST = 1.f;
const float HARASS_PATHFINDING_TILE_CREEP_COST = 0.5f;
const float PATHFINDING_TURN_COST = 3.f;
const float PATHFINDING_SECONDARY_GOAL_COST = 10.f;
const float HARASS_PATHFINDING_HEURISTIC_MULTIPLIER = 1.f;
const uint32_t WORKER_PATHFINDING_COOLDOWN_AFTER_FAIL = 50;
const uint32_t UNIT_CLUSTERING_COOLDOWN = 24;
const float UNIT_CLUSTERING_MAX_DISTANCE = 5.f;

int timeControlRatio = -1;

// Influence Map Node
struct Util::PathFinding::IMNode
{
	IMNode() :
		position(CCTilePosition(0, 0)),
		parent(nullptr),
		cost(0.f),
		heuristic(0.f)
	{
	}
	IMNode(CCTilePosition position) :
		position(position),
		parent(nullptr),
		cost(0.f),
		heuristic(0.f)
	{
	}
	IMNode(CCTilePosition position, IMNode* parent, float heuristic) :
		position(position),
		parent(parent),
		cost(0.f),
		heuristic(heuristic)
	{
	}
	IMNode(CCTilePosition position, IMNode* parent, float cost, float heuristic) :
		position(position),
		parent(parent),
		cost(cost),
		heuristic(heuristic)
	{
	}
	CCTilePosition position;
	IMNode* parent;
	float cost;
	float heuristic;

	float getTotalCost() const
	{
		return cost + heuristic;
	}

	bool isValid() const
	{
		return position != CCTilePosition(0, 0);
	}

	int getId() const
	{
		return position.x * 1000 + position.y;
	}

	bool operator<(const IMNode& rhs) const
	{
		return getTotalCost() < rhs.getTotalCost();
	}

	bool operator<=(const IMNode& rhs) const
	{
		return getTotalCost() <= rhs.getTotalCost();
	}

	bool operator==(const IMNode& rhs) const
	{
		return position == rhs.position;
	}
};

void Util::Initialize(CCBot & bot, CCRace race, const sc2::GameInfo & _gameInfo)
{
	// Initialize combat simulator
	initMappings();
	m_simulator = new CombatPredictor();
	m_simulator->init();
	m_simulator->getCombatEnvironment({}, {});
	
	switch (race)
	{
		case sc2::Race::Terran:
		{
			Util::depotType = UnitType(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER, bot);
			Util::refineryType = UnitType(sc2::UNIT_TYPEID::TERRAN_REFINERY, bot);
			Util::richRefineryType = UnitType(sc2::UNIT_TYPEID::TERRAN_REFINERYRICH, bot);
			Util::workerType = UnitType(sc2::UNIT_TYPEID::TERRAN_SCV, bot);
			Util::supplyType = UnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT, bot);
			break;
		}
		case sc2::Race::Protoss:
		{
			Util::depotType = UnitType(sc2::UNIT_TYPEID::PROTOSS_NEXUS, bot);
			Util::refineryType = UnitType(sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR, bot);
			Util::richRefineryType = UnitType(sc2::UNIT_TYPEID::PROTOSS_ASSIMILATORRICH, bot);
			Util::workerType = UnitType(sc2::UNIT_TYPEID::PROTOSS_PROBE, bot);
			Util::supplyType = UnitType(sc2::UNIT_TYPEID::PROTOSS_PYLON, bot);
			break;
		}
		case sc2::Race::Zerg:
		{
			Util::depotType = UnitType(sc2::UNIT_TYPEID::ZERG_HATCHERY, bot);
			Util::refineryType = UnitType(sc2::UNIT_TYPEID::ZERG_EXTRACTOR, bot);
			Util::richRefineryType = UnitType(sc2::UNIT_TYPEID::ZERG_EXTRACTORRICH, bot);
			Util::workerType = UnitType(sc2::UNIT_TYPEID::ZERG_DRONE, bot);
			Util::supplyType = UnitType(sc2::UNIT_TYPEID::ZERG_OVERLORD, bot);
			break;
		}
	}
	Util::gameInfo = &_gameInfo;

	//assert(gameInfo->pathing_grid.data.size() == gameInfo->width * gameInfo->height);
	m_pathable = std::vector<std::vector<bool>>(gameInfo->width);
	m_placement = std::vector<std::vector<bool>>(gameInfo->width);
	m_terrainHeight = std::vector<std::vector<float>>(gameInfo->width);
	const auto pathingGrid = sc2::PathingGrid(*gameInfo);
	const auto placementGrid = sc2::PlacementGrid(*gameInfo);
	const auto heightMap = sc2::HeightMap(*gameInfo);
	for (int x = 0; x < gameInfo->width; x++)
	{
		m_pathable[x] = std::vector<bool>(gameInfo->height);
		m_placement[x] = std::vector<bool>(gameInfo->height);
		m_terrainHeight[x] = std::vector<float>(gameInfo->height);
		for (int y = 0; y < gameInfo->height; y++)
		{
			auto point = sc2::Point2DI(x, y);
			auto index = x + y * gameInfo->width;
			//Pathable
			/*unsigned char encodedPathing = gameInfo->pathing_grid.data[index];
			m_pathable[x][y] = encodedPathing == 255 ? false : true;*/
			m_pathable[x][y] = pathingGrid.IsPathable(point);

			//Placement
			/*unsigned char encodedPlacement = gameInfo->placement_grid.data[index];
			m_placement[x][y] = encodedPlacement == 255 ? true : false;*/
			m_placement[x][y] = placementGrid.IsPlacable(point);
			
			//Terrain height
			/*unsigned char encodedHeight = gameInfo->terrain_height.data[index];
			m_terrainHeight[x][y] = float(encodedHeight) / 8.f - 15.875f;*/
			m_terrainHeight[x][y] = heightMap.TerrainHeight(point);
		}
	}
}

Util::PathFinding::IMNode* getLowestCostNode(std::set<Util::PathFinding::IMNode*> & set)
{
	Util::PathFinding::IMNode* lowestCostNode = nullptr;
	for (const auto node : set)
	{
		if (!lowestCostNode || *node < *lowestCostNode)
			lowestCostNode = node;
	}
	return lowestCostNode;
}

bool Util::PathFinding::SetContainsNode(const std::set<IMNode*> & set, IMNode* node, bool mustHaveLowerCost)
{
	for (auto n : set)
	{
		if (*n == *node)
		{
			return !mustHaveLowerCost || *n <= *node;
		}
	}
	return false;
}

bool Util::PathFinding::IsPathToGoalSafe(const sc2::Unit * unit, CCPosition goal, bool addBuffer, CCBot & bot)
{
	if (!unit)
	{
		Util::DisplayError("null unit received in Util::PathFinding::IsPathToGoalSafe", "", bot);
		return false;
	}

	int foundIndex = -1;
	SafePathResult releventResult;
	auto & safePathResults = m_lastPathFindingResultsForUnit[unit->tag];
	for (size_t i = 0; i < safePathResults.size(); ++i)
	{
		auto & safePathResult = safePathResults[i];
		if(safePathResult.m_position == goal)
		{
			releventResult = safePathResult;
			foundIndex = i;
			break;
		}
	}
	if(foundIndex >= 0 && bot.GetCurrentFrame() - releventResult.m_frame < WORKER_PATHFINDING_COOLDOWN_AFTER_FAIL)
		return releventResult.m_result;

	FailureReason failureReason;
	std::list<CCPosition> path = FindOptimalPath(unit, goal, CCPosition(), addBuffer ? 3.f : 1.f, true, false, false, false, false, true, failureReason, bot);
	const bool success = !path.empty() || failureReason == TIMEOUT;
	const SafePathResult safePathResult = SafePathResult(goal, bot.GetCurrentFrame(), success);
	if(foundIndex >= 0)
	{
		m_lastPathFindingResultsForUnit[unit->tag][foundIndex] = safePathResult;
	}
	else
	{
		m_lastPathFindingResultsForUnit[unit->tag].push_back(safePathResult);
	}
	return success;
}

CCPosition Util::PathFinding::FindOptimalPathToTarget(const sc2::Unit * unit, CCPosition goal, CCPosition secondaryGoal, const sc2::Unit* target, float maxRange, bool ignoreInfluence, CCBot & bot)
{
	bool getCloser = false;
	if (target)
	{
		const float targetRange = GetAttackRangeForTarget(target, unit, bot);
		getCloser = targetRange == 0.f || Dist(unit->pos, target->pos) > getThreatRange(unit, target, bot);
	}
	std::list<CCPosition> path = FindOptimalPath(unit, goal, secondaryGoal, maxRange, false, false, getCloser, ignoreInfluence, false, bot);
	return GetCommandPositionFromPath(path, unit, bot);
}

CCPosition Util::PathFinding::FindOptimalPathToSafety(const sc2::Unit * unit, CCPosition goal, bool shouldHeal, CCBot & bot)
{
	std::list<CCPosition> path = FindOptimalPath(unit, goal, CCPosition(), 0.f, false, false, shouldHeal, false, true, bot);
	return GetCommandPositionFromPath(path, unit, bot);
}

CCPosition Util::PathFinding::FindOptimalPathToSaferRange(const sc2::Unit * unit, const sc2::Unit * target, float range, CCBot & bot)
{
	std::list<CCPosition> path = FindOptimalPath(unit, target->pos, CCPosition(), range, false, false, false, false, true, bot);
	return GetCommandPositionFromPath(path, unit, bot);
}

/*
 * Returns pathing distance. Returns -1 if no path is found.
 * If ignoreInfluence is true, no enemy influence will be taken into account when finding the path.
 * If it is false and enemy influence is encountered along the way, it will return -1.
 */
float Util::PathFinding::FindOptimalPathDistance(const sc2::Unit * unit, CCPosition goal, bool ignoreInfluence, CCBot & bot)
{
	const auto path = FindOptimalPath(unit, goal, CCPosition(), 2.f, !ignoreInfluence, false, false, ignoreInfluence, false, bot);
	if (path.empty())
	{
		return -1.f;
	}

	float dist = 0.f;
	CCPosition lastPosition;
	for (const auto position : path)
	{
		if (lastPosition != CCPosition())
		{
			dist += Dist(lastPosition, position);
		}
		lastPosition = position;
	}
	return dist;
}

CCPosition Util::PathFinding::FindOptimalPathPosition(const sc2::Unit * unit, CCPosition goal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, CCBot & bot)
{
	auto path = FindOptimalPath(unit, goal, CCPosition(), maxRange, exitOnInfluence, considerOnlyEffects, getCloser, false, false, bot);
	if(path.empty())
	{
		return {};
	}
	return GetCommandPositionFromPath(path, unit, bot);
}

CCPosition Util::PathFinding::FindOptimalPathToDodgeEffectAwayFromGoal(const sc2::Unit * unit, CCPosition goal, float range, CCBot & bot)
{
	goal = unit->pos + (unit->pos - goal);
	std::list<CCPosition> path = Util::PathFinding::FindOptimalPath(unit, goal, CCPosition(), range, false, true, false, false, false, bot);
	return GetCommandPositionFromPath(path, unit, bot);
}

std::list<CCPosition> Util::PathFinding::FindOptimalPathWithoutLimit(const sc2::Unit * unit, CCPosition goal, CCBot & bot)
{
	FailureReason failureReason;
	return FindOptimalPath(unit, goal, CCPosition(), 5.f, true, false, true, true, false, false, failureReason, bot);
}

std::list<CCPosition> Util::PathFinding::FindOptimalPath(const sc2::Unit * unit, CCPosition goal, CCPosition secondaryGoal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, bool ignoreInfluence, bool flee, CCBot & bot)
{
	FailureReason failureReason;
	return FindOptimalPath(unit, goal, secondaryGoal, maxRange, exitOnInfluence, considerOnlyEffects, getCloser, ignoreInfluence, flee, true, failureReason, bot);
}

std::list<CCPosition> Util::PathFinding::FindOptimalPath(const sc2::Unit * unit, CCPosition goal, CCPosition secondaryGoal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, bool ignoreInfluence, bool flee, bool limitSearch, FailureReason & failureReason, CCBot & bot)
{
	std::list<CCPosition> path;
	std::set<IMNode*> opened;
	std::set<IMNode*> closed;
	std::map<int, float> bestCosts;

	const auto & lockOnTargets = bot.Commander().Combat().getLockOnTargets();
	const auto maxExploredNode = HARASS_PATHFINDING_MAX_EXPLORED_NODE * (!limitSearch ? 20 : exitOnInfluence ? 5 : bot.Config().TournamentMode ? 3 : 1);
	int numberOfTilesExploredAfterPathFound = 0;	//only used when getCloser is true
	IMNode* closestNode = nullptr;					//only used when getCloser is true
	const CCTilePosition startPosition = Util::GetTilePosition(unit->pos);
	const CCTilePosition goalPosition = Util::GetTilePosition(goal);
	const CCTilePosition secondaryGoalPosition = Util::GetTilePosition(secondaryGoal);
	const auto start = new IMNode(startPosition);
	bestCosts[start->getId()] = 0;
	opened.insert(start);

	while (!opened.empty() && closed.size() < maxExploredNode)
	{
		IMNode* currentNode = getLowestCostNode(opened);
		opened.erase(currentNode);
		if (bestCosts[currentNode->getId()] < currentNode->cost)
			continue;	// No need to check that node, we already checked it with a lower cost
		closed.insert(currentNode);

		bool shouldTriggerExit = false;
		if (flee)
		{
			if (maxRange == 0.f)
			{
				shouldTriggerExit = !HasCombatInfluenceOnTile(currentNode, unit, bot) && !HasEffectInfluenceOnTile(currentNode, unit, bot);
			}
			else
			{
				shouldTriggerExit = Dist(GetPosition(currentNode->position), goal) > maxRange || (!HasCombatInfluenceOnTile(currentNode, unit, bot) && !HasEffectInfluenceOnTile(currentNode, unit, bot));
			}
		}
		else
		{
			if (exitOnInfluence)
			{
				shouldTriggerExit = HasCombatInfluenceOnTile(currentNode, unit, bot) || HasEffectInfluenceOnTile(currentNode, unit, bot);
			}
			if (!shouldTriggerExit)
			{
				shouldTriggerExit = (ignoreInfluence ||
					(considerOnlyEffects || !HasCombatInfluenceOnTile(currentNode, unit, bot)) &&
					!HasEffectInfluenceOnTile(currentNode, unit, bot)) &&
					Util::Dist(Util::GetPosition(currentNode->position) + CCPosition(0.5f, 0.5f), goal) < maxRange;
			}
		}
		if (getCloser && shouldTriggerExit)
		{
			if (numberOfTilesExploredAfterPathFound > 10)
			{
				currentNode = closestNode;
			}
			else
			{
				shouldTriggerExit = false;
				const CCPosition shiftedPos = Util::GetPosition(currentNode->position) + CCPosition(0.5f, 0.5f);
				if (closestNode == nullptr || Util::Dist(shiftedPos, goal) < Util::Dist(Util::GetPosition(closestNode->position) + CCPosition(0.5f, 0.5f), goal))
				{
					closestNode = currentNode;
				}
				++numberOfTilesExploredAfterPathFound;
			}
		}
		if (shouldTriggerExit)
		{
			// If it exits on influence, we need to check if there is actually influence on the current tile. If so, we do not return a valid path
			if(exitOnInfluence && (HasCombatInfluenceOnTile(currentNode, unit, bot) || HasEffectInfluenceOnTile(currentNode, unit, bot)))
			{
				failureReason = INFLUENCE;
			}
			else
			{
				// If the unit wants to flee but stay in range
				if(flee && maxRange > 0.f && Dist(GetPosition(currentNode->position), goal) > maxRange)
				{
					// But this is the first node, we do not return a valid path
					if(currentNode->parent == nullptr)
					{
						failureReason = NO_NEED_TO_MOVE;
						break;
					}
					// Otherwise we return a path to the previous tile (otherwise the unit would go out of range)
					currentNode = currentNode->parent;
				}
				path = GetPositionListFromPath(currentNode, unit, bot);
			}
			break;
		}

		// Find neighbors
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				const CCTilePosition neighborPosition = GetNeighborNodePosition(x, y, currentNode, unit, bot);
				if (neighborPosition == CCTilePosition())
					continue;

				const CCPosition mapMin = bot.Map().mapMin();
				const CCPosition mapMax = bot.Map().mapMax();
				float totalCost = 0.f;

				if (neighborPosition.x < mapMin.x || neighborPosition.y < mapMin.y || neighborPosition.x >= mapMax.x || neighborPosition.y >= mapMax.y)
					continue;	// out of bounds check

				const float neighborDistance = Dist(currentNode->position, neighborPosition);
				const float creepCost = !unit->is_flying && bot.Observation()->HasCreep(GetPosition(neighborPosition)) ? HARASS_PATHFINDING_TILE_CREEP_COST : 0.f;
				const float influenceOnTile = (exitOnInfluence || ignoreInfluence) ? 0.f : GetEffectInfluenceOnTile(neighborPosition, unit, bot) + (considerOnlyEffects ? 0.f : GetCombatInfluenceOnTile(neighborPosition, unit, bot));
				float secondaryGoalCost = 0.f;
				if(secondaryGoal != CCPosition())
				{
					const auto neighborDirection = Normalized(GetPosition(neighborPosition) - GetPosition(currentNode->position));
					const auto secondaryGoalDirection = Normalized(secondaryGoal - GetPosition(currentNode->position));
					secondaryGoalCost = (1 - GetDotProduct(neighborDirection, secondaryGoalDirection)) * PATHFINDING_SECONDARY_GOAL_COST;
				}
				// Consider turning cost to prevent our units from wiggling while fleeing, but not for workers that want to know if the path is safe
				float turnCost = 0.f;
				if (!exitOnInfluence)
				{
					CCPosition facingVector;
					if (currentNode->parent == nullptr)
						facingVector = getFacingVector(unit);
					else
						facingVector = GetPosition(currentNode->position) - GetPosition(currentNode->parent->position);
					const auto directionVector = GetPosition(neighborPosition) - GetPosition(currentNode->position);
					const auto dotProduct = GetDotProduct(facingVector, directionVector);
					const auto turnValue = std::min(1.f, 1 - dotProduct);
					turnCost = turnValue * PATHFINDING_TURN_COST * Dist(currentNode->position, neighborPosition);
				}
				const float nodeCost = (influenceOnTile + creepCost + secondaryGoalCost + turnCost + HARASS_PATHFINDING_TILE_BASE_COST) * neighborDistance;
				totalCost += currentNode->cost + nodeCost;

				const float heuristic = CalcEuclidianDistanceHeuristic(neighborPosition, goalPosition, secondaryGoalPosition, bot);
				auto neighbor = new IMNode(neighborPosition, currentNode, totalCost, heuristic);

				if (bestCosts.find(neighbor->getId()) != bestCosts.end() && bestCosts[neighbor->getId()] <= totalCost)
					continue;
				
				/*if (SetContainsNode(closed, neighbor, false))
					continue;	// already explored check

				if (SetContainsNode(opened, neighbor, true))
					continue;	// node already opened and of lower cost*/

				bestCosts[neighbor->getId()] = totalCost;
				opened.insert(neighbor);
			}
		}
	}
	if(closed.size() >= maxExploredNode)
	{
		failureReason = TIMEOUT;
	}
	for (auto node : opened)
		delete node;
	for (auto node : closed)
		delete node;
	return path;
}

CCTilePosition Util::PathFinding::GetNeighborNodePosition(int x, int y, IMNode* currentNode, const sc2::Unit * rangedUnit, CCBot & bot)
{
	if (x == 0 && y == 0)
		return {};	// same tile check

	const CCTilePosition neighborPosition(currentNode->position.x + x, currentNode->position.y + y);

	if (currentNode->parent && neighborPosition == currentNode->parent->position)
		return {};	// parent tile check

	const CCPosition mapMin = bot.Map().mapMin();
	const CCPosition mapMax = bot.Map().mapMax();
	if (neighborPosition.x < mapMin.x || neighborPosition.y < mapMin.y || neighborPosition.x >= mapMax.x || neighborPosition.y >= mapMax.y)
		return {};	// out of bounds check

	if (!rangedUnit->is_flying)
	{
		if (bot.Commander().Combat().getBlockedTiles()[neighborPosition.x][neighborPosition.y])
			return {};	// tile is blocked

		// TODO check if the unit can pass between 2 blocked tiles (this will need a change in the blocked tiles map to have types of block)
		// All units can pass between 2 command structures, medium units and small units can pass between a command structure and another building 
		// while only small units can pass between non command buildings (where "between" means when 2 buildings have their corners touching diagonaly)

		if (!bot.Map().isWalkable(neighborPosition))
		{
			if (rangedUnit->unit_type != sc2::UNIT_TYPEID::TERRAN_REAPER)
				return {};	// unwalkable tile check

			if (!bot.Map().isWalkable(currentNode->position))
				return {};	// maybe the reaper is already on an unpathable tile

			if (abs(x + y) != 1)
				return {};	// cannot jump diagonaly over an unpathable tile (we kind of can, but certain cliffs don't allow it)

			const CCTilePosition furtherTile(neighborPosition.x + x, neighborPosition.y + y);
			if (!bot.Map().isWalkable(furtherTile))
				return {};	// unwalkable next tile check

			const float heightDiff = abs(bot.Map().terrainHeight(currentNode->position.x, currentNode->position.y) - bot.Map().terrainHeight(furtherTile.x, furtherTile.y));
			if (heightDiff < CLIFF_MIN_HEIGHT_DIFFERENCE || heightDiff > CLIFF_MAX_HEIGHT_DIFFERENCE)
				return {};	// unjumpable cliff check

			return furtherTile;
		}
	}

	return neighborPosition;
}

CCPosition Util::PathFinding::GetCommandPositionFromPath(std::list<CCPosition> & path, const sc2::Unit * rangedUnit, CCBot & bot)
{
	CCPosition returnPos;
	int i = 0;
	for(const auto position : path)
	{
		++i;
		returnPos = position;
		//we want to retun a node close to the current position
		if (i >= 3)
			break;
	}

	if (returnPos != CCPosition(0, 0) && rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER)
	{
		//We need to click far enough to jump cliffs
		const float squareDistance = Util::DistSq(rangedUnit->pos, returnPos);
		const float terrainHeightDiff = abs(bot.Map().terrainHeight(rangedUnit->pos.x, rangedUnit->pos.y) - bot.Map().terrainHeight(returnPos.x, returnPos.y));
		if (squareDistance < 2.5f * 2.5f && terrainHeightDiff > CLIFF_MIN_HEIGHT_DIFFERENCE)
			returnPos = rangedUnit->pos + Util::Normalized(returnPos - rangedUnit->pos) * 3.f;
	}
	if (Util::DistSq(rangedUnit->pos, returnPos) < 3*3)
	{
		returnPos = Normalized(returnPos - rangedUnit->pos) * 3 + rangedUnit->pos;
	}
#ifndef PUBLIC_RELEASE
	if (bot.Config().DrawHarassInfo)
		bot.Map().drawTile(Util::GetTilePosition(returnPos), sc2::Colors::Purple, 0.3f);
#endif
	return returnPos;
}

std::list<CCPosition> Util::PathFinding::GetPositionListFromPath(IMNode* currentNode, const sc2::Unit * rangedUnit, CCBot & bot)
{
	std::list<CCPosition> returnPositions;
	do
	{
		const CCPosition currentPosition = Util::GetPosition(currentNode->position) + CCPosition(0.5f, 0.5f);
#ifndef PUBLIC_RELEASE
		if (bot.Config().DrawHarassInfo)
			bot.Map().drawTile(Util::GetTilePosition(currentPosition), sc2::Colors::Teal, 0.2f);
#endif
		returnPositions.push_front(currentPosition);
		currentNode = currentNode->parent;
	} while (currentNode != nullptr);
	return returnPositions;
}

float Util::PathFinding::CalcEuclidianDistanceHeuristic(CCTilePosition from, CCTilePosition to, CCTilePosition secondaryGoal, CCBot & bot)
{
	float heuristic = Util::Dist(from, to) * HARASS_PATHFINDING_HEURISTIC_MULTIPLIER;
	if (secondaryGoal != CCTilePosition())
	{
		const auto dist = bot.Map().getGroundDistance(GetPosition(from), GetPosition(secondaryGoal));
		heuristic += dist * HARASS_PATHFINDING_HEURISTIC_MULTIPLIER * 2;
	}
	return heuristic;
}

bool Util::PathFinding::HasInfluenceOnTile(const CCTilePosition position, bool isFlying, CCBot & bot)
{
	return GetTotalInfluenceOnTile(position, isFlying, bot) != 0.f;
}

bool Util::PathFinding::HasCombatInfluenceOnTile(const IMNode* node, const sc2::Unit * unit, bool fromGround, CCBot & bot)
{
	if (unit->radius >= 1.f)
	{
		float totalInfluence = 0.f;
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				const auto position = CCTilePosition(node->position.x + x, node->position.y + y);
				totalInfluence += GetCombatInfluenceOnTile(position, unit->is_flying, fromGround, bot);
			}
		}
		return totalInfluence != 0.f;
	}

	return GetCombatInfluenceOnTile(node->position, unit->is_flying, fromGround, bot) != 0.f;
}

bool Util::PathFinding::HasCombatInfluenceOnTile(const CCTilePosition position, bool isFlying, bool fromGround, CCBot & bot)
{
	return GetCombatInfluenceOnTile(position, isFlying, fromGround, bot) != 0.f;
}

bool Util::PathFinding::HasCombatInfluenceOnTile(const IMNode* node, const sc2::Unit * unit, CCBot & bot)
{
	if(unit->radius >= 1.f)
	{
		float totalInfluence = 0.f;
		for (int x = -1; x <= 1; ++x)
		{
			for(int y = -1; y <= 1; ++y)
			{
				const auto position = CCTilePosition(node->position.x + x, node->position.y + y);
				totalInfluence += GetCombatInfluenceOnTile(position, unit->is_flying, bot);
			}
		}
		return totalInfluence != 0.f;
	}

	return GetCombatInfluenceOnTile(node->position, unit->is_flying, bot) != 0.f;
}

bool Util::PathFinding::HasCombatInfluenceOnTile(const CCTilePosition position, bool isFlying, CCBot & bot)
{
	return GetCombatInfluenceOnTile(position, isFlying, bot) != 0.f;
}

float Util::PathFinding::GetTotalInfluenceOnTiles(CCPosition position, bool isFlying, float radius, CCBot & bot)
{
	float totalInfluence = 0.f;
	const int minX = round(position.x - radius);
	const int minY = round(position.y - radius);
	const int maxX = round(position.x + radius);
	const int maxY = round(position.y + radius);
	for (int x = minX; x < maxX; ++x)
	{
		for (int y = minY; y < maxY; ++y)
		{
			const auto tilePosition = CCTilePosition(position.x + x, position.y + y);
			if (DistSq(position, CCPosition(tilePosition.x + 0.5f, tilePosition.y + 0.5f)) > radius * radius)
				continue;	// If center of tile is farther than radius, ignore
			totalInfluence += GetTotalInfluenceOnTile(tilePosition, isFlying, bot);
		}
	}
	return totalInfluence;
}

float Util::PathFinding::GetTotalInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, CCBot & bot)
{
	if (unit->radius >= 1.f)
	{
		float totalInfluence = 0.f;
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				const auto position = CCTilePosition(tile.x + x, tile.y + y);
				totalInfluence += GetTotalInfluenceOnTile(position, unit->is_flying, bot);
			}
		}
		return totalInfluence;
	}

	return GetTotalInfluenceOnTile(tile, unit->is_flying, bot);
}

float Util::PathFinding::GetTotalInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot)
{
	return isFlying ? bot.Commander().Combat().getTotalAirInfluence(tile) : bot.Commander().Combat().getTotalGroundInfluence(tile);
}

float Util::PathFinding::GetCombatInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, CCBot & bot)
{
	if (unit->radius >= 1.f)
	{
		float totalInfluence = 0.f;
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				const auto position = CCTilePosition(tile.x + x, tile.y + y);
				totalInfluence += GetCombatInfluenceOnTile(position, unit->is_flying, bot);
			}
		}
		return totalInfluence;
	}

	return GetCombatInfluenceOnTile(tile, unit->is_flying, bot);
}

float Util::PathFinding::GetCombatInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot)
{
	return isFlying ? bot.Commander().Combat().getAirCombatInfluence(tile) : bot.Commander().Combat().getGroundCombatInfluence(tile);
}

float Util::PathFinding::GetCombatInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, bool fromGround, CCBot & bot)
{
	if (unit->radius >= 1.f)
	{
		float totalInfluence = 0.f;
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				const auto position = CCTilePosition(tile.x + x, tile.y + y);
				totalInfluence += GetCombatInfluenceOnTile(position, unit->is_flying, fromGround, bot);
			}
		}
		return totalInfluence;
	}

	return GetCombatInfluenceOnTile(tile, unit->is_flying, fromGround, bot);
}

float Util::PathFinding::GetCombatInfluenceOnTile(CCTilePosition tile, bool isFlying, bool fromGround, CCBot & bot)
{
	if(isFlying)
	{
		if(fromGround)
			return bot.Commander().Combat().getAirFromGroundCombatInfluence(tile);
		return bot.Commander().Combat().getAirFromAirCombatInfluence(tile);
	}
	if (fromGround)
		return bot.Commander().Combat().getGroundFromGroundCombatInfluence(tile);
	return bot.Commander().Combat().getGroundFromAirCombatInfluence(tile);
}

float Util::PathFinding::GetGroundFromGroundCloakedInfluenceOnTile(CCTilePosition tile, CCBot & bot)
{
	return bot.Commander().Combat().getGroundFromGroundCloakedCombatInfluence(tile);
}

bool Util::PathFinding::HasGroundFromGroundCloakedInfluenceOnTile(CCTilePosition tile, CCBot & bot)
{
	return bot.Commander().Combat().getGroundFromGroundCloakedCombatInfluence(tile) != 0.f;
}

bool Util::PathFinding::HasEffectInfluenceOnTile(const IMNode* node, const sc2::Unit * unit, CCBot & bot)
{
	return GetEffectInfluenceOnTile(node->position, unit, bot) != 0.f;
}

bool Util::PathFinding::HasEffectInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot)
{
	return GetEffectInfluenceOnTile(tile, isFlying, bot) != 0.f;
}

bool Util::PathFinding::IsUnitOnTileWithInfluence(const sc2::Unit * unit, CCBot & bot)
{
	const CCTilePosition tile = Util::GetTilePosition(unit->pos);
	return GetCombatInfluenceOnTile(tile, unit, bot) != 0.f || GetEffectInfluenceOnTile(tile, unit, bot) != 0.f;
}

float Util::PathFinding::GetEffectInfluenceOnTile(CCTilePosition tile, const sc2::Unit * unit, CCBot & bot)
{
	if (unit->radius >= 1.f)
	{
		float totalInfluence = 0.f;
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				const auto position = CCTilePosition(tile.x + x, tile.y + y);
				totalInfluence += GetEffectInfluenceOnTile(position, unit->is_flying, bot);
			}
		}
		return totalInfluence;
	}

	return GetEffectInfluenceOnTile(tile, unit->is_flying, bot);
}

float Util::PathFinding::GetEffectInfluenceOnTile(CCTilePosition tile, bool isFlying, CCBot & bot)
{
	return isFlying ? bot.Commander().Combat().getAirEffectInfluence(tile) : bot.Commander().Combat().getGroundEffectInfluence(tile);
}

void Util::SetAllowDebug(bool _allowDebug)
{
	allowDebug = _allowDebug;
}

void Util::SetMapName(std::string _mapName)
{
	mapName = _mapName;
}

std::string Util::GetMapName()
{
	return mapName;
}

std::string Util::GetStringFromRace(const CCRace & race)
{
#ifdef SC2API
    if      (race == sc2::Race::Terran)  { return "Terran"; }
    else if (race == sc2::Race::Protoss) { return "Protoss"; }
    else if (race == sc2::Race::Zerg)    { return "Zerg"; }
    else if (race == sc2::Race::Random)  { return "Random"; }
#else
    if      (race == BWAPI::Races::Terran)  { return "Terran"; }
    else if (race == BWAPI::Races::Protoss) { return "Protoss"; }
    else if (race == BWAPI::Races::Zerg)    { return "Zerg"; }
    else if (race == BWAPI::Races::Unknown) { return "Unknown"; }
#endif
    BOT_ASSERT(false, "Unknown Race");
    return "Error";
}

CCRace Util::GetRaceFromString(const std::string & raceIn)
{
    std::string race(raceIn);
    std::transform(race.begin(), race.end(), race.begin(), ::tolower);

#ifdef SC2API
    if      (race == "terran")  { return sc2::Race::Terran; }
    else if (race == "protoss") { return sc2::Race::Protoss; }
    else if (race == "zerg")    { return sc2::Race::Zerg; }
    else if (race == "random")  { return sc2::Race::Random; }
    
    BOT_ASSERT(false, "Unknown Race: ", race.c_str());
    return sc2::Race::Random;
#else
    if      (race == "terran")  { return BWAPI::Races::Terran; }
    else if (race == "protoss") { return BWAPI::Races::Protoss; }
    else if (race == "zerg")    { return BWAPI::Races::Zerg; }
    else if (race == "random")  { return BWAPI::Races::Unknown; }

    BOT_ASSERT(false, "Unknown Race: ", race.c_str());
    return BWAPI::Races::Unknown;
#endif
}

CCPositionType Util::TileToPosition(float tile)
{
#ifdef SC2API
    return tile;
#else
    return (int)(tile * 32);
#endif
}

UnitType Util::GetSupplyProvider()
{
	return supplyType;
}

UnitType Util::GetWorkerType()
{
	return workerType;
}

UnitType Util::GetRessourceDepotType()
{
	return depotType;
}

UnitType Util::GetRefineryType()
{
	return refineryType;
}

UnitType Util::GetRichRefineryType()
{
	return richRefineryType;
}

CCPosition Util::CalcCenter(const std::vector<const sc2::Unit*> & units)
{
	if (units.empty())
	{
		return CCPosition(0, 0);
	}

	CCPositionType cx = 0;
	CCPositionType cy = 0;

	for (auto & unit : units)
	{
		BOT_ASSERT(unit, "Unit pointer was null");
		cx += unit->pos.x;
		cy += unit->pos.y;
	}

	return CCPosition(cx / units.size(), cy / units.size());
}

CCPosition Util::CalcCenter(const std::vector<const sc2::Unit *> & units, float& varianceOut)
{
	if (units.empty())
	{
		return {};
	}

	CCPosition center(0.f, 0.f);

	for (auto & unit : units)
	{
		BOT_ASSERT(unit, "Unit pointer was null");
		center += unit->pos;
	}

	center /= units.size();

	varianceOut = 0.f;
	for (auto & unit : units)
	{
		varianceOut += DistSq(center, unit->pos);
	}

	return center;
}

CCPosition Util::CalcCenter(const std::vector<Unit> & units)
{
    if (units.empty())
    {
        return CCPosition(0, 0);
    }

    CCPositionType cx = 0;
    CCPositionType cy = 0;

    for (auto & unit : units)
    {
        BOT_ASSERT(unit.isValid(), "Unit pointer was null");
        cx += unit.getPosition().x;
        cy += unit.getPosition().y;
    }

    return CCPosition(cx / units.size(), cy / units.size());
}

std::list<Util::UnitCluster> Util::GetUnitClusters(const sc2::Units & units, const std::vector<sc2::UNIT_TYPEID> & specialTypes, bool ignoreSpecialTypes, CCBot & bot)
{
	return GetUnitClusters(units, specialTypes, ignoreSpecialTypes, "", bot);
}

std::list<Util::UnitCluster> & Util::GetUnitClusters(const sc2::Units & units, const std::vector<sc2::UNIT_TYPEID> & specialTypes, bool ignoreSpecialTypes, std::string query, CCBot & bot)
{
	auto & unitClusters = m_unitClusters[query];
	auto & lastUnitClusterFrame = ignoreSpecialTypes ? m_lastUnitClusterFrame : m_lastSpecialUnitClusterFrame;
	// Return the saved clusters if they were calculated not long ago
	const auto currentFrame = bot.GetCurrentFrame();
	if (!query.empty() && currentFrame - lastUnitClusterFrame < UNIT_CLUSTERING_COOLDOWN)
		return unitClusters;

	unitClusters.clear();
	lastUnitClusterFrame = currentFrame;

	for (const auto unit : units)
	{
		if (!specialTypes.empty())
		{
			const bool specialUnit = Contains(unit->unit_type, specialTypes);
			if(specialUnit && ignoreSpecialTypes)
				continue;	// We want to ignore that type of unit
			if(!specialUnit && !ignoreSpecialTypes)
				continue;	// We want to consider only the special types and this is not one
		}
		
		std::vector<UnitCluster*> clustersToMerge;
		UnitCluster* mainCluster = nullptr;

		// Check the clusters to find if the unit is already part of a cluster and to check if it can be part of an existing cluster
		for (auto & cluster : unitClusters)
		{
			for(const auto clusterUnit : cluster.m_units)
			{
				if (Util::DistSq(clusterUnit->pos, unit->pos) <= UNIT_CLUSTERING_MAX_DISTANCE * UNIT_CLUSTERING_MAX_DISTANCE)
				{
					if (!mainCluster)
					{
						cluster.m_units.push_back(unit);
						mainCluster = &cluster;
					}
					else
					{
						clustersToMerge.push_back(&cluster);
					}
					break;
				}
			}
		}

		if (!clustersToMerge.empty())
		{
			// Merge clusters
			for (auto clusterToMerge : clustersToMerge)
			{
				// Put the units of the cluster into the main cluster
				for (const auto clusterUnit : clusterToMerge->m_units)
				{
					mainCluster->m_units.push_back(clusterUnit);
				}
				clusterToMerge->m_units.clear();
			}

			// Remove emptied clusters from the list
			auto clusterIt = unitClusters.begin();
			while (clusterIt != unitClusters.end())
			{
				if (clusterIt->m_units.empty())
					unitClusters.erase(clusterIt++);
				else
					++clusterIt;
			}
		}

		// If the unit was not already in a cluster, create one for it
		if(!mainCluster)
		{
			unitClusters.emplace_back(UnitCluster(unit->pos, { unit }));
		}
	}

	for (auto & cluster : unitClusters)
	{
		CCPosition center;
		for(const auto unit : cluster.m_units)
		{
			center += unit->pos;
		}
		cluster.m_center = CCPosition(center.x / cluster.m_units.size(), center.y / cluster.m_units.size());
	}

	return unitClusters;
}

void Util::CCUnitsToSc2Units(const std::vector<Unit> & units, sc2::Units & outUnits)
{
	for (auto unit : units)
	{
		outUnits.push_back(unit.getUnitPtr());
	}
}

void Util::Sc2UnitsToCCUnits(const sc2::Units & units, std::vector<Unit> & outUnits, CCBot & bot)
{
	for (auto unit : units)
	{
		outUnits.push_back(Unit(unit, bot));
	}
}

const sc2::Unit* Util::CalcClosestUnit(const sc2::Unit* unit, const sc2::Units & targets)
{
	float minDistance = 0;
	const sc2::Unit* closestUnit = nullptr;
	for (auto target : targets)
	{
		const float distance = Dist(target->pos, unit->pos);
		if (minDistance == 0 || distance < minDistance)
		{
			minDistance = distance;
			closestUnit = target;
		}
	}
	return closestUnit;
}

float Util::GetUnitsPower(const sc2::Units & units, const sc2::Units & targets, CCBot& bot)
{
	float unitsPower = 0;

	for (auto unit : units)
	{
		const sc2::Unit* closestTarget = CalcClosestUnit(unit, targets);
		unitsPower += GetUnitPower(unit, closestTarget, bot);
	}

	return unitsPower;
}

float Util::GetUnitsPower(const std::vector<Unit> & units, const std::vector<Unit> & targets, CCBot& bot)
{
	float unitsPower = 0;
	sc2::Units sc2Targets;
	CCUnitsToSc2Units(targets, sc2Targets);

	for (auto & unit : units)
	{
		const sc2::Unit* closestTarget = CalcClosestUnit(unit.getUnitPtr(), sc2Targets);
		unitsPower += GetUnitPower(unit, Unit(closestTarget, bot), bot);
	}

	return unitsPower;
}

float Util::GetUnitPower(const sc2::Unit* unit, const sc2::Unit* target, CCBot& bot)
{
	Unit targetUnit = target ? Unit(target, bot) : Unit();
	return GetUnitPower(Unit(unit, bot), targetUnit, bot);
}

float Util::GetUnitPower(const Unit &unit, const Unit& target, CCBot& bot)
{
	const float unitRange = target.isValid() ? GetAttackRangeForTarget(unit.getUnitPtr(), target.getUnitPtr(), bot) : GetMaxAttackRange(unit.getUnitPtr(), bot);
	///////// HEALTH
	float unitPower = pow(unit.getHitPoints() + unit.getShields(), 0.5f);
	///////// DPS
	if (target.isValid())
		unitPower *= Util::GetDpsForTarget(unit.getUnitPtr(), target.getUnitPtr(), bot);
	else
		unitPower *= Util::GetDps(unit.getUnitPtr(), bot);
	///////// DISTANCE
	if (target.isValid())
	{
		float distance = Util::Dist(unit.getPosition(), target.getPosition());
		if (unitRange < distance)
		{
			distance -= unitRange;
			const float distancePenalty = pow(0.5f, distance);
			unitPower *= distancePenalty;
		}
	}

	/*bool isRanged = unitRange >= 1.5f;

	///////// HEALTH
	float unitPower = pow(unit.getHitPoints() + unit.getShields(), 0.15f);

	///////// DPS
	if (target.isValid())
		unitPower *= Util::GetDpsForTarget(unit.getUnitPtr(), target.getUnitPtr(), bot);
	else
		unitPower *= Util::GetDps(unit.getUnitPtr(), bot);

	///////// RANGE
	if (isRanged)
		unitPower *= 3; //ranged bonus (+200%)

	///////// ARMOR
	unitPower *= 1 + Util::GetArmor(unit.getUnitPtr(), bot) * 0.1f;   //armor bonus (+10% per armor)

	///////// SPLASH DAMAGE
	//TODO bonus for splash damage

	///////// DISTANCE
	if (target.isValid())
	{
		float distance = Util::Dist(unit.getPosition(), target.getPosition());
		if (unitRange + 1 < distance)   //if the unit can't reach the closest unit (with a small buffer)
		{
			distance -= unitRange + 1;
			//float distancePenalty = unit.getType().isBuilding() ? 0.9f : 0.95f;
			float distancePenalty = pow(0.95f, distance);
			unitPower *= distancePenalty;	//penalty for distance (very fast for building but slow for units)
		}
	}*/

#ifndef PUBLIC_RELEASE
	if (bot.Config().DrawUnitPowerInfo)
		bot.Map().drawText(unit.getPosition(), "Power: " + std::to_string(unitPower));
#endif

	return unitPower;
}

/**
 * This is to be used only on enemy units in our bases
 */
float Util::GetSpecialCasePower(const Unit &unit)
{
	if (!unit.getType().isBuilding())
		return 0.f;
	const auto unitPtr = unit.getUnitPtr();
	return (unitPtr->health_max + unitPtr->shield_max) / 5;
}

float Util::GetNorm(const sc2::Point2D& point)
{
	return sqrt(pow(point.x, 2) + pow(point.y, 2));
}

void Util::Normalize(sc2::Point2D& point)
{
	const float norm = GetNorm(point);
	if(norm > EPSILON)
		point /= norm;
}

sc2::Point2D Util::Normalized(const sc2::Point2D& point)
{
	const float norm = GetNorm(point);
	if(norm > EPSILON)
		return sc2::Point2D(point.x / norm, point.y / norm);
    return sc2::Point2D(point.x, point.y);
}

/**
 * Returns a value between 1 (in the same direction) and -1 (opposite direction).
 */
float Util::GetDotProduct(const sc2::Point2D& v1, const sc2::Point2D& v2)
{
    sc2::Point2D v1n = Normalized(v1);
    sc2::Point2D v2n = Normalized(v2);
    return v1n.x * v2n.x + v1n.y * v2n.y;
}

bool Util::IsZerg(const CCRace & race)
{
#ifdef SC2API
    return race == sc2::Race::Zerg;
#else
    return race == BWAPI::Races::Zerg;
#endif
}

bool Util::IsProtoss(const CCRace & race)
{
#ifdef SC2API
    return race == sc2::Race::Protoss;
#else
    return race == BWAPI::Races::Protoss;
#endif
}

bool Util::CanUnitAttackAir(const sc2::Unit * unit, CCBot & bot)
{
	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);
	for (auto & weapon : unitTypeData.weapons)
	{
		if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Air)
			return true;
	}
	return GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Air) > 0.f;
}

bool Util::CanUnitAttackGround(const sc2::Unit * unit, CCBot & bot)
{
	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);
	for (auto & weapon : unitTypeData.weapons)
	{
		if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Ground)
			return true;
	}
	return GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Ground) > 0.f;
}

float Util::GetSpecialCaseRange(const sc2::Unit* unit, sc2::Weapon::TargetType where, bool ignoreSpells)
{
	float range = Util::GetSpecialCaseRange(unit->unit_type, where, ignoreSpells);
	if (range != 0)
		return range;

	if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON && !unit->is_powered)
	{
		range = 0.1f;	// hack so the cannons will be considered as weak
	}

	return range;
}

float Util::GetSpecialCaseRange(const sc2::UNIT_TYPEID unitType, sc2::Weapon::TargetType where, bool ignoreSpells)
{
	float range = 0.f;

	if (unitType == sc2::UNIT_TYPEID::ZERG_BANELING || unitType == sc2::UNIT_TYPEID::ZERG_BANELINGCOCOON)
	{
		if(where != sc2::Weapon::TargetType::Air)
			range = 2.2f - 0.375;	// The explosion radius is 2.2 starting from the center. After being used, the radius of the unit is added so we must substract it here.
	}
	else if (unitType == sc2::UNIT_TYPEID::TERRAN_BUNKER)
	{
		range = 7.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
	{
		if (where != sc2::Weapon::TargetType::Air)
			range = 2.0f;
	}
	else if (unitType == sc2::UNIT_TYPEID::PROTOSS_ADEPTPHASESHIFT)
	{
		if (where != sc2::Weapon::TargetType::Air)
			range = 4.f;
	}
	else if(unitType == sc2::UNIT_TYPEID::PROTOSS_CARRIER)
	{
		range = 8.f;
	}
	else if(unitType == sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED)
	{
		range = 5.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::PROTOSS_ORACLE)
	{
		if (where != sc2::Weapon::TargetType::Air)
			range = 4.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED)
	{
		if (where != sc2::Weapon::TargetType::Air)
			range = 3.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER)
	{
		range = 6.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::PROTOSS_PHOENIX)
	{
		if (where == sc2::Weapon::TargetType::Ground)
			range = 4.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::ZERG_HYDRALISK)
	{
		range = 6.f;	// Always consider they have their range upgrade because we don't want to detect it manually
	}
	else if (unitType == sc2::UNIT_TYPEID::TERRAN_CYCLONE)
	{
		if (!ignoreSpells)
			range = 7.f;
	}

	return range;
}

float Util::GetGroundAttackRange(const sc2::Unit * unit, CCBot & bot)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);

	float maxRange = GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Ground);

	if (maxRange == 0.f)
	{
		for (auto & weapon : unitTypeData.weapons)
		{
			// can attack target with a weapon
			if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Ground)
				maxRange = weapon.range;
		}
	}

	if (maxRange > 0.f)
	{
		maxRange += unit->radius;
		maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);
	}
	return maxRange;
}

float Util::GetAirAttackRange(const sc2::Unit * unit, CCBot & bot)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);

	float maxRange = GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Air);
	if (maxRange == 0.f)
	{
		for (auto & weapon : unitTypeData.weapons)
		{
			// can attack target with a weapon
			if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Air)
				maxRange = weapon.range;
		}
	}

	if (maxRange > 0.f)
	{
		maxRange += unit->radius;
		maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);
	}

	return maxRange;
}

float Util::GetAttackRangeForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot, bool ignoreSpells)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	if (!target)
		return 0.f;

	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);
	const sc2::Weapon::TargetType expectedWeaponType = target->is_flying ? sc2::Weapon::TargetType::Air : sc2::Weapon::TargetType::Ground;
	
	float maxRange = GetSpecialCaseRange(unit->unit_type, expectedWeaponType, ignoreSpells);
	if (maxRange == 0.f)
	{
		for (auto & weapon : unitTypeData.weapons)
		{
			// can attack target with a weapon
			if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == expectedWeaponType || target->unit_type == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)
				maxRange = weapon.range;
		}
	}

	if (maxRange > 0.f)
	{
		maxRange += unit->radius + target->radius;
		maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);
	}

	return maxRange; 
}

float Util::GetMaxAttackRangeForTargets(const sc2::Unit * unit, const std::vector<const sc2::Unit *> & targets, CCBot & bot)
{
    float maxRange = 0.f;
    for (const sc2::Unit * target : targets)
    {
        const float range = GetAttackRangeForTarget(unit, target, bot);
        if (range > maxRange)
            maxRange = range;
    }
    return maxRange;
}

float Util::GetMaxAttackRange(const sc2::Unit * unit, CCBot & bot)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	const sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);

	float maxRange = GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Any);

	if(maxRange == 0.f)
		maxRange = GetMaxAttackRange(unitTypeData, bot);
	else if(maxRange > 0.f)
		maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);

	if (maxRange > 0.f)
		maxRange += unit->radius;

	return maxRange;
}

float Util::GetMaxAttackRange(const sc2::UnitTypeID unitType, CCBot & bot)
{
    const sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unitType]);
    return GetMaxAttackRange(unitTypeData, bot);
}

float Util::GetMaxAttackRange(sc2::UnitTypeData unitTypeData, CCBot & bot)
{
    float maxRange = 0.0f;
    for (auto & weapon : unitTypeData.weapons)
    {
        // can attack target with a weapon
        if (weapon.range > maxRange)
            maxRange = weapon.range;
    }

	if (maxRange == 0.f)
		maxRange = GetSpecialCaseRange(unitTypeData.unit_type_id);

	if(maxRange > 0.f)
		maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);

    return maxRange;
}

float Util::GetAttackRangeBonus(const sc2::UnitTypeID unitType, CCBot & bot)
{
	const uint32_t type = unitType;
	switch(type)
	{
		case uint32_t(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET):
		case uint32_t(sc2::UNIT_TYPEID::TERRAN_POINTDEFENSEDRONE):
		case uint32_t(sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS):
			return bot.Strategy().enemyHasHiSecAutoTracking() ? 1 : 0;
		default:
			return 0;
	}
}

float Util::GetArmor(const sc2::Unit * unit, CCBot & bot)
{
    sc2::UnitTypeData unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
    return unitTypeData.armor;
}

float Util::GetGroundDps(const sc2::Unit * unit, CCBot & bot)
{
	return GetDps(unit, sc2::Weapon::TargetType::Ground, bot);
}

float Util::GetAirDps(const sc2::Unit * unit, CCBot & bot)
{
	return GetDps(unit, sc2::Weapon::TargetType::Air, bot);
}

float Util::GetDps(const sc2::Unit * unit, CCBot & bot)
{
	return GetDps(unit, sc2::Weapon::TargetType::Any, bot);
}

float Util::GetDps(const sc2::Unit * unit, const sc2::Weapon::TargetType targetType, CCBot & bot)
{
	float dps = GetSpecialCaseDps(unit, bot, targetType);
	if (dps == 0.f)
	{
		sc2::UnitTypeData unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
		for (auto & weapon : unitTypeData.weapons)
		{
			if (weapon.type == sc2::Weapon::TargetType::Any || targetType == sc2::Weapon::TargetType::Any || weapon.type == targetType)
			{
				float weaponDps = weapon.damage_;
				weaponDps *= weapon.attacks / weapon.speed;
				if (weaponDps > dps)
					dps = weaponDps;
			}
		}
	}
	return dps;
}

float Util::GetDpsForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot)
{
    const sc2::Weapon::TargetType expectedWeaponType = target->is_flying ? sc2::Weapon::TargetType::Air : sc2::Weapon::TargetType::Ground;
    float dps = GetSpecialCaseDps(unit, bot, expectedWeaponType);
    if (dps == 0.f)
    {
		sc2::UnitTypeData unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
		sc2::UnitTypeData targetTypeData = GetUnitTypeDataFromUnitTypeId(target->unit_type, bot);
        for (auto & weapon : unitTypeData.weapons)
        {
            if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == expectedWeaponType || target->unit_type == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)
            {
                float weaponDps = weapon.damage_;
                for (auto & damageBonus : weapon.damage_bonus)
                {
                    if (std::find(targetTypeData.attributes.begin(), targetTypeData.attributes.end(), damageBonus.attribute) != targetTypeData.attributes.end())
                        weaponDps += damageBonus.bonus;
                }
                weaponDps -= targetTypeData.armor;
                weaponDps *= weapon.attacks / weapon.speed;
                if (weaponDps > dps)
                    dps = weaponDps;
            }
        }
    }

    return dps;
}

float Util::GetSpecialCaseDps(const sc2::Unit * unit, CCBot & bot, sc2::Weapon::TargetType where)
{
    float dps = 0.f;

    if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_BANELING || unit->unit_type == sc2::UNIT_TYPEID::ZERG_BANELINGCOCOON)
    {
		if(where != sc2::Weapon::TargetType::Air)
			dps = 15.f;
    }
    else if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
    {
        dps = 0.f;
    }
    else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER)
    {
        //A special case must be done for bunkers since they have no weapon and the cargo space is not available
        dps = 30.f;
    }
	else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
	{
		if (where != sc2::Weapon::TargetType::Air)
			dps = 5.0f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_ADEPTPHASESHIFT)
	{
		if (where != sc2::Weapon::TargetType::Air)
			dps = 13.7f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_CARRIER)
	{
		dps = 37.4f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED)
	{
		dps = 50.f;	//DPS is not really relevant since it's a single powerful attack
	}
	else if(unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON && !unit->is_powered)
	{
		dps = 0.1f;	// hack so the cannons will be considered as weak
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_ORACLE)
	{
		if (where != sc2::Weapon::TargetType::Air)
			dps = 15.f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED)
	{
		if (where != sc2::Weapon::TargetType::Air)
			dps = 200.f;
	}
	else if(unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER)
	{
		if (where == sc2::Weapon::TargetType::Air)
			dps = 31.1f;
		else
			dps = 49.8f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOENIX)
	{
		if (where == sc2::Weapon::TargetType::Ground)
			dps = 12.7;
	}

    return dps;
}

float Util::GetDamageForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot)
{
	const sc2::Weapon::TargetType expectedWeaponType = target->is_flying ? sc2::Weapon::TargetType::Air : sc2::Weapon::TargetType::Ground;
	float damage = GetSpecialCaseDamage(unit, bot, expectedWeaponType);
	if (damage == 0.f)
	{
		sc2::UnitTypeData unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
		sc2::UnitTypeData targetTypeData = GetUnitTypeDataFromUnitTypeId(target->unit_type, bot);
		for (auto & weapon : unitTypeData.weapons)
		{
			if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == expectedWeaponType || target->unit_type == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)
			{
				float weaponDamage = weapon.damage_;
				for (auto & damageBonus : weapon.damage_bonus)
				{
					if (std::find(targetTypeData.attributes.begin(), targetTypeData.attributes.end(), damageBonus.attribute) != targetTypeData.attributes.end())
						weaponDamage += damageBonus.bonus;
				}
				weaponDamage -= targetTypeData.armor;
				weaponDamage *= weapon.attacks;
				if (weaponDamage > damage)
					damage = weaponDamage;
			}
		}
	}

	return damage;
}

float Util::GetSpecialCaseDamage(const sc2::Unit * unit, CCBot & bot, sc2::Weapon::TargetType where)
{
	float damage = 0.f;

	if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_BANELING || unit->unit_type == sc2::UNIT_TYPEID::ZERG_BANELINGCOCOON)
	{
		if (where != sc2::Weapon::TargetType::Air)
			damage = 20.f;
	}
	else if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
	{
		damage = 0.f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
	{
		if (where != sc2::Weapon::TargetType::Air)
			damage = 5.f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED)
	{
		damage = 125.f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON && !unit->is_powered)
	{
		damage = 0.f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED)
	{
		if (where != sc2::Weapon::TargetType::Air)
			damage = 200.f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER)
	{
		if (where == sc2::Weapon::TargetType::Air)
			damage = 5.f;
		else
			damage = 8.f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOENIX)
	{
		if (where == sc2::Weapon::TargetType::Ground)
			damage = 10.f;
	}

	return damage;
}

// get threats to our harass unit
std::vector<const sc2::Unit *> Util::getThreats(const sc2::Unit * unit, const std::vector<const sc2::Unit *> & targets, CCBot & bot)
{
	BOT_ASSERT(unit, "null ranged unit in getThreats");

	const auto & enemyUnitsBeingRepaired = bot.GetEnemyUnitsBeingRepaired();
	std::vector<const sc2::Unit *> threats;

	// for each possible threat
	for (auto targetUnit : targets)
	{
		BOT_ASSERT(targetUnit, "null target unit in getThreats");
		if (Util::GetDpsForTarget(targetUnit, unit, bot) == 0.f)
			continue;
		//We consider a unit as a threat if the sum of its range and speed is bigger than the distance to our unit
		//But this is not working so well for melee units, we keep every units in a radius of min threat range
		const float threatRange = getThreatRange(unit, targetUnit, bot);
		if (Util::DistSq(unit->pos, targetUnit->pos) < threatRange * threatRange)
		{
			threats.push_back(targetUnit);

			// We check if that threat is being repaired
			const auto & it = enemyUnitsBeingRepaired.find(targetUnit);
			if(it != enemyUnitsBeingRepaired.end())
			{
				// If so, we consider all the SCVs repairing it as threats
				for(const auto enemyRepairingSCV : it->second)
				{
					threats.push_back(enemyRepairingSCV);
				}
			}
		}
	}

	return threats;
}

// get threats to our harass unit
std::vector<const sc2::Unit *> Util::getThreats(const sc2::Unit * unit, const std::vector<Unit> & targets, CCBot & bot)
{
	BOT_ASSERT(unit, "null ranged unit in getThreats");

	std::vector<const sc2::Unit *> targetsPtrs(targets.size());
	for (auto& targetUnit : targets)
		targetsPtrs.push_back(targetUnit.getUnitPtr());

	return getThreats(unit, targetsPtrs, bot);
}

//calculate radius max(min range, range + speed + height bonus + small buffer)
float Util::getThreatRange(const sc2::Unit * unit, const sc2::Unit * threat, CCBot & m_bot)
{
	const float HARASS_THREAT_MIN_HEIGHT_DIFF = 2.f;
	const float HARASS_THREAT_RANGE_BUFFER = 1.f;
	const float HARASS_THREAT_RANGE_HEIGHT_BONUS = 4.f;

	const float heightBonus = unit->is_flying ? 0.f : Util::TerrainHeight(threat->pos) > Util::TerrainHeight(unit->pos) + HARASS_THREAT_MIN_HEIGHT_DIFF ? HARASS_THREAT_RANGE_HEIGHT_BONUS : 0.f;
	const float tempestAirBonus = threat->unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST && unit->is_flying ? 2.f : 0.f;
	const float threatRange = Util::GetAttackRangeForTarget(threat, unit, m_bot) + Util::getSpeedOfUnit(threat, m_bot) + heightBonus + tempestAirBonus + HARASS_THREAT_RANGE_BUFFER;

	return threatRange;
}

float Util::getThreatRange(bool isFlying, CCPosition position, float radius, const sc2::Unit * threat, CCBot & m_bot)
{
	const float HARASS_THREAT_MIN_HEIGHT_DIFF = 2.f;
	const float HARASS_THREAT_RANGE_BUFFER = 1.f;
	const float HARASS_THREAT_RANGE_HEIGHT_BONUS = 4.f;

	const float heightBonus = isFlying ? 0.f : Util::TerrainHeight(threat->pos) > Util::TerrainHeight(position) + HARASS_THREAT_MIN_HEIGHT_DIFF ? HARASS_THREAT_RANGE_HEIGHT_BONUS : 0.f;
	const float tempestAirBonus = threat->unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST && isFlying ? 2.f : 0.f;
	const float threatWeaponRange = isFlying ? Util::GetAirAttackRange(threat, m_bot) : Util::GetGroundAttackRange(threat, m_bot);
	const float threatRange = threatWeaponRange + radius + Util::getSpeedOfUnit(threat, m_bot) + heightBonus + tempestAirBonus + HARASS_THREAT_RANGE_BUFFER;

	return threatRange;
}

float Util::getAverageSpeedOfUnits(const std::vector<Unit>& units, CCBot & bot)
{
	if (units.empty())
		return 0.f;

	float squadSpeed = 0;

	for (auto & unit : units)
	{
		squadSpeed += getSpeedOfUnit(unit.getUnitPtr(), bot);
	}

	return squadSpeed / (float)units.size();
}

float Util::getSpeedOfUnit(const sc2::Unit * unit, CCBot & bot)
{
	float zergBonus = 1.f;
	if(Unit(unit, bot).getType().getRace() == CCRace::Zerg && !unit->is_burrowed && !unit->is_flying)
	{
		/* From https://liquipedia.net/starcraft2/Creep
		 * All Zerg ground units move faster when traveling on Creep, with the exception of burrowed units, Drones, Broodlings, and Changelings not disguised as Zerglings. 
		 * The increase is 30% for most units. Queens move 166.67% faster on creep, Hydralisks move 50% faster, and Spine Crawlers and Spore Crawlers move 150% faster. */
		if(bot.Observation()->HasCreep(unit->pos))
		{
			if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_QUEEN)
				zergBonus = 2.6667f;
			else if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_HYDRALISK)
				zergBonus = 1.5f;
			else if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_SPINECRAWLERUPROOTED || unit->unit_type == sc2::UNIT_TYPEID::ZERG_SPORECRAWLERUPROOTED)
				zergBonus = 2.5f;
			else if (unit->unit_type != sc2::UNIT_TYPEID::ZERG_DRONE && unit->unit_type != sc2::UNIT_TYPEID::ZERG_BROODLING && unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELING
				&& unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELINGMARINE && unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELINGMARINESHIELD && unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELINGZEALOT)
				zergBonus = 1.3f;
		}
		if (bot.Strategy().enemyHasMetabolicBoost() && unit->unit_type == sc2::UNIT_TYPEID::ZERG_ZERGLING)
			zergBonus *= 1.6f;
	}
	return GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot).movement_speed * zergBonus;
}

CCPosition Util::getFacingVector(const sc2::Unit * unit)
{
	return CCPosition(cos(unit->facing), sin(unit->facing));
}

bool Util::isUnitFacingAnother(const sc2::Unit * unitA, const sc2::Unit * unitB)
{
	const CCPosition facingVector = getFacingVector(unitA);
	const CCPosition unitsVector = Util::Normalized(unitB->pos - unitA->pos);
	return sc2::Dot2D(facingVector, unitsVector) > 0.99f;
}

bool Util::isUnitLockedOn(const sc2::Unit * unit)
{
	return unitHasBuff(unit, sc2::BUFF_ID::LOCKON);
}

bool Util::isUnitDisabled(const sc2::Unit * unit)
{
	return unitHasBuff(unit, sc2::BUFF_ID::RAVENSCRAMBLERMISSILE);
}

bool Util::isUnitLifted(const sc2::Unit * unit)
{
	return unitHasBuff(unit, sc2::BUFF_ID::GRAVITONBEAM);
}

bool Util::unitHasBuff(const sc2::Unit * unit, sc2::BUFF_ID buffId)
{
	for (const auto buff : unit->buffs)
	{
		if (buff.ToType() == buffId)
			return true;
	}
	return false;
}

bool Util::IsPositionUnderDetection(CCPosition position, CCBot & bot)
{
	const auto & enemyScans = bot.Commander().Combat().GetEnemyScans();
	for(const auto & enemyScan : enemyScans)
	{
		if (Util::DistSq(position, enemyScan) <= 13 * 13)
			return true;
	}

	std::vector<sc2::UnitTypeID> detectorTypes = {
		sc2::UNIT_TYPEID::TERRAN_MISSILETURRET,
		sc2::UNIT_TYPEID::TERRAN_RAVEN,
		sc2::UNIT_TYPEID::PROTOSS_OBSERVER,
		sc2::UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE,
		sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON,
		sc2::UNIT_TYPEID::ZERG_SPORECRAWLER,
		sc2::UNIT_TYPEID::ZERG_OVERSEER
	};
	for(const auto detectorType : detectorTypes)
	{
		auto & detectors = bot.GetKnownEnemyUnits(detectorType);
		for(const auto & detector : detectors)
		{
			const float distance = Util::DistSq(detector, position);
			float detectionRange = detector.getUnitPtr()->detect_range;
			if (detectionRange == 0)
				detectionRange = detector.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE ? 13.75f : 11;
			if(distance <= detectionRange * detectionRange)
			{
				return true;
			}
		}
	}
	auto & areasUnderDetection = bot.Analyzer().GetAreasUnderDetection();
	const int areaUnderDetectionSize = 10;
	for(auto & area : areasUnderDetection)
	{
		if(Util::DistSq(position, area.first) < areaUnderDetectionSize * areaUnderDetectionSize)
		{
			return true;
		}
	}
	return false;
}

bool Util::IsTerran(const CCRace & race)
{
#ifdef SC2API
    return race == sc2::Race::Terran;
#else
    return race == BWAPI::Races::Terran;
#endif
}

int Util::ToMapKey(const CCTilePosition position)
{
	return position.x * 1000000 + position.y;
}

CCTilePosition Util::FromCCTilePositionMapKey(const int mapKey)
{
	int y = mapKey % 1000;//Keep last 3 digits
	int x = (mapKey - y) / 1000000;
	return CCTilePosition(x, y);
}

CCTilePosition Util::GetTilePosition(const CCPosition & pos)
{
#ifdef SC2API
    return CCTilePosition((int)std::floor(pos.x), (int)std::floor(pos.y));
#else
    return CCTilePosition(pos);
#endif
}

CCPosition Util::GetPosition(const CCTilePosition & tile)
{
#ifdef SC2API
    return CCPosition((float)tile.x, (float)tile.y);
#else
    return CCPosition(tile);
#endif
}

float Util::Dist(const CCPosition & p1, const CCPosition & p2)
{
    return sqrtf((float)Util::DistSq(p1,p2));
}

float Util::Dist(const CCTilePosition & p1, const CCTilePosition & p2)
{
	return sqrtf((float)Util::DistSq(p1, p2));
}

float Util::Dist(const Unit & unit, const CCPosition & p2)
{
    return Dist(unit.getPosition(), p2);
}

float Util::Dist(const Unit & unit1, const Unit & unit2)
{
    return Dist(unit1.getPosition(), unit2.getPosition());
}

CCPositionType Util::DistSq(const CCTilePosition & p1, const CCTilePosition & p2)
{
	CCPositionType dx = p1.x - p2.x;
	CCPositionType dy = p1.y - p2.y;

	return dx * dx + dy * dy;
}

CCPositionType Util::DistSq(const Unit & unit, const CCPosition & p2)
{
	return DistSq(unit.getPosition(), p2);
}

CCPositionType Util::DistSq(const Unit & unit, const Unit & unit2)
{
	return DistSq(unit.getPosition(), unit2.getPosition());
}

CCPositionType Util::DistSq(const CCPosition & p1, const CCPosition & p2)
{
    CCPositionType dx = p1.x - p2.x;
    CCPositionType dy = p1.y - p2.y;

    return dx*dx + dy*dy;
}

float Util::DistBetweenLineAndPoint(const CCPosition & linePoint1, const CCPosition & linePoint2, const CCPosition & point)
{
	const auto numerator = abs((linePoint2.y - linePoint1.y) * point.x - (linePoint2.x - linePoint1.x) * point.y + linePoint2.x * linePoint1.y - linePoint2.y * linePoint1.x);
	const auto denominator = sqrt(pow(linePoint2.y - linePoint1.y, 2) + pow(linePoint2.x - linePoint1.x, 2));
	if (denominator == 0)
		return 0;
	return numerator / denominator;
}

sc2::Point2D Util::CalcLinearRegression(const std::vector<const sc2::Unit *> & units)
{
    float sumX = 0, sumY = 0, sumXSqr = 0, sumXY = 0, avgX, avgY, numerator, denominator, slope;
    size_t size = units.size();
    for (auto unit : units)
    {
        sumX += unit->pos.x;
        sumY += unit->pos.y;
        sumXSqr += pow(unit->pos.x, 2);
        sumXY += unit->pos.x * unit->pos.y;
    }
    avgX = sumX / size;
    avgY = sumY / size;
    denominator = size * sumXSqr - pow(sumX, 2);
    if (denominator == 0.f)
        return sc2::Point2D(0, 1);
    numerator = size * sumXY - sumX * sumY;
    slope = numerator / denominator;
    return Util::Normalized(sc2::Point2D(1, slope));
}

sc2::Point2D Util::CalcPerpendicularVector(const sc2::Point2D & vector)
{
    return sc2::Point2D(vector.y, -vector.x);
}

bool Util::Pathable(const sc2::Point2D & point)
{
	sc2::Point2DI pointI((int)point.x, (int)point.y);
	if (pointI.x < 0 || pointI.x >= gameInfo->width || pointI.y < 0 || pointI.y >= gameInfo->width)
	{
		return false;
	}
	return m_pathable[pointI.x][pointI.y];
}

bool Util::Placement(const sc2::Point2D & point)
{
	sc2::Point2DI pointI((int)point.x, (int)point.y);
	if (pointI.x < 0 || pointI.x >= gameInfo->width || pointI.y < 0 || pointI.y >= gameInfo->width)
	{
		return false;
	}
    return m_placement[pointI.x][pointI.y];
}

float Util::TerrainHeight(const sc2::Point2D & point)
{
	sc2::Point2DI pointI((int)point.x, (int)point.y);
	if (pointI.x < 0 || pointI.x >= gameInfo->width || pointI.y < 0 || pointI.y >= gameInfo->width)
	{
		return 0.0f;
	}
	return m_terrainHeight[pointI.x][pointI.y];
}

float Util::TerrainHeight(const CCTilePosition pos)
{
	return m_terrainHeight[pos.x][pos.y];
}

float Util::TerrainHeight(const int x, const int y)
{
	return m_terrainHeight[x][y];
}

void Util::VisualizeGrids(CCBot& bot) 
{
    const sc2::GameInfo& info = bot.Observation()->GetGameInfo();

    sc2::Point2D camera = bot.Observation()->GetCameraPos();
    for (float x = camera.x - 8.0f; x < camera.x + 8.0f; ++x) 
    {
        for (float y = camera.y - 8.0f; y < camera.y + 8.0f; ++y)
        {
            // Draw in the center of each 1x1 cell
            sc2::Point2D point(x + 0.5f, y + 0.5f);

            float height = TerrainHeight(sc2::Point2D(x, y));
            bool placable = Placement( sc2::Point2D(x, y));
            //bool pathable = Pathable(info, sc2::Point2D(x, y));

            sc2::Color color = placable ? sc2::Colors::Green : sc2::Colors::Red;
			bot.Map().drawTile(CCTilePosition(x, y), color);
        }
    }
}

sc2::UnitTypeID Util::GetUnitTypeIDFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::UnitTypeData & data : bot.Observation()->GetUnitTypeData())
    {
        if (name == data.name)
        {
            return data.unit_type_id;
        }
    }

    return 0;
}

sc2::UnitTypeData Util::GetUnitTypeDataFromUnitTypeId(const sc2::UnitTypeID unitTypeId, CCBot & bot)
{
    return bot.Observation()->GetUnitTypeData()[unitTypeId];
}

sc2::UpgradeID Util::GetUpgradeIDFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::UpgradeData & data : bot.Observation()->GetUpgradeData())
    {
        if (name == data.name)
        {
            return data.upgrade_id;
        }
    }

    return 0;
}

#ifdef SC2API
sc2::BuffID Util::GetBuffFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::BuffData & data : bot.Observation()->GetBuffData())
    {
        if (name == data.name)
        {
            return data.buff_id;
        }
    }

    return 0;
}

sc2::AbilityID Util::GetAbilityFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::AbilityData & data : bot.Observation()->GetAbilityData())
    {
        if (name == data.link_name)
        {
            return data.ability_id;
        }
    }

    return 0;
}

// To select unit: From https://github.com/Blizzard/s2client-api/blob/master/tests/feature_layers_shared.cc
sc2::Point2DI Util::ConvertWorldToCamera(const sc2::Point2D camera_world, const sc2::Point2D& world) {
    float camera_size = gameInfo->options.feature_layer.camera_width;
    int image_width = gameInfo->options.feature_layer.map_resolution_x;
    int image_height = gameInfo->options.feature_layer.map_resolution_y;

    // Pixels always cover a square amount of world space. The scale is determined
    // by making the shortest axis of the camera match the requested camera_size.
    float pixel_size = camera_size / std::min(image_width, image_height);
    float image_width_world = pixel_size * image_width;
    float image_height_world = pixel_size * image_height;

    // Origin of world space is bottom left. Origin of image space is top left.
    // The feature layer is centered around the camera target position.
    float image_origin_x = camera_world.x - image_width_world / 2.0f;
    float image_origin_y = camera_world.y + image_height_world / 2.0f;
    float image_relative_x = world.x - image_origin_x;
    float image_relative_y = image_origin_y - world.y;

    int image_x = static_cast<int>(image_relative_x / pixel_size);
    int image_y = static_cast<int>(image_relative_y / pixel_size);

    return sc2::Point2DI(image_x, image_y);
}
#endif

// checks where a given unit can make a given unit type now
// this is done by iterating its legal abilities for the build command to make the unit
bool Util::UnitCanMetaTypeNow(const Unit & unit, const UnitType & type, CCBot & bot)
{
#ifdef SC2API
    BOT_ASSERT(unit.isValid(), "Unit pointer was null");
    sc2::AvailableAbilities available_abilities = bot.Query()->GetAbilitiesForUnit(unit.getUnitPtr());
    
    // quick check if the unit can't do anything it certainly can't build the thing we want
    if (available_abilities.abilities.empty()) 
    {
        return false;
    }
    else 
    {
        // check to see if one of the unit's available abilities matches the build ability type
        sc2::AbilityID MetaTypeAbility = bot.Data(type).buildAbility;
        for (const sc2::AvailableAbility & available_ability : available_abilities.abilities) 
        {
            if (available_ability.ability_id == MetaTypeAbility)
            {
                return true;
            }
        }
    }
#else

#endif
    return false;
}

void Util::DisplayError(const std::string & error, const std::string & errorCode, CCBot & bot, bool isCritical)
{
	auto it = find(displayedError.begin(), displayedError.end(), errorCode);
	if (it != displayedError.end())
	{
		return;
	}

	std::stringstream ss;
	ss << (isCritical ? "[CRITICAL ERROR]" : "[ERROR]") << " : " << error << " | " << errorCode;

	if (allowDebug && isCritical)
	{
		bot.Actions()->SendChat(ss.str());
	}

	Util::Log(ss.str(), bot);
	displayedError.push_back(errorCode);
}

void Util::ClearDisplayedErrors()
{
	displayedError.clear();
}

void Util::CreateLog(CCBot & bot)
{
	time_t now = time(0);
	char buf[80];
	strftime(buf, sizeof(buf), "./data/%Y-%m-%d--%H-%M-%S.log", localtime(&now));
	file.open(buf);

	std::stringstream races;
	races << Util::GetStringFromRace(bot.GetPlayerRace(Players::Self)) << " VS " << Util::GetStringFromRace(bot.GetPlayerRace(Players::Enemy)) << " on " << Util::GetMapName();
	Util::LogNoFrame(races.str(), bot);
}

void Util::DebugLog(const std::string & function, CCBot & bot)
{
	if (allowDebug)
	{
		file << bot.GetGameLoop() << ": " << function << std::endl;
	}
}

void Util::DebugLog(const std::string & function, const std::string & message, CCBot & bot)
{
	if (allowDebug)
	{
		file << bot.GetGameLoop() << ": " << function << " | " << message << std::endl;
	}
}

void Util::LogNoFrame(const std::string & function, CCBot & bot)
{
	file << function << std::endl;
}

void Util::Log(const std::string & function, CCBot & bot)
{
	file << bot.GetGameLoop() << ": " << function << std::endl;
}

void Util::Log(const std::string & function, const std::string & message, CCBot & bot)
{
	file << bot.GetGameLoop() << ": " << function << " | " << message << std::endl;
}

int Util::GetTimeControlSpeed()
{
	return timeControlRatio;
}

int Util::GetTimeControlMaxSpeed()
{
	return -1;
}

void Util::TimeControlIncreaseSpeed()
{
	switch (timeControlRatio)
	{
		case 5:
			timeControlRatio = 50;
			break;
		case 50:
			timeControlRatio = 100;
			break;
		case 100:
			timeControlRatio = -1;
			break;
		case -1:
			timeControlRatio = -1;
			break;
		default:
			timeControlRatio = -1;
			break;
	}
}

void Util::TimeControlDecreaseSpeed()
{

	switch (timeControlRatio)
	{
		case 5:
			timeControlRatio = 5;
			break;
		case 50:
			timeControlRatio = 5;
			break;
		case 100:
			timeControlRatio = 50;
			break;
		case -1:
			timeControlRatio = 100;
			break;
		default:
			timeControlRatio = 5;
			break;
	}
}

bool Util::SimulateCombat(const sc2::Units & units, const sc2::Units & enemyUnits, CCBot & bot)
{
	CombatState state;
	for(int i=0; i<2; ++i)
	{
		const sc2::Units & playerUnits = i == 0 ? units : enemyUnits;
		for (const auto unit : playerUnits)
		{
			// Since bunkers deal no damage in the simulation, we swap them for 4 Marines with extra health
			if(unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER)
			{
				const int owner = i + 1;
				for(int j=0; j < 4; ++j)
					state.units.push_back(CombatUnit(owner, sc2::UNIT_TYPEID::TERRAN_MARINE, 200, false));
			}
			else
				state.units.push_back(CombatUnit(*unit));
		}
	}

	CombatUpgrades player1upgrades = {};

	CombatUpgrades player2upgrades = {};
	
	state.environment = &m_simulator->getCombatEnvironment(player1upgrades, player2upgrades);
	
	CombatSettings settings;
	
	// Simulate for at most 100 *game* seconds
	// Just to show that it can be configured, in this case 100 game seconds is more than enough for the battle to finish.
	settings.maxTime = 100;
	CombatResult outcome = m_simulator->predict_engage(state, settings);
	const int winner = outcome.state.owner_with_best_outcome();
	return winner == bot.IsPlayer1Human() ? 2 : 1;
}