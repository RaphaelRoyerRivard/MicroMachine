#pragma once

#include <stdexcept>

class AlphaBetaTimeOut : public std::runtime_error
{
public:
    AlphaBetaTimeOut() : std::runtime_error("AlphaBetaTimeOut") { };
    ~AlphaBetaTimeOut() = default;
};