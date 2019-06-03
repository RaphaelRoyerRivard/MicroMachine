#pragma once
#include "sc2api/sc2_api.h"
#include <vector>

struct DependencyAnalyzer {
	std::vector<std::vector<sc2::UNIT_TYPEID>> allUnitDependencies;
	void analyze();
};
