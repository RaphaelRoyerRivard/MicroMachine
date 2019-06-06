// Identical to including some cereal files, but without the annoying warnings

#pragma once
#pragma GCC diagnostic ignored "-Wunused-private-field"
// Fix annoying noexcept warnings
#include <cassert>
#define CEREAL_RAPIDJSON_ASSERT(x) assert(x)
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#pragma GCC diagnostic warning "-Wunused-private-field"