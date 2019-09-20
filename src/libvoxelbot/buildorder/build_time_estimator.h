#pragma once
#include <vector>
#include "../utilities/mappings.h"

struct BuildResources;

struct BuildOptimizerNN {

    void init();

    std::vector<std::vector<float>> predictTimeToBuild(const std::vector<std::pair<int, int>>& startingState, const BuildResources& startingResources, const std::vector < std::vector<std::pair<int, int>>>& targets) const;
};
