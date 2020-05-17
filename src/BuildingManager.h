#pragma once

#include "Common.h"
#include "BuildingPlacer.h"
#include <list>

class CCBot;

class BuildingManager
{
    CCBot &   m_bot;
	uint32_t m_lastLowPriorityFrame = 0;
	bool firstFrame = true;
    BuildingPlacer  m_buildingPlacer;
    std::vector<Building> m_buildings; //under construction
	std::vector<Building> m_previousBuildings; //previous under construction
	std::map<sc2::Tag, float> m_buildingsProgress;
	std::map<sc2::Tag, Unit> m_buildingsNewWorker;
	std::vector<Unit> m_baseBuildings;
	std::vector<Unit> m_finishedBaseBuildings;
	std::vector<Unit> m_previousBaseBuildings; //Base buildings last frame, useful to find dead buildings
	std::list<CCTilePosition> m_rampTiles;
	CCPosition m_enemyMainRamp;
	CCTilePosition m_proxyLocation;
	CCPosition m_proxyLocation2;
	CCPosition m_proxyBarracksPosition;
	CCPosition m_proxyFactoryPosition;
	bool m_proxySwapInitiated = false;
	bool m_proxySwapDone = false;
	bool m_barracksSentToEnemyBase = false;
	std::list<CCTilePosition> m_wallBuildingPosition;
	std::list<Unit> m_wallBuildings;
	std::map<UnitType, std::list<CCTilePosition>> m_nextBuildingPosition;
	std::vector<std::pair<CCTilePosition, CCTilePosition>> m_previousNextBuildingPositionByBase;
	std::map<sc2::Tag, CCPosition> liftedBuildingPositions;

    bool            m_debugMode;

    bool            isBuildingPositionExplored(const Building & b) const;
	void			castBuildingsAbilities();
	void			RunProxyLogic();
	void			LiftOrLandDamagedBuildings();
	void			updateBaseBuildings();

    void            validateWorkersAndBuildings();		    // STEP 1
    void            assignWorkersToUnassignedBuildings();	// STEP 2
	bool            assignWorkerToUnassignedBuilding(Building & b, bool filterMovingWorker = true);
    void            constructAssignedBuildings();			// STEP 3
    void            checkForStartedConstruction();			// STEP 4
    void            checkForDeadTerranBuilders();			// STEP 5
    void            checkForCompletedBuildings();			// STEP 6

    char            getBuildingWorkerCode(const Building & b) const;

public:

    BuildingManager(CCBot & bot);

	bool IsProxySwapDone() const { return m_proxySwapDone; }
    void                onStart();
	void				onFirstFrame();
    void                onFrame(bool executeMacro);
	void				lowPriorityChecks();
	void				FindRampTiles(std::list<CCTilePosition> &rampTiles, std::list<CCTilePosition> &checkedTiles, CCTilePosition currentTile);
	void				FindMainRamp(std::list<CCTilePosition> &rampTiles);
	std::vector<CCTilePosition> FindRampTilesToPlaceBuilding(std::list<CCTilePosition> &rampTiles);
	void				PlaceWallBuildings(std::vector<CCTilePosition> tilesToBlock);
	bool				ValidateSupplyDepotPosition(std::list<CCTilePosition> buildingTiles, CCTilePosition possibleTile);
	void FindOpponentMainRamp();
	bool				addBuildingTask(Building & b, bool filterMovingWorker = true);
	bool				isConstructingType(const UnitType & type);
    void                drawBuildingInformation();
	void				drawStartingRamp();
	void				drawWall();
	std::vector<Building> & getBuildings();
	std::vector<Building> getPreviousBuildings();//Cannot be passed by reference
	std::vector<Unit>	getBaseBuildings();
	std::vector<Unit>	getFinishedBuildings();
	std::vector<Unit>	getPreviousBaseBuildings();
	CCTilePosition		getWallPosition() const;
	std::list<Unit>		getWallBuildings();
	CCPosition			getEnemyMainRamp() const { return m_enemyMainRamp; }
	CCTilePosition		getProxyLocation();
	CCPosition			getProxyLocation2();
    CCTilePosition      getBuildingLocation(const Building & b, bool checkInfluenceMap);
	CCTilePosition		getNextBuildingLocation(Building & b, bool checkNextBuildingPosition, bool checkInfluenceMap);
	int					getBuildingCountOfType(const sc2::UNIT_TYPEID & b, bool isCompleted = false) const;
	int					getBuildingCountOfType(std::vector<sc2::UNIT_TYPEID> & b, bool isCompleted = false) const;
	Unit				getClosestResourceDepot(CCPosition position);
	const sc2::Unit *	getClosestMineral(const CCPosition position) const;
	const sc2::Unit *	getLargestCloseMineral(const CCTilePosition position, bool checkUnderAttack = false, std::vector<CCUnitID> skipMinerals = {}) const;
	const sc2::Unit *	getLargestCloseMineral(const Unit unit, bool checkUnderAttack = false, std::vector<CCUnitID> skipMinerals = {}) const;

    bool                isBeingBuilt(UnitType type) const;
	bool				isWallPosition(int x, int y) const;
	int					countBeingBuilt(UnitType type, bool underConstruction = false) const;
	int					countBoughtButNotBeingBuilt(sc2::UNIT_TYPEID type) const;

	void				removeBuildings(const std::vector<Building> & toRemove);
	void				removeNonStartedBuildingsOfType(sc2::UNIT_TYPEID type);

	void				updatePreviousBuildings();
	void				updatePreviousBaseBuildings();
	
	Building			CancelBuilding(Building b);
	Building			getBuildingOfBuilder(const Unit & builder) const;

	BuildingPlacer& getBuildingPlacer();

    std::vector<UnitType> buildingsQueued() const;
};
