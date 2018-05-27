#pragma once

#include <stdexcept>

class UCTCDTimeOut : public std::runtime_error
{
public:
    UCTCDTimeOut() : std::runtime_error("UCTCDTimeOut") { };
    ~UCTCDTimeOut() = default;
};