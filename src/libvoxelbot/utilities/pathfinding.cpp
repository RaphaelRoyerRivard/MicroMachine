#include "pathfinding.h"
#include <cmath>
#include <iostream>
#include <queue>
#include <vector>

using namespace std;
using namespace sc2;

struct PathfindingEntry {
    double cost;
    double h;
    Point2DI pos;

    PathfindingEntry(double _cost, Point2DI _pos)
        : cost(_cost), h(0), pos(_pos) {
    }

    PathfindingEntry(double _cost, double _h, Point2DI _pos)
        : cost(_cost), h(_h), pos(_pos) {
    }

    bool operator<(const PathfindingEntry& other) const {
        return cost + h > other.cost + other.h;
    }
};

const int MAX_MAP_SIZE = 256;

int dx[8] = { 1, 1, 1, 0, 0, -1, -1, -1 };
int dy[8] = { 1, 0, -1, 1, -1, 1, 0, -1 };
double dc[8] = { 1.41, 1, 1.41, 1, 1, 1.41, 1, 1.41 };

/** Returns a map of distances from the starting points.
 * A point is considered a starting point if the element in the startingPoints map is non-zero.
 * The costs per cell are given by the costs map.
 * Diagonal movement costs sqrt(2) times more than axis aligned movement.
 */
InfluenceMap getDistances(const InfluenceMap& startingPoints, const InfluenceMap& costs) {
    assert(startingPoints.w == costs.w);
    assert(startingPoints.h == costs.h);

    InfluenceMap distances(startingPoints.w, startingPoints.h);
    for (auto& w : distances.weights)
        w = numeric_limits<double>::infinity();

    static priority_queue<PathfindingEntry> pq;

    int h = startingPoints.h;
    int w = startingPoints.w;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (startingPoints(x, y)) {
                pq.push(PathfindingEntry(0.0, Point2DI(x, y)));
                distances(x, y) = 0;
            }
        }
    }

    while (!pq.empty()) {
        auto currentEntry = pq.top();
        auto currentPos = currentEntry.pos;
        pq.pop();
        if (currentEntry.cost > distances(currentPos)) {
            continue;
        }

        for (int i = 0; i < 8; i++) {
            int x = currentPos.x + dx[i];
            int y = currentPos.y + dy[i];
            if ((unsigned int)x >= w || (unsigned int)y >= h || isinf(costs(x, y))) {
                continue;
            }

            double newDistance = currentEntry.cost + costs(x, y) * dc[i];
            if (newDistance < distances(x, y)) {
                distances(x, y) = newDistance;
                pq.push(PathfindingEntry(newDistance, Point2DI(x, y)));
            }
        }
    }

    // Clear queue (required as it is reused for the next pathfinding call)
    while (!pq.empty())
        pq.pop();
    return distances;
}

/** Returns the shortest path between the start and end point.
 * The costs per cell are given by the costs map.
 * Diagonal movement costs sqrt(2) times more than axis aligned movement.
 */
vector<Point2DI> getPath(const Point2DI from, const Point2DI to, const InfluenceMap& costs) {
    static vector<vector<double> > cost(MAX_MAP_SIZE, vector<double>(MAX_MAP_SIZE));
    static vector<vector<int> > version(MAX_MAP_SIZE, vector<int>(MAX_MAP_SIZE));
    static vector<vector<Point2DI> > parent(MAX_MAP_SIZE, vector<Point2DI>(MAX_MAP_SIZE));
    static priority_queue<PathfindingEntry> pq;
    static int pathfindingVersion = 0;

    int w = costs.w;
    int h = costs.h;

    // Make sure map is sane
    assert(w <= MAX_MAP_SIZE);
    assert(h <= MAX_MAP_SIZE);

    pathfindingVersion++;

    pq.push(PathfindingEntry(0.0, from));
    cost[from.x][from.y] = 0;
    version[from.x][from.y] = pathfindingVersion;
    parent[from.x][from.y] = from;
    bool reached = false;

    while (!pq.empty()) {
        auto currentEntry = pq.top();
        auto currentPos = currentEntry.pos;
        pq.pop();
        if (currentEntry.cost > cost[currentPos.x][currentPos.y]) {
            continue;
        }

        if (currentEntry.pos == to) {
            reached = true;
            break;
        }

        for (int i = 0; i < 8; i++) {
            int x = currentPos.x + dx[i];
            int y = currentPos.y + dy[i];
            if (x < 0 || x >= w || y < 0 || y >= h) {
                continue;
            }
            // TODO: sqrt can be optimized
            double newCost = currentEntry.cost + costs(x, y) * dc[i];
            if ((newCost < cost[x][y] || version[x][y] != pathfindingVersion) && isfinite(cost[x][y])) {
                cost[x][y] = newCost;
                parent[x][y] = currentPos;
                version[x][y] = pathfindingVersion;
                pq.push(PathfindingEntry(newCost, 0 /* abs(x - to.x) + abs(y - to.y) */, Point2DI(x, y)));
            }
        }
    }

    // Clear queue (required as it is reused for the next pathfinding call)
    while (!pq.empty())
        pq.pop();

    if (!reached)
        return vector<Point2DI>();

    Point2DI currentPos = to;
    vector<Point2DI> path = { currentPos };
    while (currentPos != from) {
        auto p = parent[currentPos.x][currentPos.y];
        path.push_back(p);
        currentPos = p;
    }
    reverse(path.begin(), path.end());
    return path;
}