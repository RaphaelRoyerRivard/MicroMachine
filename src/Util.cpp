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
const float PATHFINDING_SECONDARY_GOAL_HEURISTIC_MULTIPLIER = 10.f;
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
		heuristic(0.f),
		influence(0.f)
	{
	}
	IMNode(CCTilePosition position) :
		position(position),
		parent(nullptr),
		cost(0.f),
		heuristic(0.f),
		influence(0.f)
	{
	}
	IMNode(CCTilePosition position, IMNode* parent, float heuristic) :
		position(position),
		parent(parent),
		cost(0.f),
		heuristic(heuristic),
		influence(0.f)
	{
	}
	IMNode(CCTilePosition position, IMNode* parent, float cost, float heuristic, float influence) :
		position(position),
		parent(parent),
		cost(cost),
		heuristic(heuristic),
		influence(influence)
	{
	}
	CCTilePosition position;
	IMNode* parent;
	float cost;
	float heuristic;
	float influence;

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
	richAssimilatorId = bot.Config().StarCraft2Version > "4.10.4" ? sc2::UNIT_TYPEID(1980) : sc2::UNIT_TYPEID::PROTOSS_ASSIMILATORRICH;
	richExtractorId = bot.Config().StarCraft2Version > "4.10.4" ? sc2::UNIT_TYPEID(1981) : sc2::UNIT_TYPEID::ZERG_EXTRACTORRICH;
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
			Util::richRefineryType = UnitType(richAssimilatorId, bot);
			Util::workerType = UnitType(sc2::UNIT_TYPEID::PROTOSS_PROBE, bot);
			Util::supplyType = UnitType(sc2::UNIT_TYPEID::PROTOSS_PYLON, bot);
			break;
		}
		case sc2::Race::Zerg:
		{
			Util::depotType = UnitType(sc2::UNIT_TYPEID::ZERG_HATCHERY, bot);
			Util::refineryType = UnitType(sc2::UNIT_TYPEID::ZERG_EXTRACTOR, bot);
			Util::richRefineryType = UnitType(richExtractorId, bot);
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

	CreateDummyUnits(bot);
}

void Util::InitializeCombatSimulator()
{
	initMappings();
	m_simulator = new CombatPredictor();
	m_simulator->init();
	m_simulator->getCombatEnvironment({}, {});
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
	std::list<CCPosition> path = FindOptimalPath(unit, goal, CCPosition(), addBuffer ? 3.f : 1.f, true, false, false, false, 0, false, true, failureReason, bot);
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

CCPosition Util::PathFinding::FindOptimalPathToTarget(const sc2::Unit * unit, CCPosition goal, CCPosition secondaryGoal, const sc2::Unit* target, float maxRange, bool considerOnlyEffects, float maxInfluence, CCBot & bot)
{
	bool getCloser = false;
	if (target)
	{
		const float targetRange = GetAttackRangeForTarget(target, unit, bot);
		getCloser = targetRange == 0.f || Dist(unit->pos, target->pos) > getThreatRange(unit, target, bot) || target->last_seen_game_loop < bot.GetCurrentFrame();
	}
	std::list<CCPosition> path = FindOptimalPath(unit, goal, secondaryGoal, maxRange, false, considerOnlyEffects, getCloser, false, maxInfluence, false, bot);
	return GetCommandPositionFromPath(path, unit, true, bot);
}

CCPosition Util::PathFinding::FindOptimalPathToSafety(const sc2::Unit * unit, CCPosition goal, bool shouldHeal, CCBot & bot)
{
	std::list<CCPosition> path = FindOptimalPath(unit, goal, bot.GetStartLocation(), 0.f, false, false, shouldHeal, false, 0, true, bot);
	return GetCommandPositionFromPath(path, unit, true, bot);
}

CCPosition Util::PathFinding::FindOptimalPathToSaferRange(const sc2::Unit * unit, const sc2::Unit * target, float range, bool moveFarther, CCBot & bot)
{
	std::list<CCPosition> path = FindOptimalPath(unit, target->pos, bot.GetStartLocation(), range, false, false, true, false, 0, true, bot);
	return GetCommandPositionFromPath(path, unit, moveFarther, bot);
}

/*
 * Returns pathing distance. Returns -1 if no path is found.
 * If ignoreInfluence is true, no enemy influence will be taken into account when finding the path.
 * If it is false and enemy influence is encountered along the way, it will return -1.
 */
float Util::PathFinding::FindOptimalPathDistance(const sc2::Unit * unit, CCPosition goal, bool ignoreInfluence, CCBot & bot)
{
	const auto path = FindOptimalPath(unit, goal, CCPosition(), 2.f, !ignoreInfluence, false, false, ignoreInfluence, 0, false, bot);
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
	auto path = FindOptimalPath(unit, goal, CCPosition(), maxRange, exitOnInfluence, considerOnlyEffects, getCloser, false, 0, false, bot);
	if(path.empty())
	{
		return {};
	}
	return GetCommandPositionFromPath(path, unit, true, bot);
}

CCPosition Util::PathFinding::FindOptimalPathToDodgeEffectAwayFromGoal(const sc2::Unit * unit, CCPosition goal, float range, CCBot & bot)
{
	goal = unit->pos + (unit->pos - goal);
	std::list<CCPosition> path = FindOptimalPath(unit, goal, CCPosition(), range, false, true, false, false, 0, false, bot);
	return GetCommandPositionFromPath(path, unit, true, bot);
}

std::list<CCPosition> Util::PathFinding::FindOptimalPathWithoutLimit(const sc2::Unit * unit, CCPosition goal, CCBot & bot)
{
	FailureReason failureReason;
	return FindOptimalPath(unit, goal, CCPosition(), 5.f, true, false, true, true, 0, false, false, failureReason, bot);
}

std::list<CCPosition> Util::PathFinding::FindOptimalPath(const sc2::Unit * unit, CCPosition goal, CCPosition secondaryGoal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, bool ignoreInfluence, float maxInfluence, bool flee, CCBot & bot)
{
	FailureReason failureReason;
	return FindOptimalPath(unit, goal, secondaryGoal, maxRange, exitOnInfluence, considerOnlyEffects, getCloser, ignoreInfluence, maxInfluence, flee, true, failureReason, bot);
}

std::list<CCPosition> Util::PathFinding::FindOptimalPath(const sc2::Unit * unit, CCPosition goal, CCPosition secondaryGoal, float maxRange, bool exitOnInfluence, bool considerOnlyEffects, bool getCloser, bool ignoreInfluence, float maxInfluence, bool flee, bool limitSearch, FailureReason & failureReason, CCBot & bot)
{
	std::list<CCPosition> path;
	std::set<IMNode*> opened;
	std::set<IMNode*> closed;
	std::map<int, float> bestCosts;

	const auto maxExploredNode = HARASS_PATHFINDING_MAX_EXPLORED_NODE * (!limitSearch ? 20 : exitOnInfluence ? 5 : bot.Config().TournamentMode ? 3 : 1);
	int numberOfTilesExploredAfterPathFound = 0;	//only used when getCloser is true
	IMNode* closestNode = nullptr;					//only used when getCloser is true
	IMNode* exitNode = nullptr;						//only used when getCloser and maxInfluence are true
	const auto startingInfluence = GetTotalInfluenceOnTile(GetTilePosition(unit->pos), unit->is_flying, bot);
	const CCTilePosition startPosition = GetTilePosition(unit->pos);
	const CCTilePosition goalPosition = GetTilePosition(goal);
	const CCTilePosition secondaryGoalPosition = unit->is_flying || IsWorker(unit->unit_type) || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT ? CCTilePosition() : GetTilePosition(secondaryGoal);
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
		if (bot.Config().DrawPathfindingTiles)
		{
			bot.Map().drawTile(currentNode->position, sc2::Colors::White, 0.9f, false);
		}

		bool shouldTriggerExit = false;
		if (flee)
		{
			if (maxRange == 0.f)
			{
				shouldTriggerExit = !HasInfluenceOnTile(currentNode->position, unit->is_flying, bot);
			}
			else
			{
				if (Dist(GetPosition(currentNode->position), goal) > maxRange)
					continue;	// We don't want to keep looking in that direction since it's too far from the goal
				//shouldTriggerExit = !HasInfluenceOnTile(currentNode->position, unit->is_flying, bot);
				const auto influenceOnTile = GetTotalInfluenceOnTile(currentNode->position, unit->is_flying, bot);
				shouldTriggerExit = influenceOnTile < startingInfluence || influenceOnTile == 0;
			}
		}
		else
		{
			if (exitOnInfluence)
			{
				shouldTriggerExit = HasInfluenceOnTile(currentNode->position, unit->is_flying, bot);
			}
			else if (maxInfluence > 0)
			{
				shouldTriggerExit = currentNode->influence > maxInfluence;	// Cumulative influence
			}
			if (!shouldTriggerExit)
			{
				shouldTriggerExit = (ignoreInfluence ||
					((considerOnlyEffects || !HasCombatInfluenceOnTile(currentNode, unit, bot) || (maxInfluence > 0 && currentNode->influence <= maxInfluence)) &&
					!HasEffectInfluenceOnTile(currentNode, unit, bot))) &&
					Dist(GetPosition(currentNode->position) + CCPosition(0.5f, 0.5f), goal) < maxRange;
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
				if (numberOfTilesExploredAfterPathFound == 0)
					exitNode = currentNode;
				shouldTriggerExit = false;
				bool closer = false;
				if (closestNode != nullptr)
				{
					const CCPosition shiftedPos = GetPosition(currentNode->position) + CCPosition(0.5f, 0.5f);
					if (flee && maxRange > 0.f)
						closer = Dist(shiftedPos, goal) < maxRange && GetTotalInfluenceOnTile(currentNode->position, unit->is_flying, bot) < GetTotalInfluenceOnTile(closestNode->position, unit->is_flying, bot);
					else
						closer = Dist(shiftedPos, goal) < Dist(GetPosition(closestNode->position) + CCPosition(0.5f, 0.5f), goal);
				}
				if (closestNode == nullptr || closer)
				{
					closestNode = currentNode;
				}
				++numberOfTilesExploredAfterPathFound;
			}
		}
		if (shouldTriggerExit)
		{
			// If it exits on influence, we need to check if there is actually influence on the current tile. If so, we do not return a valid path
			if(exitOnInfluence && HasInfluenceOnTile(GetPosition(currentNode->position), unit->is_flying, bot))
			{
				failureReason = INFLUENCE;
			}
			else if (maxInfluence > 0 && (exitNode != nullptr ? exitNode->influence : currentNode->influence) > maxInfluence)
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
				const float totalInfluenceOnTile = GetTotalInfluenceOnTile(neighborPosition, unit, bot);
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
				const float nodeCost = (influenceOnTile + creepCost + turnCost + HARASS_PATHFINDING_TILE_BASE_COST) * neighborDistance;
				totalCost += currentNode->cost + nodeCost;

				const float heuristic = CalcEuclidianDistanceHeuristic(neighborPosition, goalPosition, secondaryGoalPosition, bot);
				const float influence = totalInfluenceOnTile + currentNode->influence;
				auto neighbor = new IMNode(neighborPosition, currentNode, totalCost, heuristic, influence);

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

CCPosition Util::PathFinding::GetCommandPositionFromPath(std::list<CCPosition> & path, const sc2::Unit * rangedUnit, bool moveFarther, CCBot & bot)
{
	CCPosition returnPos;
	int i = 0;
	for(const auto position : path)
	{
		++i;
		returnPos = position;
		//we want to return a node close to the current position
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
	if (moveFarther && Util::DistSq(rangedUnit->pos, returnPos) < 3*3)
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
		heuristic += dist * PATHFINDING_SECONDARY_GOAL_HEURISTIC_MULTIPLIER;
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
			const auto tilePosition = CCTilePosition(x, y);
			if (DistSq(position, CCPosition(tilePosition.x + 0.5f, tilePosition.y + 0.5f)) > radius * radius)
				continue;	// If center of tile is farther than radius, ignore
			totalInfluence += GetTotalInfluenceOnTile(tilePosition, isFlying, bot);
		}
	}
	return totalInfluence;
}

float Util::PathFinding::GetMaxInfluenceOnTiles(CCPosition position, bool isFlying, float radius, CCBot & bot)
{
	float maxInfluence = 0.f;
	const int minX = round(position.x - radius);
	const int minY = round(position.y - radius);
	const int maxX = round(position.x + radius);
	const int maxY = round(position.y + radius);
	for (int x = minX; x < maxX; ++x)
	{
		for (int y = minY; y < maxY; ++y)
		{
			const auto tilePosition = CCTilePosition(x, y);
			if (DistSq(position, CCPosition(tilePosition.x + 0.5f, tilePosition.y + 0.5f)) > radius * radius)
				continue;	// If center of tile is farther than radius, ignore
			const auto influence = GetTotalInfluenceOnTile(tilePosition, isFlying, bot);
			if (influence > maxInfluence)
				maxInfluence = influence;
		}
	}
	return maxInfluence;
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

sc2::UNIT_TYPEID Util::GetRichAssimilatorId()
{
	return richAssimilatorId;
}

sc2::UNIT_TYPEID Util::GetRichExtractorId()
{
	return richExtractorId;
}

UnitType Util::GetResourceDepotType()
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
		if (cluster.m_units.empty())
		{
			cluster.m_center = CCPosition();
			continue;
		}
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
	for (auto & unit : units)
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
	const bool isMedivac = unit.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC;
	float unitRange = 0.f;
	if (target.isValid())
	{
		if (isMedivac)
			unitRange = bot.Commander().Combat().getAbilityCastingRanges().at(sc2::ABILITY_ID::EFFECT_HEAL) + unit.getUnitPtr()->radius + target.getUnitPtr()->radius;
		else
			unitRange = GetAttackRangeForTarget(unit.getUnitPtr(), target.getUnitPtr(), bot);
	}
	else
		unitRange = GetMaxAttackRange(unit.getUnitPtr(), bot);
	///////// HEALTH
	float unitPower = pow(unit.getHitPoints() + unit.getShields(), 0.5f);
	///////// DPS
	if (target.isValid())
		unitPower *= isMedivac ? 12.6f : std::max(1.f, GetDpsForTarget(unit.getUnitPtr(), target.getUnitPtr(), bot));
	else
		unitPower *= std::max(1.f, GetDps(unit.getUnitPtr(), bot));
	///////// DISTANCE
	if (target.isValid())
	{
		float distance = Util::Dist(unit.getPosition(), target.getPosition());
		if (unitRange < distance)
		{
			distance -= unitRange;
			const float distancePenalty = pow(0.9f, distance);
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
	return (unitPtr->health_max + unitPtr->shield_max) / 8;	// will pull 4 workers for a Pylon
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

/**
 * Computes the two vectors that are perpendicular to the reference vector.
 */
void Util::GetOrthogonalVectors(const sc2::Point2D& referenceVector, sc2::Point2D& clockwiseVector, sc2::Point2D& counterClockwiseVector)
{
	clockwiseVector.x = referenceVector.y;
	clockwiseVector.y = -referenceVector.x;
	counterClockwiseVector.x = -referenceVector.y;
	counterClockwiseVector.y = referenceVector.x;
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

void Util::CreateDummyUnits(CCBot & bot)
{
	CreateDummyVikingAssault(bot);
	CreateDummyVikingFighter(bot);
	CreateDummyStimedMarine(bot);
	CreateDummyStimedMarauder(bot);
}

void Util::CreateDummyVikingAssault(CCBot & bot)
{
	m_dummyVikingAssault = new sc2::Unit;
	m_dummyVikingAssault->unit_type = sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT;
	m_dummyVikingAssault->is_flying = false;
	m_dummyVikingAssault->health_max = 135;
	m_dummyVikingAssault->radius = 0.75;
	SetBaseUnitValues(m_dummyVikingAssault, bot);
}

void Util::CreateDummyVikingFighter(CCBot & bot)
{
	m_dummyVikingFighter = new sc2::Unit;
	m_dummyVikingFighter->unit_type = sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER;
	m_dummyVikingFighter->is_flying = true;
	m_dummyVikingFighter->health_max = 135;
	m_dummyVikingFighter->radius = 0.75;
	SetBaseUnitValues(m_dummyVikingFighter, bot);
}

void Util::CreateDummyStimedMarine(CCBot & bot)
{
	m_dummyStimedMarine = new sc2::Unit;
	m_dummyStimedMarine->unit_type = sc2::UNIT_TYPEID::TERRAN_MARINE;
	m_dummyStimedMarine->is_flying = false;
	m_dummyStimedMarine->health_max = 45;
	m_dummyStimedMarine->radius = 0.375;
	SetBaseUnitValues(m_dummyStimedMarine, bot);
	m_dummyStimedMarine->buffs.push_back(sc2::BUFF_ID::STIMPACK);
}

void Util::CreateDummyStimedMarauder(CCBot & bot)
{
	m_dummyStimedMarauder = new sc2::Unit;
	m_dummyStimedMarauder->unit_type = sc2::UNIT_TYPEID::TERRAN_MARAUDER;
	m_dummyStimedMarauder->is_flying = false;
	m_dummyStimedMarauder->health_max = 125;
	m_dummyStimedMarauder->radius = 0.5625;
	SetBaseUnitValues(m_dummyStimedMarauder, bot);
	m_dummyStimedMarauder->buffs.push_back(sc2::BUFF_ID::STIMPACKMARAUDER);
}

void Util::SetBaseUnitValues(sc2::Unit * unit, CCBot & bot)
{
	unit->energy_max = 0;
	unit->energy = 0;
	unit->alliance = sc2::Unit::Self;
	unit->owner = GetSelfPlayerId(bot);
	unit->is_alive = true;
	unit->shield_max = 0;
	unit->shield = 0;
	unit->display_type = sc2::Unit::Visible;
	unit->build_progress = 1;
	unit->cloak = sc2::Unit::NotCloaked;
	unit->detect_range = 0;
	unit->radar_range = 0;
	unit->is_blip = false;
	unit->mineral_contents = 0;
	unit->is_burrowed = false;
	unit->weapon_cooldown = 0;
	unit->add_on_tag = 0;
	unit->passengers = {};
	unit->cargo_space_taken = 0;
	unit->cargo_space_max = 0;
	unit->assigned_harvesters = 0;
	unit->ideal_harvesters = 0;
	unit->engaged_target_tag = 0;
	unit->buffs = {};
	unit->is_powered = false;
	unit->last_seen_game_loop = 0;
}

sc2::Unit Util::CreateDummyFromUnit(sc2::Unit * dummyPointer, const sc2::Unit * unit)
{
	sc2::Unit dummy = sc2::Unit(*dummyPointer);
	dummy.pos = unit->pos;
	dummy.facing = unit->facing;
	dummy.health = unit->health;
	dummy.health_max = unit->health_max;	// Useful for Marines with combat shield upgrade
	dummy.tag = unit->tag;
	dummy.last_seen_game_loop = unit->last_seen_game_loop;
	return dummy;
}

sc2::Unit Util::CreateDummyFromUnit(const sc2::Unit * unit)
{
	if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
		return CreateDummyVikingAssaultFromUnit(unit);
	if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT)
		return CreateDummyVikingFighterFromUnit(unit);
	if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE)
		return CreateDummyStimedMarineFromUnit(unit);
	if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARAUDER)
		return CreateDummyStimedMarauderFromUnit(unit);
	return sc2::Unit();
}

sc2::Unit Util::CreateDummyVikingAssaultFromUnit(const sc2::Unit * unit)
{
	return CreateDummyFromUnit(m_dummyVikingAssault, unit);
}

sc2::Unit Util::CreateDummyVikingFighterFromUnit(const sc2::Unit * unit)
{
	return CreateDummyFromUnit(m_dummyVikingFighter, unit);
}

sc2::Unit Util::CreateDummyStimedMarineFromUnit(const sc2::Unit * unit)
{
	sc2::Unit dummyStimedMarine = CreateDummyFromUnit(m_dummyStimedMarine, unit);
	dummyStimedMarine.health -= 10;
	return dummyStimedMarine;
}

sc2::Unit Util::CreateDummyStimedMarauderFromUnit(const sc2::Unit * unit)
{
	sc2::Unit dummyStimedMarauder = CreateDummyFromUnit(m_dummyStimedMarauder, unit);
	dummyStimedMarauder.health -= 20;
	return dummyStimedMarauder;
}

bool Util::CanUnitAttackAir(const sc2::Unit * unit, CCBot & bot)
{
	if (GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Air) > 0.f)
		return true;
	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);
	for (auto & weapon : unitTypeData.weapons)
	{
		if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Air)
			return true;
	}
	return false;
}

bool Util::CanUnitAttackGround(const sc2::Unit * unit, CCBot & bot)
{
	if (GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Ground) > 0.f)
		return true;
	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);
	for (auto & weapon : unitTypeData.weapons)
	{
		if (weapon.type == sc2::Weapon::TargetType::Any || weapon.type == sc2::Weapon::TargetType::Ground)
			return true;
	}
	return false;
}

float Util::GetSpecialCaseRange(const sc2::Unit* unit, sc2::Weapon::TargetType where, bool ignoreSpells)
{
	return GetSpecialCaseRange(unit->unit_type, where, ignoreSpells);
}

float Util::GetSpecialCaseRange(const sc2::UNIT_TYPEID unitType, sc2::Weapon::TargetType where, bool ignoreSpells)
{
	float range = -1.f;

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
	/*else if (unitType == sc2::UNIT_TYPEID::PROTOSS_PHOENIX)
	{
		if (where == sc2::Weapon::TargetType::Ground)
			range = 4.f;
	}*/
	else if (unitType == sc2::UNIT_TYPEID::ZERG_HYDRALISK)
	{
		range = 6.f;	// Always consider they have their range upgrade because we don't want to detect it manually
	}
	else if (unitType == sc2::UNIT_TYPEID::TERRAN_CYCLONE)
	{
		if (!ignoreSpells)
			range = 7.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::PROTOSS_VOIDRAY)
	{
		range = 6.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::PROTOSS_SENTRY)
	{
		range = 5.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::TERRAN_LIBERATORAG)
	{
		range = 0.f;
	}
	else if (unitType == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY)
	{
		range = 6.f;
	}

	return range;
}

float Util::GetGroundAttackRange(const sc2::Unit * unit, CCBot & bot)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	if ((unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON || unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY) && !unit->is_powered)
		return 0.f;

	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);

	float maxRange = GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Ground);

	if (maxRange < 0.f)
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
		if (unit->alliance == sc2::Unit::Enemy)
			maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);
	}
	
	return std::max(0.f, maxRange);
}

float Util::GetAirAttackRange(const sc2::Unit * unit, CCBot & bot)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	if ((unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON || unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY) && !unit->is_powered)
		return 0.f;

	sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);

	float maxRange = GetSpecialCaseRange(unit->unit_type, sc2::Weapon::TargetType::Air);
	if (maxRange < 0.f)
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
		if (unit->alliance == sc2::Unit::Enemy)
			maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);
	}

	return std::max(0.f, maxRange);
}

float Util::GetAttackRangeForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot, bool ignoreSpells)
{
	if (Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f)
		return 0.f;

	if ((unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON || unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY) && !unit->is_powered)
		return 0.f;

	if (!target)
		return 0.f;

	const sc2::UnitTypeData & unitTypeData = bot.Observation()->GetUnitTypeData()[unit->unit_type];
	const sc2::Weapon::TargetType expectedWeaponType = target->is_flying ? sc2::Weapon::TargetType::Air : sc2::Weapon::TargetType::Ground;
	
	float maxRange = GetSpecialCaseRange(unit->unit_type, expectedWeaponType, ignoreSpells);
	if (maxRange < 0.f)
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
		if (unit->alliance == sc2::Unit::Enemy)
			maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);
		if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY && (unit->alliance != target->alliance || target->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY))
			maxRange = 0.f;
	}

	return std::max(0.f, maxRange); 
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

	if ((unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON || unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY) && !unit->is_powered)
		return 0.f;

	const sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unit->unit_type]);
	
	float maxRange = GetSpecialCaseRange(unitTypeData.unit_type_id);
	if (maxRange < 0.f)
	{
		for (auto & weapon : unitTypeData.weapons)
		{
			// can attack target with a weapon
			if (weapon.range > maxRange)
				maxRange = weapon.range;
		}
	}

	if (maxRange > 0.f)
	{
		maxRange += unit->radius;
		if (unit->alliance == sc2::Unit::Enemy)
			maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);
	}

	return std::max(0.f, maxRange);
}

/*
 * Do not call this method, it doesn't add the unit radius and the attack range bonus is valid only for enemy units.
 */
float Util::GetMaxAttackRange(const sc2::UnitTypeID unitType, CCBot & bot)
{
    const sc2::UnitTypeData unitTypeData(bot.Observation()->GetUnitTypeData()[unitType]);
    return GetMaxAttackRange(unitTypeData, bot);
}

/*
 * Do not call this method, it doesn't add the unit radius and the attack range bonus is valid only for enemy units.
 */
float Util::GetMaxAttackRange(sc2::UnitTypeData unitTypeData, CCBot & bot)
{
	float maxRange = GetSpecialCaseRange(unitTypeData.unit_type_id);
	if (maxRange < 0.f)
	{
		for (auto & weapon : unitTypeData.weapons)
		{
			// can attack target with a weapon
			if (weapon.range > maxRange)
				maxRange = weapon.range;
		}
	}

	if (maxRange > 0.f)
		maxRange += GetAttackRangeBonus(unitTypeData.unit_type_id, bot);

    return std::max(0.f, maxRange);
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
    const sc2::UnitTypeData & unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
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
	if (dps < 0.f)
	{
		const sc2::UnitTypeData & unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
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

	dps *= GetAttackSpeedMultiplier(unit);
	
	return std::max(0.f, dps);
}

float Util::GetDpsForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot)
{
	if ((Unit(unit, bot).getType().isBuilding() && unit->build_progress < 1.f) || (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON && !unit->is_powered))
		return 0.f;
    const sc2::Weapon::TargetType expectedWeaponType = target->is_flying ? sc2::Weapon::TargetType::Air : sc2::Weapon::TargetType::Ground;
    float dps = GetSpecialCaseDps(unit, bot, expectedWeaponType);
    if (dps < 0.f)
    {
		const sc2::UnitTypeData & unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
		const sc2::UnitTypeData & targetTypeData = GetUnitTypeDataFromUnitTypeId(target->unit_type, bot);
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
                weaponDps *= weapon.attacks / weapon.speed * 1.4f;	// * 1.4f because the weapon speed is given in "normal" time instead of "faster"
                if (weaponDps > dps)
                    dps = weaponDps;
            }
        }
    }

	dps *= GetAttackSpeedMultiplier(unit);

	if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY && (unit->alliance != target->alliance || target->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY))
		dps = 0.f;

    return std::max(0.f, dps);
}

float Util::GetAttackSpeedMultiplier(const sc2::Unit * unit)
{
	if (unitHasBuff(unit, sc2::BUFF_ID::STIMPACK) || unitHasBuff(unit, sc2::BUFF_ID::STIMPACKMARAUDER))
		return 1.5f;
	return 1.f;
}

float Util::GetSpecialCaseDps(const sc2::Unit * unit, CCBot & bot, sc2::Weapon::TargetType where)
{
    float dps = -1.f;

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
		dps = 0.f;
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
	/*else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOENIX)
	{
		if (where == sc2::Weapon::TargetType::Ground && unit->energy >= 50)
			dps = 12.7;
	}*/
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_VOIDRAY)
	{
		dps = 16.8f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_SENTRY)
	{
		dps = 8.4f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_LIBERATORAG)
	{
		dps = 0.f;
	}
	/*else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY)	// Commented because we overestimate its unit power and we don't want it to generate influence
	{
		dps = 36.f;
	}*/

    return dps;
}

float Util::GetDamageForTarget(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot)
{
	const sc2::Weapon::TargetType expectedWeaponType = target->is_flying ? sc2::Weapon::TargetType::Air : sc2::Weapon::TargetType::Ground;
	float damage = GetSpecialCaseDamage(unit, bot, expectedWeaponType);
	if (damage == 0.f)
	{
		const sc2::UnitTypeData & unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
		const sc2::UnitTypeData & targetTypeData = GetUnitTypeDataFromUnitTypeId(target->unit_type, bot);
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
	/*else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOENIX)
	{
		if (where == sc2::Weapon::TargetType::Ground && unit->energy >= 50)
			damage = 10.f;
	}*/
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_VOIDRAY)
	{
		damage = 6.f;
	}
	else if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_SENTRY)
	{
		damage = 6.f;
	}

	return damage;
}

// get threats to our harass unit
void Util::getThreats(const sc2::Unit * unit, const sc2::Units & targets, sc2::Units & outThreats, CCBot & bot)
{
	BOT_ASSERT(unit, "null ranged unit in getThreats");

	const auto & enemyUnitsBeingRepaired = bot.GetEnemyUnitsBeingRepaired();

	// for each possible threat
	for (auto targetUnit : targets)
	{
		BOT_ASSERT(targetUnit, "null target unit in getThreats");//can happen if a unit is not defined in an enum (sc2_typeenums.h)
		if (targetUnit->unit_type == sc2::UNIT_TYPEID::ZERG_NYDUSCANAL)
		{
			outThreats.push_back(targetUnit);
			continue;
		}
		if (Util::GetDpsForTarget(targetUnit, unit, bot) == 0.f)
			continue;
		//We consider a unit as a threat if the sum of its range and speed is bigger than the distance to our unit
		//But this is not working so well for melee units, we keep every units in a radius of min threat range
		const float threatRange = getThreatRange(unit, targetUnit, bot);
		if (Util::DistSq(unit->pos, targetUnit->pos) < threatRange * threatRange)
		{
			outThreats.push_back(targetUnit);

			// We check if that threat is being repaired
			if (!unit->is_flying)
			{
				const auto & it = enemyUnitsBeingRepaired.find(targetUnit);
				if (it != enemyUnitsBeingRepaired.end())
				{
					// If so, we consider all the SCVs repairing it as threats
					for (const auto enemyRepairingSCV : it->second)
					{
						outThreats.push_back(enemyRepairingSCV);
					}
				}
			}
		}
	}
}

sc2::Units Util::getThreats(const sc2::Unit * unit, const sc2::Units & targets, CCBot & bot)
{
	sc2::Units threats;
	getThreats(unit, targets, threats, bot);
	return threats;
}

// get threats to our harass unit
sc2::Units Util::getThreats(const sc2::Unit * unit, const std::vector<Unit> & targets, CCBot & bot)
{
	BOT_ASSERT(unit, "null ranged unit in getThreats");

	sc2::Units targetsPtrs;
	for (auto& targetUnit : targets)
		targetsPtrs.push_back(targetUnit.getUnitPtr());

	sc2::Units threats;
	getThreats(unit, targetsPtrs, threats, bot);
	return threats;
}

//calculate radius max(min range, range + speed + height bonus + small buffer)
float Util::getThreatRange(const sc2::Unit * unit, const sc2::Unit * threat, CCBot & m_bot)
{
	const float HARASS_THREAT_MIN_HEIGHT_DIFF = 2.f;
	const float HARASS_THREAT_RANGE_BUFFER = 1.f;
	const float HARASS_THREAT_RANGE_HEIGHT_BONUS = 4.f;
	const float HARASS_THREAT_RANGE_MIN_SPEED_BONUS = 2.f;

	const float heightBonus = unit->is_flying ? 0.f : Util::TerrainHeight(threat->pos) > Util::TerrainHeight(unit->pos) + HARASS_THREAT_MIN_HEIGHT_DIFF ? HARASS_THREAT_RANGE_HEIGHT_BONUS : 0.f;
	const float tempestAirBonus = threat->unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPEST && unit->is_flying ? 2.f : 0.f;
	const float speed = std::max(HARASS_THREAT_RANGE_MIN_SPEED_BONUS, Util::getSpeedOfUnit(threat, m_bot));
	const float threatRange = Util::GetAttackRangeForTarget(threat, unit, m_bot) + speed + heightBonus + tempestAirBonus + HARASS_THREAT_RANGE_BUFFER;

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
	float speedMultiplier = 1.f;
	if(!unit->is_burrowed && !unit->is_flying && Unit(unit, bot).getType().getRace() == CCRace::Zerg)
	{
		/* From https://liquipedia.net/starcraft2/Creep
		 * All Zerg ground units move faster when traveling on Creep, with the exception of burrowed units, Drones, Broodlings, and Changelings not disguised as Zerglings. 
		 * The increase is 30% for most units. Queens move 166.67% faster on creep, Hydralisks move 50% faster, and Spine Crawlers and Spore Crawlers move 150% faster. */
		if(bot.Observation()->HasCreep(unit->pos))
		{
			if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_QUEEN)
				speedMultiplier *= 2.6667f;
			else if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_HYDRALISK)
				speedMultiplier *= 1.5f;
			else if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_SPINECRAWLERUPROOTED || unit->unit_type == sc2::UNIT_TYPEID::ZERG_SPORECRAWLERUPROOTED)
				speedMultiplier *= 2.5f;
			else if (unit->unit_type != sc2::UNIT_TYPEID::ZERG_DRONE && unit->unit_type != sc2::UNIT_TYPEID::ZERG_BROODLING && unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELING
				&& unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELINGMARINE && unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELINGMARINESHIELD && unit->unit_type != sc2::UNIT_TYPEID::ZERG_CHANGELINGZEALOT)
				speedMultiplier *= 1.3f;
		}
		if (bot.Strategy().enemyHasMetabolicBoost() && unit->unit_type == sc2::UNIT_TYPEID::ZERG_ZERGLING)
			speedMultiplier *= 1.6f;
	}
	if (unitHasBuff(unit, sc2::BUFF_ID::SLOW))
		speedMultiplier *= 0.5f;
	if (unitHasBuff(unit, sc2::BUFF_ID::STIMPACK) || unitHasBuff(unit, sc2::BUFF_ID::STIMPACKMARAUDER))
		speedMultiplier *= 1.5f;
	if (unitHasBuff(unit, sc2::BUFF_ID::MEDIVACSPEEDBOOST))
		return 4.243f;	// 5.94 on liquipedia, but movement speed in the api is 0.7143 times inferior
	// Check for our speed upgrades
	if (unit->alliance == sc2::Unit::Self)
	{
		if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BANSHEE && bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::BANSHEESPEED))
			speedMultiplier *= 1.364f;
		if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC && bot.Strategy().isUpgradeCompleted(sc2::UPGRADE_ID::MEDIVACINCREASESPEEDBOOST))
			speedMultiplier *= 1.18f;
	}
	return GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot).movement_speed * speedMultiplier;
}

/*
 * Returns the number of tiles a unit can move in a second.
 */
float Util::getRealMovementSpeedOfUnit(const sc2::Unit * unit, CCBot & bot)
{
	const float speed = Util::getSpeedOfUnit(unit, bot);
	return speed * 1.4f;
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

bool Util::isUnitAffectedByParasiticBomb(const sc2::Unit * unit)
{
	return unitHasBuff(unit, sc2::BUFF_ID::PARASITICBOMB);
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

void Util::ClearSeenEnemies()
{
	m_seenEnemies.clear();
}

bool Util::AllyUnitSeesEnemyUnit(const sc2::Unit * exceptUnit, const sc2::Unit * enemyUnit, float visionBuffer, CCBot & bot)
{
	auto & allyUnitsPair = m_seenEnemies[enemyUnit];
	auto & alliesWithVisionOfEnemy = allyUnitsPair.first;
	auto & alliesWithoutVisionOfEnemy = allyUnitsPair.second;
	if (alliesWithVisionOfEnemy.size() > 1 || (alliesWithVisionOfEnemy.size() == 1 && alliesWithVisionOfEnemy.find(exceptUnit) == alliesWithVisionOfEnemy.end()))
		return true;	// another ally unit can see the enemy unit
	if (alliesWithoutVisionOfEnemy.size() == bot.GetAllyUnits().size())
		return false;	// we already know no ally unit can see the enemy unit

	// check if we have an ally unit that can see the enemy unit
	for (const auto & allyUnit : bot.GetAllyUnits())
	{
		const auto allyUnitPtr = allyUnit.second.getUnitPtr();
		if (alliesWithoutVisionOfEnemy.find(allyUnitPtr) != alliesWithoutVisionOfEnemy.end())
			continue;	// we already know this ally unit can't see the enemy unit
		if (CanUnitSeeEnemyUnit(allyUnitPtr, enemyUnit, visionBuffer, bot))
		{
			alliesWithVisionOfEnemy.insert(allyUnitPtr);
			if (allyUnitPtr != exceptUnit)
				return true;
		}
		else
		{
			alliesWithoutVisionOfEnemy.insert(allyUnitPtr);
		}
	}
	return false;
}

bool Util::CanUnitSeeEnemyUnit(const sc2::Unit * unit, const sc2::Unit * enemyUnit, float buffer, CCBot & bot)
{
	const auto distSq = DistSq(unit->pos, enemyUnit->pos);
	if (distSq > 20 * 20)
		return false;	// Unit is just too far
	const auto & unitTypeData = GetUnitTypeDataFromUnitTypeId(unit->unit_type, bot);
	const auto sight = unitTypeData.sight_range + unit->radius + enemyUnit->radius - buffer;
	if (distSq > sight * sight)
		return false;	// Unit doesn't have enough sight range
	if (!unit->is_flying && bot.Map().terrainHeight(unit->pos) < bot.Map().terrainHeight(enemyUnit->pos))
		return false;	// Unit can't see above cliff
	return true;
}

bool Util::IsEnemyHiddenOnHighGround(const sc2::Unit * unit, const sc2::Unit * enemyUnit, CCBot & bot)
{
	if (!unit || !enemyUnit)
		return false;
	
	if (unit->is_flying)
		return false;

	if (bot.Map().terrainHeight(enemyUnit->pos) <= bot.Map().terrainHeight(unit->pos))
		return false;

	if (enemyUnit->is_flying && enemyUnit->last_seen_game_loop == bot.GetCurrentFrame())
		return false;
	
	const CCPosition closeToEnemy = enemyUnit->pos + Normalized(unit->pos - enemyUnit->pos) * enemyUnit->radius * 0.95;
	if (bot.Map().isVisible(closeToEnemy))
		return false;

	return true;
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
		auto & detectors = bot.GetEnemyUnits(detectorType);
		for(const auto & detector : detectors)
		{
			if (detector.getType().isBuilding() && detector.getBuildProgress() < 0.95f)
				continue;
			if (detectorType == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON && !detector.isPowered())
				continue;
			const float distance = DistSq(detector, position);
			float detectionRange = detector.getUnitPtr()->detect_range;
			if (detectionRange == 0)
				detectionRange = detector.getAPIUnitType() == sc2::UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE ? 13.75f : 11;
			detectionRange += detector.getUnitPtr()->radius + 1;	// We want to add a small buffer
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
		if(DistSq(position, area.first) < areaUnderDetectionSize * areaUnderDetectionSize)
		{
			return true;
		}
	}
	return false;
}

bool Util::IsUnitCloakedAndSafe(const sc2::Unit * unit, CCBot & bot)
{
	return unit->cloak == sc2::Unit::CloakedAllied && !Util::IsPositionUnderDetection(unit->pos, bot) && unit->energy >= 5 && bot.Analyzer().getUnitState(unit).GetRecentDamageTaken() == 0;
}

bool Util::IsAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::Unit * unit, const std::vector<sc2::AvailableAbilities> & availableAbilitiesForUnits)
{
	for (const auto & availableAbilitiesForUnit : availableAbilitiesForUnits)
	{
		if (availableAbilitiesForUnit.unit_tag == unit->tag)
			return IsAbilityAvailable(abilityId, availableAbilitiesForUnit);
	}
	return false;
}

bool Util::IsAbilityAvailable(sc2::ABILITY_ID abilityId, const sc2::AvailableAbilities & availableAbilities)
{
	for (const auto & availableAbility : availableAbilities.abilities)
	{
		if (availableAbility.ability_id == abilityId)
			return true;
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

bool Util::IsWorker(sc2::UNIT_TYPEID type)
{
	switch (type)
	{
	case sc2::UNIT_TYPEID::TERRAN_SCV: return true;
	case sc2::UNIT_TYPEID::TERRAN_MULE: return true;
	case sc2::UNIT_TYPEID::PROTOSS_PROBE: return true;
	case sc2::UNIT_TYPEID::ZERG_DRONE: return true;
	case sc2::UNIT_TYPEID::ZERG_DRONEBURROWED: return true;
	default: return false;
	}
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
	if (size == 0)
		return sc2::Point2D();
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

const sc2::UnitTypeData & Util::GetUnitTypeDataFromUnitTypeId(const sc2::UnitTypeID unitTypeId, CCBot & bot)
{
	const auto & unitTypes = bot.Observation()->GetUnitTypeData();
    return unitTypes[unitTypeId];
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
	assert(image_width > 0);
	assert(image_height > 0);

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

	assert(pixel_size > 0);
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
	strftime(buf, sizeof(buf), "./data/%Y-%m-%d--%H-%M-%S", localtime(&now));
	std::stringstream ss;
	ss << buf << "_" << bot.GetOpponentId() << ".log";
	file.open(ss.str());

	SetMapName(bot.Observation()->GetGameInfo().map_name);
	std::stringstream races;
	races << Util::GetStringFromRace(bot.GetPlayerRace(Players::Self)) << " VS " << Util::GetStringFromRace(bot.GetPlayerRace(Players::Enemy)) << " on " << Util::GetMapName();
	Util::LogNoFrame(races.str(), bot);
}

void Util::DebugLog(const std::string & function, CCBot & bot)
{
	if (allowDebug)
	{
		file << bot.GetGameLoop() << ": " << function << std::endl;
		std::cout << bot.GetGameLoop() << ": " << function << std::endl;
	}
}

void Util::DebugLog(const std::string & function, const std::string & message, CCBot & bot)
{
	if (allowDebug)
	{
		file << bot.GetGameLoop() << ": " << function << " | " << message << std::endl;
		std::cout << bot.GetGameLoop() << ": " << function << " | " << message << std::endl;
	}
}

void Util::LogNoFrame(const std::string & function, CCBot & bot)
{
	file << function << std::endl;
	std::cout << function << std::endl;
}

void Util::Log(const std::string & function, CCBot & bot)
{
	file << bot.GetGameLoop() << ": " << function << std::endl;
	std::cout << bot.GetGameLoop() << ": " << function << std::endl;
}

void Util::Log(const std::string & function, const std::string & message, const CCBot & bot)
{
	file << bot.GetGameLoop() << ": " << function << " | " << message << std::endl;
	std::cout << bot.GetGameLoop() << ": " << function << " | " << message << std::endl;
}

void Util::ClearChat(CCBot & bot)
{
	for (int i = 0; i < 10; i++)
	{
		bot.Actions()->SendChat(" ");
	}
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
			timeControlRatio = 15;
			break;
		case 15:
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
		case 15:
			timeControlRatio = 5;
			break;
		case 50:
			timeControlRatio = 15;
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

float Util::SimulateCombat(const sc2::Units & units, const sc2::Units & enemyUnits, CCBot & bot)
{
	return SimulateCombat(units, units, enemyUnits, bot);
}

/**
 * Uses the combat simulator of libvoxel that simulates units attacking each other, but without considering the positions of units.
 * Returns a value between 1 and 0, representing the army supply remaining after the fight.
 */
float Util::SimulateCombat(const sc2::Units & units, const sc2::Units & simulatedUnits, const sc2::Units & enemyUnits, CCBot & bot)
{
	if (units.empty() || simulatedUnits.empty())
		return 0.f;
	if (enemyUnits.empty())
		return 1.f;
	bot.StartProfiling("s.0 PrepareForCombatSimulation");
	const int playerId = GetSelfPlayerId(bot);
	CombatState state;
	for(int i=0; i<2; ++i)
	{
		const sc2::Units & playerUnits = i == 0 ? simulatedUnits : enemyUnits;
		for (const auto unit : playerUnits)
		{
			// Since bunkers deal no damage in the simulation, we swap them for 4 Marines with extra health
			if(unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER)
			{
				const int owner = i == 0 ? playerId : 3 - playerId;
				const int marineCount = owner == playerId ? unit->passengers.size() : 4;
				for(int j=0; j < marineCount; ++j)
					state.units.push_back(CombatUnit(owner, sc2::UNIT_TYPEID::TERRAN_MARINE, 200, false));
			}
			else
				state.units.push_back(CombatUnit(*unit));
		}
	}

	bool enemyHasOnlyBuildings = true;
	for (const auto & enemyUnit : enemyUnits)
	{
		if (!UnitType(enemyUnit->unit_type, bot).isBuilding())
		{
			enemyHasOnlyBuildings = false;
			break;
		}
	}
	// If the opponent has only buildings, we want to be the attacker, otherwise we are the defenders (defenders do the first hit)
	const int defenderPlayer = enemyHasOnlyBuildings ? 3 - playerId : playerId;
	
	// Calculate our army score to compare after the fight
	float armySupplyScore = 0.f;
	for (const auto unit : units)
	{
		const sc2::UnitTypeData & unitTypeData = bot.Observation()->GetUnitTypeData()[unit->unit_type];
		armySupplyScore += unitTypeData.food_required * (0.25f + 0.75f * unit->health / std::max(1.f, unit->health_max));
	}

	CombatUpgrades player1upgrades = {};

	CombatUpgrades player2upgrades = {};

	// TODO if we want to consider upgrades, we should detect enemy upgrades and get the combat environment in another thread
	// TODO because it can take over 1s to generate a new one (happens when upgrades change)
	/*for (const auto upgrade : bot.Strategy().getCompletedUpgrades())
		(playerId == 1 ? player1upgrades : player2upgrades).add(upgrade);*/
	bot.StopProfiling("s.0 PrepareForCombatSimulation");
	
	bot.StartProfiling("s.1 getCombatEnvironment");
	state.environment = &m_simulator->getCombatEnvironment(player1upgrades, player2upgrades);
	bot.StopProfiling("s.1 getCombatEnvironment");

	bot.StartProfiling("s.2 predict_engage");
	CombatSettings settings;
	
	// Simulate for at most 100 *game* seconds
	// Just to show that it can be configured, in this case 100 game seconds is more than enough for the battle to finish.
	settings.maxTime = 100;
	const CombatResult outcome = m_simulator->predict_engage(state, settings, nullptr, defenderPlayer, &bot);
	bot.StopProfiling("s.2 predict_engage");
	bot.StartProfiling("s.3 owner_with_best_outcome");
	const int winner = outcome.state.owner_with_best_outcome();
	bot.StopProfiling("s.3 owner_with_best_outcome");
	if (winner != playerId)
		return 0.f;

	bot.StartProfiling("s.4 ComputeArmyRating");
	float resultArmySupplyScore = 0.f;
	for (const auto & unit : outcome.state.units)
	{
		if (unit.owner == playerId && unit.health > 0)
		{
			const sc2::UnitTypeData & unitTypeData = bot.Observation()->GetUnitTypeData()[sc2::UnitTypeID(unit.type)];
			resultArmySupplyScore += unitTypeData.food_required * (0.25f + 0.75f * unit.health / unit.health_max);
		}
	}
	const float armyRating = resultArmySupplyScore / std::max(1.f, armySupplyScore);
	bot.StopProfiling("s.4 ComputeArmyRating");
	return armyRating;
}

int Util::GetSelfPlayerId(const CCBot & bot)
{
	const auto & playerInfo = bot.Observation()->GetGameInfo().player_info;
	if (playerInfo.empty())
		return -1;
	return playerInfo[0].player_id == bot.Observation()->GetPlayerID() ? 1 : 2;
}