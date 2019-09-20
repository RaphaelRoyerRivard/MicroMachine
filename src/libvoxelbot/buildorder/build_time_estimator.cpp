#include "build_time_estimator.h"
#include "../utilities/python_utils.h"
#include "../combat/simulator.h"

using namespace std;
using namespace sc2;

void BuildOptimizerNN::init() {
}

vector<vector<float>> BuildOptimizerNN::predictTimeToBuild(const vector<pair<int, int>>& startingState, const BuildResources& startingResources, const vector < vector<pair<int, int>>>& targets) const {
    return vector<vector<float>>(targets.size(), vector<float>(3));
}
