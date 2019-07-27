#pragma once
#include <vector>
#include <libvoxelbot/utilities/mappings.h>
#if LIBVOXELBOT_ENABLE_PYTHON
#include <pybind11/pybind11.h>
#endif

struct BuildResources;

struct BuildOptimizerNN {
#if LIBVOXELBOT_ENABLE_PYTHON
    pybind11::object predictFunction;
#endif

    void init();

    std::vector<std::vector<float>> predictTimeToBuild(const std::vector<std::pair<int, int>>& startingState, const BuildResources& startingResources, const std::vector < std::vector<std::pair<int, int>>>& targets) const;
};
