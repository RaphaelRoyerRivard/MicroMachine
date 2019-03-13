#pragma once

#include "Common.h"
#include "BuildingPlacer.h"

class CCBot;

class BuildingManager
{
    CCBot &   m_bot;

	bool firstFrame = true;
    BuildingPlacer  m_buildingPlacer;
    std::vector<Building> m_buildings; //under construction
	std::vector<Building> m_previousBuildings; //previous under construction
	std::map<sc2::Tag, float> m_buildingsProgress;
	std::map<sc2::Tag, Unit> m_buildingsNewWorker;
	std::vector<Unit> m_baseBuildings;
	std::vector<Unit> m_finishedBaseBuildings;
	std::vector<Unit> m_previousBaseBuildings; //Base buildings last frame, useful to find dead buildings
	std::list<CCTilePosition> rampTiles;
	std::list<CCTilePosition> wallBuilding;
	std::map<UnitType, std::list<CCTilePosition>> nextBuildingPosition;

    bool            m_debugMode;

    bool            isBuildingPositionExplored(const Building & b) const;
	void			castBuildingsAbilities();
	Building		CancelBuilding(Building b);
	void			updateBaseBuildings();

    void            validateWorkersAndBuildings();		    // STEP 1
    void            assignWorkersToUnassignedBuildings();	// STEP 2
	Unit            assignWorkersToUnassignedBuilding(Building & b);
    void            constructAssignedBuildings();			// STEP 3
    void            checkForStartedConstruction();			// STEP 4
    void            checkForDeadTerranBuilders();			// STEP 5
    void            checkForCompletedBuildings();			// STEP 6

    char            getBuildingWorkerCode(const Building & b) const;

public:

    BuildingManager(CCBot & bot);

    void                onStart();
	void				onFirstFrame();
    void                onFrame();
	void				lowPriorityChecks();
	void				FindRampTiles(std::list<CCTilePosition> &rampTiles, std::list<CCTilePosition> &checkedTiles, CCTilePosition currentTile);
	void				FindMainRamp(std::list<CCTilePosition> &rampTiles);
	std::vector<CCTilePosition> FindRampTilesToPlaceBuilding(std::list<CCTilePosition> &rampTiles);
	void				BuildingManager::PlaceSupplyDepots(std::vector<CCTilePosition> tilesToBlock);
	bool				ValidateSupplyDepotPosition(std::list<CCTilePosition> buildingTiles, CCTilePosition possibleTile);
    void                addBuildingTask(const UnitType & type, const CCTilePosition & desiredPosition);
	bool				isConstructingType(const UnitType & type);
    void                drawBuildingInformation();
	void				drawStartingRamp();
	void				drawWall();
	std::vector<Building> getBuildings();//Cannot be passed by reference
	std::vector<Building> getPreviousBuildings();//Cannot be passed by reference
	std::vector<Unit>	getBaseBuildings();
	std::vector<Unit>	getFinishedBuildings();
	std::vector<Unit>	getPreviousBaseBuildings();
    CCTilePosition      getBuildingLocation(const Building & b, bool checkInfluenceMap);
	CCTilePosition		getNextBuildingLocation(const Building & b, bool checkNextBuildingPosition, bool checkInfluenceMap);
	int					getBuildingCountOfType(const sc2::UNIT_TYPEID & b, bool isCompleted = false) const;
	int					getBuildingCountOfType(std::vector<sc2::UNIT_TYPEID> & b, bool isCompleted = false) const;
	Unit				getClosestResourceDepot(CCPosition position);
	const sc2::Unit *	getClosestMineral(const sc2::Unit * unit) const;

    bool                isBeingBuilt(UnitType type) const;
	int					countBeingBuilt(UnitType type) const;
	int					countBoughtButNotBeingBuilt(sc2::UNIT_TYPEID type) const;

	void				removeBuildings(const std::vector<Building> & toRemove);
	void				removeNonStartedBuildingsOfType(sc2::UNIT_TYPEID type);

	void				updatePreviousBuildings();
	void				updatePreviousBaseBuildings();

	BuildingPlacer& getBuildingPlacer();

    std::vector<UnitType> buildingsQueued() const;
};
