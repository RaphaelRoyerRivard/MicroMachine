#pragma once

#include "Common.h"
#include "Unit.h"
#include "UnitType.h"

namespace BuildingStatus
{
    enum { Unassigned = 0, Assigned = 1, UnderConstruction = 2, Size = 3 };
}

class Building
{
public:

    CCTilePosition  desiredPosition;
    CCTilePosition  finalPosition;
    CCTilePosition  position;
    UnitType        type;
    Unit            buildingUnit;//Building
    Unit            builderUnit;//Worker
    size_t          status;
    int             lastOrderFrame;
    bool            buildCommandGiven;
    bool            underConstruction;
	bool			reserveResources = true;
	bool			canBeBuiltElseWhere = true;

    Building();

    // constructor we use most often
    Building(UnitType t, CCTilePosition desired);

    // equals operator
    bool operator == (const Building & b);
};
